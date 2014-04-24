// directory.cc 
//	Routines to manage a directory of file names.
//
//	The directory is a table of fixed length entries; each
//	entry represents a single file, and contains the file name,
//	and the location of the file header on disk.  The fixed size
//	of each directory entry means that we have the restriction
//	of a fixed maximum size for file names.
//
//	The constructor initializes an empty directory of a certain size;
//	we use ReadFrom/WriteBack to fetch the contents of the directory
//	from disk, and to write back any modifications back to disk.
//
//	Also, this implementation has the restriction that the size
//	of the directory cannot expand.  In other words, once all the
//	entries in the directory are used, no more files can be created.
//	Fixing this is one of the parts to the assignment.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "utility.h"
#include "filehdr.h"
#include "directory.h"

//----------------------------------------------------------------------
// Directory::Directory
// 	Initialize a directory; initially, the directory is completely
//	empty.  If the disk is being formatted, an empty directory
//	is all we need, but otherwise, we need to call FetchFrom in order
//	to initialize it from disk.
//
//	"size" is the number of entries in the directory
//----------------------------------------------------------------------

Directory::Directory()
{
} 

//----------------------------------------------------------------------
// Directory::~Directory
// 	De-allocate directory data structure.
//----------------------------------------------------------------------

Directory::~Directory()
{ 
} 



void Directory::ClearAll(){
	for(vector<DirectoryEntry*>::iterator it = FileList.begin(); it != FileList.end(); it++ ){
		delete (*it);
	}
	for(vector<Directory*>::iterator it = FolderList.begin(); it != FolderList.end(); it++ ){
		(*it)->ClearAll();
		delete (*it);
	}
}

//----------------------------------------------------------------------
// Directory::FetchFrom
// 	Read the contents of the directory from disk.
//
//	"file" -- file containing the directory contents
//----------------------------------------------------------------------

void
Directory::FetchFrom(OpenFile *file, bool seekZero)
{
	if(seekZero == true )
		file->Seek(0);
	file->Read(DirName,FileNameMaxLen + 1);
	file->Read(FullPath,FULL_PATH_LENGTH + 1);
	FileList.clear();
	int fileNum;
	DirectoryEntry* de;
	file->ReadInt( &fileNum );
	for( int i = 0; i < fileNum; i++ ){
		de = new DirectoryEntry();
		file->Read((char*)de, sizeof(DirectoryEntry) );
		FileList.push_back(de);
	}
	int DirNum;
	Directory* dy;
	FolderList.clear();
	file->ReadInt( &DirNum );
	for( int i = 0; i < DirNum; i++ ){
		dy = new Directory();
		dy->ParentDir = this;
		dy->FetchFrom(file,false);
		FolderList.push_back(dy);
	}
}

//----------------------------------------------------------------------
// Directory::WriteBack
// 	Write any modifications to the directory back to disk
//
//	"file" -- file to contain the new directory contents
//----------------------------------------------------------------------

void
Directory::WriteBack(OpenFile *file, bool seekZero)
{
	if( seekZero ==true )
		file->Seek(0);
	int temp;
	file->Write(DirName,FileNameMaxLen + 1);
	file->Write(FullPath,FULL_PATH_LENGTH + 1);
	temp =FileList.size();
	file->WriteInt( &temp );
	for(vector<DirectoryEntry*>::iterator it = FileList.begin(); it != FileList.end(); it++ ){
		file->Write((char*)(*it), sizeof(DirectoryEntry) );	
	}
	temp =FolderList.size();
	file->WriteInt( &temp );
	for(vector<Directory*>::iterator it = FolderList.begin(); it != FolderList.end(); it++ ){
		(*it)->WriteBack(file,false);
	}
}

//----------------------------------------------------------------------
// Directory::FindIndex
// 	Look up file name in directory, and return its location in the table of
//	directory entries.  Return -1 if the name isn't in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::FindIndex(char *name)
{
	Directory* dy;
	DirectoryEntry* de;
	char tempBuf[FULL_PATH_LENGTH + 1];
	char* fullPath = tempBuf;
	strncpy(fullPath,name,FULL_PATH_LENGTH + 1);
	char* nextPath = strstr(fullPath,"/");
	if( nextPath != NULL ){
		nextPath[0] = 0;
		nextPath = fullPath;
		fullPath = fullPath + strlen(fullPath) + 1;
		for(vector<Directory*>::iterator it = FolderList.begin(); it != FolderList.end(); it++ ){
			if( !strncmp((*it)->DirName, nextPath, FileNameMaxLen) )
				return (*it)->FindIndex( fullPath );
		}
		return -1;
	}
	else{
		char* fileName = strstr(fullPath,".");
		char* extName = "";
		if( fileName != NULL ){
			fileName[0] = 0;
			fileName = fullPath;
			extName = fullPath + strlen(fileName) + 1;
		}
		else fileName = fullPath;
		for(vector<DirectoryEntry*>::iterator it = FileList.begin(); it != FileList.end(); it++ ){
		if( !strncmp((*it)->name, fileName, FileNameMaxLen) && !strncmp((*it)->ExtName,extName,FileExtNameMaxLen) )
			return (*it)->sector;
		}
	}
	return -1;
	
	//unreached
	for (int i = 0; i < tableSize; i++)
        if (table[i].inUse && !strncmp(table[i].name, name, FileNameMaxLen))
	    return i;
    return -1;		// name not in directory
}

//----------------------------------------------------------------------
// Directory::Find
// 	Look up file name in directory, and return the disk sector number
//	where the file's header is stored. Return -1 if the name isn't 
//	in the directory.
//
//	"name" -- the file name to look up
//----------------------------------------------------------------------

int
Directory::Find(char *name)
{
    int i = FindIndex(name);
	DEBUG('f', "Find file: %s, Sector: %d\n", name, i);
    if (i != -1)
		return i;
    return -1;
}

//----------------------------------------------------------------------
// Directory::Add
// 	Add a file into the directory.  Return TRUE if successful;
//	return FALSE if the file name is already in the directory, or if
//	the directory is completely full, and has no more space for
//	additional file names.
//
//	"name" -- the name of the file being added
//	"newSector" -- the disk sector containing the added file's header
//----------------------------------------------------------------------

bool
Directory::Add(char *name, int newSector)
{ 
 	char tempBuf[FULL_PATH_LENGTH + 1];
	char* fullPath = tempBuf;
	strncpy(fullPath,name,FULL_PATH_LENGTH + 1);
	Directory* dy = this;
	DirectoryEntry* de;
	vector<Directory*>::iterator itd;
	vector<DirectoryEntry*>::iterator ite;
	char* nextPath = fullPath;
	while( fullPath[0] != '\0' ){
		DEBUG('v', "Add file: %s, fullPath:%s\n", nextPath, fullPath);
		nextPath = strstr(fullPath,"/");
		if( nextPath != NULL ){
			nextPath[0] = 0;
			nextPath = fullPath;
			fullPath = fullPath + strlen(fullPath) + 1;

			for( itd = dy->FolderList.begin(); itd != dy->FolderList.end(); itd++ ){
				if( !strncmp((*itd)->DirName, nextPath, FileNameMaxLen) ){
					break;
				}
			}
			if( itd == dy->FolderList.end() )
				return false;
			else
				dy = (*itd);
		}
		else{
			char* fileName = strstr(fullPath,".");
			char* extName = "";
			if( fileName != NULL ){
				fileName[0] = '\0';
				fileName = fullPath;
				extName = fileName + strlen(fileName) + 1;
			}
			else fileName = fullPath;
			for( ite = dy->FileList.begin(); ite != dy->FileList.end(); ite++ ){
				if( !strncmp((*ite)->name, fileName, FileNameMaxLen) && !strncmp((*ite)->ExtName,extName,FileExtNameMaxLen) )
					return false;
			}
			
			de = new DirectoryEntry;
			de->inUse = true;
			de->sector = newSector;
			strncpy( de->name,fileName,FileNameMaxLen );
			strncpy( de->ExtName, extName,FileExtNameMaxLen );
			strncpy(tempBuf,name,FULL_PATH_LENGTH + 1);
			fullPath = tempBuf;
			nextPath = strrchr(fullPath,(int)'/');
			if(nextPath != NULL)
				nextPath[0] ='\0';
			else fullPath = "";
			strncpy(de->FullPath,fullPath,FULL_PATH_LENGTH + 1 );
			ASSERT(dy != NULL);
			DEBUG('v', "Add file: fullPath:%s, fileName:%s, extName:%s\n", de->FullPath, de->name,de->ExtName);
			dy->FileList.push_back(de);

			return true;
		}
	}
	return false;
}

//----------------------------------------------------------------------
// Directory::Remove
// 	Remove a file name from the directory.  Return TRUE if successful;
//	return FALSE if the file isn't in the directory. 
//
//	"name" -- the file name to be removed
//----------------------------------------------------------------------

bool
Directory::Remove(char *name)
{ 
	char tempBuf[FULL_PATH_LENGTH + 1];
	char* fullPath = tempBuf;
	strncpy(fullPath,name,FULL_PATH_LENGTH + 1);
	Directory* dy = this;
	DirectoryEntry* de;
	char* nextPath;
	while( fullPath[0] != 0 ){
		nextPath = strstr(fullPath,"/");
		if( nextPath != NULL ){
			nextPath[0] = 0;
			nextPath = fullPath;
			fullPath = fullPath + strlen(fullPath) + 1;
			vector<Directory*>::iterator itd;
			for( itd = dy->FolderList.begin(); itd != dy->FolderList.end(); itd++ ){
				if( !strncmp((*itd)->DirName, nextPath, FileNameMaxLen) ){
					break;
				}
			}
			if( itd == dy->FolderList.end() )
				return false;
			else{
				if( fullPath[0] == 0 ){
					if( (*itd)->FolderList.empty() && (*itd)->FileList.empty() ){
						dy->FolderList.erase( itd );
						delete (*itd);
						return true;
					}
					else{
						printf("Dir Not Empty\n");
						return false;
					}
				}
				else
					dy = (*itd);
			}
		}
		else{
			char* fileName = strstr(fullPath,".");
			char* extName = "";
			if( fileName != NULL ){
				fileName[0] = '\0';
				fileName = fullPath;
				extName = name + strlen(fileName) + 1;
				
			}
			else fileName = fullPath;
			DEBUG('v', "Remove file: fullPath:%s + %s, fileName:%s, extName:%s\n", dy->FullPath,dy->DirName, fileName,extName);
			for(vector<DirectoryEntry*>::iterator it = dy->FileList.begin(); it != dy->FileList.end(); it++ ){
				if( !strncmp((*it)->name, fileName, FileNameMaxLen) && !strncmp((*it)->ExtName,extName,FileExtNameMaxLen) ){
					dy->FileList.erase(it);
					delete (*it);
					return true;
				}
			}
			return false;
		}
	}
	return false;
}

//----------------------------------------------------------------------
// Directory::List
// 	List all the file names in the directory. 
//----------------------------------------------------------------------

void
Directory::List( int level )
{
	if(level == 0)
		printf("[Root]\n");
	for(vector<DirectoryEntry*>::iterator it = FileList.begin(); it != FileList.end(); it++ ){
		for( int i =0; i <= level; i++ )
			printf("-");
		printf("%s", (*it)->name);
		if((*it)->ExtName[0] != 0 )
			printf(".%s", (*it)->ExtName);
		printf("\n");
	}
	for(vector<Directory*>::iterator it = FolderList.begin(); it != FolderList.end(); it++ ){
		for( int i =0; i <= level; i++ )
			printf("-");
		printf("[%s]\n", (*it)->DirName);
		(*it)->List(level + 1);
	}
	return;
}

//----------------------------------------------------------------------
// Directory::Print
// 	List all the file names in the directory, their FileHeader locations,
//	and the contents of each file.  For debugging.
//----------------------------------------------------------------------

void
Directory::Print( int level )
{ 
    FileHeader *hdr = new FileHeader;

    printf("Directory contents:\n");
    	if(level == 0)
		printf("[Root]\n");
	for(vector<DirectoryEntry*>::iterator it = FileList.begin(); it != FileList.end(); it++ ){
		for( int i =0; i <= level; i++ )
			printf("-");
		printf("%s", (*it)->name);
		if((*it)->ExtName[0] != 0 )
			printf(".%s", (*it)->ExtName);
		printf(" Sector:%d\n",(*it)->sector);
	    hdr->FetchFrom((*it)->sector);
	    hdr->Print();

	}
	for(vector<Directory*>::iterator it = FolderList.begin(); it != FolderList.end(); it++ ){
		for( int i =0; i <= level; i++ )
			printf("-");
		printf("[%s]\n", (*it)->DirName);
		(*it)->Print(level + 1);
	}
	delete hdr;
	return;
}



bool
Directory::AddDir(char *name )
{ 
 	char tempBuf[FULL_PATH_LENGTH + 1];
	char* fullPath = tempBuf;
	strncpy(fullPath,name,FULL_PATH_LENGTH + 1);
	int len = strlen(fullPath);
	if( fullPath[len-1] != '/' )
		return false;
	Directory* dy = this;
	vector<Directory*>::iterator itd;
	char* nextPath = NULL;
	while( fullPath[0] != '\0' ){
		nextPath = strstr(fullPath,"/");
		if( nextPath != NULL ){
			nextPath[0] = 0;
			nextPath = fullPath;
			fullPath = fullPath + strlen(fullPath) + 1;

			for( itd = dy->FolderList.begin(); itd != dy->FolderList.end(); itd++ ){
				if( !strncmp((*itd)->DirName, nextPath, FileNameMaxLen) ){
					break;
				}
			}
			if( fullPath[0] == '\0' ){
				if( itd == dy->FolderList.end() ){
					
					Directory* newDir = new Directory();

					strncpy(newDir->DirName,nextPath,FileNameMaxLen );
					strncpy(tempBuf,name,FULL_PATH_LENGTH + 1 );
					fullPath = tempBuf;
					nextPath = strrchr(tempBuf,(int)'/');
					nextPath[0] = 0;
					nextPath = strrchr(tempBuf,(int)'/');
					if(nextPath != NULL )
						nextPath[0] = 0;
					else fullPath[0] = 0;
					strncpy(newDir->FullPath,fullPath,FULL_PATH_LENGTH );
					DEBUG('f', "Creating Folder %s, fullPath %s\n", newDir->DirName,newDir->FullPath);
					newDir->ParentDir = dy;
					dy->FolderList.push_back(newDir);
					return true;
				}
				else
					return false;
			}
			else{
				if( itd == dy->FolderList.end() )
					return false;
				else
					dy = (*itd);
			}
		}
		else return false;
	}
	return false;
}
