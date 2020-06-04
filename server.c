#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE	   5000
#define NAMESIZE	20
#define THREADSIZE  10

int *ns[THREADSIZE], t_size;
pthread_mutex_t m_lock;

void *recvGetRequest(void *arg);

int main(int argc, char *argv[]){
	int n,port_num, sd;
	char* dir, buffer[BUFSIZE];
	FILE *serfp;
	pthread_t tid[THREADSIZE], recv;
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

	if(pthread_mutex_init(&m_lock, NULL)!=0){
		perror("Mutex Init Failure");
		exit(1);
	}

	while(1){	//server process
		ns[t_size]=(int*)malloc(sizeof(int));
		int		str_len=0;

		pthread_mutex_lock(&m_lock);
		if((*ns[t_size] = accept(sd,(struct sockaddr *)&cli, &clientlen))==-1){
			//save newly connected client socket
			perror("accept");	//connection fail
			exit(1);
		}
		pthread_mutex_unlock(&m_lock);

		pthread_create(&recv, NULL, recvGetRequest, (void*) ns[t_size]);

		t_size++;
	}

	
	for(int i=0;i<t_size;i++)
		close(*ns[t_size]);
	close(sd);
	fclose(serfp);
	return 0;
}

void *recvGetRequest(void *arg){
	int *sock = (int*)arg;
	char buffer[BUFSIZE];
	int str_len = 0;

	while(str_len>=0){
		if((str_len=recv(*sock, buffer, BUFSIZE, 0))==-1){

			printf("No Receive");
			return (void*) NULL;
		}

		buffer[str_len]='\0';
		fputs(buffer, stdout);
	}
}
