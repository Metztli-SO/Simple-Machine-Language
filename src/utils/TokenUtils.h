#pragma once
#ifndef TOKEN_UTILS_H
#define TOKEN_UTILS_H

#include <string>

using std::string;

//This function helps to check if it contains only digits 
// 023abc will go throught the first check but will be caught using this new check
inline bool isNumberOnly(const string& str)
{
	bool hasDecimal = false;

	for (size_t i = 0; i < str.size(); ++i)
	{
		char c = str[i];

		if (i == 0 && c == '-') // allow leading minus
			continue;

		if (c == '.')
		{
			if (hasDecimal) return false; // only one decimal allowed
			hasDecimal = true;
			continue;
		}

		if (!isdigit(c))
			return false;
	}

	return !str.empty();
}

inline bool isValidIdentifier(const string& str)
{
	if (str.empty() || !isalpha(str[0]))
		return false;
	for (char c : str)
	{
		if (!isalnum(c) && c != '_')
			return false;
	}
	return true;
}

inline bool isNumberIndexOnly(const string& str)
{
	for (char i : str)
	{
		if (!isdigit(i))
			return false;
	}
	return true;
}


inline bool isArrayWithIndex(const string& argument)
{
	size_t start = argument.find('[');
	size_t end = argument.find(']');
	if (start != string::npos && end != string::npos && start < end)
	{
		return true;
	}
	return false;
}

//To determine if the parameter  is an expression like
//e.g call add(a-1, b)
inline bool isExpressionLike(const string& str)
{
	for (char c : str)
	{
		if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '^')
			return true;
	}
	return false;
}

#endif