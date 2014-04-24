
#ifndef	USER_H
#define USER_H
#include "copyright.h"
class User{
public:
	char* UserName;
	int UserID;

private:
	char* HomeDirectory;
	int Rights;
	int Password;
public:
	User();
	User(char* name, int id, char* dir, int r, int passwd);
	~User();
};
#endif // USER_H

