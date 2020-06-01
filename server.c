#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[]){
	int n;
	char* dir;
	int port_num;

	if(argc<3){
		//perror(argc);
		printf("No sufficient argument\n");
		return 1;
	}
	
	dir = (char*)malloc(sizeof(char)*strlen(argv[1]));
	strcpy(dir,argv[1]);
	port_num = atoi(argv[2]);
	//save server directory and port num
	
	printf("server directory = %s\nPort num = %d\n",dir,port_num);
	return 0;
}
