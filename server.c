#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]){
	int n,port_num, sd;
	char* dir;
	FILE *serfp;
	struct sockaddr_in cli, sin;
	int clientlen=sizeof(cli);

	if(argc<3){
		//perror(argc);
		printf("No sufficient argument\n");
		exit(1);
	}

	serfp = fopen("log.txt","w");
	//open server log file
	
	dir = (char*)malloc(sizeof(char)*strlen(argv[1]));
	strcpy(dir,argv[1]);
	port_num = atoi(argv[2]);
	//save server directory and port num

	printf("server directory = %s\nPort num = %d\n",dir,port_num);
	
	if((sd = socket(AF_INET, SOCK_STREAM, 0))==-1){
		perror("socket");
		exit(1);
	}
	memset((char*)&sin, '\0', sizeof(sin));

	sin.sin_family=AF_INET;
	sin.sin_port=htons(port_num);
	sin.sin_addr.s_addr=inet_addr("0.0.0.0");

	if(bind(sd,(struct sockaddr*)&sin, sizeof(sin))==-1){
		perror("bind");
		exit(1);
	}

	if(listen(sd,5)==-1){	//wait client connect
		perror("listen");
		exit(1);
	}

	while(1){	//server process
	
	}

	
	fclose(serfp);
	return 0;
}
