#pragma once
#ifndef FN_NAME_VALIDATOR_H
#define FN_NAME_VALIDATOR_H

#include "TokenUtils.h"

#include <string>
#include <vector>
#include <iostream>
#include <sstream>

using std::cerr;
using std::endl;
using std::string;
using std::vector;

struct fnValidation {
	string fnName = " ";
	bool isValid = false;
	bool isDefinition = false;
	vector<string> parameters;
	vector<string> parameterTypes;
};

// Pulled verbatim out of the original Parser.h — parses "name(param1, param2) {"
// or "name(param1, param2)" into fnName/parameters/parameterTypes, and tells
// the caller whether the trailing "{" makes this a definition vs. a declaration.
inline fnValidation validateFnName(string& arg1, bool& hasError, int& currentLineNum)
{
	fnValidation fnresult;

	//To check for empty declaration and raise an error
	if (arg1.empty())
	{
		hasError = true;
		cerr << "Error on line " << currentLineNum << ": ";
		cerr << "\nError: Invalid function declaration" << endl;
		return fnresult;
	}

	//Find brackets
	size_t firstBracket = string::npos;
	size_t secondBracket = string::npos;
	{
		//( or ) might be in a quote so this makes sure it doesnt pick the wrong bracket
		bool inQuotes = false;
		for (size_t i = 0; i < arg1.size();i++)
		{
			char c = arg1[i];
			//if it is the first " inquotes becomes true else it becomes false
			if (c == '"') {
				inQuotes = !inQuotes;
			}
			else if (c == '(' && !inQuotes && firstBracket == string::npos)
			{
				firstBracket = i;
			}
			else if (c == ')' && !inQuotes && secondBracket == string::npos) {
				secondBracket = i;
			}
		}
	}

	//This ensures the one of the bracket are not missing
	if ((firstBracket == string::npos) || (secondBracket == string::npos))
	{
		hasError = true;
		cerr << "Error on line " << currentLineNum << ": ";
		cerr << "\nError: Invalid function declaration" << endl;
		fnresult.isValid = false;
		return fnresult;
	}

	//This ensures ( is not the first character
	if (firstBracket == 0) {
		hasError = true;
		cerr << "Error on line " << currentLineNum
			<< ":\nError: Invalid function declaration" << endl;
		fnresult.isValid = false;
		return fnresult;
	}

	//This is to extract the fn name from the arg1
	string fnName = arg1.substr(0, firstBracket);

	//this trim any whitespace (in case of " add() ")
	size_t start = fnName.find_first_not_of(" \t");
	size_t end = fnName.find_last_not_of(" \t");
	if (start != string::npos) {
		fnName = fnName.substr(start, end - start + 1);
	}

	//this validate the function name
	if (!(isalpha(fnName[0]) && isValidIdentifier(fnName)))
	{
		hasError = true;
		cerr << "Error: Invalid variable name '" << fnName << "'" << endl;
		fnresult.isValid = false;
		return fnresult;
	}
	if (fnName.empty()) {
		hasError = true;
		cerr << "Error on line " << currentLineNum
			<< ":\nError: Empty function name" << endl;
		fnresult.isValid = false;
		return fnresult;
	}

	//If every other check doesnt return then it means the function name is valid
	fnresult.isValid = true;
	fnresult.fnName = fnName;

	//split Parameters by comma(,)
	string paramStr = arg1.substr(firstBracket + 1, secondBracket - firstBracket - 1);


	if (!paramStr.empty()) {
		//This also helps to make sure , inside quotes(" ") doesnt get picked
		vector<string> rawParam;
		{
			string current;
			bool inQuotes = false;
			for (char c : paramStr)
			{
				if (c == '"') inQuotes = !inQuotes;
				if (c == ',' && !inQuotes && !current.empty())
				{
					rawParam.push_back(current);
					current.clear();
				}
				else
				{
					current += c;
				}
			}
			rawParam.push_back(current);
		}
		for (string& param : rawParam) {
			// trim surrounding whitespace on the whole param first
			size_t ps = param.find_first_not_of(" \t");
			size_t pe = param.find_last_not_of(" \t");
			if (ps != string::npos)
			{
				param = param.substr(ps, pe - ps + 1);
			}

			// A quoted string literal is a single, complete argument so to avoid splitting it and we want to assign the type to be string
			// e.g "hi, there" will be split into "hi, and there" without this check
			if (param.size() > 1 && param.front() == '"' && param.back() == '"')
			{
				fnresult.parameterTypes.push_back("string");
				fnresult.parameters.push_back(param);
				continue;
			}

			std::istringstream paramIss(param);
			//Fn requires type declaration for arrays
			string first, second;
			paramIss >> first;


			//Trim whitespace in case of "   array   a"
			size_t pstart = first.find_first_not_of(" \t");
			size_t pend = first.find_last_not_of(" \t");
			if (pstart != string::npos) {
				first = first.substr(pstart, pend - pstart + 1);
			}
			//This will only pass if there is a type declaration e.g array a -> first = array second = a
			if (paramIss >> second) {
				//push both the type and name
				fnresult.parameterTypes.push_back(first);
				fnresult.parameters.push_back(second);
			}
			//This only pass if there is no type declaration and it diefaults to "var"
			else
			{
				fnresult.parameterTypes.push_back("var");
				fnresult.parameters.push_back(first);
			}
		}
	}
	//check if it is definition or declaration
	string afterParen = arg1.substr(secondBracket + 1);
	// Trim leading whitespace
	start = afterParen.find_first_not_of(" \t");
	if (start != string::npos) {
		afterParen = afterParen.substr(start);
	}
	//This is to ensure that there is no trailing whitespace and extraneous characters 
	// after the openinng braces if it is a function definition
	size_t trailEnd = afterParen.find_last_not_of(" \t");
	if (trailEnd != string::npos) {
		afterParen = afterParen.substr(0, trailEnd + 1);
	}

	//To check if it does not has {
	if (afterParen.empty())
	{
		fnresult.isDefinition = false;
	}
	//To check if it has {
	else if (afterParen == "{") {
		fnresult.isDefinition = true;
	}
	else {
		hasError = true;
		cerr << "Error on line " << currentLineNum << ": ";
		cerr << "\nError: Invalid function prototype." << endl;
		return fnresult;
	}
	return fnresult;
}

#endif // !FN_NAME_VALIDATOR_H
