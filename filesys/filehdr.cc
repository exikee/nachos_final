// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"
#include "time.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{ 
	
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	numBytes = fileSize;
	int curSector = 0;
	int adSectors = 0;
	int i,j;
	bool result = true;
	int hdrBuf[L1_INDEX_NUM];
	int hdrL2Buf[L1_INDEX_NUM];
	int sectorsGet[NumDirect + L1_INDEX_NUM + L1_INDEX_NUM * L1_INDEX_NUM];
	int getNum = 0;
	CreateTime = time(NULL);
	ModifiedTime = time(NULL);
	L1Index = -1;
	L2Index = -1;
    numSectors  = divRoundUp(fileSize, SectorSize);
	if(numSectors > NumDirect ){
		if( numSectors > NumDirect + L1_INDEX_NUM ){
			if( numSectors > NumDirect + L1_INDEX_NUM + L1_INDEX_NUM * L1_INDEX_NUM )		//>max sector index
				result = false;
			else{
				adSectors = divRoundUp( numSectors - NumDirect - L1_INDEX_NUM ,L1_INDEX_NUM );
				if(freeMap->NumClear() < 2 + numSectors + adSectors )			//L2 index check
					result = false;
				else{
					for( i = 0; i < 2 + numSectors + adSectors; i++ )
						sectorsGet[ i ] = freeMap->Find();
				}
			}
		}
		else{
			if(freeMap->NumClear() < numSectors + 1 )							//L1 index check
				result = false;
			else{
				for( i = 0; i < 1 + numSectors; i++ )
					sectorsGet[ i ] = freeMap->Find();
			}
		}
	}
	else{
		if( freeMap->NumClear() < numSectors )									//Direct check
			result = false;
		else{
			for( i = 0; i < numSectors; i++ )
				sectorsGet[ i ] = freeMap->Find();
		}

	}
	(void) interrupt->SetLevel(oldLevel);
	if( result = false ){
		return false;
	}
	//allocate sector
	for( i = 0; i < NumDirect && curSector < numSectors; i++, curSector++ ){	//Direct
		dataSectors[ i ] = sectorsGet[ getNum++ ];
		ClearSector( dataSectors[ i ] );
	}
	if( curSector < numSectors ){			//L1
		L1Index = sectorsGet[ getNum++ ];
		ClearSector( L1Index );
		for( i = 0; i < L1_INDEX_NUM; i++ )
			hdrBuf[ i ] = -1;
		for( i = 0; i < L1_INDEX_NUM && curSector < numSectors; i++, curSector++ ){
			hdrBuf[ i ] = sectorsGet[ getNum++ ];
			ClearSector( hdrBuf[ i ] );
		}
		synchDisk->WriteSector( L1Index, (char *)hdrBuf );
		if( curSector < numSectors ){		//L2
			L2Index = sectorsGet[ getNum++ ];
			ClearSector( L2Index );
			for( i = 0; i < L1_INDEX_NUM; i++ )
				hdrL2Buf[ i ] = -1;
			for( j = 0; j < adSectors; j++ ){
				hdrL2Buf[ j ] = sectorsGet[ getNum++ ];
				ClearSector( hdrL2Buf[ j ] );
				for( i = 0; i < L1_INDEX_NUM; i++ )
					hdrBuf[ i ] = -1;
				for( i = 0; i < L1_INDEX_NUM && curSector < numSectors; i++, curSector++ ){
					hdrBuf[ i ] = sectorsGet[ getNum++ ];
					ClearSector( hdrBuf[ i ] );
				}
				synchDisk->WriteSector( hdrL2Buf[ j ], (char *)hdrBuf );
			}
			synchDisk->WriteSector( L2Index, (char*)hdrL2Buf );
		}
	}
    return true;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(BitMap *freeMap)
{
	IntStatus oldLevel = interrupt->SetLevel(IntOff);
	int curSector = 0;
	int i,j;
	int hdrBuf[L1_INDEX_NUM];
	int hdrL2Buf[L1_INDEX_NUM];
	for( i = 0; i < NumDirect && curSector < numSectors; i++, curSector++ ){	//Direct
		ASSERT(freeMap->Test((int) dataSectors[i]));
		freeMap->Clear((int) dataSectors[i]);
	}
	if( curSector < numSectors ){			//L1
		ASSERT(freeMap->Test( L1Index ) );
		synchDisk->ReadSector( L1Index, (char*)hdrBuf);
		for( i = 0; i < L1_INDEX_NUM && curSector < numSectors; i++, curSector++ ){
			ASSERT(freeMap->Test( hdrBuf[ i ] ) );
			freeMap->Clear( hdrBuf[ i ] );
		}
		if( curSector < numSectors ){		//L2
			ASSERT(freeMap->Test( L2Index ) );
			synchDisk->ReadSector( L2Index, (char*)hdrL2Buf );
			for( j = 0; curSector < numSectors; j++ ){
				ASSERT(freeMap->Test( hdrL2Buf[ j ] ) );
				synchDisk->ReadSector( hdrL2Buf[ j ], (char*)hdrBuf);
				for( i = 0; i < L1_INDEX_NUM && curSector < numSectors; i++, curSector++ ){
					ASSERT(freeMap->Test( hdrBuf[ i ] ) );
					freeMap->Clear( hdrBuf[ i ] );
				}
				freeMap->Clear( hdrL2Buf[ j ] );	
			}
			freeMap->Clear( L2Index );
		}
		freeMap->Clear( L1Index );
	}
	(void) interrupt->SetLevel(oldLevel);
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
	DEBUG('v', "FetchFrom HeadSectors:%d\n",sector );
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
	DEBUG('v', "WriteBack HeadSectors:%d\n",sector );
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
	int curSector  = offset / SectorSize;
	int i,j;
	int hdrBuf[L1_INDEX_NUM];
	int hdrL2Buf[L1_INDEX_NUM];
	int adSector = 0;
	int offSector = 0;
	if( curSector < 0 )
		return -1;
	if( curSector < NumDirect )
		return( dataSectors[ curSector ] );
	else if( curSector < NumDirect + L1_INDEX_NUM ){
		synchDisk->ReadSector( L1Index, (char *)hdrBuf );
			return hdrBuf[ curSector - NumDirect ];
	}
	else if(curSector < NumDirect + L1_INDEX_NUM + L1_INDEX_NUM * L1_INDEX_NUM ){
		adSector = ( curSector - NumDirect - L1_INDEX_NUM ) / L1_INDEX_NUM;
		offSector = ( curSector - NumDirect - L1_INDEX_NUM ) - adSector * L1_INDEX_NUM;
		synchDisk->ReadSector( L2Index, (char *)hdrL2Buf );
		synchDisk->ReadSector( hdrL2Buf[ adSector ], (char *)hdrBuf );
		return hdrBuf[ offSector ];
	}
	return -1;
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
	int loop1,loop2;
		
	int curSector = 0;
	int hdrBuf[L1_INDEX_NUM];
	int hdrL2Buf[L1_INDEX_NUM];
    char *data = new char[SectorSize];
	printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);

	for( i = 0; i < NumDirect && curSector < numSectors; i++, curSector++ ){	//Direct
		printf("%d ", dataSectors[i]);
	}
	if( curSector < numSectors ){			//L1
		synchDisk->ReadSector( L1Index, (char*)hdrBuf);
		for( i = 0; i < L1_INDEX_NUM && curSector < numSectors; i++, curSector++ ){
			printf("%d ", hdrBuf[ i ] );
		}
		if( curSector < numSectors ){		//L2
			synchDisk->ReadSector( L2Index, (char*)hdrL2Buf );
			for( j = 0; curSector < numSectors; j++ ){
				synchDisk->ReadSector( hdrL2Buf[ j ], (char*)hdrBuf);
				for( i = 0; i < L1_INDEX_NUM && curSector < numSectors; i++, curSector++ ){
					printf("%d ", hdrBuf[ i ] );
				}
			}
		}
	
	}
	curSector = 0;
	struct tm *tblock;
	time_t timer = CreateTime;
	tblock = localtime(&timer);
	printf("\nCreated time is: %s",asctime(tblock));
	timer = ModifiedTime;
	tblock = localtime(&timer);
	
	printf("Modified time is: %s\n",asctime(tblock));
	printf("File contents:\n");
	for( i = k = 0; i < NumDirect && curSector < numSectors; i++, curSector++ ){	//Direct
		synchDisk->ReadSector(dataSectors[i], data);
		for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
			if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
				printf("%c", data[j]);
			else
				printf("\\%x", (unsigned char)data[j]);
			}
		printf("\n"); 
	}
	if( curSector < numSectors ){			//L1
		synchDisk->ReadSector( L1Index, (char*)hdrBuf);
		for( loop1 = k = 0; loop1 < L1_INDEX_NUM && curSector < numSectors; loop1++, curSector++ ){	//L1
			synchDisk->ReadSector( hdrBuf[ loop1 ], data );
			for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
				if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
					printf("%c", data[j]);
				else
					printf("\\%x", (unsigned char)data[j]);
				}
			printf("\n"); 
		}
		if( curSector < numSectors ){		//L2
			synchDisk->ReadSector( L2Index, (char*)hdrL2Buf );
			for( loop2 = 0; curSector < numSectors; loop2++ ){
				synchDisk->ReadSector( hdrL2Buf[ loop2 ], (char*)hdrBuf );
				for( loop1 = k = 0; loop1 < L1_INDEX_NUM && curSector < numSectors; loop1++, curSector++ ){
					synchDisk->ReadSector( hdrBuf[ loop1 ], data );
					for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
						if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
							printf("%c", data[j]);
						else
							printf("\\%x", (unsigned char)data[j]);
					}
					printf("\n"); 
				}
			}
		}
	}
    delete [] data;
}
//****EDIT****
bool FileHeader::ChangeSize( BitMap *freeMap,int newSize ){
	
	if( newSize > MaxFileSize )
		return false;
	if( newSize <= numBytes )
		return true;
	IntStatus oldLevel = interrupt->SetLevel(IntOff);

	int sectorsGet[NumDirect + L1_INDEX_NUM + L1_INDEX_NUM * L1_INDEX_NUM];
	int newNumSectors  = divRoundUp(newSize, SectorSize);
	int curSector;
	int totalCurSectors = 0;
	int adSectors;
	int i,j;
	int getNum = 0;
	int hdrBuf[L1_INDEX_NUM];
	int hdrL2Buf[L1_INDEX_NUM];
	bool result = true;

	
	if( newNumSectors <= numSectors ){
		(void) interrupt->SetLevel(oldLevel);
		numBytes = newSize;
		return true;
	}
	
	if( numSectors > NumDirect ){
		if( numSectors > NumDirect + L1_INDEX_NUM ){
			if( numSectors > NumDirect + L1_INDEX_NUM + L1_INDEX_NUM * L1_INDEX_NUM )		//>max sector index
				result = false;
			else{
				adSectors = divRoundUp( numSectors - NumDirect - L1_INDEX_NUM ,L1_INDEX_NUM );
				totalCurSectors = numSectors + adSectors + 2;
			}
		}
		else{
			totalCurSectors = numSectors + 1;
		}
	}
	else{
		totalCurSectors = numSectors;
	}

	if(newNumSectors > NumDirect ){
		if( newNumSectors > NumDirect + L1_INDEX_NUM ){
			if( newNumSectors > NumDirect + L1_INDEX_NUM + L1_INDEX_NUM * L1_INDEX_NUM )		//>max sector index
				result = false;
			else{
				adSectors = divRoundUp( newNumSectors - NumDirect - L1_INDEX_NUM ,L1_INDEX_NUM );
				if(freeMap->NumClear() < 2 + newNumSectors + adSectors - totalCurSectors )			//L2 index check
					result = false;
				else{
					for( i = 0; i < 2 + newNumSectors + adSectors - totalCurSectors; i++ )
						sectorsGet[ i ] = freeMap->Find();
				}
			}
		}
		else{
			if(freeMap->NumClear() < newNumSectors + 1 - totalCurSectors )							//L1 index check
				result = false;
			else{
				for( i = 0; i < 1 + newNumSectors - totalCurSectors; i++ )
					sectorsGet[ i ] = freeMap->Find();
			}
		}
	}
	else{
		if( freeMap->NumClear() < newNumSectors - totalCurSectors )									//Direct check
			result = false;
		else{
			for( i = 0; i < newNumSectors - totalCurSectors; i++ )
				sectorsGet[ i ] = freeMap->Find();
		}

	}
	if( result = false ){
		(void) interrupt->SetLevel(oldLevel);
		return false;
	}
	curSector = numSectors;
	numSectors = newNumSectors;
	numBytes = newSize;
	
	DEBUG('v', "Cur Exist Sector:%d, NewSectorNum:%d\n",curSector,numSectors );
	(void) interrupt->SetLevel(oldLevel);
	//allocate sector
	for( i = curSector; i < NumDirect && curSector < numSectors; i++, curSector++ ){	//Direct
		dataSectors[ i ] = sectorsGet[ getNum++ ];
		ClearSector( dataSectors[ i ] );
		DEBUG('v', "Insert New Direct Sector:%d in dataSectors[ %d ]\n",dataSectors[i],i );
	}
	if( curSector < numSectors ){			//L1
		if( L1Index == -1 ){
			L1Index = sectorsGet[ getNum++ ];
			ClearSector( L1Index );
			for( i = 0; i < L1_INDEX_NUM; i++ )
				hdrBuf[ i ] = -1;
			DEBUG('v', "Insert New L1Index:%d\n",L1Index );
		}
		else{
			synchDisk->ReadSector( L1Index, (char*)hdrBuf );
		}
		for( i = curSector - NumDirect; i < L1_INDEX_NUM && curSector < numSectors; i++, curSector++ ){
			ASSERT( hdrBuf[ i ] == -1 );
			hdrBuf[ i ] = sectorsGet[ getNum++ ];
			ClearSector( hdrBuf[ i ] );
			DEBUG('v', "Insert New L1 Sector:%d in hdrBuf[ %d ]\n",hdrBuf[i],i );
		}
		synchDisk->WriteSector( L1Index, (char *)hdrBuf );
		if( curSector < numSectors ){		//L2
			if(L2Index == -1){
				L2Index = sectorsGet[ getNum++ ];
				ClearSector( L2Index );
				for( i = 0; i < L1_INDEX_NUM; i++ )
					hdrL2Buf[ i ] = -1;
				
				DEBUG('v', "Insert New L2Index:%d\n",L2Index );
			}
			else
				synchDisk->ReadSector( L2Index, (char*)hdrL2Buf );
			for( j = ( curSector - ( NumDirect + L1_INDEX_NUM ) ) / L1_INDEX_NUM; curSector < numSectors; j++ ){
				if( hdrL2Buf[ j ] == -1 ){
					hdrL2Buf[ j ] = sectorsGet[ getNum++ ];
					ClearSector( hdrL2Buf[ j ] );
					for( i = 0; i < L1_INDEX_NUM; i++ )
						hdrBuf[ i ] = -1;
					
					DEBUG('v', "Insert New L2 Index:%d in hdrL2Buf[ %d ]\n",hdrL2Buf[j],j );
				}
				else
					synchDisk->ReadSector( hdrL2Buf[ j ], (char*)hdrBuf );
				for( i = ( curSector - ( NumDirect + L1_INDEX_NUM ) ) - ( j * L1_INDEX_NUM ); i < L1_INDEX_NUM && curSector < numSectors; i++, curSector++ ){
					ASSERT( hdrBuf[ i ] == -1 );
					hdrBuf[ i ] = sectorsGet[ getNum++ ];
					ClearSector( hdrBuf[ i ] );
					DEBUG('v', "Insert New L2 Sector:%d in hdrBuf[ %d ]\n",hdrBuf[i],i );
				}
				synchDisk->WriteSector( hdrL2Buf[ j ], (char *)hdrBuf );
			}
			synchDisk->WriteSector( L2Index, (char*)hdrL2Buf );
		}
	}
	return true;

}

void FileHeader::ClearSector( int sectorNum ){
	char allZero[SectorSize] ={0};
	synchDisk->WriteSector(sectorNum, (char *)allZero);
}

void FileHeader::SetModifiedTime(){
	ModifiedTime = time(NULL);
}
//****END****
