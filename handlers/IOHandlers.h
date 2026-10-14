#pragma once
#ifndef IOHANDLERS_H
#define IOHANDLERS_H

#include "../CodegenContext.h"
#include "../AddressResolver.h"
#include "../postfix.h"
#include "../TempPool.h"
#include "../Opcode.h"
#include "../MemoryLayout.h"
#include "../TokenUtils.h"
#include "../Parser.h"
#include "../StringLiteralAllocator.h"

#include <sstream>
#include <string>
#include <iostream>
#include <algorithm>

using std::string;
using std::cerr;
using std::endl;
using std::cout;

class IOHandlers{
	public:
		void execute(const string& command, std::istringstream& iss, CodegenContext& ctx)
		{
			if (command == "input") {
				parseInput(iss, ctx);
			}
			else if (command == "print") {
				parsePrint(iss, ctx);
			}
		}
	private:
		InfixToPostfix intpo;
		postfix post;
		void parseInput(std::istringstream& iss, CodegenContext& ctx)
		{
			string arg1;
			while (iss >> arg1)
			{
				if (ctx.st.type(arg1, ctx.currentFn) == "string")
				{
					int adr = parseAddress(arg1, ctx.st);
					ctx.instructionMemory[ctx.nextInstruction++] = Op::READ_STRING + adr;
					return;
				}

				auto [adr, isIndirect] = resolveToken(arg1, ctx.st, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError, ctx.currentFn);


				if (adr == -1)
				{
					ctx.hasError = true;
					cerr << "Error on line " << ctx.currentLineNum << ": ";
					cerr << "Error: Variable '" << arg1 << "' could not be Located" << endl;
					continue;
				}
				if (ctx.st.type(arg1, ctx.currentFn) == "array")
				{
					//I want to allow arrays to be initalized with 0 
					//This is because i encountered issue in dynamic allocation of arrays when 
					// they are not initialized with 0
				}
				else {
					ctx.st.initializeVariable(arg1, adr, ctx.currentFn);
				}

				// Use 1300 for indirect addressing, 10000 for direct addressing
				int inputOpcode = isIndirect ? Op::INDIRECT_READ : Op::READ;
				ctx.instructionMemory[ctx.nextInstruction++] = inputOpcode + adr;

			}
		}
		void parsePrint(std::istringstream& iss, CodegenContext& ctx) {

			string rest;
			getline(iss, rest);

			//This checks if the print statement has an insertion operator i.e << to print multiple arguments
			bool insertionOperator = isInsertionOperator(rest, ctx.hasError);


			if (insertionOperator == true)
			{
				string current = rest;

				while (true)
				{
					size_t pos = findRealInsert(current);

					//To get each part e.g print a<<b gets a first then b second
					string part = (pos != string::npos) ? current.substr(0, pos) : current;

					//To get the first part after the space 
					//Note: when i get the whole line some space are added e.g print "hello,world" 
					//it gets from the position after t which means spaces are there
					size_t start = part.find_first_not_of(' ');
					size_t end = part.find_last_not_of(' ');

					if (start != string::npos)
					{
						//To get the real string literal to print
						part = part.substr(start, end - start + 1);
					}

					handlePrintArg(part, ctx.st, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError, ctx.currentLineNum,ctx.currentFn);

					//to break the loop when there is nothing to print
					if (ctx.hasError || pos == string::npos)
					{
						break;
					}

					//to move past the last string literal
					current = current.substr(pos + 2);
				}
			}
			else {
				size_t start = rest.find_first_not_of(" \t\r\n");
				size_t end = rest.find_last_not_of(" \t\r\n");
				if (start != string::npos)
				{
					rest = rest.substr(start, end - start + 1);
				}

				handlePrintArg(rest, ctx.st, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError, ctx.currentLineNum,ctx.currentFn);
			}
		}
		bool isInsertionOperator(const string& rest, bool& hasError)
		{
			size_t i = 0;

			while (i < rest.size())
			{
				size_t insertPos = rest.find("<<", i);
				size_t quoteStart = rest.find('"', i);

				if (insertPos == string::npos)
				{
					return false;
				}

				// << found before any opening quote — it's a real insertion operator
				if (quoteStart == string::npos || insertPos < quoteStart)
					return true;

				// There is a quote before the << , find the closing quote
				size_t quoteEnd = rest.find('"', quoteStart + 1);

				if (insertPos > quoteStart && insertPos < quoteEnd)
				{
					i = quoteEnd + 1;
				}
				else
				{
					return true;
				}
			}
			return false;
		}
		size_t findRealInsert(const string& str)
		{
			size_t i = 0;
			while (i < str.size())
			{
				size_t insertPos = str.find("<<", i);
				size_t quoteStart = str.find('"', i);

				if (insertPos == string::npos)
					return string::npos;

				// << comes before any quote — real insertion operator
				if (quoteStart == string::npos || insertPos < quoteStart)
					return insertPos;

				// A quote comes first, find the closing quote
				size_t quoteEnd = str.find('"', quoteStart + 1);

				if (quoteEnd == string::npos)
					return string::npos; // malformed string

				// << is inside the string literal, skip past it
				if (insertPos > quoteStart && insertPos < quoteEnd)
					i = quoteEnd + 1;
				else
					return insertPos; // << is after the closing quote
			}
			return string::npos;
		}
		void handlePrintArg(const string& rest, SymbolTable& st, double* instructionMemory,
			int& nextInstruction, int& nextData, bool& hasError, int currentLineNum,string& currentFn)
		{

			if (rest.empty())
			{
				hasError = true;
				cerr << "Error on line " << currentLineNum << ": ";
				cerr << "Error: Missing argument for print statement" << endl;
				return;
			}
			else if (rest.size() > 1 && rest.front() == '"' && rest.back() == '"')
			{
				string literal = rest.substr(1, rest.size() - 2);

				//I used Zero for print size because it is a string literal and it is not allocated to a variable
				//I used zero fot the starting address because it is a string literal and it is allocated at runtime
				//because it doesnt have a variable it is allocated to
				int addr = allocateStringLiterals(literal, nextData, instructionMemory, hasError, 0, 0,currentLineNum);
				instructionMemory[nextInstruction++] = Op::PRINT_STRING + addr;
				return;
			}
			else if (isExpressionLike(rest))
			{
				vector<string> exprPostfix = intpo.Infix(rest, hasError);
				auto [resultLoc, isIndirect] = post.generateCode(exprPostfix, instructionMemory, nextInstruction, nextData, st, hasError, currentLineNum, currentFn);
				if (hasError || resultLoc == -1)
				{
					hasError = true;
					cerr << "Error on line " << currentLineNum << ": ";
					cerr << "Error: Invalid expression '" << rest << "' in print statement." << endl;
					return;
				}
				// -999 means the last arithmetic op already left the value sitting in the accumulator —
				// same sentinel convention 'let' already relies on, 
				// there is no opcode to print directly from accumulator, so i load it into a temp storage then allocate it to resultLoc 
				if (resultLoc == -999)
				{
					resultLoc = tempPool.acquire(hasError);
					if (resultLoc == -1)
					{
						return;
					}
					instructionMemory[nextInstruction++] = Op::STORE + resultLoc;
				}
				instructionMemory[nextInstruction++] = Op::WRITE + resultLoc;
			}
			else
			{
				string arg1;
				std::istringstream iss(rest);
				while (iss >> arg1)
				{
					auto [addr, isIndirect] = resolveToken(arg1, st, instructionMemory, nextInstruction, nextData, hasError, currentFn);
					if (addr == -1)
					{
						hasError = true;
						cerr << "Error on line " << currentLineNum << ": ";
						cerr << "Error: Variable '" << arg1 << "' could not be Located" << endl;
						return;
					}

					if (st.type(arg1,currentFn) == "string")
					{
						bool isParameterString = false;

						//String printing is different 
						// for string parameters you have to use resolveifIndirect to fully resolve it
						// beacuse it contains a location of an address not the actual value/base address
						//but local string variable values are on the stack not a pointer/address of the value
						int paramIndex = st.getParameterIndex(currentFn, arg1);
						if (paramIndex != -1)
						{
							isParameterString = true;
						}

						if (isIndirect) {
							if (isParameterString) {
								//resolve fullly
								int resolvedAddr = resolveIfIndirect(addr, isIndirect, instructionMemory, nextInstruction, nextData, hasError);
								instructionMemory[nextInstruction++] = Op::PRINT_STRING_INDIRECT + resolvedAddr;
							}
							else {
								instructionMemory[nextInstruction++] = Op::PRINT_STRING_INDIRECT + addr;
							}
						}
						else
						{
							instructionMemory[nextInstruction++] = Op::PRINT_STRING + addr;
						}
						return;
					}

					if (st.type(arg1, currentFn) == "array")
					{
						//Array will always get free pass because it is initialized with 0
					}
					else {
						//This is to check if the variable has been initialized 
						//check symbolTable::checkInitializationStatus and isInitialized for more info
						if (!st.checkInitializationStatus(arg1, addr, currentFn))
						{
							hasError = true;
							cerr << "Error on line " << currentLineNum << ": ";
							cerr << "Error: Variable '" << arg1 << "' has not been Initialized" << endl;
							return;
						}
					}

					// Use 1300 for indirect addressing, 1100 for direct addressing
					int printOpcode = isIndirect ? Op::INDIRECT_WRITE : Op::WRITE;
					instructionMemory[nextInstruction++] = printOpcode + addr;
				}
			}
		}
};
#endif // !IOHANDLERS_H
