#pragma once
#ifndef FUNCTION_HANDLER_H
#define FUNCTION_HANDLER_H

#include "../CodegenContext.h"
#include "../AddressResolver.h"
#include "../postfix.h"
#include "../Stacks.h"
#include "../TempPool.h"
#include "../Opcode.h"
#include "../TokenUtils.h"
#include "../MemoryLayout.h"
#include "../StringLiteralAllocator.h"
#include "../FnNameValidator.h"

#include <string>
#include <sstream>
#include <stack>
#include <vector>
#include <iostream>

using std::string;
using std::vector;
using std::cerr;
using std::endl;

class FunctionHandler {
public:
	void execute(const string& command, std::istringstream& iss, CodegenContext& ctx) {
		if (command == "fn") {
			parseFunction(iss, ctx);
		}
		else if (command == "ret") {
			parseReturn(iss, ctx);
		}
		else if (command == "}.") {
			parseEndFunction(ctx);
		}
		else if (command == "call") {
			parseCallCommand(iss, ctx);
		}
	}
	string currentFunctionName() const
	{
		return fnStack.empty() ? "" : fnStack.top().fnName;
	}

	//I made it public because the call command will be used by different class e.g let,print ,call
	// e.g "let x = call fn (....)" they both need identical instruction so i made this place the central logic
	void generateFunctionCall(string& fnName, vector<string>& parameters, CodegenContext& ctx) {

		//The first thing is to check if the functionName is declared
		bool isDeclared = ctx.st.isFunctionDeclared(fnName);

		//also check whether it is defined
		bool isDefined = ctx.st.isFunctionDefined(fnName);


		if (!isDeclared && !isDefined)
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "\nError: " << fnName << " has not been declared" << endl;
			return;
		}

		//If it return false then this statement will execute then return
		if (!(ctx.st.validatefnCount(fnName, parameters))) {
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: parameter count mismatch in redeclaration of function '" << fnName << "'" << endl;
			return;
		}
		if (isDefined || isDeclared)
		{
			//This is to validate the parameters type against the recorded parameter type
			bool isFnTypeValidated = ctx.st.validateFnTypes(fnName, parameters, ctx.hasError, fnName);
			if (!isFnTypeValidated) {
				ctx.hasError = true;
				cerr << "Error on line " << ctx.currentLineNum << ": ";
				cerr << "Error: parameter type mismatch in redeclaration of function '" << fnName << "'" << endl;
				return;
			}
		}

		//Get the firstInstruction location
		int firstInstruction = ctx.st.returnFnFirstInstruction(fnName);

		//The purpose of this next 3 instructions is to load return adress into the stack pointer
		//Load the return address into the accumulator
		//the actual return address will be patched at the end 
		int returnAddrPlaceholderIdx = ctx.nextInstruction;
		ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + 0;
		//load the return address from the accumulator into temp location at the top of stackPointer
		ctx.instructionMemory[ctx.nextInstruction++] = Op::SP_PUSH;
		//then increment the stackPointer
		ctx.instructionMemory[ctx.nextInstruction++] = Op::SP_INC;

		//THE PURPOSE OF THIS NEXT 4 INSTRUCTION IS TO SET PARAM BASE AND LOAD IT INTO THE STACK POINTER
		// the param base is the base location of the first function parameter(local variable if no argument passed)
		// check resolve parameteraddress or resolveLocalAddress in "address resolver.cpp" to understand more about param base
		//load value of paramBase into the accumulator
		ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_BASE;
		//load the accumulator into the stackpointer
		ctx.instructionMemory[ctx.nextInstruction++] = Op::SP_PUSH;
		//Increment the stackPointer
		ctx.instructionMemory[ctx.nextInstruction++] = Op::SP_INC;

		// capture the callee's frame base into a scratch cell — register NOT touched
		//The frameBaseScratch is a temporary storage so we request for a location so it can be released later
		int frameBaseScratch = tempPool.acquire(ctx.hasError);
		if (frameBaseScratch == -1)
		{
			return;
		}
		// acc = parameterPointer
		ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_PTR;
		//load the framBase scratch into accumulator
		ctx.instructionMemory[ctx.nextInstruction++] = Op::STORE + frameBaseScratch;
		//push from accumulator to save for restore 
		ctx.instructionMemory[ctx.nextInstruction++] = Op::SP_PUSH;
		//increment stack pointer
		ctx.instructionMemory[ctx.nextInstruction++] = Op::SP_INC;
		
		//THIS PART LOADS THE PARAMETER INTO THE PARAMETER STACK POINTER
		for (int i = 0;i < parameters.size();i++) {
			bool check = false;
			string parameter = parameters[i];
			//Chck if it is a global reference i.e ::a
			bool isGlobalRef = parameter.size() >= 2 && parameter[0] == ':' && parameter[1] == ':';
			string checkName = isGlobalRef ? parameter.substr(2) : parameter;

			//I want string to also decay to pointer i will fix it later
			//I used a seperate check for array because i want array to decay to pointers when passed as parameters to functions
			//SO instead of loading the value of the array into the parameter stack pointer 
			// i want to load the address of the array into the parameter stack pointer
			//e.g load array a so a[4] becomes address of base pointer + 4 instead of loading the value of a[4] into the parameter stack pointer
			if ((ctx.st.type(parameter, currentFunctionName()) == "array" || ctx.st.type(parameter, currentFunctionName()) == "string")
				&& isValidIdentifier(checkName) && !isArrayWithIndex(checkName)) {
				bool isParameterArray = false;
				bool isFunctionLocalArray = false;
				bool isGlobalArray = false;
				int arrayBaseAddr;

				//If the array is a global variable
				if (currentFunctionName().empty() || isGlobalRef) {
					arrayBaseAddr = ctx.st.lookup(checkName);
					isGlobalArray = true;
				}
				else {
					//check if the array variable is a function parameter
					int paramIndex = ctx.st.getParameterIndex(ctx.currentFn, parameter);
					if (paramIndex != -1)
					{
						isParameterArray = true;
					}
					else
					{
						isFunctionLocalArray = true;
					}
					auto [arrAddr, isIndir] = resolveToken(parameter, ctx.st, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError, ctx.currentFn);
					arrayBaseAddr = arrAddr;
				}


				if (arrayBaseAddr == -1)
				{
					ctx.hasError = true;
					cerr << "Error on line " << ctx.currentLineNum << ": ";
					cerr << "Error: Array '" << parameter << "' could not be Located" << endl;
					return;
				}

				string baseStr = std::to_string(arrayBaseAddr);
				int baseConstLoc = getOrCreateConstant(baseStr, ctx.st, ctx.instructionMemory, ctx.nextData, ctx.hasError);

				//if it is a global array or a local array pass it using direct addressing
				if (isGlobalArray || isFunctionLocalArray)
				{
					if (isGlobalArray) {
						//Load it into the base address of the array into the accumulator from the constant location
						ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + baseConstLoc;
					}
					else if (isFunctionLocalArray)
					{
						ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + arrayBaseAddr;
					}
					//load it into the parameter pointer from the accumulator
					ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_PTR_STORE;
					//Increment the parameter pointer after loading it
					ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_INC;
				}
				//If it is a parameter and an array the parameter address is on the stack and it contains the base address
				//resolve token returns the parameter pointer location in a temp location
				// e.g resolve token returns loc 25; loc 25 contains loc 110 ; loc 110 contains the base address of the array 1.e indirect addressing
				else if (isParameterArray) {

					ctx.instructionMemory[ctx.nextInstruction++] = Op::INDIRECT_LOAD + arrayBaseAddr;
					//load it into the parameter pointer from the accumulator
					ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_PTR_STORE;
					//Increment the parameter pointer after loading it
					ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_INC;
				}
			}
			//if it is a string literal being passed e.g 
			// call printer("hello") 
			//It uses allocatestring literal on a compile time so the "hello" is being treated as a global variable and the base address is passed to the argument
			//if i was to assign the "hello" on the parameter stack i have to allocate the literals on runtime which is very expensive and somehow confusing
			//so "hello" returns an address that contains the size of the string and pass it to the parameter
			else if (parameter.size() > 1 && parameter.front() == '"' && parameter.back() == '"')
			{
				string literal = parameter.substr(1, parameter.size() - 2);
				int base = allocateStringLiteral(literal, ctx.nextData, ctx.instructionMemory, ctx.hasError, 0, 0);
				string baseStr = std::to_string(base);
				int baseConstLoc = getOrCreateConstant(baseStr, ctx.st, ctx.instructionMemory, ctx.nextData, ctx.hasError);
				ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + baseConstLoc;
				ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_PTR_STORE;
				ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_INC;
			}
			//To check if it it is a variable or an indexed array
			//indexed array is a variable with an index e.g a[4] which will not decay to a pointer because it is not a base address of an array
			//check st.validateFnTypes for more explanation
			else if (isValidIdentifier(parameter) || isArrayWithIndirectAddress(parameter, ctx.hasError) || (parameter.find('[') != string::npos && parameter.back() == ']' && !isExpressionLike(parameter))) {
				//If the parameter is a variable the value should already be in the memory
				auto [address, isIndirect] = resolveToken(parameter, ctx.st,ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.hasError, ctx.currentFn);
				if (address == -1)
				{
					ctx.hasError = true;
					cerr << "Error on line " << ctx.currentLineNum << ": ";
					cerr << "Error: Variable '" << parameter << "' could not be Located" << endl;
					return;
				}
				int loadOpcode = (isIndirect ? Op::INDIRECT_LOAD : Op::LOAD);
				check = !ctx.st.checkInitializationStatus(parameter, address, ctx.currentFn);

				//This falls through if it has not been initialized
				if (check) {
					ctx.hasError = true;
					cerr << "Error on line " << ctx.currentLineNum << ": ";
					cerr << "Error: Variable '" << parameter << "' has not been Initialized" << endl;
					return;
				}
				//Load it into the accumulator from the address
				ctx.instructionMemory[ctx.nextInstruction++] = loadOpcode + address;
				//load it into the parameter pointer from the accumulator
				ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_PTR_STORE;
				//Increment the parameter pointer after loading it
				ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_INC;
			}
			else if (isNumberOnly(parameter)) {	//That means it is a constant like numbers e.g 6,50
				//get the address of the constant in the memory or create a new one if it does not exist
				int address = getOrCreateConstant(parameter, ctx.st, ctx.instructionMemory, ctx.nextData, ctx.hasError);

				//Load it into the accumulator from the address
				ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + address;
				//load it into the parameter pointer from the accumulator
				ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_PTR_STORE;
				//Increment the parameter pointer after loading it
				ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_INC;
			}
			else if (isExpressionLike(parameter))
			{
				vector<string> exprPostfix = intpo.Infix(parameter, ctx.hasError);
				auto [resultLoc, isIndirect] = post.generateCode(exprPostfix, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.st, ctx.hasError, ctx.currentLineNum, ctx.currentFn);

				if (ctx.hasError || resultLoc == -1)
				{
					ctx.hasError = true;
					cerr << "Error on line " << ctx.currentLineNum << ": ";
					cerr << "Error: Invalid expression '" << parameter << "' in call statement." << endl;
					return;
				}

				// -999 means the last arithmetic op already left the value sitting in the accumulator —
				// same sentinel convention 'let' already relies on, so no extra load needed.
				if (resultLoc != -999)
				{
					ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + resultLoc;
				}

				ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_PTR_STORE;  // push accumulator into parameter pointer
				ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_INC;  // increment parameter pointer
				}
			else {
				ctx.hasError = true;
				cerr << "Error on line " << ctx.currentLineNum << ": ";
				cerr << "\nError: Invalid parameter '" << parameter
					<< "' in call statement. Only variables or numeric constants are allowed."
					<< endl;
				return;
			}
		}

		//now apply the captured frame base, right before entering the callee
		ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + frameBaseScratch;  // acc = mem[scratch]
		ctx.instructionMemory[ctx.nextInstruction++] = Op::SET_PARAM_BASE;           // paramBase = acc

		if (isDefined)
		{
			//branch to the first fn instruction
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH + firstInstruction;
		}
		if (isDeclared && !isDefined)
		{
			//It will be patched by fnFlags at compiler::compile
			ctx.fnFlags[ctx.nextInstruction] = fnName;
			ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH + 0;
		}

		//Patch the return adress
		int actualReturnAddr = ctx.nextInstruction;

		string retStr = std::to_string(actualReturnAddr);
		int returnAddrLoc = getOrCreateConstant(retStr, ctx.st, ctx.instructionMemory, ctx.nextData, ctx.hasError);
		// getOrCreateConstant already wrote retStr's own numeric value in — but we want
		// this cell to hold actualReturnAddr specifically (same value here, so it's fine,
		// but worth the explicit overwrite for clarity/safety if that ever diverges):
		ctx.instructionMemory[returnAddrLoc] = actualReturnAddr;

		ctx.instructionMemory[returnAddrPlaceholderIdx] += returnAddrLoc;
	}
private:
	InfixToPostfix intpo;
	postfix post;

	//This frame holds the branch location containing the instruction to branch to if it is not called
	// a function name , the total number of local variables
	// and the location for every ret instruction to be patched
	struct fnFrame {
		int branchLocation = -1;
		string fnName = "";
		int localReserveIndex = -1;
		//This contains the total number of ret branch location which needs to be patched to }. first instruction
		//a ret means returning a value from a function which always means it is the last instruction before }. first instruction
		vector<int> pendingReturns = {};
	};
	//The fnStack holds the patch instruction and the function name
	std::stack<fnFrame> fnStack;

	void parseFunction(std::istringstream& iss, CodegenContext& ctx) {
		string rest;
		getline(iss, rest);

		auto [fnName, isValid, isDefinition, parameters, parameterTypes] = validateFnName(rest, ctx.hasError, ctx.currentLineNum);
		//Return if fnName is not valid
		if (!isValid)
		{
			return;
		}

		if (!isDefinition)
		{
			//Register the function name,parameter count and the parameter types
			ctx.st.declareFunction(fnName, parameters, parameterTypes, ctx.hasError);
			return;
		}

		//The first thing to do is to branch to an instruction will later be patched by "}."
		ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH + 0;
		//I want to load the local variable of the function into the parameter stack but at fn I dont know the total 
		//number of local variable of each function so i plan on using the repatch method
		int localReserveIndex = ctx.nextInstruction;
		//Load the number of local variable into the accumulator(it is 0 because the total number is unknown right now
		//It will be patched later
		ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + 0;
		//This add the total count to the parameter pointer
		ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_ADD_ACC;

		//The next thing is to push the first instruction after branch as entrypoint to the function
		//source code and the fnction name and the instruction for total variable count
		fnStack.push({ ctx.nextInstruction - 3, fnName, localReserveIndex,{} });	

		ctx.st.defineFunction(fnName, parameters, parameterTypes, localReserveIndex, ctx.hasError);
	}

	void parseReturn(std::istringstream& iss, CodegenContext& ctx) {

		if (currentFunctionName().empty())
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: 'ret' statement outside of a function" << endl;
			return;
		}

		//this is to get the value to be returned from the function
		string rest;
		getline(iss, rest);
		//To trim whitespace
		size_t start = rest.find_first_not_of(" \t\r\n");
		size_t end = rest.find_last_not_of(" \t\r\n");
		rest = (start != string::npos) ? rest.substr(start, end - start + 1) : "";

		if (rest.empty())
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: 'ret' requires a return value" << endl;
		}
		else if (rest.size() > 1 && rest.front() == '"' && rest.back() == '"')
		{
			string literal = rest.substr(1, rest.size() - 2);
			//allocate the string literal on the global data memory and return the base address
			int addr = allocateStringLiteral(literal, ctx.nextData, ctx.instructionMemory, ctx.hasError, 0, 0);
			if (addr == -1)
			{
				ctx.hasError = true;
				return;
			}

			// word count including one space for storing the size of the word
			int size = (((int)literal.size() + 1) / 2) + 1;
			ctx.st.setReturnedLiteral(currentFunctionName(), addr, size, ctx.hasError);

			string baseStr = std::to_string(addr);
			int baseConstLoc = getOrCreateConstant(baseStr, ctx.st, ctx.instructionMemory, ctx.nextData, ctx.hasError);

			//Load the base addr into the accumulator
			ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + baseConstLoc;

			ctx.st.setReturnType(currentFunctionName(), "string", ctx.hasError);
		}
		else
		{
			//to handle string return
			//If a variable (whether local variable or function parameter) is of type string ..It is not supported for now
			if (isValidIdentifier(rest) && ctx.st.type(rest, currentFunctionName()) == "string") {
				ctx.hasError = true;
				cerr << "Error on line " << ctx.currentLineNum << ": ";
				cerr << "Error: only returning string literals is supported for now (variable '" << rest << "' is not a literal)" << endl;
				return;
			}
			else 
			{
				//get the result if it is an expression like or not
				vector<string> postfixTokens = intpo.Infix(rest, ctx.hasError);
				auto [resultLoc, isindirect] = post.generateCode(postfixTokens, ctx.instructionMemory, ctx.nextInstruction, ctx.nextData, ctx.st, ctx.hasError, ctx.currentLineNum, ctx.currentFn);

				if (ctx.hasError || resultLoc == -1)
				{
					ctx.hasError = true;
					cerr << "Error on line " << ctx.currentLineNum << ": ";
					cerr << "Error: Invalid expression '" << rest << "' in 'ret' statement." << endl;
					return;
				}
				//if the result is not in the accumulator then load it into the accumulator
				//we load the return value into the assigned variable
				if (resultLoc != -999)
				{
					ctx.instructionMemory[ctx.nextInstruction++] = Op::LOAD + resultLoc;
				}
				ctx.st.setReturnType(currentFunctionName(), "var", ctx.hasError);
			}
		}

		if (fnStack.empty()) {
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Internal error: no function frame available for 'ret'" << endl;
			return;
		}
		

		//at every potential ret the program counter branches to }. first instruction
		//and since }. first instruction is not known until the end of the instruction in a function
		// it will be pushed into stack to be patched later by }.
		//e.g inside a function 
		// if a>b { 
		// ret a
		// } 
		// ret 0
		//...
		// }.
		//Every ret is a potential return instruction which signifies no other instruction after it can run so i patch the }. first instruction later
		int patchIndex = ctx.nextInstruction;
		ctx.instructionMemory[ctx.nextInstruction++] = Op::BRANCH + 0;
		fnStack.top().pendingReturns.push_back(patchIndex);
	}
	
	void parseEndFunction(CodegenContext& ctx) {
		if (fnStack.empty())
		{
			ctx.hasError = true;
			cerr << "Error on line " << ctx.currentLineNum << ": ";
			cerr << "Error: '}.' without matching function definition" << endl;
			return;
		}

		//Gets the branchLocation and the ret branch instruction to be patched
		//check "parseReturn" for more comments about why i push every ret branch instruction
		auto [branchLocation, fnName, localReserveIndex, pendingReturns] = fnStack.top();
		fnStack.pop();

		//We have to patch the total number of local variable a function has
		// check parseFunction for more info for more info
		int localCount = ctx.st.getLocalCount(fnName);
		string countStr = std::to_string(localCount);
		int countLoc = getOrCreateConstant(countStr, ctx.st, ctx.instructionMemory, ctx.nextData, ctx.hasError);

		ctx.instructionMemory[countLoc] = localCount;
		ctx.instructionMemory[localReserveIndex] += countLoc;

		//Whenever a ret command is encountered the program counter should branch to "}." first instruction and since the instruction is not known until the end of the function
		// we have to store and  patch the ret branch instruction to point to  }. first instruction which is to be added to the ret instruction
		int epilogueStart = ctx.nextInstruction;
		//This is to patch each of ret branch instruction
		for (int patchIndex : pendingReturns)
		{
			ctx.instructionMemory[patchIndex] = Op::BRANCH + epilogueStart;
		}

		//First thing is to decrement the stack pointer
		ctx.instructionMemory[ctx.nextInstruction++] = Op::SP_DEC;
		//load the saved parameter pointer in the stack pointer and make it the current parameter pointer
		ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_RESTORE;
		//decrement again to get the parametercount
		ctx.instructionMemory[ctx.nextInstruction++] = Op::SP_DEC;
		//Load the value from the stackPointer into the parameterBase
		//For every call of function 3 values are pushed to the stack pointer, The first one is return address and 
		// second is the number of parameter count and the third is the parameter pointer current value
		ctx.instructionMemory[ctx.nextInstruction++] = Op::PARAM_BASE_LOAD;
		//Decrement the stack pointer again to get the return address
		ctx.instructionMemory[ctx.nextInstruction++] = Op::SP_DEC;

		//The last Instruction of a function is always branch back to the next instruction after it has been called
		ctx.st.FnaddLastInstruction(fnName, ctx.nextInstruction);

		//This branches to the location inside the value of top of stack pointer i.e indirect branch
		ctx.instructionMemory[ctx.nextInstruction++] = Op::SP_BRANCH;

		//This patches the branch target instruction that wasnt known when parsing function prototype
		ctx.instructionMemory[branchLocation] += ctx.nextInstruction;
	}

	void parseCallCommand(std::istringstream& iss, CodegenContext& ctx) {
		string rest;
		getline(iss, rest);

		auto [fnName, isValid, isDefinition, parameters, parameterType] = validateFnName(rest, ctx.hasError, ctx.currentLineNum);
		if (!isValid)
		{
			return;
		}
		
		generateFunctionCall(fnName, parameters, ctx);
	}
};

#endif