#pragma once
#ifndef COMPILER_H
#define COMPILER_H

#define MEM_SIZE Mem::TOTAL_SIZE

#include "CodegenContext.h"
#include "MemoryLayout.h"
#include "SymbolTable.h"
#include "Parser.h"

#include <string>
#include<fstream>
#include <map>

using std::string;
using std::ios;

class Compiler
{
public:
	Compiler() {
		for (int i = 0; i < MEM_SIZE; i++) instructionMemory[i] = 0;
		//This initializes the memory space 0 till global_end to -1 
		//299 is for holding return address for gosub statements
		for (int i = 0;i <= Mem::GLOBAL_END;i++)
		{
			instructionMemory[i] = -1;
		}
		instructionMemory[299] = 0;

		for (int i = 0; i < MEM_SIZE; i++) {
			flags[i] = -1;
		}
		//This initializes the memory location for the temporary location range
		tempPool.initialize(Mem::TEMP_START, Mem::TEMP_END);
	}
	int compile(const string& filename)
	{
		std::ifstream source(filename);
		if (!source)
		{
			cerr << "File could not be opened!";
			return -1;
		}
		string line;
		bool hasError = false;
		while (getline(source, line))
		{
			string currentFn = parser.currentFunctionName(); //To get current function name
			symbolTable.interpret(line, nextData, hasError, currentFn);
			symbolTable.intializeConstants(instructionMemory);

			CodegenContext codeContext{
				instructionMemory,
				nextInstruction,
				nextData,
				hasError,
				0,
				symbolTable,
				lineToAddr,
				flags,
				fnFlags,
				fnRetFlags,
				currentFn
			};

			parser.parseLine(line, codeContext);

			//Once the instruction or data run out of memory space the compiler should stop compilation process
			if (stopInstruction) {
				break;
			}
		}
		symbolTable.intializeConstants(instructionMemory);

		//This ensures that the last line of the source code is a halt statement
		// This prevents the program from running into -1
		if (nextInstruction == 0 || instructionMemory[nextInstruction - 1] != 4300)
		{
			if (nextInstruction < MEM_SIZE)
			{
				instructionMemory[nextInstruction++] = Op::HALT; // Add halt instruction at the end
			}
			else
			{
				hasError = true;
				cerr << "Error: Not enough memory to append halt instruction" << endl;
			}
		}

		//To check if the block in if has been closed
		if (!parser.isBlockCloseds())
		{
			hasError = true;
			cerr << "Error: '}' not found";
		}

		if (!parser.isWhileClosed())
		{
			hasError = true;
			cerr << "Error : 'wend' not found";
		}
		if (!parser.isForNextClosed())
		{
			hasError = true;
			cerr << "Error: 'next' not found";
		}

		//The purpose of this is to patch the incorrect branch address when a function is called but never defined
		for (auto& [addr, name] : fnFlags)
		{
			int firstInstr = symbolTable.returnFnFirstInstruction(name);
			if (firstInstr == -1)
			{
				hasError = true;
				cerr << "Error: function '" << name << "' declared but never defined" << endl;
				continue; 
			}

			//This adds the first instruction address to the opcode which is Op::BRANCH
			int opcode = (instructionMemory[addr] / 1000) * 1000;
			instructionMemory[addr] = opcode + firstInstr;
		}
		//The purpose of this is to validate the function Return type against the variable type
		for (auto& [argument,innerMap] : fnRetFlags)
		{
			for (auto& [type, fnName] : innerMap)
			{
				/*if (!(symbolTable.isFunctionDefined(fnName))) {
					hasError = true;
					cerr << "Error: function '" << fnName << "' declared but never defined" << endl;
					continue;
				}*/
				string fnType = symbolTable.getReturnType(fnName);
				if ((type != fnType) && !isArrayWithIndex(argument))
				{
					hasError = true;
					cerr << "Error: argument '" << argument
						<< "' type mismatch - expected " << fnType
						<< " but found " << type << endl;
					continue;
				}

			}
		}
		if (hasError)
		{
			cerr<<"\nCompilation failed due to errors.No SML code generated" << endl;
			return -1;
		}

		//The purpose of this 
		for (int i = 0; i < nextInstruction; i++)
		{
			if (flags[i] != -1)
			{
				int targetLine = flags[i];
				if (lineToAddr.count(targetLine) > 0)
				{
					int targetAddr = lineToAddr[targetLine];
					int opcode = (instructionMemory[i] / 1000) * 1000;
					instructionMemory[i] = opcode + targetAddr;
				}
			}
		}
		
		std::ofstream readCode("SML code.txt", ios::out);
		if (!readCode)
		{
			cerr << "File could not be created";
			exit(1);
		}
		readCode.clear();

		for (int i = 0;i < MEM_SIZE;i++)
		{
			readCode << instructionMemory[i];
			readCode << endl;
		}

		return 0;
	}
private:
	int nextData = Mem::GLOBAL_END;
	int flags[MEM_SIZE];
	SymbolTable symbolTable;
	Parser parser;
	double instructionMemory[MEM_SIZE];
	int nextInstruction = Mem::INSTRUCTION_START;
	std::map<int, int> lineToAddr;
	std::map<int, string> fnFlags;
	std::map<string, std::map<string,string>>fnRetFlags;
};

#endif