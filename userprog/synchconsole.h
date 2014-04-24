
#ifndef SYNCHCONSOLE_H
#define SYNCHCONSOLE_H

#include "copyright.h"
#include "utility.h"
#include "console.h"
#include "synch.h"
extern void CBSynchReadAvail(int arg);
extern void CBSynchWriteDone(int arg);
class SynchConsole{
  public:
    SynchConsole(char *inputFile = NULL,char *outputFile = NULL); // Initialize the console device
    ~SynchConsole();		// Deallocate console device

    char GetChar();		// Read a character, waiting if necessary
    void PutChar(char ch);	// Write a character, waiting if necessary
	void ReadAvail();
	void WriteDone();
  private:
    Console *console;	// the hardware keyboard
    Lock *lock;			// only one reader at a time
	Semaphore *SynchReadAvail;
	Semaphore *SynchWriteDone;
};



#endif // SYNCHCONSOLE_H
