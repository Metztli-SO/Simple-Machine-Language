#pragma once 
#ifndef DECLARATION_HANDLER_H
#define DECLARATION_HANDLER_H

#include "../CodegenContext.h"
#include "../AddressResolver.h"
#include "../Opcode.h"

#include <string>
#include <sstream>

using std::string;

class DeclarationHandler {
public:
	void execute(const string& command, std::istringstream& iss, CodegenContext& ctx)
	{
		if (command == "array") {
			parseArray(iss, ctx);
		}
		else if (command == "string") {
			parseString(iss, ctx);
		}
	}

private:
		void parseArray(std::istringstream & iss, CodegenContext & ctx) {
			string size, arg1;

			iss >> arg1 >> size;

			int arraySize = std::stoi(size);
			if (!ctx.currentFn.empty())
			{
				//we need to add a constant holding 0 to the symbol table if it doesn't already exist,
				// so we can use it to zero out the array
				int zeroConstLoc = getOrCreateConstant("0", ctx.st, ctx.instructionMemory, ctx.nextData, ctx.hasError);

				// local array — zero it at RUNTIME, via paramBase + offset for each slot
				// the base offset is known to the symbol table, so we can look it up
				//we add keep adding 1 to the base offset to get the next address of the array
				int offset = ctx.st.lookupLocal(ctx.currentFn, arg1);   // base offset of this array
				for (int i = 0; i < arraySize; i++)
				{
					auto [loc, isInd] = resolveLocalAddress(offset + i, ctx.instructionMemory, ctx.st, ctx.nextInstruction, ctx.nextData, ctx.hasError);
					ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + zeroConstLoc;
					ctx.instructionMemory[ctx.nextInstruction++] = (isInd ? Op::INDIRECT_STORE : Op::STORE) + loc;
				}
			}
			else
			{
				//global array — zero it at COMPILE TIME, in the instruction memory
				//its base address is already allocated in the symbol table, so just fill it with 0s
				int addr = parseAddress(arg1, ctx.st);
				for (int i = 0; i < arraySize; ++i) {
					ctx.instructionMemory[addr + i] = 0;
				}
			}

			return;
		}

		void parseString(std::istringstream& iss, CodegenContext& ctx) {
			string stringName;
			int size, nameAddr;
			iss >> stringName >> size;
			nameAddr = parseAddress(stringName, ctx.st);
			ctx.instructionMemory[nameAddr] = size;
		}
};
#endif