// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#define PCBUFFER_SIZE 10
// testnum is set in main.cc
int testnum;
void FiveTick(){
	interrupt->OneTick();
	interrupt->OneTick();
	interrupt->OneTick();
	interrupt->OneTick();
	interrupt->OneTick();
}
//barrier
Barrier* barrier;
void Barr( int arg ){
	for( int i = 0; i < 10; i++){
		printf("Thread#%d in loop#%d\n",arg,i);
		FiveTick();
	}
		printf("Thread#%d Waiting\n",arg);
		barrier->Synchronous();
		printf("Thread#%d End\n",arg);
}

void BarrierTest(){
	Thread* t;
	barrier = new Barrier("test",4);
	t = new Thread("Barrier1");
	t->SetPriority(4);
	t->Fork(Barr,1);
	t = new Thread("Barrier2");
	t->SetPriority(3);
	t->Fork(Barr,2);
	t = new Thread("Barrier3");
	t->SetPriority(2);
	t->Fork(Barr,3);
	t = new Thread("Barrier4");
	t->SetPriority(1);
	t->Fork(Barr,4);

}

//S/R MAILBOX
Thread* Rece;
Thread* Send1;
Thread* Send2;
void SMsg(int arg){
	int i = 10;
	int j;
	int result;
	int msg;
	char* data = "MAIL";
	char* mail = new char[128];
	result = PostMail(currentThread,Rece,data,5);
	printf("%s Send mail \"%s\", result = %d\n",currentThread->GetName(),data,result);
	while(i--){
		result = Send(currentThread,Rece,i);
		printf("%s Send message \"%d\", to:%s, result = %d\n",currentThread->GetName(),i,Rece->GetName(), result);
	}	
}
void RMsg(int arg){
	int i = 10;
	int j;
	int result;
	int msg;
	char* data = new char[128];
	char* mail = new char[128];
	while(i--){
		result = Receive(Send1,currentThread,msg);
		printf("Receive message \"%d\",from:%s , result = %d\n",msg,Send1->GetName(),result);
		
		result = Receive(Send2,currentThread,msg);
		printf("Receive message \"%d\",from:%s , result = %d\n",msg,Send2->GetName(),result);
	}
	result = GetMail(Send1,currentThread,mail);
	printf("Get mail \"%s\",from:%s , result = %d\n",mail,Send1->GetName(),result);
	result = GetMail(Send2,currentThread,mail);
	printf("Get mail \"%s\",from:%s , result = %d\n",mail,Send2->GetName(),result);
		
	
}

void SRTest(){

	Rece = currentThread;
	Send1 = new Thread("send1");
	Send1->SetPriority(1);
	Send2 = new Thread("send2");
	Send2->SetPriority(2);
	Send1->Fork(SMsg,0);
	Send2->Fork(SMsg,0);
	RMsg(0);
}

//ProducerConsumer---BEGIN

class ProCon{
public:
	unsigned int ItemCount;
	unsigned int GoodsCount;
	Condition* Full;
	Condition* Empty;
	Lock* WRLock;
	Lock* FLock;
	Lock* ELock;

	List *GoodsQueue;
	ProCon(){
		ItemCount = 0;				//used buffer size
		GoodsCount = 0;				//Goods Num
		Full = new Condition("PCFull");
		Empty = new Condition("PCEmpty");
		FLock = new Lock("FLock");
		ELock = new Lock("ELock");
		WRLock = new Lock("WRLock");
		GoodsQueue = new List();
	}
	~ProCon(){
		delete Full;
		delete Empty;
		delete WRLock;
		delete FLock;
		delete ELock;
	}

	int Add(){
		while(ItemCount >= PCBUFFER_SIZE ){		//full
			FLock->Acquire();
			Full->Wait(FLock);					//wait siginal full release
			FLock->Release();	
		}
		//WRLock->Acquire();
		int* temp = new int;
		*temp = GoodsCount++;
		ItemCount++;
		GoodsQueue->Append((void*)GoodsCount );
		if( ItemCount == 1 ){					//empty->add
			ELock->Acquire();
			Empty->Signal(ELock);				//siginal consumer
			ELock->Release();
		}
		//WRLock->Release();
		return *temp;
	}
	int Remove(){
		while (ItemCount <= 0){					//wait empty release
			ELock->Acquire();
			Empty->Wait(ELock);
			ELock->Release();
		}
	//	WRLock->Acquire();
		int item = (int)GoodsQueue->Remove();
		ItemCount--;
		if (ItemCount == PCBUFFER_SIZE - 1 ){	
			FLock->Acquire();
			Full->Signal( FLock );
			FLock->Release();
		}
    //    WRLock->Release();
		return item;
	}
}ProConTest;

void producer(int arg){
	int item;
	while (true){
		item = ProConTest.Add();
		printf("Producer#%d adds Goods#%d. Buffer Used: %d\n",arg,item,ProConTest.ItemCount);
		FiveTick();
	}
}

void consumer(int arg){
	int item;
	while (true){
		item = ProConTest.Remove();
		printf("Consumer#%d takes Goods#%d. Buffer Used: %d\n",arg,item,ProConTest.ItemCount);
		FiveTick();
	}
}
void PCTest( int arg){
	Thread* t;
	int i = 0;
	
	t = new Thread("Producer1");
	t->SetPriority(4);
	t->Fork(producer,1);
	t = new Thread("Consumer1");
	t->SetPriority(2);
	t->Fork(consumer,1);
	
	t = new Thread("Producer2");
	t->SetPriority(3);
	t->Fork(producer,2);
	t = new Thread("Consumer2");
	t->SetPriority(4);
	t->Fork(consumer,2);
	t = new Thread("Consumer3");
	t->SetPriority(1);
	t->Fork(consumer,3);
}

//ProducerConsumer---END

//DP---BEGIN
Semaphore* Room;
Semaphore *Chop[5];

void Philosophers( int arg ){

	printf("Philosophers#%d thinking\n",arg);
	while(true){
		Room->P();
		printf("Philosophers#%d hungury\n",arg);
		Chop[arg]->P();
		printf("Philosophers#%d take Chop#%d\n",arg,arg);
		Chop[(arg+1)%5]->P();
		printf("Philosophers#%d take Chop#%d\n",arg,(arg+1)%5);
		printf("Philosophers#%d eating\n",arg);
		FiveTick();
		FiveTick();
		FiveTick();
		FiveTick();
		Chop[(arg+1)%5]->V();
		
		printf("Philosophers#%d release Chop#%d\n",arg,(arg+1)%5);
		Chop[arg]->V();
		printf("Philosophers#%d release Chop#%d\n",arg,arg);
		Room->V();
		FiveTick();
		FiveTick();
		FiveTick();
		FiveTick();
		printf("Philosophers#%d thinking\n",arg);
	}
}

void DPTest( int arg ){
	int i;
	Thread* t;
	Room = new Semaphore("Room",4);
	Chop[0] = new Semaphore("Chop0",1);
	Chop[1] = new Semaphore("Chop1",1);
	Chop[2] = new Semaphore("Chop2",1);
	Chop[3] = new Semaphore("Chop3",1);
	Chop[4] = new Semaphore("Chop4",1);

	t = new Thread("Philosopher#0");
	t->SetPriority(2);
	t->Fork(Philosophers,0);
	t = new Thread("Philosopher#1");
	t->SetPriority(1);
	t->Fork(Philosophers,1);
	t = new Thread("Philosopher#2");
	t->SetPriority(2);
	t->Fork(Philosophers,2);
	t = new Thread("Philosopher#3");
	t->SetPriority(1);
	t->Fork(Philosophers,3);
	t = new Thread("Philosopher#4");
	t->SetPriority(2);
	t->Fork(Philosophers,4);

}
//DP---END

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 20; num++) {
		printf("[ %d ]*** thread %d looped %d times\n",stats->totalTicks, which, num);
		FiveTick();
		FiveTick();
    }
}
void
SimpleThread2(int which)
{
    int num;
	for (num = 0; num < 10; num++) {
		printf("[ %d ]*** thread %d looped %d times\n",stats->totalTicks, which, num);
		FiveTick();
		FiveTick();
    }
}

//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");
	
    Thread *t = new Thread("Forked1");
	t->SetPriority(1);
    t->Fork(SimpleThread2, 1);
	
    t = new Thread("Forked2");
	t->SetPriority(2);
    t->Fork(SimpleThread2, 2);
	
    t = new Thread("Forked3");
	t->SetPriority(3);
    t->Fork(SimpleThread2, 3);
	
    t = new Thread("Forked4");
	t->SetPriority(4);
    t->Fork(SimpleThread2, 4);
	
	t = new Thread("PRI5");
	t->SetPriority(5);
    t->Fork(SimpleThread, 5);
	
    t = new Thread("Forked44");
	t->SetPriority(4);
    t->Fork(SimpleThread2, 44);
	FiveTick();
    t = new Thread("Forked33");
	t->SetPriority(3);
    t->Fork(SimpleThread2, 33);
	
    t = new Thread("Forked22");
	t->SetPriority(2);
    t->Fork(SimpleThread2, 22);
	
    t = new Thread("Forked11");
	t->SetPriority(1);
    t->Fork(SimpleThread2, 11);
    SimpleThread(0);
}

//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
    case 1:
	ThreadTest1();
	break;
	case 2:
		PCTest(0);
		break;
	case 3:
		DPTest(0);
		break;
	case 4:
		SRTest();
		break;
	case 5:
		BarrierTest();
		break;
	default:
	printf("No test specified.\n");
	break;
    }
}

