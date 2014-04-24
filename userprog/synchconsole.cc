// synchconsole.cc 
//	Routines providing synchronized access to the keyboard 
//	and console display hardware devices.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synchconsole.h"
#include "interrupt.h"
#include "machine.h"
#include "system.h"
void CBSynchReadAvail(int arg) {
	((SynchConsole*)arg)->ReadAvail();
	
}
void CBSynchWriteDone(int arg) {
	((SynchConsole*)arg)->WriteDone();
}



//----------------------------------------------------------------------
// SynchConsoleInput::SynchConsoleInput
//      Initialize synchronized access to the keyboard
//
//      "inputFile" -- if NULL, use stdin as console device
//              otherwise, read from this file
//----------------------------------------------------------------------

SynchConsole::SynchConsole(char *inputFile,char *outputFile)
{
    console = new Console(inputFile, outputFile, CBSynchReadAvail, CBSynchWriteDone, (int)this);
    lock = new Lock("synchConsole");
	SynchReadAvail = new Semaphore("consoleInput", 0);
	SynchWriteDone = new Semaphore("consoleOutput", 0);
}

//----------------------------------------------------------------------
// SynchConsoleInput::~SynchConsoleInput
//      Deallocate data structures for synchronized access to the keyboard
//----------------------------------------------------------------------

SynchConsole::~SynchConsole()
{ 
    delete console; 
    delete lock; 
	
    delete SynchWriteDone;
    delete SynchReadAvail;
}

//----------------------------------------------------------------------
// SynchConsoleInput::GetChar
//      Read a character typed at the keyboard, waiting if necessary.
//----------------------------------------------------------------------

char
SynchConsole::GetChar()
{
    char ch;

    lock->Acquire();
    SynchReadAvail->P();	// wait for EOF or a char to be available.
    ch = console->GetChar();
    lock->Release();
    return ch;
}


//----------------------------------------------------------------------
// SynchConsoleOutput::~SynchConsoleOutput
//      Deallocate data structures for synchronized access to the keyboard
//----------------------------------------------------------------------

void
SynchConsole::PutChar(char ch)
{	
    lock->Acquire();
	console->PutChar(ch);
	SynchWriteDone->P();
    lock->Release();
}

void SynchConsole::ReadAvail(){
	SynchReadAvail->V();
}

void SynchConsole::WriteDone(){
	SynchWriteDone->V(); 
}
