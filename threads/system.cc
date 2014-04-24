// system.cc 
//	Nachos initialization and cleanup routines.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "thread.h"


// This defines *all* of the global data structures used by Nachos.
// These are all initialized and de-allocated by this file.

Thread *currentThread;			// the thread we are running now
Thread *threadToBeDestroyed;  		// the thread that just finished
Scheduler *scheduler;			// the ready list
Interrupt *interrupt;			// interrupt status
Statistics *stats;			// performance metrics
Timer *timer;				// the hardware timer device,
					// for invoking context switches
//****EDIT****


std::map<int, Thread*> ThreadList;

std::map<int, User*> UserList;
ScheTimer* STimer;
int TotalThreadCount;
int ForkedThreadID;


//SYNCH CONSOLE
SynchConsole* SynchCmd;

unsigned int MailCount;
std::vector<MailMsg*> MailBox;
Semaphore* MailLock;

//****END****


#ifdef FILESYS_NEEDED
FileSystem  *fileSystem;
map<int,FileEntry*> FileEntryList;
#endif

#ifdef FILESYS
SynchDisk   *synchDisk;
#endif

#ifdef USER_PROGRAM	// requires either FILESYS or FILESYS_STUB
Machine *machine;	// user program memory and registers
#endif

#ifdef VMC
#include "../userprog/bitmap.h"
#include "../userprog/addrspace.h"
deque<int> PTList;
BitMap* MemMap;
BitMap* VAddrBitMap;
std::map<int, TranslationEntry*> VAddrEntry;
TranslationEntry* PmEntry[ 32 ];
#endif

#ifdef NETWORK
PostOffice *postOffice;
#endif


// External definition, to allow us to take a pointer to this function
extern void Cleanup();


//----------------------------------------------------------------------
// TimerInterruptHandler
// 	Interrupt handler for the timer device.  The timer device is
//	set up to interrupt the CPU periodically (once every TimerTicks).
//	This routine is called each time there is a timer interrupt,
//	with interrupts disabled.
//
//	Note that instead of calling Yield() directly (which would
//	suspend the interrupt handler, not the interrupted thread
//	which is what we wanted to context switch), we set a flag
//	so that once the interrupt handler is done, it will appear as 
//	if the interrupted thread called Yield at the point it is 
//	was interrupted.
//
//	"dummy" is because every interrupt handler takes one argument,
//		whether it needs it or not.
//----------------------------------------------------------------------
static void
TimerInterruptHandler(int dummy)
{
	if (interrupt->getStatus() != IdleMode)
		interrupt->YieldOnReturn();
	
}

//----------------------------------------------------------------------
// Initialize
// 	Initialize Nachos global data structures.  Interpret command
//	line arguments in order to determine flags for the initialization.  
// 
//	"argc" is the number of command line arguments (including the name
//		of the command) -- ex: "nachos -d +" -> argc = 3 
//	"argv" is an array of strings, one for each command line argument
//		ex: "nachos -d +" -> argv = {"nachos", "-d", "+"}
//----------------------------------------------------------------------
void
Initialize(int argc, char **argv)
{
	int argCount;
	char* debugArgs = "";
	bool randomYield = FALSE;
	
#ifdef USER_PROGRAM
	bool debugUserProg = FALSE;	// single step user program
#endif
#ifdef FILESYS_NEEDED
	bool format = FALSE;	// format disk
#endif
#ifdef NETWORK
	double rely = 1;		// network reliability
	int netname = 0;		// UNIX socket name
#endif
	
	for (argc--, argv++; argc > 0; argc -= argCount, argv += argCount) {
	argCount = 1;
	if (!strcmp(*argv, "-d")) {
		if (argc == 1)
		debugArgs = "+";	// turn on all debug flags
		else {
			debugArgs = *(argv + 1);
			argCount = 2;
		}
	} else if (!strcmp(*argv, "-rs")) {
		ASSERT(argc > 1);
		RandomInit(atoi(*(argv + 1)));	// initialize pseudo-random
						// number generator
		randomYield = TRUE;
		argCount = 2;
	}
#ifdef USER_PROGRAM
    
	if (!strcmp(*argv, "-s"))
		debugUserProg = TRUE;
#endif
#ifdef FILESYS_NEEDED
	if (!strcmp(*argv, "-f"))
		format = TRUE;
#endif
#ifdef NETWORK
	if (!strcmp(*argv, "-l")) {
		ASSERT(argc > 1);
		rely = atof(*(argv + 1));
		argCount = 2;
	} else if (!strcmp(*argv, "-m")) {
		ASSERT(argc > 1);
		netname = atoi(*(argv + 1));
		argCount = 2;
	}
#endif
	}

	DebugInit(debugArgs);			// initialize DEBUG messages
	stats = new Statistics();			// collect statistics
	interrupt = new Interrupt;			// start up interrupt handling
	scheduler = new Scheduler();		// initialize the ready queue
	if (randomYield)				// start the timer (if needed)
		timer = new Timer(TimerInterruptHandler, 0, randomYield);
//****EDIT****

	STimer = new ScheTimer(0,100);
	TotalThreadCount = 0;
	ForkedThreadID = REVERSED_THREADID_NUM;
	MailCount = 0;
	MailLock = new Semaphore("MailLock", 1);
	User* u = new User("Admin",1,"/",255,0);
	UserList.insert(std::pair<int, User*>( u->UserID, u ) );
	//VM

	
#ifdef VMC
	MemMap = new BitMap(32);
	VAddrBitMap = new BitMap(256);
	for(int i = 0; i < 32; i++ ){
		PmEntry[ i ] = NULL;
	}
#endif
//****END****

	threadToBeDestroyed = NULL;

	// We didn't explicitly allocate the current thread we are running in.
	// But if it ever tries to give up the CPU, we better have a Thread
	// object to save its state. 
	currentThread = new Thread("main");
	currentThread->SetStatus(RUNNING);
	currentThread->SetPriority(4);
	currentThread->SetThreadID(1);
	currentThread->SetUserID(0);
	ThreadList.insert(std::pair<int, Thread*>(1, currentThread) );
	TotalThreadCount++;
	interrupt->Enable();
	//scheduler->Run(currentThread);

	CallOnUserAbort(Cleanup);			// if user hits ctl-C
	
#ifdef USER_PROGRAM
	machine = new Machine(debugUserProg);	// this must come first
	SynchCmd = NULL;//new SynchConsole(); //stdio
#endif

#ifdef FILESYS
	synchDisk = new SynchDisk("DISK");
#endif

#ifdef FILESYS_NEEDED
	fileSystem = new FileSystem(format);
#endif

#ifdef NETWORK
	postOffice = new PostOffice(netname, rely, 10);
#endif
}

//----------------------------------------------------------------------
// Cleanup
// 	Nachos is halting.  De-allocate global data structures.
//----------------------------------------------------------------------
void
Cleanup()
{
	printf("\nCleaning up...\n");
#ifdef NETWORK
	delete postOffice;
#endif
	
#ifdef USER_PROGRAM
	delete machine;
#endif

#ifdef FILESYS_NEEDED
	delete fileSystem;
#endif

#ifdef FILESYS
	delete synchDisk;
#endif
	delete STimer;
	delete timer;
	delete scheduler;
	delete interrupt;
	
	Exit(0);
}

//****EDIT****
//add,SUCCESS:return ThreadID;FAULT:return -1;


int AddThread( Thread* thread ){
	if( TotalThreadCount >=  MAX_THREAD_NUM ){
#ifdef THREADDEBUG
		cout <<"["<< stats->totalTicks <<"]System: AddThread. MAXTHREAD. Name:"<< thread->GetName() << ", TID:"<< thread->GetThreadID() << ", P: "<< thread->GetPriority() << endl;
#endif
		return -1;
	}
	if( thread == NULL )
		return -1;
	int	tid = ForkedThreadID;
	std::map<int, Thread*>::iterator itFind;
	int i = 256;
	while( i-- ){
		itFind = ThreadList.find(tid);
		if(itFind != ThreadList.end() )
			tid = ( tid- REVERSED_THREADID_NUM + 1) %(  MAX_THREADID_NUM - REVERSED_THREADID_NUM )  + REVERSED_THREADID_NUM;
		else
			break;
	}
	if( i == 0 )
		return -1;
	ThreadList.insert(std::pair<int, Thread*>(tid, thread) );
	ForkedThreadID = tid;
	TotalThreadCount++;
	return tid;
}

//del,SUCCESS:return 0;FAULT:return -1;
int DelThread( Thread* thread ){
	if( TotalThreadCount <= 0 )
		return -1;
	if( thread == NULL )
		return -1;
	int tid = thread->GetThreadID();
	std::map<int, Thread*>::iterator itFind;

	if( (itFind = ThreadList.find(tid) ) == ThreadList.end() )
		return -1;
#ifdef THREADDEBUG
	cout <<"["<< stats->totalTicks <<"]System: DelThread. TID:" << (*itFind).first << endl;
#endif
	ThreadList.erase(itFind);
	TotalThreadCount--;
	return 0;
}


//S/R msg
int Send( Thread* send, Thread* receive, int msg){

	if( send == NULL || receive == NULL )
		return -1;
	int tid = send->GetThreadID();
	std::map<int, Thread*>::iterator itFind;
	if( (itFind = ThreadList.find(tid) ) == ThreadList.end() )
		return -1;
	tid = receive->GetThreadID();
	if( (itFind = ThreadList.find(tid) ) == ThreadList.end() )
		return -1;
	receive->MsgMutex->P();
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	MSG* m = new MSG;
	m->t = send;
	m->msg = msg;
	if( receive->InsertMsg( m ) != -1 ){
		receive->MsgMutex->V();
		receive->SMsg->V();
		(void) interrupt->SetLevel(oldLevel);
		return 0;
	}
	else{
		receive->MsgMutex->V();
		(void) interrupt->SetLevel(oldLevel);
		return -1;
	}
}

int Receive( Thread* send, Thread* receive, int& msg){
	if( send == NULL || receive == NULL )
		return -1;
	int tid = send->GetThreadID();
/*	map <int, Thread*>::iterator itFind;
	if( (itFind = ThreadList.find(tid) ) == ThreadList.end() )
		return -1;*/
	tid = receive->GetThreadID();
	std::map<int, Thread*>::iterator itFind;
	if( (itFind = ThreadList.find(tid) ) == ThreadList.end() )
		return -1;
	int PCount = 0;
	receive->SMsg->P();
	while(1){
		receive->MsgMutex->P();
		IntStatus oldLevel = interrupt->SetLevel(IntOff);
		for( std::vector<MSG*>::iterator iter = receive->MsgQueue.begin(); iter != receive->MsgQueue.end(); iter++ ){
			if( (*iter)->t == send ){
				msg = (*iter)->msg;
				delete (*iter);
				receive->MsgQueue.erase( iter );
				receive->MsgMutex->V();
				for( int i = 0; i < PCount;i++ )
					receive->SMsg->V();
				(void) interrupt->SetLevel(oldLevel);
				return 0;
			}
		}
		receive->MsgMutex->V();
		(void) interrupt->SetLevel(oldLevel);
		receive->SMsg->P();
		PCount++;
	}
}

//mailbox

int PostMail( Thread* send, Thread* receive, char* dat, unsigned int len ){
	if( MailCount > MAX_MAIL_NUM )
		return -1;
	if( len > MAX_MAIL_LENGTH )
		return -1;
	if( send == NULL || receive == NULL )
		return -1;
	int tid = send->GetThreadID();
	std::map<int, Thread*>::iterator itFind;
	if( (itFind = ThreadList.find(tid) ) == ThreadList.end() )
		return -1;
	tid = receive->GetThreadID();
	if( (itFind = ThreadList.find(tid) ) == ThreadList.end() )
		return -1;

	MailMsg* m = new MailMsg;
	m->sendThread = send;
	m->receiveThread = receive;
	memset(m->data,0,MAX_MAIL_LENGTH);
	memcpy( m->data, dat, len );
	MailLock->P();
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	MailCount++;
	MailBox.push_back( m );
	MailLock->V();
	(void) interrupt->SetLevel(oldLevel);
	return 0;
}

int GetMail(Thread* send, Thread* receive, char* dat ){
	if( MailCount <= 0 )
		return -1;
	if( send == NULL || receive == NULL )
		return -1;
	int tid = send->GetThreadID();
/*	map <int, Thread*>::iterator itFind;
	if( (itFind = ThreadList.find(tid) ) == ThreadList.end() )
		return -1;
*/	tid = receive->GetThreadID();
	std::map<int, Thread*>::iterator itFind;
	if( (itFind = ThreadList.find(tid) ) == ThreadList.end() )
		return -1;

	MailLock->P();
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	for( std::vector<MailMsg*>::iterator iter = MailBox.begin(); iter != MailBox.end(); iter++ ){
		if( (*iter)->sendThread == send && (*iter)->receiveThread == receive ){
			memcpy( dat,(*iter)->data, MAX_MAIL_LENGTH );
			delete (*iter);
			MailBox.erase( iter );
			MailCount--;
			MailLock->V();
			(void) interrupt->SetLevel(oldLevel);
			return 0;
		}
	}
	MailLock->V();
	(void) interrupt->SetLevel(oldLevel);
	return -1;
}
