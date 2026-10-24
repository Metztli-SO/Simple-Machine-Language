#pragma once
#ifndef MEMORY_LAYOUT_H
#define MEMORY_LAYOUT_H

#include <iostream>

using std::cerr;
using std::endl;

//I want to stop generating instructions once the instruction/data space is finished
//Parser::parseline has a check for every line, if this check is absent it will keep printing the error message until the end of the instructions
extern bool stopInstruction;

//This contains the Memory Layout for the Compiler

namespace Mem {
	//The instruction takes the first 150 location with 149 inclusive
	constexpr int INSTRUCTION_START = 0;
	constexpr int INSTRUCTION_END = 149;

	//Global variables take next 50 location in the memory and it grows downdard from global end
	constexpr int GLOBAL_START = 150;
	constexpr int GLOBAL_END = 199;

	//Temporary addresses takes next 20 location
	constexpr int TEMP_START = 200;
	constexpr int TEMP_END = 219;

	//The parameter stack(contains local variables and function parameters) takes the next 50 location in the memory
	constexpr int PARAMETER_START = 220;
	constexpr int PARAMETER_END = 269;

	//The stack (contains return address,local variable count and paramBase) takes the next 29 location in the memory
	constexpr int STACK_START = 270;
	constexpr int STACK_END = 298;

	//This address contains the gosub return address
	constexpr int GOSUB_RETURN_ADDR = 299;

	constexpr int TOTAL_SIZE = 300;

	//static_assert is a compile-time assertion mechanism which verifies conditions during compilation 
	// and generates an error if a condition is not satisfied.
	//This is to ensure no change in the memory layout and if the layout is wrong it generates errror
	static_assert(GLOBAL_START == INSTRUCTION_END + 1);
	static_assert(TEMP_START == GLOBAL_END + 1);
	static_assert(PARAMETER_START == TEMP_END + 1);
	static_assert(STACK_START == PARAMETER_END + 1);
	static_assert(GOSUB_RETURN_ADDR == STACK_END + 1);
	static_assert(GOSUB_RETURN_ADDR == TOTAL_SIZE - 1);

	inline void ensureInstructionSpace(int nextInstruction, bool& hasError) {
		if (nextInstruction > INSTRUCTION_END) {
			hasError = true;
			cerr << "Error: Out of instruction memory (limit " << INSTRUCTION_END << ")" << endl;
			stopInstruction = true;
		}
	}

	inline void ensureGlobalDataSpace(int nextData, bool& hasError) {
		if (nextData < GLOBAL_START) {
			hasError = true;
			cerr << "Error: Out of global/constant data memory (limit " << GLOBAL_START << ")" << endl;
			stopInstruction = true;
		}
	}
}

#endif
