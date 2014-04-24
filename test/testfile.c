#include "syscall.h"

int main(){
	OpenFileId myFile;
	char buffer[12];
	myFile = 0;
	Create("file.txt");
	
	myFile = Open("file.txt");
	if( myFile != 0){
		Write("File Write\n",11,myFile);
		Close(myFile);
		myFile=Open("file.txt");
		if(myFile != 0){
			Read( buffer,11,myFile );
			Close(myFile);
			Write(buffer,11,1);
			Write("FileOpenSuccess\n",16,1);
		}
	}
}
