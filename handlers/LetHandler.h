#pragma once
#ifndef LET_HANDLER_H
#define LET_HANDLER_H

#include "../CodegenContext.h"
#include "../AddressResolver.h"
#include "../postfix.h"
#include "../Stacks.h"
#include "../TempPool.h"
#include "../Opcode.h"
#include "../MemoryLayout.h"
#include "../TokenUtils.h"
#include "../StringLiteralAllocator.h"
#include "../FnNameValidator.h"
#include "FunctionHandler.h"

#include <string>
#include <sstream>
#include <vector>
#include <iostream>

using std::string;
using std::vector;
using std::cerr;
using std::endl;

class FunctionHandler;

class LetHandler {
public:
	void execute(const string& command, std::istringstream& iss, CodegenContext& ctx,FunctionHandler& fnHandler) {

		string arg1, sign, expression, rest;
		iss >> arg1 >> sign;

		if (sign != "=")
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Invalid operand" << endl;
			return;
		}

		auto [indexLoc, isIndirect] = resolveToken(arg1, ctx.st, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError, ctx.currentFn);

		//get the size of the argument so the string can be allocated
		int arg1Size = ctx.st.isParameterOf(ctx.currentFn, arg1) ? 0 : ctx.st.getSize(arg1, ctx.currentFn);
		if (indexLoc == -1 || arg1Size == -1)
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: Variable '" << arg1 << "' could not be Located" << endl;
			return;
		}
		if (ctx.st.type(arg1, ctx.currentFn) == "array")
		{
			//I want to allow arrays to be initalized with 0 
			//This is because i encountered issue in dynamic allocation of arrays when 
			// they are not initialized with 0
		}
		else {
			ctx.st.initializeVariable(arg1, indexLoc, ctx.currentFn);
		}

		getline(iss, rest);
		size_t start = rest.find_first_not_of(' ');
		if (start != string::npos)
		{
			rest = rest.substr(start);
		}

		if (rest.empty())
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: Missing argument for print statement" << endl;
			return;
		}
		else if (rest.size() > 1 && rest.front() == '"' && rest.back() == '"')
		{
			handleLiteral(rest, arg1, indexLoc, isIndirect, arg1Size, ctx);
		}

		//this is to allow variables to be assigned values returned from functions
		//e.g let a = call add(a,b)
		else if (rest.substr(0, 5) == "call ")
		{
			handleCall(rest, arg1, indexLoc, isIndirect, arg1Size, ctx, fnHandler);
		}
		else
		{
			handleExpression(rest, indexLoc, isIndirect, ctx);
		}
	}
private:
	InfixToPostfix intpo;
	postfix post;

	void handleLiteral(const string& rest, const string& arg1, int indexLoc, bool isIndirect, int arg1Size, CodegenContext& ctx)
	{
		string literal = rest.substr(1, rest.size() - 2);
		//The location of the local variable or parameter is known at runtime so the literals have to be allocated 
		//also at runtime and its very expensive
		if (isIndirect)
		{
			// A string parameter holds a POINTER passed in by the caller — its real
			// storage size isn't known here, so in-place literal assignment isn't
			// supported for parameters yet. Only local (non-parameter) strings own
			// their own storage and can be written this way.
			if (ctx.st.isParameterOf(ctx.currentFn, arg1))
			{
				ctx.hasError = true;
				cerr << "Error on line " << ctx.currentLineNum << ": ";
				cerr << "Error: Cannot assign a string literal directly to string parameter '" << arg1 << "'" << endl;
				return;
			}
			int baseOffset = ctx.st.lookupLocal(ctx.currentFn, arg1);
			if (baseOffset == -1)
			{
				ctx.hasError = true;
				cerr << "Error on line " << ctx.currentLineNum << ": ";
				cerr << "Error: Could not resolve local string variable '" << arg1 << "'" << endl;
				return;
			}
			allocateLocalStringLiteral(literal, baseOffset, arg1Size, ctx.instructionMemory, ctx.st, ctx.nextInstruction, ctx.nextData, ctx.hasError);
		}
		else {
			assignGlobalStringLiteral(literal, indexLoc, arg1Size, ctx.instructionMemory, ctx.st, ctx.nextInstruction, ctx.nextData, ctx.hasError);
		}
	
	}

	void handleCall(string rest, string& arg1, int indexLoc, bool isIndirect, int arg1Size, CodegenContext& ctx, FunctionHandler& fnHandler) {

		string subCommand = rest.substr(0, 4);
		//to move past "call "
		rest = rest.substr(5);
		//to trim whitespaces
		size_t start = rest.find_first_not_of(" \t\r\n");
		if (start != string::npos)
		{
			rest = rest.substr(start);
		}

		//This is to get the function name so as to get the return type
		auto [fnName, isValid, isDefinition, parameters, parameterTypes] = validateFnName(rest, ctx.hasError, ctx.currentLineNum);
		if (!isValid)
		{
			return;
		}

		string fnType = ctx.st.getReturnType(fnName);
		string type = ctx.st.type(arg1, ctx.currentFn);
		if (ctx.st.isFunctionDefined(fnName))
		{
			//if the return type does not match the assigned variable type
			if ((type != fnType) && !isArrayWithIndex(arg1))
			{
				ctx.hasError = true;
				cerr << "Error on line " << ctx.currentLineNum << ": ";
				cerr << "Error: argument '" << arg1
					<< "' type mismatch - expected " << fnType
					<< " but found " << ctx.st.type(arg1, ctx.currentFn) << endl;
				return;
			}
			//If the a string is to be returned
			if (ctx.st.type(arg1, ctx.currentFn) == "string")
			{
				//The function must be defined for it
				if (!ctx.st.isFunctionDefined(fnName))
				{
					ctx.hasError = true;
					cerr << "Error on line " << ctx.currentLineNum << ": ";
					cerr << "Error: function '" << fnName << "' must be defined before being called for its string return value" << endl;
					return;
				}
				//If the function doesnt return string , or if the function tries to return features that are not supported
				int returnedSize = ctx.st.getReturnedLiteralSize(fnName);
				if (returnedSize == -1)
				{
					ctx.hasError = true;
					cerr << "Error on line " << ctx.currentLineNum << ": ";
					cerr << "Error: function '" << fnName << "' does not return a string literal" << endl;
					return;
				}

				if (arg1Size < returnedSize)
				{
					ctx.hasError = true;
					cerr << "Error on line " << ctx.currentLineNum << ": ";
					cerr << "\nError: destination string '" << arg1 << "' (size " << arg1Size
						<< ") is too small for returned string (size " << returnedSize << ")" << endl;
					return;
				}
			}
		}
		//If we only have the function declaration, we dont know the function return type so we map the argument name to the type expected and name of the function
		else if (ctx.st.isFunctionDeclared(fnName))
		{
			//a string returning function's literal address/size are only known once the body has been compiled
			//so if arg1 expects a string result and the function has not been declared,it raises an error
			if (type == "string")
			{
				ctx.hasError = true;
				cerr << "Error on line " << ctx.currentLineNum << ": ";
				cerr << "\nError: function '" << fnName << "' must be defined before being called for its string return value (forward declarations are not supported for string-returning functions)" << endl;
				return;
			}

			ctx.fnRetFlags[arg1][type] = fnName;
		}
		else
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "\nError: Function declaration return type not supported yet, Function definition needed";
			return;
		}

		if (isIndirect) {
			//IndexLox must be a runtime address which must survive recursion
			//if it is stored in a fixed cell its value will be overwritten by the last value during recursion
			//so i want to push the actual runtime value onto the stack before calling the function
			//load the location into the accumulator
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + indexLoc;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SP_PUSH;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SP_INC;
		}

		//This is to generate the function call instruction 
		// e.g let a = call add(a,b) the actual call instruction is generated in the function handler class so i call it here to avoid code duplication
		fnHandler.generateFunctionCall(fnName, parameters, ctx);

		//If the assigned variable is a string
		if (ctx.st.type(arg1, ctx.currentFn) == "string") {
			int srcBase = ctx.st.getReturnedLiteralAddr(fnName);
			int returnedSize = ctx.st.getReturnedLiteralSize(fnName);

			//copying the string literal into the variable
			for (int i = 0; i < returnedSize; i++)
			{
				if (!isIndirect)
				{
					ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + (srcBase + i);
					ctx.instructionMemory[ctx.nextInstruction++] = Op::STORE + (indexLoc + i);
				}

				//if the variable is a function local variable or parameter
				else {
					// LOCAL vs PARAMETER destination — why we can't treat 'indexLoc' the same way
							// By this point in the code, resolveToken() has already run on arg1 (the
							// destination variable name) and told us two things via (indexLoc, isIndirect):
							//   - isIndirect == true means "indexLoc is NOT the final destination address
							//     itself — it's a TEMP CELL that, at runtime, holds some address."
							//   - But WHICH address ends up in that temp cell depends entirely on whether
							//     arg1 is a function parameter or a local variable, because those two
							//     things are resolved by completely different functions:

							//     resolveParameterAddress() -> temp cell holds (paramBase + paramIndex).
							//         That location is the PARAMETER SLOT. For string/array parameters,
							//         the slot does NOT contain the string's actual header/data words —
							//         it contains a POINTER that was written into it back at the CALL
							//         SITE (see the "call" codegen: arrays/strings decay to their base
							//         address before being pushed into the parameter stack). So the
							//         parameter slot is a pointer TO a pointer, effectively:
							//             memory[indexLoc]         -> the parameter SLOT's address
							//             memory[memory[indexLoc]] -> the pointer stored there
							//             memory[memory[memory[indexLoc]] + i] -> the actual i'th word
							//         We only need to go one level deeper than indexLoc itself, i.e.
							//         ONE INDIRECT_LOAD off indexLoc gives us the real base address.
							//
							//     resolveLocalAddress() -> temp cell ALSO holds (paramBase + offset),
							//         but here that IS the address where the local string's own
							//         header+data words physically live — a local string variable
							//         OWNS its storage directly, unlike a parameter which just holds
							//         a pointer to someone else's storage. So:
							//             memory[indexLoc] -> the local variable's base address
							//         Reading indexLoc's VALUE (a plain LOAD, no INDIRECT) already
							//         gives us the destination base address directly. Doing an
							//         INDIRECT_LOAD here would be one dereference too many — it would
							//         read whatever DATA currently happens to be sitting in the local
							//         string's first word (e.g. its header word, a leftover value from
							//         a previous assignment, or garbage on first use) and then treat
							//         THAT number as if it were a memory address to write through.
							//         That's the bug: silently corrupting memory at a "random" address
							//         instead of writing to the string.
							//
							// This exact same fork already exists in handlePrintArg() — see how it
							// checks isParameterString before deciding whether to fully resolve the
							// address via resolveIfIndirect() (parameter: extra dereference) or hand
							// addr straight to PRINT_STRING_INDIRECT (local: single dereference,
							// which PRINT_STRING_INDIRECT's opcode 53 handling does internally by
							// dereferencing memory[location] once). We're doing the write-side
							// equivalent of that same read-side distinction here.
							//
							// isIndirect alone can't tell us which of the two cases we're in — both
							// paths set isIndirect = true. We need a SEPARATE check (is arg1 actually
							// a parameter of the current function?) to disambiguate.

					bool isParamDest = ctx.st.getParameterIndex(ctx.currentFn, arg1) != -1;
					int dataCell = tempPool.acquire(ctx.hasError);
					if (dataCell == -1)
						return;

					// srcBase is a compile-time-known constant (the literal's address in the
							// global data region, computed once via allocateStringLiteral back at 'ret'
							// time), so srcBase + i can be baked directly into the LOAD opcode's operand
							// -- no runtime address arithmetic needed for the SOURCE side at all.
							// We stash it in dataCell because the very next few instructions are going
							// to reuse the accumulator to compute the DESTINATION address, and we can't
							// let that clobber the source word we just loaded.
					ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + (srcBase + i);
					ctx.instructionMemory[ctx.nextInstruction++] = Op::STORE + dataCell;

					// Unlike the source, the destination's base address is NOT known at compile
					// time (it depends on paramBase, i.e. which call frame we're currently
					// inside), so this has to be computed with real instructions every time
					// this code executes, not baked into an operand like srcBase+i was.
					int destAddrCell = tempPool.acquire(ctx.hasError);
					if (destAddrCell == -1)
						return;

					// i is compile-time-known (it's the unrolled loop counter), but the VM has
					// no "add immediate" opcode — every ADD needs its operand to be a memory
					// location. getOrCreateConstant makes sure the literal number i has a home
					// in the constant pool (reusing one if this exact value was already needed
					// elsewhere) and pre-loads it into memory so ADD can read it.
					int iConstLoc = getOrCreateConstant(std::to_string(i), ctx.st, ctx.instructionMemory, ctx.nextData, ctx.hasError);

					if (isParamDest)
					{
						//The base address  of arg1 is known at runtime
						//so we load the value indirectly into the accumulator
						//dont foget function paramete contain a pointer to the base address of the original string
						//so we are going dereferencing back to the original string base address
						ctx.instructionMemory[ctx.nextInstruction++] = Op::INDIRECT_LOAD + indexLoc;
					}
					else {
						//arg1 is a local variable : it owns its storage, so the VALUE sitting
						// at indexLoc (paramBase + offset) already is the base address we want.
						// Plain LOAD, not INDIRECT_LOAD — going one level deeper here would
						// read the string's own data instead of its address (the bug we're
						// fixing).
						ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + indexLoc;
					}

					// add the value with the increment at runtime
					//e.g value is 100, increment is 2. accumulator = 112
					ctx.instructionMemory[ctx.nextInstruction++] = Op::ADD + iConstLoc;
					//store the address to be filled in a temporary address
					ctx.instructionMemory[ctx.nextInstruction++] = Op::STORE + destAddrCell;

					//load the string value(dont forget 2 characters per one data memory) into accumulator
					ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + dataCell;
					//load the value into the addressed to be filled
					//damn this is a beautiful art lol
					ctx.instructionMemory[ctx.nextInstruction++] = Op::INDIRECT_STORE + destAddrCell;
				}

				//The temporary storage only get released after the line has finished executing nut beacuase of the large amount of string 
				//the string might burn out the whole 20 spaces allocated to tempPool , the temporary storage is useless after being allocated
				//so i release it after every loop
				tempPool.release(2);
			}
			return;
		}

		// By the time the function has been called and the function returns the value
		//the value will be stored in the accumulator
		if (isIndirect)
		{
			int tempResult = tempPool.acquire(ctx.hasError);
			if (tempResult == -1)
			{
				return;
			}
			//save the return value into a temp address
			ctx.instructionMemory[ctx.nextInstruction++] = Op::STORE + tempResult;

			//decrement the parameter pointer to get the runtime address
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SP_DEC;
			//load the location into the accumulator
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SP_POP_TO_ACC;

			int savedAddrCell = tempPool.acquire(ctx.hasError);
			if (savedAddrCell == -1)
			{
				return;
			}
			//load the location of the runtime variable to be assigned the value
			ctx.instructionMemory[ctx.nextInstruction++] = Op::STORE + savedAddrCell;

			//reload the return value into the accumulator
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + tempResult;
			//store the return value indirectly into the runtime location of the variable
			ctx.instructionMemory[ctx.nextInstruction++] = Op::INDIRECT_STORE + savedAddrCell;
		}
		else {
			//then store the value into the assigned variable
			ctx.instructionMemory[ctx.nextInstruction++] = Op::STORE + indexLoc;
		}
	}

	void handleExpression(const string& rest, int indexLoc, bool isIndirect, CodegenContext& ctx) {
		vector<string> postfix = intpo.Infix(rest, ctx.hasError);
		auto [resultLoc, isindirect2] = post.generateCode(postfix, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.st, ctx.hasError, ctx.currentLineNum, ctx.currentFn);

		int storeOpcode = isIndirect ? Op::INDIRECT_STORE : Op::STORE;
		if (resultLoc != -1)
		{
			if (resultLoc == -999)
			{
				ctx.instructionMemory[ctx.nextInstruction++] = storeOpcode + indexLoc;
			}
			else
			{
				ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + resultLoc;
				ctx.instructionMemory[ctx.nextInstruction++] = storeOpcode + indexLoc;
			}
		}
	}

};
#endif // !LET_HANDLER_H
