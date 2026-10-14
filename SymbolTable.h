#pragma once
#ifndef SYMBOLTABLE_H
#define SYMBOLTABLE_H

#include <algorithm>
#include <unordered_set>
#include <string>
#include <sstream>
#include <cctype>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include "MemoryLayout.h"
#include "TokenUtils.h"

using std::vector;
using std::string;
using std::remove;
using std::cerr;
using std::endl;
using std::cout;
struct TableEntry
{
	string symbol= "";
	string type = "";
	int location = -1;
	int size=0;
	bool hasBeenInitialized = false;
	std::vector<bool> elementInitialized;
	string scope = "";
};

struct FunctionEntry
{
	string name = "";
	vector<string> parameterNames;
	vector<string> parameterTypes;
	string returnType = "void";
	int returnedLiteralAddr = -1;   // compile-time base address of the literal this fn returns via 'ret', if any
	int returnedLiteralSize = -1;   // its word count (header word included), same convention as allocateStringLiteral
	bool isDeclared = false;
	bool isDefined = false;
	int firstInstruction = -1;
	int lastInstruction = -1;
	int entryPoint = -1;
};

//This is to make sure string literals in quote dont fall through to wrong check
static string maskQuotedRegions(const string& line)
{
	string masked = line;
	bool inQuotes = false;
	for (size_t i = 0; i < masked.size();i++) {
		char c = masked[i];
		if (c == '"')
		{
			inQuotes = true;
		}
		if (inQuotes && isspace(static_cast<unsigned char>(c))) {
			//This is a control character that is not considered whitespace and is not alphanumeric.
			//It means replace the space with that so iss doesnt treat characters in literals as different
			// iss tokenize by space which means iss >> "hello world!" will be \"hello\ and \world"\
			//but this replaces the space with \x01 which doesnt alloe the string to be divided by space
			masked[i] = '\x01';
		}
	}
	return masked;
}

class SymbolTable
{
public:
	SymbolTable() {}



	void interpret(string& line, int& nextData,bool& hasError, const string& currentFn)
	{
		std::istringstream iss(maskQuotedRegions(line));
		string token;
		int position = 0;

		while (iss >> token)
		{
			token.erase(remove(token.begin(), token.end(), ','), token.end());

			if (position == 0)
			{
				assign(token, "line", nextData, hasError);
			}
			//To check for comments
			else if (token == "rem")
			{
				break;
			}
			//Check if it is an array declaration
			else if (token == "array")
			{
				string arrayName;
				int size;
				if (iss >> arrayName >> size)
				{
					arrayName = baseSymbol(arrayName);
					if (isValidIdentifier(arrayName) && isNotKeyword(arrayName,hasError)) {
						declareArray(arrayName, "array", nextData, size, hasError,currentFn);
					}
					else {
						hasError = true;
						cerr << "Error: Invalid variable name '" << arrayName << "'" << endl;
						return;
					}
				}
				return;
			}
			//fn will be declared or defined later
			else if (token == "fn" || token == "call") {
				break;
			}
			else if (token == "string")
			{
				string stringName;
				int size;
				if (iss >> stringName >> size)
				{
					stringName = baseSymbol(stringName);
					if (isNotKeyword(stringName, hasError))
					{
						declareArray(stringName, "string", nextData, size, hasError);
					}
					
				}
				return;
			}
			else if (token == "let")
			{
				string varName,sign;
				if (iss >> varName >> sign)
				{
					if (sign != "=")
					{
						hasError = true;
						cerr << "Error: Invalid operand in let statement" << endl;
						return;
					}

					//I want to allow variables that are not declared to be used in let statements as
					//long as they contain string literals because they will be allocated at runtime 
					//and they dont need to be declared e.g let a = "Hello" without declaring a as a string variable 

					//to check it it has been declared
					if (!isDeclaredInOwnScope(varName, currentFn) && isNotKeyword(varName, hasError)) {
						if (isGlobalMarked(varName))
						{
							hasError = true;
							cerr << "Error: Global variable '" << varName.substr(2) << "' has not been declared" << endl;
							return;
						}
						string strLiteral;

						getline(iss, strLiteral);
						size_t start = strLiteral.find_first_not_of(' ');
						if (start != string::npos)
						{
							strLiteral = strLiteral.substr(start);
						}

						// I declare the variable as a string and allocate it at runtime 
						// if it is not declared and it contains a string literal
						if (strLiteral.size() > 1 && strLiteral.front() == '"' && strLiteral.back() == '"')
						{
							strLiteral = strLiteral.substr(1, strLiteral.size() - 2);
							int strSize = static_cast<int>(strLiteral.size());
							int wordCount = ((strSize + 1) / 2) + 1; // Calculate the number of words needed to store the string
							declareArray(varName, "string", nextData, wordCount, hasError,currentFn);
						}
						//This is to allow automatic creation of variables that has not been declared or initialized 
						//from functions that return string 
						//e.g let nitDeclared = call returnString()
						else if (strLiteral.substr(0, 5) == "call ")
						{
							//To move past the call and space characters
							// call returnString -> returnString()
							string callExpr = strLiteral.substr(5);
							//To trim whitespaces
							size_t cstart = callExpr.find_first_not_of(" \t");
							if (cstart != string::npos)
								callExpr = callExpr.substr(cstart);

							//To get the position of the first bracket
							size_t paren = callExpr.find('(');
							if (paren == string::npos)
							{
								hasError = true;
								cerr << "Error: Invalid function call syntax in let statement" << endl;
								return;
							}
							//To get the name by subtracting the postion of ( from the entire line
							string fnName = callExpr.substr(0, paren);
							size_t fend = fnName.find_last_not_of(" \t");
							if (fend != string::npos) 
								fnName = fnName.substr(0, fend + 1);

							string fnType = getReturnType(fnName);
							varName = baseSymbol(varName);

							//So if the return type is string declare the variable with type string
							if (fnType == "string")
							{
								//The function must be defined , not just declared  to allow auto declaration of variable for strings
								if (!isFunctionDefined(fnName))
								{
									hasError = true;
									cerr << "Error: function '" << fnName
										<< "' must be defined before being called to auto-declare variable '"
										<< varName << "'" << endl;
									return;
								}

								// Same word-count convention as the quoted-literal case and as
								// Parser's 'ret "..."' handling: includes the +1 header word.
								int wordCount = getReturnedLiteralSize(fnName);
								if (wordCount == -1)
								{
									hasError = true;
									cerr << "Error: function '" << fnName
										<< "' does not return a string literal" << endl;
									return;
								}
								if (!isCurrentFunctionParameter(varName, currentFn) && !isDeclaredInOwnScope(varName, currentFn))
								{
									declareArray(varName, "string", nextData, wordCount, hasError, currentFn);
								}
							}
							else
							{
								// Non-string return (var, or still void/unset) — same as the
								// existing default path below: declare as a plain var.
								if (!isCurrentFunctionParameter(varName, currentFn) && !isDeclaredInOwnScope(varName, currentFn))
								{
									assign(varName, "var", nextData, hasError, currentFn);
								}
							}
							iss.clear();
							iss.str(strLiteral);
						}
						else if (isalpha(varName[0]) && isValidIdentifier(varName))
						{
							varName = baseSymbol(varName);
							// check the "else if (isalpha(token[0]) && isValidIdentifier(token))" for more explanation
							if (!isCurrentFunctionParameter(varName, currentFn) && !isDeclaredInOwnScope(varName, currentFn))
							{
								assign(varName, "var", nextData, hasError,currentFn);
							}
							//This is to clear the iss and reset it back when the argument to be added is not argument
							iss.clear();
							iss.str(strLiteral);
						}
						else
						{
							hasError = true;
							cerr << "Error: Invalid variable name '" << varName << "'" << endl;
							return;
						}
					}
					
				}

			}
			else if (token == "for") {
				string argument;
				iss >> argument;

				argument = baseSymbol(argument);

				if (!isDeclaredInOwnScope(argument, currentFn) && isNotKeyword(argument, hasError))
				{
					assign(argument, "var", nextData, hasError, currentFn);
				}
			}
			else if (isKeyWord(token))
			{

			}
			else if (isdigit(token[0]) && isNumberOnly(token))
			{
				if (lookupConstant(token) == -1)
				{
					assign(token, "const", nextData, hasError);
				}

			}
			else if (isalpha(token[0]) && isValidIdentifier(token))
			{
				token = baseSymbol(token);
				if (!isCurrentFunctionParameter(token, currentFn) && lookupScoped(token, currentFn) == -1)
				{
					//This is a very important part
					//i want there to be distinction between a global variable and a local variable with the same name
					//to declare a local variable with the same name and you want it to always overshadow the global variable
					//e.g let a(local variable) = 5 - 9
					//when the interpreting gets to else if token == let , it checks the scope and sees that no variable with the same scope
					//matches it it returns -1
					//so to declare/define a local variable that overshadow the global variable you use let e.g let a = 0
					//To explicitly use a global variable you use global keyword

					//This means it is a global variable
					if (currentFn.empty() && isNotKeyword(token, hasError))
					{
						assign(token, "var", nextData, hasError, currentFn);
					}
					else
					{
						hasError = true;
						cerr << "Error: Variable '" << token << "' has not been declared" << endl;
					}
				}
			}
			//For you to explictly use a global variable the variable must have been declared and initialized in the global scope
			//if it is not declared in the global scope it will throw an error
			else if (isGlobalMarked(token) && isNotKeyword(token, hasError))
			{
				string realName = baseSymbol(token);
				if (lookup(realName) == -1)
				{
					hasError = true;
					cerr << "Error: Global variable '" << realName << "' has not been declared" << endl;
				}
			}
			else
			{
				//To check if it is inrecement or decrement
				size_t pos;
				bool decrement = false;
				bool increment = ((pos = token.find("++")) != string::npos);

				if (!increment)
					decrement = ((pos = token.find("--")) != string::npos);
				//This fixes the issue of pos of being overwritten
				//I used the pos name for both increment and decrement. If it is increment it will not get overwritten by decrement
				// because it will not be executed if increment is true
				if (increment || decrement)
				{
					
					string tok;
					tok = token.substr(0, pos);
					if (!isCurrentFunctionParameter(tok, currentFn) && lookupScoped(tok, currentFn) == -1)
					{
						if (currentFn.empty())
						{
							hasError = true;
							cerr << "Error: variable " << tok << " has not been initialized\n"; 
						}
						//this is error message for local variable 
						else
						{
							hasError = true;
							cerr << "Error: variable " << tok << " has not been declared\n";
						}
					}
					
					//to check if 1 exist
					if (lookupConstant("1") == -1)
					{
						assign("1", "const", nextData, hasError);
					}
				}
			}
			position++;
		}
	}
	//I made the scope to default to "" because i want only variables as local variables
	//constants,line number are going to be global variable so i only pass functionName through variable
	void assign(string token, const string& type, int& nextData, bool& hasError, const string& scope = "")
	{
		if (type == "var")
		{
			if (!token.empty() && isdigit(token[0]))
			{
				hasError = true;
				cerr << "Error: Invalid variable name '" << token << "'" << endl;
				return;
			}
		}

		symbolTable[move].symbol = token;
		symbolTable[move].type = type;
		symbolTable[move].scope = scope;
		if (type == "line")
		{
			symbolTable[move].location = nextLineNumber;
			nextLineNumber++;
		}
		//Local variable
		else if (scope != "" && type != "const")
		{
			symbolTable[move].location = nextLocalOffset[scope]++;
		}
		//Global variable,const,array,string
		else
		{
			//To ensure the global variable doesnt run out of its memory layout
			Mem::ensureGlobalDataSpace(nextData, hasError);
			symbolTable[move].location = nextData;
			//To map the location to the number
			if (type == "const") {
				constIndex.insert({ token,nextData });
			}
			nextData--;
		}
		move++;
	}
	string type(string& argument, const string& currentFn)
	{
		bool globalVariable = isGlobalMarked(argument);
		string name = baseSymbol(argument);

		int paramIndex = getParameterIndex(currentFn, argument);
		//first of all check if it it is a parameter of a function
		if (!globalVariable && paramIndex != -1)
		{
			int fnIdx = returnFnNumber(currentFn);
			return functionTable[fnIdx].parameterTypes[paramIndex];
		}
		//check the local variable
		if (!globalVariable && !currentFn.empty())
		{
			for (int i = 0;i < move;i++)
			{
				if (symbolTable[i].symbol == name && symbolTable[i].scope == currentFn)
				{
					return symbolTable[i].type;
				}
			}
		}
		for (int i = 0;i < move;i++)
		{
			if (symbolTable[i].symbol == name && symbolTable[i].scope == "")
			{
				return symbolTable[i].type;
			}
		}
		return "";
	}
	int lookupScoped(const string& name, const string& currentFn)
	{
		bool globalVariable = isGlobalMarked(name);
		string base = baseSymbol(name);
		if (!globalVariable && !currentFn.empty())
		{
			for (int i = 0; i < move; i++)
				if (symbolTable[i].symbol == base && symbolTable[i].scope == currentFn)
					return symbolTable[i].location;
		}
		for (int i = 0; i < move; i++)
			if (symbolTable[i].symbol == base && symbolTable[i].scope == "")
				return symbolTable[i].location;
		return -1;
	}
	int getSize(const string& argument, const string& scope = "")
	{
		string name = baseSymbol(argument);
		for (int i = 0;i < move;i++)
		{
			if (symbolTable[i].symbol == name && symbolTable[i].scope == scope)
			{
				return symbolTable[i].size;
			}
		}
		return -1;
	}
	int lookupConstant(const string& value)
	{
		auto it = constIndex.find(value);
		return (it != constIndex.end()) ? it->second : -1;
	}
	int lookup(string argument)
	{
		string name = baseSymbol(argument);

		for (int i = 0;i < move;i++)
		{
			if (symbolTable[i].symbol == name && symbolTable[i].scope == "")
			{
				return symbolTable[i].location;
			}
		}
		return -1;
	}
	int lookupArraySize(string argument, int index, const string& scope = "")
	{
		string name = baseSymbol(argument);
		if (!scope.empty())
		{
			for (int i = 0;i < move;i++)
			{
				if (symbolTable[i].symbol == name && symbolTable[i].type == "array" && symbolTable[i].scope == scope)
				{
					if (index < 0 || index >= symbolTable[i].size)
					{
						throw std::out_of_range("Array index out of bounds");
					}
					return symbolTable[i].size;
				}
			}
		}
		for (int i = 0;i < move;i++)
		{
			if (symbolTable[i].symbol == name && symbolTable[i].type == "array" && symbolTable[i].scope == "")
			{
				if (index < 0 || index >= symbolTable[i].size)
				{
					throw std::out_of_range("Array index out of bounds");
				}
				return symbolTable[i].location + index;
			}
		}
		return -1;
	}
	//I want to avoid unitialized varirable to be printed when i pass let a = 5 and then print a . Without this it would have 
	//sent an error a has not been initialized but whith this flag that will initialize a variable at compile time
	// so 1 let a = 5 2 print a will pass BUT 1 print b(b has not been initialized) an error will be raised
	void initializeVariable(string argument, int loc, const string& currentFn)
	{
		bool globalVariable = isGlobalMarked(argument);
		string name = baseSymbol(argument);

		// Pass 1: local scope takes priority if we're inside a function
		if (!globalVariable && !currentFn.empty())
		{
			for (int i = 0; i < move; i++)
			{
				if (symbolTable[i].symbol == name && symbolTable[i].scope == currentFn)
				{
					if (symbolTable[i].type != "array")
						symbolTable[i].hasBeenInitialized = true;
					return;
				}
			}
		}

		// Pass 2: fall back to global
		for (int i = 0; i < move; i++)
		{
			if (symbolTable[i].symbol == name && symbolTable[i].scope == "")
			{
				if (symbolTable[i].type == "array") return;
				if (symbolTable[i].location == loc)
					symbolTable[i].hasBeenInitialized = true;
				return;
			}
		}
	}

	//This returns the initialization status of a variable so variable that has not been initialized will raise and error
	bool checkInitializationStatus(string argument, int loc, const string& currentFn)
	{
		bool globalVariable = isGlobalMarked(argument);
		//some stement require making sure the variables are initialized 
		// but because fn parameters belong to a special memory and not symbol table it will always return false
		//but this check helps to ensure that parameters will always return true
		if (!globalVariable || isCurrentFunctionParameter(argument, currentFn))
		{
			return true;   // parameters are guaranteed initialized by the call-site check
		}

		string name = baseSymbol(argument);

		//Search local variable first
		if (!globalVariable && !currentFn.empty())
		{
			for (int i = 0; i < move; i++)
			{
				if (symbolTable[i].symbol == name && symbolTable[i].scope == currentFn)
				{
					if (symbolTable[i].type == "array") 
						return true;
					return symbolTable[i].hasBeenInitialized;
				}
			}
		}

		for (int i = 0; i < move; i++)
		{
			if (symbolTable[i].symbol == name && symbolTable[i].scope == "")
			{
				if (symbolTable[i].type == "array") 
					return true;
				if (symbolTable[i].location == loc)
					return symbolTable[i].hasBeenInitialized;
			}
		}

		return false;
	}
	void intializeConstants(double mem[])
	{
		for (int i = 0;i < move;i++)
		{
			if (symbolTable[i].type == "const")
			{
				double value = std::stod(symbolTable[i].symbol);
				int loc = symbolTable[i].location;
				if (loc >= 0 && loc < 300)
				{
					mem[loc] = value;
					symbolTable[i].hasBeenInitialized = true;
				}
			}
		}
	}
	void declareArray(string token, const string& type, int& nextData, int& size,bool& hasError, const string& scope = "")
	{
		if (isDeclaredInOwnScope(token, scope))   // check within the right scope, not global lookup()
		{
			hasError = true;
			cout << "Error: Array '" << token << "' already declared." << endl;
			return;
		}
		symbolTable[move].symbol = token;
		symbolTable[move].type = type;
		symbolTable[move].scope = scope;
		symbolTable[move].size = size;
		symbolTable[move].hasBeenInitialized = false;
		symbolTable[move].elementInitialized.assign(size, true);

		//If it is a local variable the location is known at runtime beacuse it is being allocated by the parameter and 
		// it will be the nextLocalOffset of the function and it will be incremented by the size of the array
		if (scope != "")
		{
			symbolTable[move].location = nextLocalOffset[scope]; 
			nextLocalOffset[scope] += size;                        
		}
		else
		{
			//make sure the data space doesnt go beyond its boundaries
			Mem::ensureGlobalDataSpace(nextData, hasError);
			int startLoc = nextData - size + 1;
			symbolTable[move].location = startLoc;
			nextData = startLoc - 1;
		}
		move++;
	}
	//This declares a function
	void declareFunction(string& name,vector<string>& parameters,vector<string>& parameterTypes, bool& hasError)
	{
		//To check if the function exists
		if (!isNotKeyword(name, hasError) || isFunctionDeclared(name))
		{
			return;
		}

		if (fnMove >= 300)
		{
			hasError = true;
			cerr << "Error: Too many functions declared" << endl;
			return;
		}

		//Next thing is to check if the parameter names are valid identifiers
		validateFunctionParameterName(name, parameters, hasError);

		functionTable[fnMove].name = name;
		functionTable[fnMove].isDeclared = true;

		//Copy the parameter count into the functionTable
		if (!parameters.empty()) {
			functionTable[fnMove].parameterNames.resize(parameters.size());
			functionTable[fnMove].parameterTypes = parameterTypes;
		}
		else
		{
			functionTable[fnMove].parameterNames.clear();   // ensure empty
		}
		//mapping a function name to the move for fnIndex
		fnIndex.insert({ name,fnMove });
		fnMove++;
		
	}
	void validateFunctionParameterName(string& name, vector<string>& parameters, bool& hasError)
	{
		for (auto& param : parameters)
		{
			if (!isValidIdentifier(param) || !isNotKeyword(param,hasError))
			{
				hasError = true;
				cerr << "Error: Invalid parameter name '" << param << "' in function '" << name << "'" << endl;
				return;
			}
		}
	}
	void defineFunction(string& name,vector<string>& parameters,vector<string>& parameterTypes, int& fInstruction,bool& hasError)
	{
		int fnLocation = -1;
		if (isFunctionDefined(name))
		{
			hasError = true;
			cerr << "Error: Multiple definition of function " << name << endl;
		}
		//To check if the function was already declared
		if (isNotKeyword(name, hasError) && isFunctionDeclared(name))
		{
			fnLocation = returnFnNumber(name);

			//This checks if the number of parameter count of declaration matches the definition
			if (functionTable[fnLocation].parameterNames.size() != parameters.size())
			{
				hasError = true;
				cerr << "Error: parameter count mismatch in redeclaration of function '" << name << "'" << endl;
				return;
			}
		}
		//Next thing is to check if the parameter names are valid identifiers
		validateFunctionParameterName(name, parameters, hasError);

		//To check if the function has not been declared
		if (fnLocation == -1)
		{
			fnLocation = fnMove++;
		}

		functionTable[fnLocation].name = name;
		functionTable[fnLocation].isDefined = true;
		functionTable[fnLocation].firstInstruction = fInstruction;

		if (!parameters.empty()) {
			functionTable[fnLocation].parameterNames = parameters;
			functionTable[fnLocation].parameterTypes = parameterTypes;
		}
		else
		{
			functionTable[fnLocation].parameterNames.clear();
		}
		nextLocalOffset[name] = parameters.size();  // locals start right after the last parameter's offset

		//mapping a function name to the move for fnIndex
		fnIndex.insert({ name,fnLocation });
	}
	//To confirm if the parameters passed has the same parameter count as the function declaration or definition
	bool validatefnCount(string& name, vector<string>& parameter) {
		int fnLocation = returnFnNumber(name);

		// This checks if the parameter count in call is equal to the parameter count in function definition/declaration
			if (functionTable[fnLocation].parameterNames.size() != parameter.size())
			{
				return false;
			}

		//Else return true
			return true;
	}
	//To confirm the parameters passed are the same type
	bool validateFnTypes(string& name, vector<string>& arguments, bool& hasError, const string& currentFn) {
		int fnLocation = returnFnNumber(name);
		auto& expectedTypes = functionTable[fnLocation].parameterTypes;

		for (int i = 0; i < arguments.size();i++)
		{
			//I want to check if the argument is an indexed array or a non-indexed array. 
			// If it is an indexed array, it should be treated as a scalar value and it shouldnt decay to a pointer and an array
			// if it is a non-indexed array, it should be treated  as an array type and decay to a pointer. 
			// This is because in the function definition, the parameter type can be either an array or a scalar value, and I want to ensure that the argument passed matches the expected type.
			bool isLiteral = arguments[i].size() > 1 && arguments[i].front() == '"' && arguments[i].back() == '"';
			bool isIndexed = arguments[i].find('[') != string::npos;
			bool argIsArray = (type(arguments[i],currentFn) == "array") && !isIndexed;
			bool argIsString = isLiteral || ((type(arguments[i], currentFn) == "string") && !isIndexed);

			//If the expexted type is an array, then the argument must be an array and not an indexed array. 
			if (expectedTypes[i] == "array") {
				if (!argIsArray) {
					hasError = true;
					cerr << "Error: argument '" << arguments[i] << "' is not an array, expected an array type" << endl;
					return false;
				}
			}
			else if (expectedTypes[i] == "string") {
				if (!argIsString) {
					hasError = true;
					cerr << "Error: argument '" << arguments[i] << "' is not a string, expected a string" << endl;
					return false;
				}
			}

			//If the expected type is a scalar value i.e a non-array or an indexed array, then the argument must not be an array.
			// a[0] as an argument is a scalar value and it should not be treated as an array.
			//while a as an argument is an array and it should be treated as an array.
			else
			{
				if (argIsArray || argIsString) {
					hasError = true;
					cerr << "Error: argument '" << arguments[i] << (argIsArray ? "' is an array, ": "' is a string, ")
						<< "expected a scalar value" << endl;
					return false;
				}
			}
		}

		return true;
	}
	bool isFunctionDeclared(string& name)
	{
		int index = returnFnNumber(name);
		if (index == -1)
		{
			return false;
		}
		return functionTable[index].isDeclared;
	}
	bool isFunctionDefined(string& name)
	{
		int index = returnFnNumber(name);
		if (index == -1)
		{
			return false;
		}
		return functionTable[index].isDefined;
	}
	int returnFnNumber(string name)const
	{
		auto it = fnIndex.find(name);
		return (it != fnIndex.end() ? it->second : -1);
	}
	void FnaddLastInstruction(string& name,int lInstruction)
	{
		int fnLocation = returnFnNumber(name);

		functionTable[fnLocation].lastInstruction = lInstruction;
	}
	int returnFnFirstInstruction(string& name)
	{
		int index = returnFnNumber(name);
		if (index == -1)
		{
			return -1;
		}
		return functionTable[index].firstInstruction;
	}
	int returnFnParameterCount(string& name)const
	{
		int index = returnFnNumber(name);
		if (index == -1)
		{
			return -1;
		}
		return functionTable[index].parameterNames.size();
	}

	void setReturnType(const string& fnName, const string& type, bool& hasError) {
		int idx = returnFnNumber(fnName);
		if (idx == -1)
		{
			hasError = true;
			cerr << "Error: 'ret' used in unknown function '" << fnName << "'" << endl;
			return;
		}
		//The functiontable return type is default void so as long as it has not been overwritten it remains void
		if (functionTable[idx].returnType == "void")
		{
			functionTable[idx].returnType = type;
		}
		else if (functionTable[idx].returnType != type)
		{
			hasError = true;
			cerr << "Error: function '" << fnName << "' has inconsistent return types ('"
				<< functionTable[idx].returnType << "' vs '" << type << "')" << endl;
		}
	}

	//return the function return type
	string getReturnType(string& fnName)
	{
		int idx = returnFnNumber(fnName);
		if (idx == -1) 
			return "";
		return functionTable[idx].returnType;
	}

	//returns the index of the parameter name
	int getParameterIndex(const string& fnName, const string& paramName)
	{
		if (isGlobalMarked(paramName)) 
			return -1;

		int fnIdx = returnFnNumber(fnName);
		if (fnIdx == -1) return -1;
		auto& params = functionTable[fnIdx].parameterNames;
		for (int i = 0; i < params.size(); i++) {
			if (params[i] == paramName) {
				return i;
			}
			if (params[i] == baseSymbol(paramName) && functionTable[fnIdx].parameterTypes[i] == "array") {
				return i;
			}
		}
			
				
		return -1;
	}
	//checks if that variable is a parameter of the function
	bool isParameterOf(const string& fnName, const string& token) const
	{
		if (fnName.empty()) return false;
		int idx = returnFnNumber(fnName);
		if (idx == -1) return false;
		auto& params = functionTable[idx].parameterNames;
		for (int i = 0; i < params.size(); i++) {
			if (params[i] == token) {
				return true;
			}
			if (params[i] == baseSymbol(token) && functionTable[idx].parameterTypes[i] == "array") {
				return true;
			}
		}
			

		return false;
	}
	
	int lookupLocal(const string& currentFn, const string& name)
	{
		if (currentFn.empty()) 
			return false;
		//:: is meant for global variable so if it is marked as global it should not be treated as a local variable
		if (isGlobalMarked(name)) 
			return -1;
		string base = baseSymbol(name);
		for (int i = 0; i < move; i++)
		{
			if (symbolTable[i].symbol == name &&
				symbolTable[i].scope == currentFn)
			{
				return symbolTable[i].location;   // the offset
			}
			if(symbolTable[i].symbol == base && symbolTable[i].scope == currentFn && symbolTable[i].type == "array")
			{
				return symbolTable[i].location;
			}
		}
		return -1;
	}

	bool isLocalOf(const string& currentFn, const string& token)
	{
		if (currentFn.empty()) 
			return false;
		return lookupLocal(currentFn, token) != -1;
	}
	//This is to get the total number of local variable a function has , It will be needed at fn and }.
	//check fn and }. for more details
	int getLocalCount(string& currentFn)
	{
		int offset = nextLocalOffset[currentFn];
		return offset - returnFnParameterCount(currentFn);
	}

	//Records the string literal base address and size that a function returns
	//If a function is being called in an expression and a string literal is to be assigned to it ,we need to know where the base address of that string
	//as well as the size to compare 
	void setReturnedLiteral(const string& fnName, int addr, int size, bool& hasError)
	{
		int idx = returnFnNumber(fnName);
		if (idx == -1)
		{
			hasError = true;
			cerr << "Error: 'ret' used in unknown function '" << fnName << "'" << endl;
			return;
		}
		if (functionTable[idx].returnedLiteralAddr == -1)
		{
			functionTable[idx].returnedLiteralAddr = addr;
			functionTable[idx].returnedLiteralSize = size;
		}
		else if (functionTable[idx].returnedLiteralAddr != addr)
		{
			hasError = true;
			cerr << "Error: function '" << fnName << "' returns different string literals across multiple 'ret' statements" << endl;
		}
	}

	int getReturnedLiteralAddr(const string& fnName)
	{
		int idx = returnFnNumber(fnName);
		return (idx == -1) ? -1 : functionTable[idx].returnedLiteralAddr;
	}

	int getReturnedLiteralSize(const string& fnName)
	{
		int idx = returnFnNumber(fnName);
		return (idx == -1) ? -1 : functionTable[idx].returnedLiteralSize;
	}

	//The purpose of this is to strip away index mostly from array so it returns the bare argument
	string baseSymbol(const string& argument) const
	{
		string name = argument;
		//Check if the variable is a global variable with :: prefix and remove it to get the base name
		if (isGlobalMarked(name)) 
			name = name.substr(2);

		size_t pos = name.find('[');
		if (pos != string::npos)
		{
			name = name.substr(0, pos);
		}

		return name;
	}

	bool isNotKeyword(string argument, bool& hasError)
	{
		argument = baseSymbol(argument);
		if (isKeyWord(argument))
		{
			hasError = true;
			cerr << "Error: '" << argument << "' is a reserved keyword and cannot be used as an identifier.\n";
			return false;
		}
		return true;
	}
private:
	//An unordered map gives 0(n) worst case so instead of looping through function table always
	std::unordered_map<string, int>fnIndex;
	//an unordered map for constant and their location
	std::unordered_map<string, int> constIndex;
	//To check if the variable is a global one with :: prefix
	bool isGlobalMarked(const string& s) const
	{
		return s.size() >= 2 && s[0] == ':' && s[1] == ':';
	}

	//Check if the variable has been declared in its own scope
	bool isDeclaredInOwnScope(const string& name, const string& currentFn)
	{
		bool globalVariable = isGlobalMarked(name);
		string base = baseSymbol(name);
		//Onlu=y global variables and local variables
		if (globalVariable || currentFn.empty())
			return lookup(base) != -1;                       
		return isCurrentFunctionParameter(base, currentFn) || isLocalOf(currentFn, base);  // in-function: only param/local counts, global is irrelevant here
	}
	std::map<string, int> nextLocalOffset;  // fnName -> next free offset in that function's frame
	//This is to check if the variable is a parameter passed to a function
	// Parameter variable have a special address in the parameter pointer stack
	bool isCurrentFunctionParameter(const string& token, const string& currentFn)
	{
		if (currentFn.empty()) return false;
		return getParameterIndex(currentFn, token) != -1;
	}
	
	bool isKeyWord(const string& token)
	{
		static const std::unordered_set<string> keywords = {
			"rem","input","let","print","goto","if","end","enter","array",
			"gosub","return","for","next","string","while","wend","fn","call","to",
			"ret"
		};

		return keywords.count(token) > 0;
	}
	
	
	TableEntry symbolTable[300];
	FunctionEntry functionTable[300];
	int nextLineNumber = 0;
	int move = 0;
	int fnMove = 0;
};
	
#endif
