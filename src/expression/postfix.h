#ifndef POSTFIX_H
#define POSTFIX_H

#include "../memory/AddressResolver.h"
#include "../memory/TempPool.h"
#include "../sml/Opcode.h"

using namespace std;

struct LocEntry {
	int location; //where the value lives or adress lives(i.e direct or indirect)
	bool isIndirect;
};

class postfix
{
public:
	postfix() {}

	LocEntry generateCode(const vector<string>& postfix, double memory[], int& ic, int& dc, SymbolTable& st, bool& hasError,int currentLineNum,const string& currentFn)
	{
		stack <LocEntry> locStack;
		int i = 0;
		for (const string& token : postfix)
		{
			bool isLast = (i == (int)postfix.size() - 1);
			if (token == "+" || token == "-" || token == "*" || token == "/" || token == "^" || token == "%")
			{
				if(locStack.size() < 2) {
					hasError = true;
					cerr << "Error on line " << currentLineNum << ": ";
					cerr << "Error: Invalid expression, missing operand near '" << token << "'" << endl;
					return {};
				}
				LocEntry rightLoc = locStack.top(); locStack.pop();
				LocEntry leftLoc = locStack.top();locStack.pop();

				int leftAddr = resolveIfIndirect(leftLoc, memory, ic, dc, hasError);
				int rightAddr = resolveIfIndirect(rightLoc, memory, ic, dc, hasError);

				if (leftAddr == -1 || rightAddr == -1) {
					hasError = true;
					return {};
				}
				memory[ic++] = Op::LOAD + leftAddr;

				if (token == "+")
				{
					memory[ic++] = Op::ADD+rightAddr;
				}
				else if (token == "-")
				{
					memory[ic++] = Op::SUB + rightAddr;
				}
				else if (token == "/")
				{
					memory[ic++] = Op::DIV + rightAddr;
				}
				else if (token == "*")
				{
					memory[ic++] = Op::MUL + rightAddr;
				}
				else if (token == "^")
				{
					memory[ic++] = Op::POW + rightAddr;
				}
				else if (token == "%")
				{
					memory[ic++] = Op::MOD + rightAddr;
				}

				if (isLast)
				{
					locStack.push({ -999,false });
				}
				else
				{
					int tempLoc = dc--;
					memory[ic++] = Op::STORE + tempLoc;
					locStack.push({ tempLoc,false });
				}
			}
			else
			{

				if (isdigit(token[0]) || (token[0] == '-' && token.size() > 1 && isdigit(token[1])))
				{
					int loc = getOrCreateConstant(token, st, memory, dc, hasError);

					locStack.push({ loc, false });
				}
				else
				{
					auto[addr, isIndirect] = resolveToken(token, st, memory, ic, dc, hasError, currentFn);

					if (addr == -1)
					{
						hasError = true;
						cerr << "Error on line " << currentLineNum << ": ";
						cerr << "Error: Undefined variable '" << token << "' " << endl;
						return {};
					}

					//This helps to prevent unitiliazed variables from going through
					bool check = !st.checkInitializationStatus(token, addr, currentFn);
					if (check)
					{
						hasError = true;
						cerr << "Error on line " << currentLineNum << ": ";
						cerr << "Error: Variable '" << token << "' has not been Initialized" << endl;
						return {};
					}
					locStack.push({addr, isIndirect});
				}
			}
			i++;
		}
		if (!locStack.empty())
		{
			LocEntry result = locStack.top();
			locStack.pop();
			int location = resolveIfIndirect(result, memory, ic, dc, hasError);
			result.location = location;
			result.isIndirect = false;

			locStack.push(result);
		}
		
		//This is to check if there are leftover operands in the stack which means the expression is invalid
		//e.g let a = 5 + 3 2 which is invalid because there is a leftover operand 2 in the stack and it is at the top of the stack
		//which means 2 will be assigned to a instead of the result of 5 + 3 which is 8
		if (locStack.size() > 1)
		{
			hasError = true;
			cerr << "Error on line " << currentLineNum << ": ";
			cerr << "Error: Invalid expression - leftover operands" << endl;
			return {};
		}
		return locStack.empty() ? LocEntry{-999, false} : locStack.top();
	}
private:
	int  resolveIfIndirect(LocEntry posLoc, double memory[], int& ic, int& dc, bool& hasError)
	{
		if (!posLoc.isIndirect)
		{
			return posLoc.location;
		}
		else
		{
			int temp = tempPool.acquire(hasError);
			if (temp == -1)
			{
				return -1;
			}
			memory[ic++] = Op::INDIRECT_LOAD + posLoc.location;
			memory[ic++] = Op::STORE + temp;
			return temp;
		}

		return -1;
	}
};
#endif