// thread.cc 
//	Routines to manage threads.  There are four main operations:
//
//	Fork -- create a thread to run a procedure concurrently
//		with the caller (this is done in two steps -- first
//		allocate the Thread object, then call Fork on it)
//	Finish -- called when the forked procedure finishes, to clean up
//	Yield -- relinquish control over the CPU to another ready thread
//	Sleep -- relinquish control over the CPU, but thread is now blocked.
//		In other words, it will not run again, until explicitly 
//		put back on the ready queue.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "thread.h"
#include "switch.h"
#include "synch.h"
#include "system.h"

#define STACK_FENCEPOST 0xdeadbeef	// this is put at the top of the
					// execution stack, for detecting 
					// stack overflows

//----------------------------------------------------------------------
// Thread::Thread
// 	Initialize a thread control block, so that we can then call
//	Thread::Fork.
//
//	"threadName" is an arbitrary string, useful for debugging.
//----------------------------------------------------------------------
void YieldNow( int arg ){
	DEBUG('e', "[%d]YieldNow\n",stats->totalTicks);
	interrupt->YieldOnReturn();
}
Thread::Thread(char* threadName)
{
	Name = new char[strlen(threadName) + 1];
	memset(Name,0,strlen(Name));
	strcpy(Name,threadName);
    StackTop = NULL;
    Stack = NULL;
    Status = JUST_CREATED;
	SMsg = new Semaphore("SendingMsg",0);
	MsgMutex = new Semaphore("MsgQLock",1);
	JoinLock = new Lock("JL");
	JoinCond = new Condition("JC");
	JoinNum = 0;
	
#ifdef USER_PROGRAM
	Space = NULL;
#endif
//****EDIT****
	Priority = 1;
	UserID = -1;
	ThreadID = -1;
//****END****
}

//----------------------------------------------------------------------
// Thread::~Thread
// 	De-allocate a thread.
//
// 	NOTE: the current thread *cannot* delete itself directly,
//	since it is still running on the stack that we need to delete.
//
//      NOTE: if this is the main thread, we can't delete the stack
//      because we didn't allocate it -- we got it automatically
//      as part of starting up Nachos.
//----------------------------------------------------------------------

Thread::~Thread()
{
    DEBUG('t', "Deleting thread \"%s\"\n", Name);
	
    ASSERT(this != currentThread);
	delete Name;
	delete SMsg;
	delete MsgMutex;
    if (Stack != NULL)
		DeallocBoundedArray((char *) Stack, StackSize * sizeof(int));
	
#ifdef USER_PROGRAM
	delete Space;
#endif
	
}

//----------------------------------------------------------------------
// Thread::Fork
// 	Invoke (*func)(arg), allowing caller and callee to execute 
//	concurrently.
//
//	NOTE: although our definition allows only a single integer argument
//	to be passed to the procedure, it is possible to pass multiple
//	arguments by making them fields of a structure, and passing a pointer
//	to the structure as "arg".
//
// 	Implemented as the following steps:
//		1. Allocate a stack
//		2. Initialize the stack so that a call to SWITCH will
//		cause it to run the procedure
//		3. Put the thread on the ready queue
// 	
//	"func" is the procedure to run concurrently.
//	"arg" is a single argument to be passed to the procedure.
//----------------------------------------------------------------------

int
Thread::Fork(VoidFunctionPtr func, int arg)
{
    DEBUG('t', "Forking thread \"%s\" with func = 0x%x, arg = %d\n",
	  Name, (int) func, arg);
    
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	int tid = AddThread(this);
	if( tid > 0 ){
#ifdef THREADDEBUG
		cout <<"["<< stats->totalTicks <<"]Thread: Fork. Name:"<< Name << ", TID:"<< tid << ", P: "<< Priority << endl;
#endif
		ThreadID = tid;
		StackAllocate(func, arg);

		scheduler->ReadyToRun(this);	// ReadyToRun assumes that interrupts 
										// are disabled!
		
		if( Priority == MAX_PRIORITY && scheduler->Critial == false ){
			scheduler->Critial = true;
			interrupt->Schedule(YieldNow, 0, 1, TimerInt); 
		}
		
		(void) interrupt->SetLevel(oldLevel);
		return tid;
	}
	else{
		(void) interrupt->SetLevel(oldLevel);
		return 0;
	}
}    

//----------------------------------------------------------------------
// Thread::CheckOverflow
// 	Check a thread's stack to see if it has overrun the space
//	that has been allocated for it.  If we had a smarter compiler,
//	we wouldn't need to worry about this, but we don't.
//
// 	NOTE: Nachos will not catch all stack overflow conditions.
//	In other words, your program may still crash because of an overflow.
//
// 	If you get bizarre results (such as seg faults where there is no code)
// 	then you *may* need to increase the stack size.  You can avoid stack
// 	overflows by not putting large data structures on the stack.
// 	Don't do this: void foo() { int bigArray[10000]; ... }
//----------------------------------------------------------------------

void
Thread::CheckOverflow()
{
    if (Stack != NULL)
#ifdef HOST_SNAKE			// Stacks grow upward on the Snakes
	ASSERT(Stack[StackSize - 1] == STACK_FENCEPOST);
#else
	ASSERT((int) *Stack == (int) STACK_FENCEPOST);
#endif
}

//----------------------------------------------------------------------
// Thread::Finish
// 	Called by ThreadRoot when a thread is done executing the 
//	forked procedure.
//
// 	NOTE: we don't immediately de-allocate the thread data structure 
//	or the execution stack, because we're still running in the thread 
//	and we're still on the stack!  Instead, we set "threadToBeDestroyed", 
//	so that Scheduler::Run() will call the destructor, once we're
//	running in the context of a different thread.
//
// 	NOTE: we disable interrupts, so that we don't get a time slice 
//	between setting threadToBeDestroyed, and going to sleep.
//----------------------------------------------------------------------

//
void
Thread::Finish ()
{
    (void) interrupt->SetLevel(IntOff);		
    ASSERT(this == currentThread);
	JoinLock->Acquire();
	if( JoinNum > 0)
		JoinCond->Broadcast(JoinLock);
	JoinLock->Release();
	
#ifdef THREADDEBUG
	cout << "[" <<stats->totalTicks  <<"]Thread: Finish. Name:"<<Name<< ", TID:"<<ThreadID<<", Pri:"<<Priority<<endl;
#endif
	if( Priority == MAX_PRIORITY )
		scheduler->Critial = false;
	DelThread(this);
    threadToBeDestroyed = currentThread;
    Sleep();					// invokes SWITCH
    // not reached
}

//----------------------------------------------------------------------
// Thread::Yield
// 	Relinquish the CPU if any other thread is ready to run.
//	If so, put the thread on the end of the ready list, so that
//	it will eventually be re-scheduled.
//
//	NOTE: returns immediately if no other thread on the ready queue.
//	Otherwise returns when the thread eventually works its way
//	to the front of the ready list and gets re-scheduled.
//
//	NOTE: we disable interrupts, so that looking at the thread
//	on the front of the ready list, and switching to it, can be done
//	atomically.  On return, we re-set the interrupt level to its
//	original state, in case we are called with interrupts disabled. 
//
// 	Similar to Thread::Sleep(), but a little different.
//----------------------------------------------------------------------

void
Thread::Yield ()
{
    Thread *nextThread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    
    ASSERT(this == currentThread);
    if( ( Status == BLOCKED )|| ( scheduler->ReadyListWeight[scheduler->CurQueue] <= 0 ) || scheduler->Critial == true ){
		DEBUG('t', "Yielding thread \"%s\"\n", GetName());

		nextThread = scheduler->FindNextToRun();
		if (nextThread != NULL) {
			if( nextThread != currentThread ){
#ifdef THREADDEBUG
		cout << "["<< stats->totalTicks <<"]Scheduler: SWITCH. Name: "<< currentThread->GetName() << ", TID: "<< currentThread->GetThreadID() << ", P: "<< currentThread->GetPriority() <<
				" <----> Name: "<< nextThread->GetName() << ", TID: "<< nextThread->GetThreadID()  <<", P: "<< nextThread->GetPriority() << endl;
#endif
			scheduler->ReadyToRun(this);
			scheduler->Run(nextThread);
			}
		}
		else{
	
#ifdef THREADDEBUG
			cout << "["<< stats->totalTicks <<"]: Yield. Name: "<< Name << ", TID: "<< ThreadID << ", P: "<< Priority <<
				" <----> NULL." << endl;
#endif
		}
	}
    (void) interrupt->SetLevel(oldLevel);
}

//----------------------------------------------------------------------
// Thread::Sleep
// 	Relinquish the CPU, because the current thread is blocked
//	waiting on a synchronization variable (Semaphore, Lock, or Condition).
//	Eventually, some thread will wake this thread up, and put it
//	back on the ready queue, so that it can be re-scheduled.
//
//	NOTE: if there are no threads on the ready queue, that means
//	we have no thread to run.  "Interrupt::Idle" is called
//	to signify that we should idle the CPU until the next I/O interrupt
//	occurs (the only thing that could cause a thread to become
//	ready to run).
//
//	NOTE: we assume interrupts are already disabled, because it
//	is called from the synchronization routines which must
//	disable interrupts for atomicity.   We need interrupts off 
//	so that there can't be a time slice between pulling the first thread
//	off the ready list, and switching to it.
//----------------------------------------------------------------------
void
Thread::Sleep ()
{
    Thread *nextThread;
    
    ASSERT(this == currentThread);
    ASSERT(interrupt->getLevel() == IntOff);
    
    DEBUG('t', "Sleeping thread \"%s\"\n", GetName());

    Status = BLOCKED;
    while ((nextThread = scheduler->FindNextToRun()) == NULL){

		interrupt->Idle();	// no one to run, wait for an interrupt
	}
	scheduler->Run(nextThread); // returns when we've been signalled
}

//----------------------------------------------------------------------
// ThreadFinish, InterruptEnable, ThreadPrint
//	Dummy functions because C++ does not allow a pointer to a member
//	function.  So in order to do this, we create a dummy C function
//	(which we can pass a pointer to), that then simply calls the 
//	member function.
//----------------------------------------------------------------------

static void ThreadFinish()    { currentThread->Finish(); }
static void InterruptEnable() { interrupt->Enable(); }
void ThreadPrint(int arg){ Thread *t = (Thread *)arg; t->Print(); }

//----------------------------------------------------------------------
// Thread::StackAllocate
//	Allocate and initialize an execution stack.  The stack is
//	initialized with an initial stack frame for ThreadRoot, which:
//		enables interrupts
//		calls (*func)(arg)
//		calls Thread::Finish
//
//	"func" is the procedure to be forked
//	"arg" is the parameter to be passed to the procedure
//----------------------------------------------------------------------

void
Thread::StackAllocate (VoidFunctionPtr func, int arg)
{
    Stack = (int *) AllocBoundedArray(StackSize * sizeof(int));

#ifdef HOST_SNAKE
    // HP stack works from low addresses to high addresses
    StackTop = Stack + 16;	// HP requires 64-byte frame marker
    Stack[StackSize - 1] = STACK_FENCEPOST;
#else
    // i386 & MIPS & SPARC stack works from high addresses to low addresses
#ifdef HOST_SPARC
    // SPARC stack must contains at least 1 activation record to start with.
    StackTop = Stack + StackSize - 96;
#else  // HOST_MIPS  || HOST_i386
    StackTop = Stack + StackSize - 4;	// -4 to be on the safe side!
#ifdef HOST_i386
    // the 80386 passes the return address on the stack.  In order for
    // SWITCH() to go to ThreadRoot when we switch to this thread, the
    // return addres used in SWITCH() must be the starting address of
    // ThreadRoot.
    *(--StackTop) = (int)ThreadRoot;
#endif
#endif  // HOST_SPARC
    *Stack = STACK_FENCEPOST;
#endif  // HOST_SNAKE
    
    MachineState[PCState] = (int) ThreadRoot;
    MachineState[StartupPCState] = (int) InterruptEnable;
    MachineState[InitialPCState] = (int) func;
    MachineState[InitialArgState] = arg;
    MachineState[WhenDonePCState] = (int) ThreadFinish;
}

#ifdef USER_PROGRAM
#include "machine.h"

//----------------------------------------------------------------------
// Thread::SaveUserState
//	Save the CPU state of a user program on a context switch.
//
//	Note that a user program thread has *two* sets of CPU registers -- 
//	one for its state while executing user code, one for its state 
//	while executing kernel code.  This routine saves the former.
//----------------------------------------------------------------------

void
Thread::SaveUserState()
{
	for (int i = 0; i < NumTotalRegs; i++)
		UserRegisters[i] = machine->ReadRegister(i);
#ifndef VMC
	memcpy(this->mainMemory,machine->mainMemory,4096);
#endif
}

//----------------------------------------------------------------------
// Thread::RestoreUserState
//	Restore the CPU state of a user program on a context switch.
//
//	Note that a user program thread has *two* sets of CPU registers -- 
//	one for its state while executing user code, one for its state 
//	while executing kernel code.  This routine restores the former.
//----------------------------------------------------------------------

void
Thread::RestoreUserState()
{
	for (int i = 0; i < NumTotalRegs; i++)
		machine->WriteRegister(i,UserRegisters[i]);
#ifndef VMC
	memcpy(machine->mainMemory,this->mainMemory,4096);
#endif
}
#endif


//****EDIT****
//threads
void Thread::SetPriority( int p ){
	Priority = p;
}
int Thread::GetPriority(){
	return Priority;
}
void Thread::SetUserID( int uid ){
	UserID = uid;
}
int Thread::GetUserID(){
	return UserID;
}
void Thread::SetThreadID( int tid ){
	ThreadID = tid;
}
int Thread::GetThreadID(){
	return ThreadID;
}

//S/R msg
int Thread::InsertMsg(MSG* msg){
	if(MsgQueue.size() > MSG_QUEUE_SIZE )
		return -1;
	MsgQueue.push_back(msg);
	return 0;
}


//syscall

int* Thread::GetMachineReg(){
	return MachineState;
}
void Thread::CopyThreadInfo( Thread* t ){
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	UserID = t->GetUserID();
	Priority = t->GetPriority();

#ifdef USER_PROGRAM
	for (int i = 0; i < NumTotalRegs; i++) {
		UserRegisters[i] = machine->ReadRegister(i);
	}
	UserRegisters[PrevPCReg] = this->CurPC;
	UserRegisters[PCReg] = this->CurPC;
	UserRegisters[NextPCReg] = this->CurPC+4;
	
#ifndef VMC
	Space = t->Space;
	memcpy(this->mainMemory,machine->mainMemory,4096);
#else
	Space = new AddrSpace(t->Space);
#endif

#endif
	(void) interrupt->SetLevel(oldLevel);
}

