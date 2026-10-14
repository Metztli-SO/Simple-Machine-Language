#ifndef STACKS_H
#define STACKS_H
#include <iostream>
#include <stack>
#include <string>
#include <vector>
#include <sstream>
using namespace std;

class InfixToPostfix
{
public:
    InfixToPostfix() {}

    vector<string> Infix(string expression,bool& hasError)
    {
        vector<string> tokens = tokenize(expression,hasError);
        if (hasError) {
            return {};
		}

        vector<string> postfix;
        stack<string> S;

        for(size_t index = 0;index <tokens.size();index++)  
        {
            const string& token = tokens[index];
            if (token == "(")
            {
                S.push(token);
            }
            else if (token == ")")
            {
                while (!S.empty() && S.top() != "(")
                {
                    postfix.push_back(S.top());
                    S.pop();
                }
                if (!S.empty()) S.pop(); // remove '('
            }
            else if (IsOperator(token[0]))   // +, -, *, /, ^
            {
                while (!S.empty() && S.top() != "(" && HasHigherPrecedence(S.top(), token))
                {
                    postfix.push_back(S.top());
                    S.pop();
                }
                S.push(token);
            }
            else // operand (variable or number)
            {
                postfix.push_back(token);
            }
        }

        // Pop all remaining operators
        while (!S.empty())
        {
            postfix.push_back(S.top());
            S.pop();
        }
        
        return postfix;
    }

    bool IsOperator(char c)
    {
        return c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '%';
    }

    bool IsRightAssociative(const string& op)
    {
        return op == "^";
    }

    int GetOperatorWeight(const string& op)
    {
        if (op == "+" || op == "-") return 1;
        if (op == "*" || op == "/" || op == "%") return 2;
        if (op == "^") return 3;
        return -1;
    }

    bool HasHigherPrecedence(const string& op1, const string& op2)
    {
        int weight1 = GetOperatorWeight(op1);
        int weight2 = GetOperatorWeight(op2);

        if (weight1 == -1 || weight2 == -1) return false;

        if (weight1 == weight2)
        {
            return !IsRightAssociative(op1);
        }

        return weight1 > weight2;
    }
private:
    vector<string> tokenize(const string& expr,bool& hasError) {
        vector<string> tokens;
        size_t i = 0;
        size_t n = expr.size();
        int parenDepth = 0;
        int bracketDepth = 0;
        while (i < n)
        {
            char c = expr[i];

            if (isspace(c)) 
            { 
                i++;
                continue; 
            }

            if (c == '(')
            {
                parenDepth++;
                tokens.push_back("(");
                i++;
                continue;
            }
            if (c == ')')
            {
                parenDepth--;
                if (parenDepth < 0)
                {
                    hasError = true;
                    cerr << "Error: Unmatched ')' in expression '" << expr << "'" << endl;
                    return {};
                }
                tokens.push_back(")");
                i++;
                continue;
            }
            // Explicit-global '::' prefix — must be immediately followed by an identifier
            if (c == ':')
            {
                if (i + 1 < n && expr[i + 1] == ':')
                {
                    size_t nameStart = i + 2;
                    if (nameStart >= n || !(isalpha(expr[nameStart]) || expr[nameStart] == '_'))
                    {
                        hasError = true;
                        cerr << "Error: '::' must be followed by a variable name in expression '" << expr << "'" << endl;
                        return {};
                    }
                    string ident = "::";
                    i += 2;
                    while (i < n && (isalnum(expr[i]) || expr[i] == '_' || expr[i] == '['))
                    {
                        if (expr[i] == '[') bracketDepth++;
                        if (expr[i] == ']')
                        {
                            bracketDepth--;
                            if (bracketDepth < 0)
                            {
                                hasError = true;
                                cerr << "Error: Unmatched ']' in expression '" << expr << "'" << endl;
                                return {};
                            }
                        }
                        ident += expr[i];
                        i++;
                    }
                    tokens.push_back(ident);
                    continue;
                }
                else
                {
                    hasError = true;
                    cerr << "Error: Stray ':' in expression '" << expr << "' (did you mean '::'?)" << endl;
                    return {};
                }
            }
            if (IsOperator(c))
            {
                // unary vs binary: unary only at expression start,
                // right after '(', or right after another operator.
                bool isUnary = tokens.empty() ||
                    tokens.back() == "(" ||
                    (tokens.back().size() == 1 && IsOperator(tokens.back()[0]));

                if (c == '-' && isUnary && i + 1 < n && (isdigit(expr[i + 1]) || expr[i + 1] == '.'))
                {
                    // Merge unary minus into the following numeric literal
                    string num = "-";
                    i++;
                    while (i < n && (isdigit(expr[i]) || expr[i] == '.'))
                    {
                        num += expr[i];
                        i++;
                    }
                    tokens.push_back(num);
                    continue;
                }
                else if (isUnary)
                {
                    // Unary +, or unary - on a non-numeric operand (e.g. "-x") —
                    // not supported yet; push back the variable 
                    // rather than silently producing wrong code.
                    tokens.push_back(string(1, c));
                    i++;
                    continue;
                }
                else
                {
                    tokens.push_back(string(1, c));
                    i++;
                    continue;
                }
            }

            if (isalpha(c) || c == '_')
            {
                string ident;
                while (i < n && (isalnum(expr[i]) || expr[i] == '_' || expr[i] == '[' || expr[i] == ']'))
                {
					if (expr[i] == '[') bracketDepth++;
                    if (expr[i] == ']')
                    {
                        bracketDepth--;
                        if (bracketDepth < 0)
                        {
                            hasError = true;
                            cerr << "Error: Unmatched ']' in expression '" << expr << "'" << endl;
                            return {};
                        }
                    }
                    ident += expr[i];
                    i++;
                }
                tokens.push_back(ident);
                continue;
            }

            if (isdigit(c) || c == '.')
            {
                string num;
                while (i < n && (isdigit(expr[i]) || expr[i] == '.'))
                {
                    num += expr[i];
                    i++;
                }
                tokens.push_back(num);
                continue;
            }
            //unrecognized character — this is an error
            hasError = true;
            cerr << "Error: Unrecognized character '" << c << "' in expression '" << expr << "'" << endl;
            return {};
        }

        if (parenDepth != 0)
        {
            hasError = true;
            cerr << "Error: Unmatched '(' in expression '" << expr << "'" << endl;
            return {};
        }

        if (bracketDepth != 0)
        {
            hasError = true;
            cerr << "Error: Unmatched '[' in expression '" << expr << "'" << endl;
            return {};
        }

        return tokens;
    }
};
#endif