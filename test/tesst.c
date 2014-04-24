#include "syscall.h"
//#include "synch.h"
void test(){
	while(1)
		Write("test", 4, 1)
	;
	Exit(0);
}

int
main()
{

//	Semaphore* test;
    SpaceId newProc;
    char prompt[4], ch, buffer[60];
    int i = 1000;

    prompt[0] = '-';
    prompt[1] = '-';
	prompt[2] = '\n';
	
	//Read(prompt, 3, 0);
	
	Write(prompt, 3, 1);
	Write("main", 4, 1);
	Fork("test",4,test,0);
	
	while(1)
		Write("main", 4, 1)
		;
}