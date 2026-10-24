#pragma once
#ifndef SML_H
#define SML_H

#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <cmath>
#include <string>
#include <limits>
#include<ios>
#include <fstream>

#include "../memory/MemoryLayout.h"

using std::string;
using std::pow;
using std::setw;
using std::setfill;
using std::cerr;
using std::cout;
using std::cin;
using std::endl;
using std::showpos;
using std::noshowpos;
using std::left;
using std::right;
using std::internal;
using std::fixed;
using std::setprecision;

//The total Memory Size
static const int MEMORY_SIZE = Mem::TOTAL_SIZE;
//Tstack pointer stores the return address,total number of a function local variable,the parambase
static int stackPointers = Mem::STACK_START;
//parameterPointer is a bump allocator
//it always points at the next free slot in the 220–269 parameter region.
// It only ever moves forward(push) or gets restored backward(pop - all - at - once on return) —
static int parameterPointer= Mem::PARAMETER_START;
// paramBase is the base address of the CURRENTLY EXECUTING function's frame -
// the fixed reference point every parameter/local inside that function is
// addressed from, as paramBase + offset (opcode 24 loads it; see
// resolveParameterAddress/resolveLocalAddress in AddressResolver.cpp for the
// codegen side). It's captured from parameterPointer's value right before that
// call's arguments are pushed (see the "call" codegen in Parser.h), so it marks
// exactly where this call's frame begins. Analogous to x86's EBP versus ESP:
// parameterPointer is the growing stack pointer, paramBase is the fixed frame
// pointer for whichever call is currently running. Saved and restored across
// every call (opcodes 38/29) so recursion and nested calls each see their own
// correct paramBase without corrupting the caller's.
static int paramBase = Mem::PARAMETER_START;

class Memory
{
public:
	Memory()
	{
		debug = true;
		initialize();
		halt = false;
		instructionCounter = 0;
		accumulator = 0.0;
		Perform();
	}
	void initialize()
	{
		for (int i = 0;i < MEMORY_SIZE;i++)
		{
			memory[i] = 0.0;
		}
	}
	void Perform() {
		/*cout << "****\tWelcome to My Simpletron!\t****" << endl;
		cout << "*** Please enter your program one instruction ***\n"
			<< "*** (or data word) at a time. I will type the ***\n"
			<< "*** location number and a question mark (?).  ***\n"
			<< "*** You then type the word for that location. ***\n"
			<< "*** Type the sentinel -99999 to stop entering ***\n"
			<< "*** your program. ***\n";*/
			//banner();
		readFile();
		instructionCounter = 0;
		//displaySmlCode();
		while (!halt)
		{
			decode();
			execute();
			/*if(debug)
			{

			}*/
		}
		dump();
	}
	void banner() {
		cout << "__________________________\n"
			<< "\tSimpletron\n"
			<< "__________________________\n";
	}
	void readFile()
	{
		bool canStop = false; //Flag is to control instructionCount  if the -1 has been reached
		std::ifstream read("SML code.txt", std::ios::in);
		if (!read)
		{
			cerr << "File could not be opened!";
			exit(1);
		}
		double check = 0.0;
		for (int i = 0;i < MEMORY_SIZE;i++)
		{
			read >> check;
			memory[i] = check;
			if (memory[i] == -1)
			{
				canStop = true;
			}
			else if (!canStop)
			{
				instructionCount++;
			}
		}
		/*check = 0.0;
		read.seekg(0);
		read >> check;
		while (check != 0) {
			instructionCount++;
			read >> check;
		}*/
	}
	void displaySmlCode()
	{
		while (instructionCounter < instructionCount)
		{
			cout << setw(2) << setfill('0') << instructionCounter << " ? " << memory[instructionCounter] << "\n";
			instructionCounter++;
		}
		instructionCounter = 0;
		cout << "*** Program loading completed ***\n*** Program execution begins  ***\n";
	}
	void decode()
	{
		//The instruction format is 5 digit the first 2 is the opcode which determines what type of instruction while the last three digit 
		//is the address/location of the operand
		//e.g 10080 means 10 is the opcode and 080 is the address/location of tge operand
		//which means read from the screen into address 80
		instructionRegister = memory[instructionCounter++];
		operationCode = instructionRegister / 1000;
		location = instructionRegister % 1000;
	}
	void execute()
	{
		switch (operationCode)
		{
		case 10:	//read
			//cout << "Enter your Number: ";
			cin >> memory[location];
			break;
		case 11://write to screen
			cout << memory[location];
			break;
		case 12://newline
			cout << endl;
			break;

		//13 - 14 is for indirect printing and inputing of values
		case 13://read  from the screen indirect
			cin >> memory[static_cast<int>(memory[location])];
			break;
		case 14://write indirect to the screen
			cout << memory[static_cast<int>(memory[location])];
			break;

			//15-19 are for stack pointer
			//This increments the stack pointer for every call a return address is being pushed into the stack pointer
		case 15:
			++stackPointers;
			break;
			//This decrements the stack Pointer for every }. reached the return address is being popped off the stack pointer
		case 16:
			--stackPointers;
			break;
			//This branches to the current address on the top of stack pointer
		case 17:
			instructionCounter = static_cast<int>(memory[stackPointers]);
			break;
		case 18: // store accumulator into memory[stackPointers] — the actual stack push
			//Guard to ensure it doesnt flow beyond stack end and corrupt other data
			//270 - 298 is for stack return address
			if (stackPointers <= Mem::STACK_END)
			{
				memory[stackPointers] = accumulator;
			}
			else {
				cerr << "Error: Function stack is out of memory (stackPointers=" << stackPointers << ")" << endl;
				halt = true;
			}
			break;
		// acc = mem[stackPointers]
		case 19:
			accumulator = memory[stackPointers];
			break;

		case 20://Load to accumulator
			accumulator = memory[location];
			break;
		case 21://Load from accumulator
			memory[location] = accumulator;
			break;

			//This is for indirect adressing mode
			//This is neccessary because variables would need to be used as index for arrays
		case 22://Load to accumulator indirect
			accumulator = memory[static_cast<int>(memory[location])];
			break;
		case 23: //Load from accumulaor indirect
			memory[static_cast<int>(memory[location])] = accumulator;
			break;

			//24-29 is reserved as paramters opcode
			// See paramBase declaration comment above for the full explanation.
		case 24:
			accumulator = paramBase;
			break;

		// See parameterPointer declaration comment above for the full explanation.
		case 25:
			accumulator = parameterPointer;
			break;

		// Push one parameter/local value into the slot parameterPointer currently
		// points at (part of the push-then-increment pattern; see case 27).
		case 26:
			//Guard to ensure it doesnt flow beyond stack end and corrupt the other memory
			// 220 - 269 is for storing parameter passed to functions
			if (parameterPointer <= Mem::PARAMETER_END) {
				memory[parameterPointer] = accumulator;
			}
			else {
				cerr << "Error: Parameter stack is out of memory (ParameterPointer=" << parameterPointer << ")" << endl;
				halt = true;
			}
			break;

		// Advances parameterPointer past the slot just written by case 26 -
		// together these two opcodes are the "push" half of the frame protocol.
		case 27:
			//This increments the parameter pointer after each parameter is loaded into memory
			parameterPointer++;
			break;

		case 28: //This reset the parampointer after every function goes out of scope
			// NOTE: resets parameterPointer to the CURRENT paramBase, not the caller's saved value  
			// this opcode is not used by the current call/return codegen,
			// which instead uses case 46 (PARAM_RESTORE) to unwind via the saved stack
			// value. Kept for reference / potential future use.
			parameterPointer = paramBase;
			break;
		case 29:
			// Restores the CALLER's paramBase from the stack
			paramBase = static_cast<int>(memory[stackPointers]);
			break;

		case 30://add
			accumulator += memory[location];
			break;
		case 31://subtract
			accumulator -= memory[location];
			break;
		case 32://divide
			if (memory[location] != 0)
			{
				accumulator /= memory[location];
			}
			else {
				cerr << "Memory at Location " << location << " is not available" << endl;
				halt = true;
			}
			break;
		case 33://multiply
			accumulator *= memory[location];
			break;
		case 34://modulus 
		{
			int tempm = 0;
			tempm = static_cast<int>(memory[location]);
			int tempacc = static_cast<int>(accumulator);
			if (tempm != 0)
			{
				tempacc = tempacc % tempm;
				accumulator = tempacc;
			}
			else {
				cerr << "Memory at Location " << location << " is not available" << endl;
				halt = true;
			}
			break;
		}
		case 35:// raise to power
			accumulator = pow(accumulator, memory[location]);
			break;
		case 36: //Branch indirect Needed to acesss
			instructionCounter = static_cast<int>(memory[location]);
			break;

		case 38:
			// Sets paramBase to the CALLEE's captured frame base, right before
			// branching into the function body (last step of the call sequence,
			// after all arguments have been pushed using the OLD paramBase).
			// write accumulator into paramBase directly
			paramBase = static_cast<int>(accumulator);
			break;

		case 39://branch if not equal to zero
			if (accumulator != 0)
			{
				instructionCounter = location;
			}
			break;
		case 40://branch
			instructionCounter = location;
			break;
		case 41://branch if negative
			if (accumulator < 0)
			{
				instructionCounter = location;
			}
			break;
		case 42://branch if Zero
			if (accumulator == 0)
			{
				instructionCounter = location;
			}
			break;
		case 43: //end
			halt = true;
			break;
		case 44://debug
			if (static_cast<int>(instructionRegister) == 4401)
			{
				debug = true;
			}
			else if (static_cast<int>(instructionRegister) == 4400)
			{
				debug = false;
			}
			break;
		
		case 45:
			// Reserves this function's local-variable slots by advancing
			// parameterPointer past its already-pushed parameters (accumulator
			// holds the local count, patched in once known - see "fn"/"}." in Parser.h).
			parameterPointer += accumulator;
			break;
		case 46:
			//restores the parameterpointer to its pre_call value, which releases the call's parameter+local slots fo reuse
			parameterPointer = static_cast<int>(memory[stackPointers]);
			break;

		case 50: //read
		{
			string s;
			cout << endl;
			//cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			cout << "Enter your words: ";
			getline(cin, s);

			int len = memory[location];
			int base = location;
			memory[base] = (len) * 100;
			int allowedLen = (len - 1) * 2;

			int count = 0;
			for (size_t i = 0;i < allowedLen;i += 2)
			{
				int high = (i < s.size()) ? static_cast<int>(s[i]) : 0;
				int low = ((i + 1) < s.size()) ? static_cast<int>(s[i + 1]) : 0;
				memory[base + count + 1] = high * 1000 + low;
				count++;
			}
			break;
		}
		case 51: //print string
		{
			int base = location;
			printString(base);
			break;
		}
		//branch if positive
		// if the result in the accumulator is greater than 0 it will branch to the location specified
		case 52:
		{
			if (accumulator > 0)
			{
				instructionCounter = location;
			}
			break;
		}

		//it print string indirect , location holds the address of the string's base, not the base itself
		case 53: {
			// dereference once to get the real base
			int base = static_cast<int>(memory[location]);
			printString(base);
			break;
		}

		default:
			cout << instructionCounter;
			cerr << "*** Invalid operation code ***\n";
			cerr << "*** Simpletron execution abnormally terminated ***\n";
			halt = true;

		}
	}

	void checkOverflow() {
		if (accumulator > 9999 || accumulator < -9999) {
			cerr << "*** Accumulator overflow ***\n";
			cerr << "*** Simpletron execution abnormally terminated ***\n";
			halt = true;
		}
	}

	void dump()
	{
		std::ofstream debug("dump.txt", std::ios::out);
		cout << "\n*** DEBUG MEMORY DUMP ***\n";
		cout << "REGISTERS: \n" << left << setw(22) << setfill(' ') << "accumulator" << right << showpos << setw(4) << setfill('0') << internal << accumulator
			<< left << setw(22) << setfill(' ') << "\ninstructionCounter" << right << showpos << setw(2) << setfill('0') << internal << instructionCounter
			<< left << setw(22) << setfill(' ') << "\ninstructionRegister" << right << showpos << setw(4) << setfill('0') << internal << instructionRegister
			<< left << setw(22) << setfill(' ') << "\noperationCode" << right << showpos << setw(2) << setfill('0') << internal << operationCode
			<< left << setw(22) << setfill(' ') << "\noperand" << right << showpos << setw(2) << setfill('0') << internal << location
			<< left << setw(22) << setfill(' ') << "\nStack Pointer" << right << showpos << setw(2) << setfill('0') << internal << stackPointers << "\n";
		cout << "\nMEMORY:\n";
		for (int i = 0;i < 10;i++)
		{
			debug << setw(9) << setfill(' ') << noshowpos << i;
			cout << setw(9) << setfill(' ') << noshowpos << i;
			if (i == 9)
			{
				debug << endl;
				cout << endl;
			}
		}
		for (int i = 0;i < MEMORY_SIZE;i++)
		{
			if ((i % 10) == 0)
			{
				debug << noshowpos << right << setw(3) << i << " ";
				cout << noshowpos << right << setw(3) << i << " ";
			}
			debug << showpos << left << setw(8) << setfill('0') << internal << memory[i] << " ";
			cout << showpos << left << setw(8) << setfill('0') << internal << memory[i] << " ";
			if ((i - 9) >= 0 && ((i - 9) % 10) == 0)
			{
				debug << endl;
				cout << endl;
			}
		}
	}
private:
	double memory[MEMORY_SIZE];
	double accumulator;
	int instructionCounter;
	int instructionRegister = 0;
	int operationCode;
	int location;
	int instructionCount = 0;
	int check = 0;
	bool halt;
	bool debug;

	void printString(int base) {
		bool strcheck = false;
		int size = static_cast<int>(memory[base] / 100);
		int allowedSize = (size) * 2;
		string s;
		int count = 0;
		for (int i = 0;i < allowedSize;i += 2)
		{
			int word = static_cast<int>(memory[base + count + 1]);

			int high = word / 1000;
			int low = word % 1000;

			//This is to check it the string doesnt have any character with \ + other letters like \n 
			//for newline
			if (!strcheck)
			{
				if (high == 0)break;

				if (high == 92 && low == 110)
				{
					s += "\n";
					count++;
					continue;
				}
				s += static_cast<char>(high);

				if (low == 0)break;

				//to check if lower is "\"
				if (low == 92)
				{
					strcheck = true;
					count++;
					continue;
				}
				if ((i + 1) < allowedSize)
				{
					s += static_cast<char>(low);
				}
				count++;
			}
			else {
				if (high == 0)break;
				if (high == 110)
				{
					s += "\n";
					strcheck = false;
				}

				if (low == 0)break;
				if (low == 92)
				{
					strcheck = true;
					count++;
					continue;
				}
				if ((i + 1) < allowedSize)
				{
					s += static_cast<char>(low);
				}
				count++;
			}
		}
		cout << s;
	}

};

#endif