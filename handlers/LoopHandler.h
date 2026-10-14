#pragma once
#ifndef LOOP_HANDLER_H
#define LOOP_HANDLER_H

#include "../CodegenContext.h"
#include "../AddressResolver.h"
#include "../Opcode.h"
#include "../TokenUtils.h"

#include <string>
#include <sstream>
#include <iostream>

using std::string;
using std::cerr;
using std::endl;

class Parser;

class LoopHandler {
public:
	void execute(const string& command, std::istringstream& iss, CodegenContext& ctx,Parser& parser)
	{
		if (command == "for")
			parseFor(iss, ctx, parser);
		else if (command == "next")
			parseNext(ctx);
		else if (command == "while")
			parseWhile(iss, ctx);
		else if (command == "wend")
			parseWend(ctx);
	}

	// Used by Parser::isForNextClosed()/isWhileClosed() for Compiler::compile's end-of-file "did every block get closed" checks.
	bool forNextClosed() const {
		return nextStack.empty();
	}
	bool whileClosed() const {
		return whileStack.empty();
	}
private:

	struct forFrame {
		int branchLocation = -1;
		int branchLocation2 = -1;
	};

	struct nextFrame {
		int location = -1;
		bool isIndirect = false;
	};

	struct whileFrame {
		int firstWhileInstruction = -1;
		int branchLocation = -1;
		int branchLocation2 = -1;
		bool doubleBranch = false;
	};

	std::stack<nextFrame> nextStack;
	std::stack<int> returnForStack;
	std::stack<whileFrame> whileStack;
	std::stack<forFrame> forStack;

	void parseFor(std::istringstream& iss, CodegenContext& ctx, Parser& parser)
	{
		int initializeloc, limitLoc, incrementLoc;
		string arg1, sign, initialize, limit, step, increment, arg3;
		iss >> arg1 >> sign >> initialize;

		if (sign != "=")
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Invalid operand" << endl;
			return;
		}
		if (!(isdigit(initialize[0])))
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: Invalid initialization value for 'for' loop" << endl;
			return;
		}
		auto [arg1loc, indirect] = resolveToken(arg1, ctx.st, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError, ctx.currentFn);
		initializeloc = parseAddress(initialize, ctx.st);

		//Initialize the variable so it will pass initialization check
		ctx.st.initializeVariable(arg1, arg1loc, ctx.currentFn);

		if (!indirect)
		{
			nextStack.push({ arg1loc, false });
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + initializeloc;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::STORE + arg1loc;
		}
		else {
			nextStack.push({ arg1loc, indirect });
			//If the variable is a local variable or a prameter. they live on a stack parameter pointer known at runtime 
				//so resolveToken always return the location of the value in a temp location
				//so we need to load the initialize constant indirectly into the temp location value
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + initializeloc;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::INDIRECT_STORE + arg1loc;
		}

		iss >> arg3;
		if (arg3 == "to")
		{
			iss >> limit;

			auto [limitloc, indL] = resolveToken(limit, ctx.st, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError, ctx.currentFn);
			// This is very important because if it is indirect it returns the location not the value
			//resolve if indirect returns a location that stores the original value
			limitLoc = resolveIfIndirect(limitloc, indL, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError);

			if (!ctx.st.checkInitializationStatus(limit, limitLoc, ctx.currentFn))
			{
				ctx.hasError = true;
				cerr << "Error on line " << ctx.currentLineNum << ": ";
				cerr << "Error: Variable '" << arg1 << "' has not been Initialized" << endl;
				return;
			}

			if (limitLoc == -1)
			{
				ctx.hasError = true;
				cerr << "Error on line " << ctx.currentLineNum << ": ";
				cerr << "Error: Invalid limit value for 'for' loop" << endl;
				return;
			}
			// Generate code to compare loop variable with limit and jump to end if condition is met
			// This would involve generating code to load the loop variable, compare it with the limit, and conditionally jump to the end of the loop if the condition is not satisfied.
		}
		else
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: Missing 'to' keyword in 'for' loop" << endl;
			return;
		}

		//This is the first check to decide it the loop is safe to run e.g for 5 to 4 
		//This check logic helps to prevent that
		if (!indirect)
		{
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + limitLoc; // load the limit location into the accumulator
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SUB + arg1loc; //the accumulator minus the argument value to check
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_NEG + 0; //Branch if Negative
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_ZERO + 0; // Branch if Zero this checks for edge case like 2 <2
		}
		else {
			ctx.instructionMemory[ctx.nextInstruction++] = Op::INDIRECT_LOAD + arg1loc; // load the argument value indirectly into the accumulator
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SUB + limitLoc; //the accumulator minus the limit value
			//The branch check goes as follows
			//e.g for a = 2 to 7 [arg1 = 2 limit =7] load 2 into accumulator then 2-7 which is -5 so the argument is still withing checj
			// but if a = 10 and limit = 7 10 -7 =3 which is above the limit
			//so branch if positive or branch if zero
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_POS + 0;//Branch if positive
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_ZERO + 0; // Branch if Zero this checks for edge case like 2 <2
		}

		//This forframe contains the location for both branch instruction
			// The compiler doesnt knwo which instructions to execute for now so it patches and add the instruction
			// to branch to		
		forFrame forframe;
		forframe.branchLocation = ctx.nextInstruction - 2;
		forframe.branchLocation2 = ctx.nextInstruction - 1;
		forStack.push(forframe);


		// Note: The actual loop control logic (incrementing and condition checking) would need to be implemented in the generated code, which may require additional instructions and handling.
		nextStack.push({ limitLoc, false });
		if (iss >> step >> increment)
		{
			int location;
			if (isNumberOnly(increment)) {
				location = parseAddress(increment, ctx.st);
			}
			else {
				auto [incLoc, isIndirect] = resolveToken(increment, ctx.st, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError, ctx.currentFn);
				location = incLoc;
				if (isIndirect) {
					location = resolveIfIndirect(location, isIndirect, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError);
				}
			}
			nextStack.push({ location, false });
			returnForStack.push(ctx.nextInstruction);
		}
		else
		{
			increment = "1";
			incrementLoc = getOrCreateConstant(increment, ctx.st, ctx.instructionMemory, ctx.nextData, ctx.hasError);
			nextStack.push({ incrementLoc, false });
			returnForStack.push(ctx.nextInstruction);
		}
	}

	void parseNext(CodegenContext& ctx) {
		//The stack should always contain 3 element : incrementFrame,limitFrame,argFrame
		if (nextStack.size() < 3)
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: 'next' statement without matching 'for'" << endl;
			return;
		}

		nextFrame incrementFrame = nextStack.top();
		nextStack.pop();
		nextFrame limitFrame = nextStack.top();
		nextStack.pop();
		nextFrame argFrame = nextStack.top();
		nextStack.pop();

		int incrementLoc = incrementFrame.location;
		int limitLoc = limitFrame.location;
		int argLoc = argFrame.location;
		bool isIndirect = argFrame.isIndirect;

		//This contains the return address for the branch target check
		int nextAdr = returnForStack.top();
		returnForStack.pop();

		// If it is a global variable
		if (!isIndirect)
		{
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + incrementLoc;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::ADD + argLoc;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::STORE + argLoc;

			//loading the argument into the accumulator and minus the limit to check the branch location
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + argLoc;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SUB + limitLoc;

			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_NEG + nextAdr;
		}
		else
		{
			//load the value indirectly into accumulator
			ctx.instructionMemory[ctx.nextInstruction++] = Op::INDIRECT_LOAD + argLoc;
			//add the increment with the value
			ctx.instructionMemory[ctx.nextInstruction++] = Op::ADD + incrementLoc;
			//store the value back indirectly into the argument Location
			ctx.instructionMemory[ctx.nextInstruction++] = Op::INDIRECT_STORE + argLoc;

			//This part is to check if the for loop condition is still true
			//load the argument indirectly into the accumulator
			ctx.instructionMemory[ctx.nextInstruction++] = Op::INDIRECT_LOAD + argLoc;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SUB + limitLoc; //minus the argument from the limitLoc

			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_NEG + nextAdr;
		}

		//This is to patch the first branch check to confirm if the condition is True to execute the for statements
		if (!forStack.empty())
		{
			forFrame forframe = forStack.top();
			forStack.pop();

			ctx.instructionMemory[forframe.branchLocation] += ctx.nextInstruction;
			ctx.instructionMemory[forframe.branchLocation2] += ctx.nextInstruction;
		}
		else {
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: 'next' statement without matching 'for' (for-check stack)" << endl;
			return;
		}

	}

	void parseWhile(std::istringstream& iss, CodegenContext& ctx)
	{
		string arg1, sign, argument2;
		iss >> arg1 >> sign >> argument2;

		//This is very important because resolveToken might generate additional instruction
		//so this makes sure the first instruction of the while statement is correct
		int conditionStart = ctx.nextInstruction;

		auto [loc1, indA] = resolveToken(arg1, ctx.st, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError, ctx.currentFn);
		auto [loc2, indB] = resolveToken(argument2, ctx.st, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError, ctx.currentFn);

		if (loc1 == -1 || loc2 == -1)
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: Variable '" << (loc1 == -1 ? arg1 : argument2) << "' could not be Located" << endl;
			return;
		}

		//This helps to prevent unitiliazed variables from going through
		bool check1 = !ctx.st.checkInitializationStatus(arg1, loc1, ctx.currentFn) && ctx.instructionMemory[loc1] == -1;
		bool check2 = (!ctx.st.checkInitializationStatus(argument2, loc2, ctx.currentFn) && ctx.instructionMemory[loc2] == -1);
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
			cerr << "Error: Variable '" << argument2 << "' has not been Initialized" << endl;
			return;
		}

		//This is very important because if it is indirect it returns the location not the value
		//resolve if indirect returns a location that stores the original value
		int adr1 = resolveIfIndirect(loc1, indA, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError);
		int adr2 = resolveIfIndirect(loc2, indB, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError);

		if (sign == "==")
		{
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + adr1;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SUB + adr2;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_NE + 0;
		}
		else if (sign == "!=")
		{
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + adr1;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SUB + adr2;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_ZERO + 0;
		}
		else if (sign == "<")
		{
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + adr2;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SUB + adr1;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_NEG + 0;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_ZERO + 0;
		}
		else if (sign == ">")
		{
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + adr1;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::SUB + adr2;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_NEG + 0;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH_ZERO + 0;
		}
		else {
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "\nError: Unknown Comparison operator '" << sign << "' " << endl;
			return;
		}

		//sign > and < has two branch check if i have two check for every sign there will be error and inconsistencies
			//in instructions so this bool checks if it has < or > 
		bool doublebranchLocation = (sign == ">" || sign == "<") ? true : false;

		whileFrame ws;

		//This tells the compiler to go back to the beginning of the first while execution instruction
		int firstWhileInstruction = conditionStart;
		//This is for the branchinstruction to be updated at wend
		int branchLocation = ctx.nextInstruction - 1;

		if (doublebranchLocation)
		{
			int branchLocation2 = ctx.nextInstruction - 2;
			ws.branchLocation2 = branchLocation2;
		}

		ws.branchLocation = branchLocation;
		ws.doubleBranch = doublebranchLocation;
		ws.firstWhileInstruction = firstWhileInstruction;

		whileStack.push(ws);
	}

	void parseWend(CodegenContext& ctx)
	{
		if (whileStack.empty())
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: 'wend' statement without matching 'while'" << endl;
			return;
		}

		whileFrame ws = whileStack.top();
		whileStack.pop();

		//This makes the compiler branch back to the first while execution instruction to evaluate if its condition is still true
		// e.g "while a > b .... wend" first while instruction is load a into accumulator 
		ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH + ws.firstWhileInstruction;

		//This updates the branch instruction because the compiler then didnt know which instrution will be evaluated
		//next after while loop condition is false
		ctx.instructionMemory[ws.branchLocation] += ctx.nextInstruction;

		//This checks if it is a double branch instruction so it updates the second branch check
		if (ws.doubleBranch)
		{
			ctx.instructionMemory[ws.branchLocation2] += ctx.nextInstruction;
		}
	}
};


#endif