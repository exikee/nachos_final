// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "scheduler.h"
#include "system.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads to empty.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
//****EDIT****

	CurQueue = 1;
	Critial = false;
	for( int i = 0; i < MAX_PRIORITY; i++ )
		ReadyListWeight[ i ] = i * QUEUE_TIME_RATE;
//****END****

    //readyList = new List; 
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    //delete readyList; 
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    DEBUG('t', "Putting thread %s on ready list.\n", thread->GetName());

    thread->SetStatus(READY);
//****EDIT****
	ReadyList[thread->GetPriority()].Append((void*)thread);
//****END****

}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------
/*
Thread *
Scheduler::FindNextToRun ()
{
    return (Thread *)readyList->Remove();
}
*/
//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread)
{
    Thread *oldThread = currentThread;

#ifdef USER_PROGRAM			// ignore until running user programs 
    if (currentThread->Space != NULL) {	// if this thread is a user program,
		

        currentThread->SaveUserState(); // save the user's CPU registers
		
		currentThread->Space->SaveState();
		
    }
#endif    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    currentThread = nextThread;		    // switch to the next thread
    ASSERT(currentThread != NULL);
	currentThread->SetStatus(RUNNING);      // nextThread is now running
    
    DEBUG('t', "Switching from thread \"%s\" to thread \"%s\"\n",
	  oldThread->GetName(), nextThread->GetName());
    
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);
    
    DEBUG('t', "Now in thread \"%s\"\n", currentThread->GetName());

    // If the old thread gave up the processor because it was finishing,
    // we need to delete its carcass.  Note we cannot delete the thread
    // before now (for example, in Thread::Finish()), because up to this
    // point, we were still running on the old thread's stack!
    if (threadToBeDestroyed != NULL) {
        delete threadToBeDestroyed;
	threadToBeDestroyed = NULL;
    }
    
#ifdef USER_PROGRAM
    if (currentThread->Space != NULL) {		// if there is an address space
        currentThread->RestoreUserState();     // to restore, do it.
	currentThread->Space->RestoreState();
    }
#endif
}

//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    printf("Ready list contents:\n");
    ReadyList[CurQueue].Mapcar((VoidFunctionPtr) ThreadPrint);
}

//****EDIT****

Thread* Scheduler::FindNextToRun (){
#ifdef THREADDEBUG		
	cout << "[" <<stats->totalTicks << "]Scheduler: FindNextToRun. "<< endl;

#endif
    ASSERT(interrupt->getLevel() == IntOff);
	if( !ReadyList[ MAX_PRIORITY ].IsEmpty() ){				//real-time queue
		Critial = true;
		return (Thread*)ReadyList[ MAX_PRIORITY ].Remove();
	}
	if( ReadyList[ CurQueue ].IsEmpty() || ReadyListWeight[CurQueue] <= 0 ){
		ReadyListWeight[CurQueue] += CurQueue * QUEUE_TIME_RATE;
		if(ReadyListWeight[CurQueue] > CurQueue * 2 * QUEUE_TIME_RATE)
			ReadyListWeight[CurQueue] = CurQueue * 2* QUEUE_TIME_RATE;
		//round robin
		for( int i = CurQueue % ( MAX_PRIORITY - 1 ) + 1; i != CurQueue; i = i % ( MAX_PRIORITY - 1 ) + 1 ){	
			if( (!ReadyList[ i ].IsEmpty()) && ReadyListWeight[i] > 0 ){
				CurQueue = i;
				return (Thread*)ReadyList[ CurQueue ].Remove();
			}
			ReadyListWeight[i] += i * QUEUE_TIME_RATE;
			if(ReadyListWeight[ i ] > i * 2* QUEUE_TIME_RATE )
				ReadyListWeight[ i ] = i * 2* QUEUE_TIME_RATE;
		}
		if( ReadyList[ CurQueue ].IsEmpty() )
			return NULL;
		else
			return (Thread*)ReadyList[ CurQueue ].Remove();
	}
	else{
		return (Thread*)ReadyList[ CurQueue ].Remove();
	}	
}