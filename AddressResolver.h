#pragma once
#ifndef ADDRESS_RESOLVER_H
#define ADDRESS_RESOLVER_H

#include "SymbolTable.h"
#include <string>
#include "TokenUtils.h"

struct resolvedAddress {
    int location = -1;
    bool isIndirect = false;
};


int parseAddress(const string& cleaned, SymbolTable& st);

//	This resolves iindirect address e.g a[b] or a[c] or a[5] and
// returns the address of the variable to be used in the instruction memory
int resolveIndirectAddress(const string& token, SymbolTable& st, double instructionMemory[], 
    int& nextInstruction, int& nextData, bool& hasError, const string& currentFn);

//This checks if the token is an array with indirect address e.g a[b] or a[5] or a[c] 
bool isArrayWithIndirectAddress(const string& token, bool& hasError);

resolvedAddress returnAddress(string arg1, SymbolTable& st, double instructionMemory[], 
    int& nextInstruction, int& nextData, bool& hasError, const string& currentFn);

bool isNumberOnly(const string& str);

//To help resolve indirect address 
int resolveIfIndirect(int location, bool isIndirect, double instructionMemory[],
    int& nextInstruction, int& nextData, bool& hasError);

resolvedAddress resolveParameterAddress(const string& currentFn,int paramIndex,const string& token,double instructionMemory[],SymbolTable& st, 
    int& nextInstruction, int& nextData, bool& hasError);

resolvedAddress resolveLocalArrayAddress(const string& token, int baseOffset, string& indexToken, double instructionMemory[], SymbolTable& st,
    int& nextInstruction, int& nextData, bool& hasError, const string& currentFn);

resolvedAddress resolveLocalAddress(int offset, double instructionMemory[], SymbolTable& st,
    int& nextInstruction, int& nextData, bool& hasError);

resolvedAddress resolveToken(const string& token, SymbolTable& st, double instructionMemory[],
    int& nextInstruction, int& nextData, bool& hasError, const string& currentFn);

int allocateStringLiteral(const string& str, int& nextData, double instructionMemory[],
    bool& hasError, int size, int startingAddress);

void validateRuntimeArrayIndex(const string& token, int& indexAddr, SymbolTable& st, double instructionMemory[],
    int& nextInstruction, int& nextData, bool& hasError, const string& currentFn);

bool hasBalancedBrackets(const string& token, bool& hasError);

bool hasValidColonPrefix(const string& token, bool& hasError);

int getOrCreateConstant(const string& valueStr, SymbolTable& st, double instructionMemory[],
    int& nextData, bool& hasError);

#endif