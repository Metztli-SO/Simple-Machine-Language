#pragma once
#ifndef STRING_LITERAL_ALLOCATOR_H
#define STRING_LITERAL_ALLOCATOR_H

#include <string>
#include <iostream>
#include "MemoryLayout.h"

using std::cerr;
using std::string;

//This is to allocate string literals
int allocateStringLiterals(const string& str, int& nextData, double instructionMemory[], 
	bool& hasError, int size, int startingAddress,int lineNum)
{
	int len, numWords, base, allowableSize;

	int strSize = str.size();

	//This is for string literals that are nit assugned to a variable e.g print "hello"
	if (size == 0 && startingAddress == 0)
	{
		len = str.size();
		numWords = (len + 1) / 2;
		base = nextData - numWords;
		allowableSize = len * 2;
		//cout << "Allocating string '" << str << "' at base " << base << ", nextData becomes " << base - 1 << endl;
		instructionMemory[base] = (numWords) * 100;
	}
	else {
		len = (size - 1) * 2;
		if (strSize > len)//check if the string literal exceeds the declared size of the varable
		{
			hasError = true;
			cerr << "Error on line " << lineNum << ": ";
			cerr << "Error: String literal exceeds declared size of variable" << endl;
			return -1;
		}
		numWords = size - 1;
		base = startingAddress;
		instructionMemory[base] = (len) * 100;
	}
	if (base < Mem::GLOBAL_START)
	{
		cerr << "Error: Not enough memory to allocate string literal" << endl;
		hasError = true;
		return -1;
	}

	int count = 0;
	int usedLen = (len < strSize) ? len : strSize;
	for (int i = 0;i < usedLen;i += 2)
	{
		int high = (i < usedLen) ? static_cast<int>(str[i]) : 0;
		int low = ((i + 1) < usedLen) ? static_cast<int>(str[i + 1]) : 0;
		instructionMemory[base + 1 + count] = high * 1000 + low;
		count++;
	}
	nextData = base - 1;
	return base;
}

//Allocating string variables to string is a core and somehow interesting part
	//local variables are alocated at parameter stack so it is allocated at runtime
	//so this one is for local string literals
void allocateLocalStringLiteral(const string& str, int baseOffset, int declaredSize,
	double instructionMemory[], SymbolTable& st, int& nextInstruction, int& nextData, bool& hasError)
{
	int strSize = static_cast<int>(str.size());
	int len = (declaredSize - 1) * 2;   // max string literal storable, same formula as allocateStringLiteral
	if (strSize > len)
	{
		hasError = true;
		cerr << "Error: String literal exceeds declared size of variable" << endl;
		return;
	}
	int numWords = (strSize + 1) / 2;
	//header word(numWords * 100), matching allocateStringLiteral's convention
	int header = numWords * 100;
	//I want to load the numwords into the location indirectly and because allocate string literal is different it doesnt allow
	// indirect load or store
	int headerConstLoc = getOrCreateConstant(std::to_string(header), st, instructionMemory, nextData, hasError);
	auto [loc0, isInd0] = resolveLocalAddress(baseOffset, instructionMemory, st, nextInstruction, nextData, hasError);
	instructionMemory[nextInstruction++] = Op::LOAD + headerConstLoc;
	//load the numwords*100 indirectly into the location of the baseOffset+ paramBase
	instructionMemory[nextInstruction++] = (isInd0 ? Op::INDIRECT_STORE : Op::STORE) + loc0;

	int usedLen = (len < strSize) ? len : strSize;
	int count = 0;
	//the location is being allocated at runtime so i have to allocate each string characters into the location also at runtime
	for (int i = 0; i < usedLen; i += 2)
	{
		int high = static_cast<int>(str[i]);
		int low = ((i + 1) < usedLen) ? static_cast<int>(str[i + 1]) : 0;
		int packed = high * 1000 + low;

		int packedConstLoc = getOrCreateConstant(std::to_string(packed), st, instructionMemory, nextData, hasError);
		auto [loc, isInd] = resolveLocalAddress(baseOffset + 1 + count, instructionMemory, st, nextInstruction, nextData, hasError);
		instructionMemory[nextInstruction++] = Op::LOAD + packedConstLoc;
		instructionMemory[nextInstruction++] = (isInd ? Op::INDIRECT_STORE : Op::STORE) + loc;
		count++;
	}
}

//This is for string literals that are to be allocated to global variables
//assigning the literal at compile time means the last literals asigned will be the one to be written e.g
//let a = "hello" it will be assigned at runtime 
//print a
// let a = "hay"
//print a
// if it was assigned at compile time hay is the last assignment, so it prints out hayhay
void assignGlobalStringLiteral(const string& str, int baseAddr, int declaredSize,
	double instructionMemory[], SymbolTable& st, int& nextInstruction, int& nextData, bool& hasError) {
	int strSize = static_cast<int>(str.size());
	int len = (declaredSize - 1) * 2;

	if (strSize > len)
	{
		hasError = true;
		cerr << "Error: String literal exceeds declared size of variable" << endl;
		return;
	}

	int numWords = (strSize + 1) / 2;
	// header word (numWords * 100) — same convention as allocateStringLiteral,
	// but now written by a real runtime store instead of a compile-time store
	int header = numWords * 100;
	int headerConstLoc = getOrCreateConstant(std::to_string(header), st, instructionMemory, nextData, hasError);
	instructionMemory[nextInstruction++] = Op::LOAD + headerConstLoc;
	instructionMemory[nextInstruction++] = Op::STORE + baseAddr;

	int usedLen = (len < strSize) ? len : strSize;
	int count = 0;
	for (int i = 0; i < usedLen; i += 2)
	{
		int high = static_cast<int>(str[i]);
		int low = ((i + 1) < usedLen) ? static_cast<int>(str[i + 1]) : 0;
		int packed = high * 1000 + low;

		int packedConstLoc = getOrCreateConstant(std::to_string(packed), st, instructionMemory, nextData, hasError);
		instructionMemory[nextInstruction++] = Op::LOAD + packedConstLoc;
		instructionMemory[nextInstruction++] = Op::STORE + (baseAddr + 1 + count);
		count++;
	}
}

#endif // !STRING_LITERAL_ALLOCATOR_H
