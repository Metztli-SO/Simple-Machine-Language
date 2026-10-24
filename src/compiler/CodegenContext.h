#pragma once
#ifndef CODEGEN_CONTEXT_H
#define CODEGEN_CONTEXT_H

#include "SymbolTable.h"
#include <map>
#include <string>

struct CodegenContext {
    double* instructionMemory;
    int& nextInstruction;
    int& nextData;
    bool& hasError;
    int currentLineNum;
    SymbolTable& st;
    std::map<int, int>& lineToAddr;
    int* flags;  
    std::map<int, string>& fnFlags;
    std::map<string, std::map<string, string>>& fnRetFlags;
    string currentFn;
};
#endif