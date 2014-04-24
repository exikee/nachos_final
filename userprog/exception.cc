// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "synchconsole.h"
#include "syscall.h"
#include "thread.h"
#include "filehdr.h"
#include "filesys.h"
#include "directory.h"
#include "utility.h"
#include "synch.h"
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

//****EDIT****

#define PF_LRU



int ExitValue=0;
int TLBLast = 0;
int PTLast = 0;
int TLBMissCount = 0;
int PageFaultCount = 0;
void dummy( int a ){
	ASSERT(currentThread->Space != NULL);
	if (currentThread->Space != NULL) { // if there is an address space
		currentThread->RestoreUserState(); // to restore, do it.		
		currentThread->Space->RestoreState();
	}
	machine->Run();
}

void ForkNewThread(){
	int nameOffset = machine->ReadRegister(4);
	int nameNum = machine->ReadRegister(5);
	char name[20]={0};
	for( int i = 0; i < nameNum; i++)
		if( machine->ReadMem(nameOffset + i , 1, (int*)(name + i) ) == false )
			return;

	int funcOffset = machine->ReadRegister(6);
	int funcArg = machine->ReadRegister(7);
	int regPC = machine->ReadRegister(PCReg);
	
	//Thread
	Thread* newThread = new Thread("test");
	
	newThread->CurPC = funcOffset;
	newThread->CopyThreadInfo( currentThread );
	if (!newThread->Fork(dummy,0) ){
		printf("Create Thread Failed\n");
		delete newThread;
	}
	else{
	}
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg, machine->ReadRegister(PCReg)+4);
}

void WriteToConsole(char* outFile){
	int bufferOffset = machine->ReadRegister(4);
	int bufferNum = machine->ReadRegister(5);

	int ch;
	DEBUG('e', "[%d]Write Begin\n",stats->totalTicks);
	for( int i = 0; i < bufferNum; i++){
		if( machine->ReadMem(bufferOffset + i , 1, (&ch) ) == false )
			return;
		//SynchCmd->PutChar((char)ch);
		DEBUG('e', "[%d]Write char:%c, bufferNum:%d.\n",stats->totalTicks,ch,bufferNum);
		cout<< (char)ch;
	}
	DEBUG('e', "[%d]Write End\n",stats->totalTicks);
	
	
	machine->WriteRegister(2,0 );
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg, machine->ReadRegister(PCReg)+4);
}
void ReadFromConsole(char* inFile){
	int bufferOffset = machine->ReadRegister(4);
	int bufferNum = machine->ReadRegister(5);
	char ch;
	for( int i = 0; i < bufferNum; i++){
		//machine->WriteMem(bufferOffset + i , 1, (int)SynchCmd->GetChar() );
		if( scanf("%c",&ch) != EOF )
			if( machine->WriteMem(bufferOffset + i , 1, (int)ch ) == false )
				return;
		else
			break;
	}
	
	machine->WriteRegister(2,0 );
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg, machine->ReadRegister(PCReg)+4);
}

int ReadFromFile( OpenFile* file ){
	int bufferOffset = machine->ReadRegister(4);
	int bufferNum = machine->ReadRegister(5);
	int result = 0;
	if(bufferNum > 0 && bufferNum <= 128 ){
		char* buf = new char[bufferNum];
		result = file->Read(buf,bufferNum);
		for( int i = 0; i < bufferNum; i++){
			machine->WriteMem(bufferOffset + i , 1, (int)(buf[i]) );
		}
	}
	
	machine->WriteRegister(2,result );
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg, machine->ReadRegister(PCReg)+4);
	return result;
}

int WriteToFile( OpenFile* file ){
	int bufferOffset = machine->ReadRegister(4);
	int bufferNum = machine->ReadRegister(5);
	int result = 0;
	if(bufferNum > 0 && bufferNum <= 128 ) {
		char* buf = new char[bufferNum];
		for( int i = 0; i < bufferNum; i++){
			machine->ReadMem(bufferOffset + i , 1, (int*)(&buf[i]) );		
		}
		result = file->Write(buf,bufferNum);
	}
	
	machine->WriteRegister(2,result );
	machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
	machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
	machine->WriteRegister(NextPCReg, machine->ReadRegister(PCReg)+4);
	
	return result;
}


//****END****
void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
	int result = 0;
    if (which == SyscallException){
		if(type == SC_Halt) {
			DEBUG('a', "Shutdown, initiated by user program.\n");
   		interrupt->Halt();
		}
//****EDIT****
		else if( type == SC_Exit ){							//1
			ExitValue = machine->ReadRegister(4);
			DEBUG('j', "Enter Exit\n" );
			DEBUG('E', "Finish Exit,Code %d\n",ExitValue );	
			currentThread->Finish();
			
			DEBUG('j', "Finish Exit\n" );
		}
#ifdef VMC
		else if( type == SC_Exec ){							//2
			DEBUG('j', "Enter Exec\n" );
			char fileName[FULL_PATH_LENGTH + 1]={0};
			int bufOffset = machine->ReadRegister(4);
			for( int i = 0; i < FULL_PATH_LENGTH; i++ ){
				if( machine->ReadMem( bufOffset + i, 1, (int*)(&fileName[i]) ) == false ) return;
				if( fileName[i] == '\0' )
					break;
			}
			OpenFile *executable = fileSystem->Open(fileName);
			
			if( executable != NULL ){
				Thread* newThread = new Thread(fileName);
				AddrSpace *sspace;
				DEBUG('j', "ExecThread: %s\n",fileName );
				
				int UserReg[40];
				
				for (int i = 0; i < 40; i++)
					UserReg[i] = machine->ReadRegister(i);
				sspace = new AddrSpace(executable);
				sspace->InitRegisters();
				newThread->Space = sspace;
				newThread->SaveUserState();

				result = newThread->Fork(dummy,0);
				if( result == 0 ){
					delete newThread;
					ASSERT(false);
				}
				else{
					delete executable;
					DEBUG('j', "Exec Thread tid: %d\n",result );
				}
				
				for (int i = 0; i < 40; i++)
					machine->WriteRegister(i,UserReg[i]);
			}
			else{
				printf("Unable to open file %s\n", fileName);
			}
			
			machine->WriteRegister(2,result);
			machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
			machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
			machine->WriteRegister(NextPCReg, machine->ReadRegister(PCReg)+4);
			
		}
#else
		else if( type == SC_Exec ){							//2
			DEBUG('j', "Enter Exec\n" );
			char fileName[FULL_PATH_LENGTH + 1]={0};
			int bufOffset = machine->ReadRegister(4);
			for( int i = 0; i < FULL_PATH_LENGTH; i++ ){
				machine->ReadMem( bufOffset + i, 1, (int*)(&fileName[i]) );
				if( fileName[i] == '\0' )
					break;
			}
			OpenFile *executable = fileSystem->Open(fileName);
			
			if( executable != NULL ){
				Thread* newThread = new Thread(fileName);
				AddrSpace *sspace;
				DEBUG('j', "ExecThread: %s\n",fileName );
				
				int UserReg[40];
				char mainMem[MEMORY_SIZE];
				
				for (int i = 0; i < 40; i++)
					UserReg[i] = machine->ReadRegister(i);
				memcpy(mainMem,machine->mainMemory,MEMORY_SIZE);
				TranslationEntry *pTable = machine->pageTable;
				int nPages =  machine->pageTableSize;
				sspace = new AddrSpace(executable);
				sspace->InitRegisters();
				newThread->Space = sspace;
				newThread->SaveUserState();

				result = newThread->Fork(dummy,0);
				if( result == 0 ){
					delete newThread;
					ASSERT(false);
				}
				else{
					delete executable;
					DEBUG('j', "Exec Thread tid: %d\n",result );
				}
				
				for (int i = 0; i < 40; i++)
					machine->WriteRegister(i,UserReg[i]);
				memcpy(machine->mainMemory,mainMem,MEMORY_SIZE);
				machine->pageTable = pTable;
				machine->pageTableSize = nPages;
				
			}
			else{
				printf("Unable to open file %s\n", fileName);
			}
			
			machine->WriteRegister(2,result);
			machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
			machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
			machine->WriteRegister(NextPCReg, machine->ReadRegister(PCReg)+4);
			
		}
#endif
		else if( type == SC_Join ){						//3
			int tid = machine->ReadRegister(4);
			if(tid != 0 ){
				std::map<int, Thread*>::iterator itFind;
				if( (itFind = ThreadList.find(tid) ) != ThreadList.end() ){
					Thread* thr = (*itFind).second;
					ASSERT(thr !=NULL);
					thr->JoinLock->Acquire();
					thr->JoinNum++;
					
					thr->JoinCond->Wait(thr->JoinLock);
					
					thr->JoinLock->Release();
				}
			}

			machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
			machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
			machine->WriteRegister(NextPCReg, machine->ReadRegister(PCReg)+4);
			
		}
		else if( type == SC_Create ){						//4
			char fileName[FULL_PATH_LENGTH + 1]={0};
			int bufOffset = machine->ReadRegister(4);
			for( int i = 0; i < FULL_PATH_LENGTH; i++ ){
				if( machine->ReadMem( bufOffset + i, 1, (int*)(&fileName[i]) ) == false)
					return;
				if( fileName[i] == '\0' )
					break;
			}
			
			int result = (int)fileSystem->Create(fileName,128);
			DEBUG('e', "[%d]Create file : %s, result : %d\n",stats->totalTicks,fileName,result);
			machine->WriteRegister(2,result);
			machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
			machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
			machine->WriteRegister(NextPCReg, machine->ReadRegister(PCReg)+4);

		}
		else if( type == SC_Open ){							//5
			char fileName[FULL_PATH_LENGTH]={0};
			int bufOffset = machine->ReadRegister(4);
			for( int i = 0; i < FULL_PATH_LENGTH; i++ ){
				if( machine->ReadMem( bufOffset + i, 1, (int*)(&fileName[i]) ) == false )
					return;
				if( fileName[i] == '\0' )
					break;
			}
			OpenFile* file = fileSystem->Open(fileName);
			DEBUG('j', "Open: %d\n",(int)file );
			int result;
			if( file == NULL )
				result = 0;
			else
				result = (int)file;
			machine->WriteRegister(2,result);
			machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
			machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
			machine->WriteRegister(NextPCReg, machine->ReadRegister(PCReg)+4);

		}
		else if( type == SC_Read ){							//6
			int readType = machine->ReadRegister(6);
			if( readType == ConsoleInput ){
				ReadFromConsole(NULL);
			}
			else{
				OpenFile* file = (OpenFile*)readType;
				ReadFromFile( file );
			}
		}
		else if( type == SC_Write ){						//7
			int writeType = machine->ReadRegister(6);
			if( writeType == ConsoleOutput ){
				WriteToConsole(NULL);
			}
			else{
				OpenFile* file = (OpenFile*)writeType;
				WriteToFile( file );
			}
		}
		else if( type == SC_Close ){						//8
			int closeType = machine->ReadRegister(4);
			int result = 0;
			if( closeType != 0 ){
				
				OpenFile* file = (OpenFile*)closeType;
				DEBUG('j', "Close: %d\n",(int)file );
				fileSystem->FileClose( file );
			}
			machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
			machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
			machine->WriteRegister(NextPCReg, machine->ReadRegister(PCReg)+4);
		}
		else if( type == SC_Fork ){							//9
			ForkNewThread();
		}
		else if( type == SC_Yield ){							//9
			interrupt->Schedule(YieldNow, 0, 1, TimerInt); 
			machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
			machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
			machine->WriteRegister(NextPCReg, machine->ReadRegister(PCReg)+4);
		}
		else{
			printf("Unexpected user mode exception %d %d\n", which, type);
			ASSERT(FALSE);
		}
	}	
//****END****
//VMC
#ifdef VMC
	else if(which == PageFaultException){
		TLBMissCount++;
		//VM
		IntStatus oldLevel = interrupt->SetLevel(IntOff);
		int badAddr = machine->ReadRegister(BadVAddrReg);
		int i;
		
		int vmIndex = badAddr / 128;
		TranslationEntry* userEntry = &(currentThread->Space->pageTable[vmIndex]);

		if( userEntry->valid ){		//in phy mem
			DEBUG('T', "[%d]Valid Addr No PageFault. Count = %d.  Addr = %d. PC = %d. Stored in Pm:%d\n",stats->totalTicks,PageFaultCount,badAddr,machine->ReadRegister(PCReg),userEntry->physicalPage);
			
		}
		else{
			
			//exchange
			
			PageFaultCount++;
			DEBUG('V', "[%d]PageFault. Addr = %d. Count = %d.\n",stats->totalTicks, badAddr, PageFaultCount );
			char MemSector[128];
			fileSystem->VmFile->ReadAt(MemSector,128,userEntry->virtualPage * 128);

			if( MemMap->NumClear() > 0 ){
				userEntry->valid = true;
				userEntry->physicalPage = MemMap->Find();
				PmEntry[ userEntry->physicalPage ] = userEntry;
				memcpy(&(machine->mainMemory[ userEntry->physicalPage * 128 ] ),MemSector,128 );
				//printf("%d:%d,%d\n",userEntry->physicalPage,-1,userEntry->virtualPage);
				DEBUG('V', "[%d]PageFault. New Empty Memory. PC = %d. Stored in Pm:%d\n",stats->totalTicks,machine->ReadRegister(PCReg),userEntry->physicalPage);
			}
			else{
				int exchange = -1;
				
				
#ifdef PF_NRU	//NRU
				while( exchange == -1 ){
					
					for( i = ( PTLast + 1 ) % 32; i != PTLast; i = ( i + 1 ) % 32 ){
						
						if( PmEntry[ i ]->dirty == false && PmEntry[ i ]->use == false ){
							exchange = i;
							PTLast = i;
							break;
						}
					}
					if( exchange == -1 ){
						for( i = ( PTLast + 1 ) % 32; i != PTLast; i = ( i + 1 ) % 32 ){
							ASSERT(PmEntry[ i ] != NULL );
							if( PmEntry[ i ]->use == false ){
								exchange = i;
								PTLast = i;
								break;
							}
							PmEntry[ i ]->use = false;
						}
					}
				}
#endif
#ifdef PF_LRU	//LRU
				exchange = PTList.front();
				PTList.pop_front();
				PTList.push_back(exchange);
#endif
#ifdef PF_FIFO//FIFO
				exchange = PTLast;
				PTLast = ( exchange + 1 ) % 32;
#endif
				//Clear TLB
				for ( i = 0; i < TLBSize; i++){
					if( machine->tlb[i].physicalPage == exchange ){
						machine->tlb[i].valid  = false;
						break;
					}
				}
				//swap memory
				if( PmEntry[ exchange ]->dirty  == true )
					fileSystem->VmFile->WriteAt(&(machine->mainMemory[ PmEntry[ exchange ]->physicalPage * 128 ]),128, PmEntry[ exchange ]->virtualPage * 128 );
				PmEntry[ exchange ]->valid = false;
				PmEntry[ exchange ]->physicalPage = -1;
				userEntry->valid = true;
				userEntry->physicalPage = exchange;
				userEntry->use = false;
				userEntry->dirty = false;
				//printf("%d:%d,%d\n",exchange,PmEntry[ exchange ]->virtualPage,userEntry->virtualPage);
				
				PmEntry[ exchange ] = userEntry;

				memcpy(&(machine->mainMemory[ userEntry->physicalPage * 128 ] ),MemSector,128 );	
				
				DEBUG('V', "[%d]Page Exchange at PC = %d. Swap in Pm:%d\n",stats->totalTicks,machine->ReadRegister(PCReg),userEntry->physicalPage);
			}
		}
		//TLB MISS
		TranslationEntry *entry = NULL;

				//find empty first
		for ( i = 0; i < TLBSize; i++ ){
			if( machine->tlb[i].valid == false ){
				entry = &machine->tlb[i];
				TLBLast = i;
				break;
			}
		}
		//if no empty entry
		int type = 0;
		while( entry == NULL ){
			type++;
			for ( i = ( TLBLast + 1 ) % TLBSize; i != TLBLast; i = ( i + 1 ) % TLBSize ){
				
				if( machine->tlb[i].use == false && machine->tlb[i].dirty == false ){
					entry = &machine->tlb[i];
					TLBLast = i;
					
					DEBUG('T', "[%d]TLB	NRU Find, Type = %d\n",stats->totalTicks,type);
					break;
				}
			}
			if( entry == NULL ){
				type++;
				for ( i = ( TLBLast + 1 ) % TLBSize; i != TLBLast; i = ( i + 1 ) % TLBSize ){
					if( machine->tlb[i].use == false ){
						entry = &machine->tlb[i];
						TLBLast = i;
						DEBUG('T', "[%d]TLB	NRU Find, Type = %d\n",stats->totalTicks,type);
						break;
					}
					machine->tlb[i].use = false;
				}
			}
		} 

		//set entry
		ASSERT(userEntry->virtualPage >= 0);
		ASSERT(userEntry->physicalPage >= 0);

		entry->valid = true;
		entry->virtualPage = userEntry->virtualPage;
		entry->physicalPage = userEntry->physicalPage;
		entry->readOnly = userEntry->readOnly;
		entry->use = false;
		entry->dirty = false;
		(void) interrupt->SetLevel(oldLevel);
		DEBUG('T', "[%d]TLB Miss, Count:%d, at Thread:%s, PC = %d, vPage = %d, pPage = %d\n",
			stats->totalTicks,TLBMissCount,currentThread->GetName(),machine->ReadRegister(PCReg),userEntry->virtualPage,userEntry->physicalPage);
		DEBUG('T', "\tvPage\tpPage\n");	
		for ( i = 0; i < TLBSize; i++ ){
			DEBUG('T', "\t%d\t%d\n",machine->tlb[i].virtualPage,machine->tlb[i].physicalPage);	
		}
		
	}
#endif
	else {

		printf("Unexpected user mode exception %d %d\n", which, type);
		ASSERT(FALSE);
    }
}
