#pragma once
#ifndef CONTROL_FLOW_HANDLER_H
#define CONTROL_FLOW_HANDLER_H

#include "../CodegenContext.h"
#include "../AddressResolver.h"
#include "../Opcode.h"

#include <string>
#include <sstream>
#include <iostream>

using std::string;
using std::cerr;
using std::endl;

class Parser;

class ControlFlowHandler {
public:
	void execute(const string& command, std::istringstream& iss, CodegenContext& ctx, Parser& parser) {
		if (command == "if") {
			parseIf(iss, ctx, parser);
		}
		else if (command == "}")
		{
			parseCloseBrace(ctx);
		}
		else if (command == "goto")
		{
			parseGoto(iss, ctx);
		}
	}

	// Used by Parser::isBlockCloseds() for Compiler::compile's end-of-file check.
	bool blockClosed() const { 
		return ifStack.empty(); 
	}

private:
	//The purpose of this is mainly to check if the "if" keyword has two branch prediction
	//If i was to use a static variable to holf the double branch once it is changed it cant be changed back 
	// which means if keyord with a single branch check will suffer it and it will lead to bugs hard to locate
	struct IfFrame {
		int branchLocation;
		int jumpInstruction;
		bool doubleBranch;
	};
	std::stack<IfFrame> ifStack;

	void parseIf(std::istringstream& iss, CodegenContext& ctx, Parser& parser)
	{
		int jumpInstruction = 0;
		string arg1,sign, arg3, subCommand;
		iss >> arg1 >> sign >> arg3 >> subCommand;

		auto [loc1, isInd1] = resolveToken(arg1, ctx.st, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError, ctx.currentFn);
		auto [loc2, isInd2] = resolveToken(arg3, ctx.st, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError, ctx.currentFn);
		if (loc1 == -1 || loc2 == -1)
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: Variable '" << arg1 << "' could not be Located" << endl;
			return;
		}

		//This helps to prevent unitiliazed variables from going through
		bool check1 = !ctx.st.checkInitializationStatus(arg1, loc1, ctx.currentFn) && ctx.instructionMemory[loc1] == -1;
		bool check2 = (!ctx.st.checkInitializationStatus(arg3, loc2, ctx.currentFn) && ctx.instructionMemory[loc2] == -1);

		if (check1)
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: Variable '" << arg1 << "' has not been Initialized" << endl;
			return;
		}
		if (check2)
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: Variable '" << arg3 << "' has not been Initialized" << endl;
			return;
		}

		//This is very important because if it is indirect it returns the location not the value
		//resolve if indirect returns a location that stores the original value
		int adr1 = resolveIfIndirect(loc1, isInd1, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError);
		int adr2 = resolveIfIndirect(loc2, isInd2, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError);

		if (sign == "==")
		{
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + adr1;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SUB + adr2;
			jumpInstruction = ctx.nextInstruction + 2;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_NE + jumpInstruction;
		}
		else if (sign == "!=")
		{
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + adr1;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SUB + adr2;
			jumpInstruction = ctx.nextInstruction + 2;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_ZERO + jumpInstruction;
		}
		else if (sign == "<")
		{
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + adr2;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SUB + adr1;
			jumpInstruction = ctx.nextInstruction + 2;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_NEG + jumpInstruction;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_ZERO + jumpInstruction;
		}
		else if (sign == ">")
		{
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + adr1;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SUB + adr2;
			jumpInstruction = ctx.nextInstruction + 2;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_NEG + jumpInstruction;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_ZERO + jumpInstruction;
		}
		else {
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "\nError: Unknown Comparison operator '" << sign << "' " << endl;
			return;
		}

		//sign > and < has two branch check if i have two check for every sign there will be error and inconsistencies
		//in instructions so this bool checks if it has < or > 
		bool doublebranchLocation = (sign == ">" || sign == "<");
		if (subCommand == "{")
		{
			IfFrame f;

			//This is to push the branch instruction i.e the next instruction because the next instruction at the end
			//of the scope is not known yet so "I push to stack the location of the branch instruction"
			f.branchLocation = ctx.nextInstruction - 1;
			//I pushed JumpInstruction because i orignially added nextInstruction + 2 for the if statement with no blocks i.e {}
			// if i didnt push it i will have an inaccurate result at '}' when i minus what was added to the branch instruction
			f.jumpInstruction = jumpInstruction;
			//This is to check if it is double branch
			//If i was to use it as a static vairbale it wont work for nested if loops because once one if loop declare
			//double branch true everything else is made true
			f.doubleBranch = doublebranchLocation;

			ifStack.push(f);
		}
		else {
			int branchLocation = ctx.nextInstruction - 1;
			int branchLocation2 = ctx.nextInstruction - 2;

			//This is to execute the remaining command
			//this is a one line command e.g if a > b print a
			callParserForSubCommand(subCommand, iss, ctx,parser);

			if (!doublebranchLocation) {
				//Some instructions are mode than 2 cycles so this is neccessary to be sure
				ctx.instructionMemory[branchLocation] -= jumpInstruction;
				ctx.instructionMemory[branchLocation] += ctx.nextInstruction;
			}
			//This is for < and > because they have more than one branch check
			else {
				ctx.instructionMemory[branchLocation] -= jumpInstruction;
				ctx.instructionMemory[branchLocation] += ctx.nextInstruction;
				ctx.instructionMemory[branchLocation2] -= jumpInstruction;
				ctx.instructionMemory[branchLocation2] += ctx.nextInstruction;
			}
		}
	}

	void callParserForSubCommand(string subCommand,std::istringstream& iss, CodegenContext& ctx,Parser& parser);
	void parseCloseBrace(CodegenContext& ctx) {
		if (ifStack.empty())
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": \n";
			cerr << "Error: '}' found without a matching 'if { ...}'" << endl;
			return;
		}

		IfFrame f = ifStack.top();
		ifStack.pop();

		//The jumpInstruction meant for the normal subcommand only adds two instruction cycle if a == b print c one instruction cycle
		// for the branch prediction and second one for the subCommand so it becomes 0
		ctx.instructionMemory[f.branchLocation] -= f.jumpInstruction;
		//This one is to make the next instruction to be executed proper 1.e it branches to the correct instruction
		ctx.instructionMemory[f.branchLocation] += ctx.nextInstruction;

		//This is to check for symbols that has double branch check like < and >
		//This fixes the bug so it checks if it has double branch if it does it minus 1 because the first branch is one instruction
		//count before the next one
		if (f.doubleBranch) {
			int branchLocation2 = f.branchLocation - 1;
			ctx.instructionMemory[branchLocation2] -= f.jumpInstruction;
			ctx.instructionMemory[branchLocation2] += ctx.nextInstruction;
		}
	}

	void parseGoto(std::istringstream& iss, CodegenContext& ctx)
	{
		string arg1;
		iss >> arg1;
		int targetLine = std::stoi(arg1);
		if (ctx.lineToAddr.count(targetLine) > 0)
		{
			ctx.instructionMemory[ctx.nextInstruction] = Op::BRANCH + ctx.lineToAddr[targetLine];
		}
		else
		{
			ctx.instructionMemory[ctx.nextInstruction] = Op::BRANCH + 0;
			ctx.flags[ctx.nextInstruction] = targetLine;
		}
		ctx.nextInstruction++;
	}

};
#endif