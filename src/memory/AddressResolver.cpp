#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

#include "AddressResolver.h"
#include "../compiler/SymbolTable.h"
#include "TempPool.h"
#include "../sml/Opcode.h"
#include "../utils/TokenUtils.h"

using std::cerr;
using std::endl;

static int errStrAddr = -1; // Global variable to store the address of the error string


int parseAddress(const string& cleaned, SymbolTable& st)
{
	string token = cleaned;

	token.erase(remove(token.begin(), token.end(), ','), token.end());
	if (token.empty())
		return -1;

	bool localErr = false;
	if (!hasBalancedBrackets(token, localErr) || !hasValidColonPrefix(token, localErr))
	{
		cerr << "Error: Malformed token '" << token << "'" << endl;
		return -1;
	}

	//Explicit-global '::' prefix — strip before any further processing
	if (token.size() >= 2 && token[0] == ':' && token[1] == ':') 
		token = token.substr(2);

	size_t pos = token.find('[');
	bool isArray = (pos != string::npos && token.back() == ']');

	int addr;
	if (isArray)
	{
		string arrayName = token.substr(0, pos);
		string indexExpr = token.substr(pos + 1, token.size() - pos - 1);

		try
		{
			int index = std::stoi(indexExpr);
			addr = st.lookupArraySize(arrayName, index);
			return addr;
		}
		catch (const std::out_of_range& e)
		{
			cerr << "Error: " << e.what() << " for array '" << arrayName << "' " << endl;
			return -1;
		}
		catch (...)
		{
			cerr << "Error: Invalid index expression for array '" << arrayName << "' " << endl;
			return -1;
		}
	}


	else if (isValidIdentifier(token))
	{
		return st.lookup(token);
	}
	else if (isdigit(token[0]))
	{
		return st.lookupConstant(token);
	}
	return -1;
}
//	This resolves iindirect address e.g a[b] or a[c] or a[5] and
// returns the address of the variable to be used in the instruction memory
int resolveIndirectAddress(const string& token, SymbolTable& st, double instructionMemory[], 
	int& nextInstruction, int& nextData, bool& hasError, const string& currentFn)
{
	if (token.size() < 3)
	{
		hasError = true;
		cerr << "Error: Invalid indirect address format '" << token << "' " << endl;
		return -1;
	}
	size_t start = token.find('[');
	size_t end = token.find(']');
	if (start == string::npos || end == string::npos || start >= end)
	{
		hasError = true;
		cerr << "Error: Invalid indirect address format '" << token << "' " << endl;
		return -1;
	}
	string arrayName = token.substr(0, start);
	string innerToken = token.substr(start + 1, end - start - 1);

	//Explicit-global '::' prefix — strip before any further processing
	if (arrayName.size() >= 2 && arrayName[0] == ':' && arrayName[1] == ':')
		arrayName = arrayName.substr(2);

	if(!isValidIdentifier(innerToken))
	{
		hasError = true;
		cerr << "Error: Invalid index token '" << innerToken << "' in indirect address '" << token << "' " << endl;
		return -1;
	}

	int arrayAddr = st.lookup(arrayName);
	//I made this because a parameter might be used as index
	//e.h fn get(idx) { print nums[idx] }
	auto[innerTokenaddr,isInd] = resolveToken(innerToken,st,instructionMemory,nextInstruction,nextData,hasError,currentFn);
	
	if (innerTokenaddr == -1)
	{
		hasError = true;
		cerr << "Error: Could not resolve address for '" << innerToken << "' " << endl;
		return -1;
	}

	//This helps to resolve the address if it is indirect
	int innerTokenaddress = resolveIfIndirect(innerTokenaddr, isInd, instructionMemory, nextInstruction, nextData, hasError);
	
	if (arrayAddr == -1)
	{
		hasError = true;
		cerr << "Error: Could not resolve address for array '" << arrayName << "' " << endl;
		return -1;
	}

	//I want to make the code safe because i dont trust programmers to always use the correct index for arrays 
		// so i will check if the index is less than the size of the array thought it comes at the cost of performance 
		// because it will generate more instruction 
		// but it is worth it to prevent bugs
	validateRuntimeArrayIndex(token, innerTokenaddress, st, instructionMemory, nextInstruction, nextData, hasError, currentFn);

	string baseStr = std::to_string(arrayAddr);
	int baseConstAddr = getOrCreateConstant(baseStr, st, instructionMemory, nextData, hasError);
	instructionMemory[nextInstruction++] = Op::LOAD + innerTokenaddress;
	instructionMemory[nextInstruction++] = Op::ADD + baseConstAddr;
	int tempAddr = tempPool.acquire(hasError);
	if (tempAddr == -1)
	{
		return -1;
	}
	instructionMemory[nextInstruction++] = Op::STORE + tempAddr;

	return tempAddr;

}

//This checks if the token is an array with indirect address e.g a[b] or a[5] or a[c] 
bool isArrayWithIndirectAddress(const string& token, bool& hasError)
{
	size_t start = token.find('[');
	size_t end = token.find(']');
	if (start != string::npos && end != string::npos && start < end)
	{
		string innerToken = token.substr(start + 1, end - start - 1);
		if (innerToken.empty())
		{
			hasError = true;
			cerr << "Error: Invalid indirect address format '" << token << "' " << endl;
			return false;
		}
		return !(isNumberIndexOnly(innerToken));
	}
	return false;
}


resolvedAddress returnAddress(string arg1, SymbolTable& st, double instructionMemory[], 
	int& nextInstruction, int& nextData, bool& hasError, const string& currentFn)
{
	resolvedAddress result;
	//This part is to check it it requires indirect address resolution e.g let a[b] = 5 or let a[b] = c[d]
	bool isIndirect = isArrayWithIndirectAddress(arg1, hasError);
	int address;

	if (isIndirect) {
		if (hasError)
		{
			//This is to prevent the program from continuing if there is an error in the indirect address
			return {};
		}
		address = resolveIndirectAddress(arg1, st, instructionMemory, nextInstruction, nextData, hasError,currentFn);
	}
	else {
		address = parseAddress(arg1, st);	//get location of the argument
	}
	result.location = address;
	result.isIndirect = isIndirect;

	return result;
}


int resolveIfIndirect(int location, bool isIndirect, double instructionMemory[],
	int& nextInstruction, int& nextData, bool& hasError)
{
	if (!isIndirect) return location;
	int temp = tempPool.acquire(hasError);
	if (temp == -1)
	{
		return -1;
	}
	instructionMemory[nextInstruction++] = Op::INDIRECT_LOAD + location;
	instructionMemory[nextInstruction++] = Op::STORE + temp;
	return temp;
}

resolvedAddress resolveParameterAddress(const string& currentFn, int paramIndex, const string& token,double instructionMemory[], SymbolTable& st,
	int& nextInstruction, int& nextData, bool& hasError)
{
	resolvedAddress result;

	int innerTokenAddr = -1;

	size_t pos = token.find('[');
	bool isArray = (pos != string::npos && token.back() == ']');
	//Arrays have different way of resolving their address because they are stored in a different way in memory
	if (isArray) {
		if (token.size() < 3)
		{
			hasError = true;
			cerr << "Error: Invalid indirect address format '" << token << "' " << endl;
			return {};
		}
		size_t start = token.find('[');
		size_t end = token.find(']');
		if (start == string::npos || end == string::npos || start >= end)
		{
			hasError = true;
			cerr << "Error: Invalid indirect address format '" << token << "' " << endl;
			return {};
		}

		//validate if the token is of type array or string
		string pass = token;
		string type = st.type(pass, currentFn);
		if (!(type == "array" || type == "string")) {
			hasError = true;
			cerr << "Error: Expected type array or string, got \"" << type << "\"" << endl;
		}

		string innerToken = token.substr(start + 1, end - start - 1);
		//if the index is number only
		if (isNumberIndexOnly(innerToken)){
			innerTokenAddr = getOrCreateConstant(innerToken, st, instructionMemory, nextData, hasError);
		}
		//if the index is a variable or parameter
		else if (isValidIdentifier(innerToken))
		{
			auto[innerTokenaddress, isInd] = resolveToken(innerToken, st, instructionMemory, nextInstruction, nextData, hasError,currentFn);
			if (innerTokenaddress == -1)
			{
				hasError = true;
				cerr << "Error: Could not resolve address for '" << innerToken << "' " << endl;
				return {};
			}
			innerTokenAddr = resolveIfIndirect(innerTokenaddress, isInd, instructionMemory, nextInstruction, nextData, hasError);
			if (innerTokenAddr == -1)
			{
				hasError = true;
				cerr << "Error: Could not resolve address for '" << innerToken << "' " << endl;
				return {};
			}
		}
		//else it is an error because it is not a number or a variable
		else {
			hasError = true;
			cerr << "Error: Invalid index expression for array '" << token << "' " << endl;
			return {};
		}
	}
	// 1. Load frameBase's current value into accumulator
	instructionMemory[nextInstruction++] = Op::PARAM_BASE;
	// 2. Add paramIndex to it (paramIndex needs to be a compile-time constant — 
	string paramIndexStr = std::to_string(paramIndex);
	int paramIndexLoc = getOrCreateConstant(paramIndexStr, st, instructionMemory, nextData, hasError);
	instructionMemory[nextInstruction++] = Op::ADD + paramIndexLoc;

	//if it is an array with indirect address e.g a[b] or a[c] or a[5]
	//also add the innerTokenAddr to the accumulator
	//The array stores the base address of the array in memory and the innerTokenAddr is the index of the array
	//e.g if the base address of the array is 100 and the index is 2 then the address of the element is 102
	//so to get the address of a parameter we add the memory(frame base address + paramIndex) + innerTokenAddr
	if (isArray) {
		int tempSpace = tempPool.acquire(hasError);
		if (tempSpace == -1)
		{
			return {};
		}
		//load the parameter address that stores the base address of the array into the tempSpace
		instructionMemory[nextInstruction++] = Op::STORE + tempSpace;
		//load the base address of the array into the accumulator using indirect addressing
		instructionMemory[nextInstruction++] = Op::INDIRECT_LOAD + tempSpace;
		//add the innerTokenAddr to the base address of the array to get the address of the element
		instructionMemory[nextInstruction++] = Op::ADD + innerTokenAddr;
	}
	int tempAddr = tempPool.acquire(hasError);
	if (tempAddr == -1)
	{
		return {};
	}
	instructionMemory[nextInstruction++] = Op::STORE + tempAddr;

	result.location = tempAddr;
	result.isIndirect = true;
	
	return result;
}

//if the local variable is an array it is being resolved here. 
// The baseOffset is the offset of the local array in the function's local variable space,
// and indexToken is the index expression for the array element being accessed.
//Every local variable is being stored in the parameter stack and is being accessed using the parameter pointer
//so for an array the address of the array index is calculated by adding the baseOffset of the array to the indexToken 
// and then adding that to the parameter pointer
resolvedAddress resolveLocalArrayAddress(const string& token,int baseOffset, string& indexToken, double instructionMemory[], SymbolTable& st,
	int& nextInstruction, int& nextData, bool& hasError, const string& currentFn) {
	string pass = token;
	string type = st.type(pass, currentFn);
	if (!(type == "array" || type == "string")) {
		hasError = true;
		cerr << "Error: Expected type array or string, got \"" << type << "\"" << endl;
	}

	//accumulator = paramBase (same base as parameters)
	instructionMemory[nextInstruction++] = Op::PARAM_BASE;   

	string offsetStr = std::to_string(baseOffset);
	int offsetLoc = getOrCreateConstant(offsetStr, st, instructionMemory, nextData, hasError);

	//add the baseOffset to the accumulator to get the base address of the local array in the parameter pointer
	instructionMemory[nextInstruction++] = Op::ADD + offsetLoc;   

	//i am saving the value of the accumulator (the base address of the local array + offset) into a temporary memory location
	// More instructions will use the accumulator to calculate the address of the specific element in the local array so might get overwwitten
	int baseSumAddr = -1;

	//the index is being resolved exactly like resolveParameterAddress's array branch does
	int indexAddr;
	if (isNumberIndexOnly(indexToken)) {
		int index = std::stoi(indexToken);

		//I want to validate if the index is not more than the size of the array 
		// so i will check if the index is less than the size of the array
		//so it raises an exception if the index is more than the size of the array
		// and i will catch it and display an error message
		try {
			int addr = st.lookupArraySize(token, index, currentFn);
		}
		catch (const std::out_of_range& e)
		{
			hasError = true;
			cerr << "Error: " << e.what() << " for array '" << st.baseSymbol(token) << "' " << endl;
			return {};
		}
		catch (...)
		{
			hasError = true;
			cerr << "Error: Invalid index expression for array '" << st.baseSymbol(token) << "' " << endl;
			return {};
		}

		indexAddr = getOrCreateConstant(indexToken, st, instructionMemory, nextData, hasError);
	}
	else if(isValidIdentifier(indexToken)){
		//Resolving the token will generate a lot of instruction and use the accumulator
		//so we need to save the base address of the local array + offset into a temporary memory location before resolving the token
		baseSumAddr = tempPool.acquire(hasError);
		if (baseSumAddr == -1)
		{
			return {};
		}
		instructionMemory[nextInstruction++] = Op::STORE + baseSumAddr;
		auto [indexLocation, isInd] = resolveToken(indexToken, st, instructionMemory, nextInstruction, nextData, hasError, currentFn);
		if (indexLocation == -1)
		{
			hasError = true;
			cerr << "Error: Could not resolve address for '" << indexToken << "' " << endl;
			return {};
		}
		indexAddr = resolveIfIndirect(indexLocation, isInd, instructionMemory, nextInstruction, nextData, hasError);
		if (indexAddr == -1)
		{
			hasError = true;
			cerr << "Error: Could not resolve address for '" << indexToken << "' " << endl;
			return {};
		}

		//I want to make the code safe because i dont trust programmers to always use the correct index for arrays 
		// so i will check if the index is less than the size of the array thought it comes at the cost of performance 
		// because it will generate more instruction 
		// but it is worth it to prevent bugs
		validateRuntimeArrayIndex(token,indexAddr, st, instructionMemory, nextInstruction, nextData, hasError, currentFn);
		
		//after it has been resolved we need to restore the base address of the local array + offset into the accumulator
		instructionMemory[nextInstruction++] = Op::LOAD + baseSumAddr;
	}
	else {
		hasError = true;
		cerr << "Error: Invalid index expression for array '" << indexToken << "' " << endl;
		return {};
	}
	//add the indexAddr to the accumulator to get the address of the specific element in the local array
	//e.g if the baseOffset of the array is 100 and the index is 2 then the address of the element is 102
	instructionMemory[nextInstruction++] = Op::ADD + indexAddr;   
	int tempAddr = tempPool.acquire(hasError);
	if (tempAddr == -1)
	{
		return {};
	}
	instructionMemory[nextInstruction++] = Op::STORE + tempAddr;

	resolvedAddress result;
	result.location = tempAddr;
	result.isIndirect = true;
	return result;
}

resolvedAddress resolveLocalAddress(int offset, double instructionMemory[], SymbolTable& st,
	int& nextInstruction, int& nextData, bool& hasError)
{
	instructionMemory[nextInstruction++] = Op::PARAM_BASE;   // acc = paramBase (same base as parameters)

	string offsetStr = std::to_string(offset);
	int offsetLoc = getOrCreateConstant(offsetStr, st, instructionMemory, nextData, hasError);
	
	instructionMemory[nextInstruction++] = Op::ADD + offsetLoc;  // acc = paramBase + offset
	int tempAddr = tempPool.acquire(hasError);
	if (tempAddr == -1)
	{
		return {};
	}
	instructionMemory[nextInstruction++] = Op::STORE + tempAddr;   // mem[temp] = acc (the address)
	resolvedAddress result;
	result.location = tempAddr;
	result.isIndirect = true;   // same as parameters — it's an address stored in a temp cell
	return result;
}
resolvedAddress resolveToken(const string& token, SymbolTable& st, double instructionMemory[],
	int& nextInstruction, int& nextData, bool& hasError, const string& currentFn)
{
	//To make sure some of the expressions are valid and to prevent errors from being generated in the instruction memory
	if (!hasBalancedBrackets(token, hasError) || !hasValidColonPrefix(token, hasError))
	{
		return {};
	}

	size_t bracketPos = token.find('[');
	if (bracketPos != string::npos)
	{
		string baseName = token.substr(0, bracketPos);
		if (st.type(baseName, currentFn) == "string")
		{
			hasError = true;
			cerr << "Error: String indexing is not supported yet ('" << token << "')" << endl;
			return {};
		}
	}

	// :: prefix indicates a global variable, so we strip it and look up the global variable directly
	// I used it because my favorite programming language C++ uses it to indicate global variables 
	// and i wanted to use it in my language too
	if (!token.empty() && token.substr(0, 2) == "::") {
		string globalVar = token.substr(2);
		return returnAddress(globalVar, st, instructionMemory, nextInstruction, nextData, hasError, currentFn);
	}
	if (!currentFn.empty()) {
		//The first is to check if the variable being passed is a parameter of the function
		int paramIdx = st.getParameterIndex(currentFn, token);
		if (paramIdx != -1)
		{
			// build resolvedAddress via frameBase + paramIdx, emit the temp-computation instructions
			return resolveParameterAddress(currentFn, paramIdx, token, instructionMemory, st, nextInstruction, nextData, hasError);
		}

		//This checks if the variable is a local variable of the function
		if (st.isLocalOf(currentFn, token))
		{
			size_t bracketPos = token.find('[');
			if (bracketPos != string::npos && token.back() == ']')
			{
				string indexToken = token.substr(bracketPos + 1, token.size() - bracketPos - 2);
				int baseOffset = st.lookupLocal(currentFn, token);
				return resolveLocalArrayAddress(token,baseOffset, indexToken, instructionMemory, st, nextInstruction, nextData, hasError, currentFn);
			}
			int offset = st.lookupLocal(currentFn, token);
			return resolveLocalAddress(offset, instructionMemory, st, nextInstruction, nextData, hasError);
		}
	}
	return returnAddress(token, st, instructionMemory, nextInstruction, nextData, hasError,currentFn);
}

int allocateStringLiteral(const string& str, int& nextData, double instructionMemory[], 
	bool& hasError, int size, int startingAddress)
{
	int len, numWords, base, allowableSize;

	int strSize = str.size();

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

void validateRuntimeArrayIndex(const string& token,int& indexAddr, SymbolTable& st, double instructionMemory[], 
	int& nextInstruction, int& nextData, bool& hasError, const string& currentFn) {
	int arraySize = -1;
	//If it is a local variable of a function i will get the size of the array from the symbol table 
	// using this way
	if (!currentFn.empty()) {
		arraySize = st.lookupArraySize(token, 0, currentFn); // Get the size of the array but i made index default 0
	}
	//else use getSize to get the size of the global array from the symbol table
	else {
		arraySize = st.getSize(token);
	}
	string sizeStr = std::to_string(arraySize);
	int arraySizeLoc = getOrCreateConstant(sizeStr, st, instructionMemory, nextData, hasError);

	//First Check if the index is less than 0 e.g a[-1] or a[-5] or a[-c] and 
	// if it is less than 0 then it will print an error message and halt the program
	//negative index for now is not allowed 
	instructionMemory[nextInstruction++] = Op::LOAD + indexAddr; // Load the index into the accumulator
	// Placeholder for the jump instruction which says branch if the accumulator is negative (the index is out of bounds)
	int negJumpPatch = nextInstruction;
	//branch if the accumulator is negative (the index is out of bounds)
	instructionMemory[nextInstruction++] = Op::BRANCH_NEG + 0; // Placeholder for the jump instruction

	//Second Check if the index is greater than or equal to the size of the array e.g a[5] for an array of size 5
	instructionMemory[nextInstruction++] = Op::LOAD + arraySizeLoc; // Load the size of the array into the accumulator
	instructionMemory[nextInstruction++] = Op::SUB + indexAddr; // Subtract the index from the size of the array
	int negPatch = nextInstruction;
	// Placeholder for the jump instruction which says branch if the accumulator is positive (the index is within bounds) 
	instructionMemory[nextInstruction++] = Op::BRANCH_POS + 0;

	//patch the jump instruction to jump to the next instruction if the index is negative (the index is out of bounds)
	instructionMemory[negJumpPatch] = Op::BRANCH_NEG + nextInstruction;

	//I want it to print an error message if the index is out of bounds 
	// so i will allocate a string literal for the error message 
	if (errStrAddr == -1) {
		string errorMessage = "\nError: Array Index '" + st.baseSymbol(token) + "' out of bounds\n";
		errStrAddr = allocateStringLiteral(errorMessage, nextData, instructionMemory, hasError, 0, 0);
	}
	//Print out the message and halt the program if the index is out of bounds
	instructionMemory[nextInstruction++] = Op::PRINT_STRING + errStrAddr;
	instructionMemory[nextInstruction++] = Op::HALT; // Halt the program
	// Patch the jump instruction to jump to the next instruction if the index is within bounds
	instructionMemory[negPatch] = Op::BRANCH_POS + nextInstruction;
}

int validateCompileTimeArrayIndex(const string& token, int index, SymbolTable& st, bool& hasError, const string& currentFn = "") {
	try {
		int addr = st.lookupArraySize(token, index, currentFn);
		return addr;
	}
	catch (const std::out_of_range& e)
	{
		hasError = true;
		cerr << "Error: " << e.what() << " for array '" << st.baseSymbol(token) << "' " << endl;
		return {};
	}
	catch (...)
	{
		hasError = true;
		cerr << "Error: Invalid index expression for array '" << st.baseSymbol(token) << "' " << endl;
		return {};
	}
}
bool hasBalancedBrackets(const string& token, bool& hasError)
{
	int depth = 0;
	for (char c : token)
	{
		if (c == '[') depth++;
		if (c == ']')
		{
			depth--;
			if (depth < 0)
			{
				hasError = true;
				cerr << "Error: Unmatched ']' in '" << token << "'" << endl;
				return false;
			}
		}
	}
	if (depth != 0)
	{
		hasError = true;
		cerr << "Error: Unmatched '[' in '" << token << "'" << endl;
		return false;
	}
	return true;
}

bool hasValidColonPrefix(const string& token, bool& hasError)
{
	// A single ':' anywhere that isn't part of '::' is malformed
	for (size_t i = 0; i < token.size(); i++)
	{
		if (token[i] != ':') continue;
		bool isDoubled = (i + 1 < token.size() && token[i + 1] == ':') ||
			(i > 0 && token[i - 1] == ':');
		if (!isDoubled)
		{
			hasError = true;
			cerr << "Error: Stray ':' in '" << token << "' (did you mean '::'?)" << endl;
			return false;
		}
	}
	return true;
}

int getOrCreateConstant(const string& valueStr, SymbolTable& st, 
	double instructionMemory[],
	int& nextData, bool& hasError)
{
	int loc = st.lookupConstant(valueStr);
	if (loc == -1)
	{
		st.assign(valueStr, "const", nextData, hasError);
		loc = st.lookupConstant(valueStr);
		// Always write the value immediately — a constant created mid-line (during codegen)
		// won't be picked up by Compiler's intializeConstants() until the NEXT line's pass,
		// but instructions on THIS line may reference it before then.
		instructionMemory[loc] = std::stod(valueStr);
	}
	return loc;
}