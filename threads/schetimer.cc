
#include "copyright.h"
#include "system.h"
#include "timer.h"
#include "schetimer.h"
//debug
void ReadyListPrint( int ptr ){
	cout << ((Thread*)ptr)->GetName() <<" ";
}
void printqueue(){
	cout<<"CurQueue"<<scheduler->CurQueue<<endl;
	for( int i = 0; i < 6; i++ ){
		cout<<"QUEUE#"<<i<<" remaining weight:"<<scheduler->ReadyListWeight[ i ] <<". READY:";
		scheduler->ReadyList[i].Mapcar((VoidFunctionPtr)ReadyListPrint);
		cout << endl;
	}
}
//enddebug

//
static void ScheHandler( int arg ){
    MachineStatus status = interrupt->getStatus();
    if ( status != IdleMode ) {
#ifdef THREADDEBUGQUEUE
		cout << "[" <<stats->totalTicks << "]ScheTimer: ScheHandler. "<< endl;
		printqueue();
#endif

		if( scheduler->Critial == true ){
		}
		else{
			int weight = --(scheduler->ReadyListWeight[ scheduler->CurQueue ]);
			if( weight <= 0 ){
				interrupt->YieldOnReturn();
			}
		}
	}
}

//settimer
static void ScheTimerHandler(int arg){
	ScheTimer *p = (ScheTimer *)arg;
	p->ScheTimerExpired();
}

ScheTimer::ScheTimer( int callArg, int OneTick ){
    handler = ScheHandler;
    arg = callArg;
	ScheOneTick = OneTick;
    interrupt->Schedule(ScheTimerHandler, (int) this, ScheOneTick, TimerInt); 
}

void ScheTimer::ScheTimerExpired() {
    interrupt->Schedule(ScheTimerHandler, (int) this, ScheOneTick, TimerInt);
	(*handler)(arg);
}

