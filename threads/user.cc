#include "User.h"
#include "utility.h"
User::User(){
	UserName = NULL;
	UserID = -1;
	HomeDirectory = NULL;
	Rights = 0;
	Password = NULL;
	
}

User::User(char* name, int id, char* dir, int r, int passwd){
	UserName = name;
	UserID = id;
	HomeDirectory = dir;
	Rights = r;
	Password = passwd;
}

User::~User(){
}