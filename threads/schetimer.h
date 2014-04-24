
#ifndef	SCHETIMER_H
#define SCHETIMER_H
#include "utility.h"

#include "copyright.h"
#include "system.h"
#include "timer.h"
class ScheTimer{
public:
    ScheTimer( int callArg, int OneTick );
    ~ScheTimer() {}
    void ScheTimerExpired();
private:
    VoidFunctionPtr handler;
    int arg;
	int ScheOneTick;
};
#endif // SCHETIMER_H

