#pragma once
#ifndef TEMPPOOL_h
#define TEMPPOOL_H

#include <iostream>

using std::endl;
using std::cerr;

class TempPool {
	public:
		void initialize(int rangeStart, int rangeEnd) {
			//the low is the rangestart which is 200
			low = rangeStart;
			//the high is the range end which is 219
			high = rangeEnd;
			//the temp memory is being allocated downward from range end, same convention as nextData--
			top = rangeEnd;
		}
		// this returns a temporary adrress/location for variables,instructions or anything 
		int acquire(bool& hasError) {
			if (top < low) {
				hasError = true;
				cerr<< "Error: Out of temp storage (limit " << low << ")" << endl;
				return -1;
			}
			return top--;
		}
		//This release the memory that was allocated when it is no longer needed
		// This prevents memory wastage by releasing unused memory
		void release() {
			top = high;
		}

		//A overloaded release which tells the compiler to release a certain amount of memory space
		void release(int number) {
			top -= number;
		}
	private:
		int top = 0;
		int low = 0;
		int high = 0; 
};
extern TempPool tempPool;
#endif