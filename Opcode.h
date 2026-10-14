#pragma once
#ifndef OPCODE_H
#define OPCODE_H

//These are the opcodes used . There are more explanation for each one of them in "SML.h"
//The instruction format is 5 digit the first 2 is the opcode which determines what type of instruction while the last three digit 
//is the address/location of the operand
//e.g 10080 means 10 is the opcode and 080 is the address/location of tge operand
//which means read from the screen into address 80
namespace Op {
    constexpr int READ = 10000;
    constexpr int WRITE = 11000;
    constexpr int NEWLINE = 12000;
    constexpr int INDIRECT_READ = 13000;
    constexpr int INDIRECT_WRITE = 14000;

    //SP means stackPointers, ACC means accumulator
    constexpr int SP_INC = 15000;
    constexpr int SP_DEC = 16000;
    constexpr int SP_BRANCH = 17000;
    constexpr int SP_PUSH = 18000;
    constexpr int SP_POP_TO_ACC = 19000;

    constexpr int LOAD = 20000;
    constexpr int STORE = 21000;
    constexpr int INDIRECT_LOAD = 22000;
    constexpr int INDIRECT_STORE = 23000;

    constexpr int PARAM_BASE = 24000;
    constexpr int PARAM_PTR = 25000;
    constexpr int PARAM_PTR_STORE = 26000;
    constexpr int PARAM_INC = 27000;
    constexpr int PARAM_RESET = 28000;
    constexpr int PARAM_BASE_LOAD = 29000;

    constexpr int ADD = 30000;
    constexpr int SUB = 31000;
    constexpr int DIV = 32000;
    constexpr int MUL = 33000;
    constexpr int MOD = 34000;
    constexpr int POW = 35000;
    constexpr int BRANCH_INDIRECT = 36000;

    constexpr int SET_PARAM_BASE = 38000;
    //branch_ne means branch if not equal to zero
    constexpr int BRANCH_NE = 39000;
    constexpr int BRANCH = 40000;
    constexpr int BRANCH_NEG = 41000;
    constexpr int BRANCH_ZERO = 42000;
    constexpr int HALT = 43000;
    constexpr int DEBUG_TOGGLE = 44000;

    constexpr int PARAM_ADD_ACC = 45000;
    constexpr int PARAM_RESTORE = 46000;

    constexpr int READ_STRING = 50000;
    constexpr int PRINT_STRING = 51000;
    constexpr int BRANCH_POS = 52000;
    constexpr int PRINT_STRING_INDIRECT = 53000;

}
#endif // !OPCODE_H

