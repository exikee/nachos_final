// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(char* debugName, int initialValue)
{
	name = new char[strlen(debugName) + 1];
	memset(name,0,strlen(name));
	strcpy(name,debugName);
	value = initialValue;
	queue = new List;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
	delete queue;
	delete name;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
	IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
	
	while (value == 0) { 			// semaphore not available
		
		ASSERT(currentThread != NULL );
		queue->Append((void *)currentThread);	// so go to sleep
#ifdef SYNCHDEBUG
		cout <<"["<< stats->totalTicks <<"]Semaphore: "<< name <<"->P. Thread:" << currentThread->GetName() << " SLEEP."<< endl;
#endif

		currentThread->Sleep();
	} 
	value--; 					// semaphore available, 
						// consume its value
	#ifdef SYNCHDEBUG
		cout <<"["<< stats->totalTicks <<"]Semaphore: "<< name <<"->P End. Thread:" << currentThread->GetName() << " Get "<< name << endl;
#endif
	(void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

void
Semaphore::V()
{
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	Thread *thread;

	thread = (Thread *)queue->Remove();
	if (thread != NULL){	   // make thread ready, consuming the V immediately
		scheduler->ReadyToRun(thread);
#ifdef SYNCHDEBUG
		cout <<"["<< stats->totalTicks <<"]Semaphore: "<< name <<"->V. Thread:" << thread->GetName() << " READY."<< endl;
#endif
	}else{
#ifdef SYNCHDEBUG
		cout <<"["<< stats->totalTicks <<"]Semaphore: "<< name <<"->V. Thread:" << currentThread->GetName() << " Release."<< endl;
#endif
	}
	value++;
	(void) interrupt->SetLevel(oldLevel);
}

// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in the network assignment won't work!
Lock::Lock(char* debugName) {
	Name = new char[strlen(debugName) + 1];
	memset(Name,0,strlen(Name));
	strcpy(Name,debugName);
	LSemaphore = new Semaphore( "Lock", 1);
	LockHolder = NULL;
}
Lock::~Lock() {
	//delete LSemaphore;
	//delete Name;
}
void Lock::Acquire() {
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
#ifdef SYNCHDEBUG
		cout <<"["<< stats->totalTicks <<"]Lock: "<< Name <<"->Acquire." << endl;
#endif
	LSemaphore->P();
	LockHolder = currentThread;
	(void) interrupt->SetLevel(oldLevel);
}

bool Lock::IsHeldByCurrentThread() { 
	return LockHolder == currentThread;
}
void Lock::Release() {
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	ASSERT( IsHeldByCurrentThread() );
	LockHolder = NULL;
	LSemaphore->V();
#ifdef SYNCHDEBUG
		cout <<"["<< stats->totalTicks <<"]Lock: "<< Name <<"->Release." << endl;
#endif
	(void) interrupt->SetLevel(oldLevel);
}
//Condition
Condition::Condition(char* debugName) {
	Name = new char[strlen(debugName) + 1];
	memset(Name,0,strlen(Name));
	strcpy(Name,debugName);
	WaitQueue = new List();
}
Condition::~Condition() {
	//delete WaitQueue;
	//delete Name;
	
}
void Condition::Wait(Lock* conditionLock) {
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	Semaphore *waiter;
	ASSERT(conditionLock->IsHeldByCurrentThread());

	waiter = new Semaphore( "Condition", 0);
	WaitQueue->Append((void*)waiter);
#ifdef SYNCHDEBUG
	cout <<"["<< stats->totalTicks <<"]Condition: "<< Name <<"->Wait." << endl;
#endif
	conditionLock->Release();
	
	waiter->P();
	conditionLock->Acquire();
	ASSERT(waiter!=NULL);
	delete waiter;
	(void) interrupt->SetLevel(oldLevel);

}


void Condition::Signal(Lock* conditionLock) {
	IntStatus oldLevel = interrupt->SetLevel(IntOff);

	Semaphore *waiter;	
	ASSERT(conditionLock->IsHeldByCurrentThread());
	
	if (!WaitQueue->IsEmpty()) {
		waiter = (Semaphore*)WaitQueue->Remove();
		waiter->V();
#ifdef SYNCHDEBUG
	cout <<"["<< stats->totalTicks <<"]Condition: "<< Name <<"->Signal." << endl;
#endif
	}
	(void) interrupt->SetLevel(oldLevel);
}

void Condition::Broadcast(Lock* conditionLock) {
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
#ifdef SYNCHDEBUG
	cout <<"["<< stats->totalTicks <<"]Condition: "<< Name <<"->Broadcast." << endl;
#endif
	while (!WaitQueue->IsEmpty()) {
		Signal(conditionLock);
	}
	(void) interrupt->SetLevel(oldLevel);

}

//Barrier

Barrier::Barrier( char* name, int num ){
	BarrierName = name;
	TotalNum = num;
	ArriveCount = 0;
	CondLock =  new Lock("BLock");
	Cond = new Condition("Barrier");
}

Barrier::~Barrier(){
	delete CondLock;
	delete Cond;
}

int Barrier::Synchronous(){
	CondLock->Acquire();
	ArriveCount++;
	if ( ArriveCount == TotalNum ){
		Cond->Broadcast(CondLock);
	}
	else if( ArriveCount < TotalNum ){
		Cond->Wait(CondLock);
	}
	CondLock->Release();
	return 0;
}