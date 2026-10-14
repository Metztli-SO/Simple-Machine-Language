#include "SML.h"
#include "Compiler.h"

#include <fstream>
#include <iostream>

using std::ios;
using std::fstream;
int main()
{
	Compiler compiler;

	int result = compiler.compile("program.txt");
	
	if(result==0)
	{ 
		std::cout << "\nCompilation successful. Executing the program..." << std::endl;
		Memory mem;
	}
	else
	{
		std::cerr << "Compilation failed. Please fix the errors and try again." << std::endl;
	}
	
	return 0;
}