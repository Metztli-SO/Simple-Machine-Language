#pragma once
#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <sstream>
#include <map>
#include <algorithm>
#include <stack>

#include "TokenUtils.h"
#include "CodegenContext.h"
#include "Stacks.h"
#include "postfix.h"
#include "AddressResolver.h"
#include "TempPool.h"
#include "Opcode.h"
#include "handlers/IOHandlers.h"
#include "handlers/IncrementDecrementHandler.h"
#include "handlers/DeclarationHandler.h"
#include "handlers/LetHandler.h"
#include "handlers/ControlFlowHandler.h"
#include "handlers/LoopHandler.h"
#include "handlers/FunctionHandler.h"
#include "handlers/GosubHandler.h"

using std::string;
using std::remove;
using std::endl;




class Parser
{
public:
	Parser() {}

	void parseLine(const string& line, CodegenContext& ctx)

	{
		std::istringstream iss(line);
		string command;
		if (!(iss >> lineNum >> command))
		{
			return;
		}

		int currentLineNum = std::stoi(lineNum);
		ctx.currentLineNum = currentLineNum;

		if (ctx.lineToAddr.find(currentLineNum) == ctx.lineToAddr.end())
		{
			ctx.lineToAddr[currentLineNum] = ctx.nextInstruction;
		}

		executeCommand(command, iss, ctx);
		
		//The memory layout is in "MemoryLayout.h" 
		//This is to make sure both the instruction and data dont run out of the memoray space allocated to them
		Mem::ensureGlobalDataSpace(ctx.nextData, ctx.hasError);
		Mem::ensureInstructionSpace(ctx.nextInstruction, ctx.hasError);

		//This makes sure every temporary location that was allocated is being released 
		//Every temporary location is just to store values temporarily so after each line the temp storage becomes useless/redundant
		tempPool.release();
	}

	void executeCommand(const string& command, std::istringstream& iss,CodegenContext& ctx)
	{
		string arg1, sign;

		if (command == "rem")
		{
			return;
		}
		else if (command == "print" || command == "input") {
			io.execute(command, iss, ctx);
		}
		else if (command == "if" || command == "}" || command == "goto") {
			controlFlow.execute(command, iss, ctx, *this);
		}
		else if (command == "let")
		{
			letHandler.execute(command, iss, ctx, fnHandler);
		}
		else if (command == "fn" || command == "ret" || command == "}." || command == "call")
		{
			fnHandler.execute(command, iss, ctx);
		}
		else if (command == "gosub" || command == "return") {
			gosubHandler.execute(command, iss, ctx);
		}
		else if (command == "for" || command == "next" || command == "while" || command == "wend") {
			loop.execute(command, iss, ctx, *this);
		}
		else if (command == "end")
		{
			ctx.instructionMemory[ctx.nextInstruction++] = Op::HALT;
		}

		else if (command == "array" || command == "string") {
			decl.execute(command, iss, ctx);
		}

		else {
			incDec.execute(command, iss, ctx);
		}
	}

	bool isBlockCloseds()
	{
		return controlFlow.blockClosed();
	}
	bool isWhileClosed()
	{
		return loop.whileClosed();
	}
	bool isForNextClosed()
	{
		return loop.forNextClosed();
	}
	bool isGosubClosed()
	{
		return gosubHandler.isGosubClosed();
	}
	//This is to return the current function name for scope rule and for the symbol table to avoid function Parameter
	string currentFunctionName() const
	{
		return fnHandler.currentFunctionName();
	}

private:
	InfixToPostfix intpo;
	postfix post;

	IOHandlers io;
	IncrementDecrementHandler incDec;
	DeclarationHandler decl;
	LetHandler letHandler;
	ControlFlowHandler controlFlow;
	LoopHandler loop;
	FunctionHandler fnHandler;
	GosubHandler gosubHandler;
		
	string lineNum;
	
};
#endif

//The compiler was giving me a lot of issues during the compilation of the ControlFlowHandler.cpp file 
// because of the circular dependency between the Parser and ControlFlowHandler classes.
//SO i made the definition here and rhe declaration in the ControlFlowHandler.h file to avoid the circular dependency issue
inline void ControlFlowHandler::callParserForSubCommand(string subCommand,std::istringstream& iss, CodegenContext& ctx,Parser& parser) {
	parser.executeCommand(subCommand, iss, ctx);
}