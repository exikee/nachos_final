// system.h 
//	All global variables used in Nachos are defined here.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef SYSTEM_H
#define SYSTEM_H

#include "copyright.h"
#include "utility.h"
#include "thread.h"
#include "scheduler.h"
#include "interrupt.h"
#include "stats.h"
#include "timer.h"
#include "schetimer.h"
#include "iostream"
#include "map"
#include "synch.h"
#include "../userprog/SynchConsole.h"
#include "User.h"
using namespace std;
//THREAD
#define MAX_THREAD_NUM 6
#define MAX_THREADID_NUM 6
#define REVERSED_THREADID_NUM 1
#define MAX_MAIL_NUM 32
#define MAX_MAIL_LENGTH 32
// Initialization and cleanup routines
extern void Initialize(int argc, char **argv); 	// Initialization,
						// called before anything else
extern void Cleanup();				// Cleanup, called when
						// Nachos is done.

extern Thread *currentThread;			// the thread holding the CPU
extern Thread *threadToBeDestroyed;  		// the thread that just finished
extern Scheduler *scheduler;			// the ready list
extern Interrupt *interrupt;			// interrupt status
extern Statistics *stats;			// performance metrics
extern Timer *timer;				// the hardware alarm clock


#ifdef USER_PROGRAM
#include "machine.h"
extern Machine* machine;	// user program memory and registers


#endif

#ifdef FILESYS_NEEDED 		// FILESYS or FILESYS_STUB 
#include "filesys.h"
extern FileSystem  *fileSystem;
class FileEntry{
public:
	int hdrSector;
	int openCount;
	Lock* fileLock;
	vector<OpenFile*> openFileList;
};
extern map<int,FileEntry*> FileEntryList;
#endif

#ifdef FILESYS
#include "synchdisk.h"
extern SynchDisk   *synchDisk;
#endif

#ifdef NETWORK
#include "post.h"
extern PostOffice* postOffice;
#endif


#ifdef VMC
//VM
#include <deque>
using std::deque;
extern deque<int> PTList;

class BitMap;
class AddrSpace;
extern BitMap* MemMap;
extern BitMap* VAddrBitMap;
extern TranslationEntry *PmEntry[32];


extern std::map<int, TranslationEntry*> VAddrEntry;		//save thread ptr
#endif

//****EDIT****
//DEBUG
//****EDIT****
//#define THREADDEBUG				//thread msg
//#define THREADDEBUGQUEUE		//show queue msg
//#define SYNCHDEBUG				//semaphore,lock,condition...
//****END****

//sys thread list
extern std::map<int, User*> UserList;		//save user
extern int TotalThreadCount;
extern std::map<int, Thread*> ThreadList;		//save thread ptr
extern int AddThread( Thread* thread );
extern int DelThread( Thread* thread );

//timer
extern ScheTimer* STimer;					//yield timer
//S/R msg
extern int Send( Thread* send, Thread* receive, int msg);
extern int Receive( Thread* send, Thread* receive, int& msg);

//mailbox
extern int PostMail( Thread* send, Thread* receive, char* dat, unsigned int len );
extern int GetMail(Thread* send, Thread* receive, char* dat );

struct MailMsg{
	Thread* sendThread ;
	Thread* receiveThread;
	char data[MAX_MAIL_LENGTH];
};
extern unsigned int MailCount;
extern std::vector<MailMsg*> MailBox;
extern Semaphore* MailLock;
//console I/O
extern SynchConsole* SynchCmd;
//****END****



#endif // SYSTEM_H


