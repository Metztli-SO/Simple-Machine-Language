#pragma once
#ifndef INCREMENT_DECREMENT_HANDLER_H
#define INCREMENT_DECREMENT_HANDLER_H

#include "../compiler/CodegenContext.h"
#include "../memory/AddressResolver.h"
#include "../sml/Opcode.h"

#include <string>

using std::string;

class IncrementDecrementHandler {
public:
	void execute(const string& command, std::istringstream& iss, CodegenContext& ctx)
	{
		size_t pos;

		bool decrement = false;
		bool increment = ((pos = command.find("++")) != string::npos);

		if (!increment)
			decrement = ((pos = command.find("--")) != string::npos);
		if (increment || decrement)
		{
			string tok;
			tok = command.substr(0, pos);
			auto [addr, isIndirect] = resolveToken(tok, ctx.st, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError, ctx.currentFn);
			int storeOpcode = isIndirect ? Op::INDIRECT_STORE : Op::STORE;
			int loadOpcode = isIndirect ? Op::INDIRECT_LOAD : Op::LOAD;
			int addrFor1 = getOrCreateConstant("1", ctx.st, ctx.instructionMemory, ctx.nextData, ctx.hasError);
			if (increment) {
				ctx.instructionMemory[ctx.nextInstruction++] = loadOpcode + addr;
				ctx.instructionMemory[ctx.nextInstruction++] = Op::ADD + addrFor1;
				ctx.instructionMemory[ctx.nextInstruction++] = storeOpcode + addr;
			}
			else if (decrement)
			{
				ctx.instructionMemory[ctx.nextInstruction++] = loadOpcode + addr;
				ctx.instructionMemory[ctx.nextInstruction++] = Op::SUB + addrFor1;
				ctx.instructionMemory[ctx.nextInstruction++] = storeOpcode + addr;
			}
		}
		else {
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "\nError: Unknown Command" << endl;
			return;
		}
	}
};
#endif // !INCREMENT_DECREMENT_HANDLER_H
