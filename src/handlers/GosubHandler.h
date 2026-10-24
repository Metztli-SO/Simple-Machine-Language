#pragma once
#ifndef GOSUB_HANDLER_H
#define GOSUB_HANDLER_H

#include "../compiler/CodegenContext.h"
#include "../memory/MemoryLayout.h"
#include "../sml/Opcode.h"

#include <string>
#include <sstream>
#include <stack>
#include <iostream>

using std::string;
using std::cerr;
using std::endl;

//Note: the gosub rn doesnt support recursion because the return address is stored in a single memory location and it will be overwritten by the next gosub statement
class GosubHandler {
public:
	void execute(const string& command, std::istringstream& iss, CodegenContext& ctx) {
		if (command == "gosub") {
			parseGosub(iss, ctx);
		}
		else if (command == "return") {
			parseReturn(ctx);
		}
	}

	bool isGosubClosed() const {
		return returnStack.empty();
	}

private:
	std::stack <int> returnStack;

	//Memory location for holding gosub return address
	static const int tempForHoldingReturnAddr = Mem::GOSUB_RETURN_ADDR;

	void parseGosub(std::istringstream& iss, CodegenContext& ctx) {
		string arg1;
		iss >> arg1; //target line number
		int targetLine = std::stoi(arg1);

		int returnAddr = ctx.nextInstruction + 3;

		//This converts the returnaddr instruction to string because lookupconstant accepts string as an argument not integers
		string retStr = std::to_string(returnAddr);

		int returnAddrLoc = ctx.st.lookupConstant(retStr);
		//This ensures the return address already exist in the memory
		if (returnAddrLoc == -1)
		{
			ctx.st.assign(retStr, "const", ctx.nextData, ctx.hasError);
			returnAddrLoc = ctx.st.lookupConstant(retStr);
		}
		//This loads the return address directly into rhe memory
		ctx.instructionMemory[returnAddrLoc] = returnAddr;
		//Load the return address into the accumulator
		ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + returnAddrLoc;
		//load the rerutn address from the accumulator into temp location that will store the return address
		ctx.instructionMemory[ctx.nextInstruction++] = Op::STORE + tempForHoldingReturnAddr;
		if (ctx.lineToAddr.count(targetLine) > 0)
		{
			ctx.instructionMemory[ctx.nextInstruction] = Op::BRANCH + ctx.lineToAddr[targetLine];
		}
		else
		{
			ctx.instructionMemory[ctx.nextInstruction] = Op::BRANCH + 0;
			ctx.flags[ctx.nextInstruction] = targetLine;
		}


		returnStack.push(returnAddr);
		ctx.nextInstruction++;
	}
	void parseReturn(CodegenContext& ctx) {
		//This branches to the location inside the value of tempForHoldingReturnAddr i.e indirect branch
		ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_INDIRECT + tempForHoldingReturnAddr;
		//This is to ensure there is no return statement without a matching gosub statements
		//i.e a mere return will be an error
		if (!returnStack.empty())
		{
			int returnAddr = returnStack.top();
			returnStack.pop();
		}
		else
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: Return statement without matching Gosub" << endl;
			return;
		}
	}
};
#endif