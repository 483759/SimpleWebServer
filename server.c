#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE	   5000
#define NAMESIZE	20
#define THREADSIZE  10

int *ns[THREADSIZE], t_size;
int sd;
pthread_mutex_t m_lock;

void keycontrol(int sig);
void *recvGetRequest(void *arg);
void responseHTTP(int* sock, char* file, int type);
char* getClientInfo(char* file, char* cli_ip, int* port_num);
char* openFile(char* file, int* type);
void saveLogfile(char* cli_ip, char* file, int size);

int main(int argc, char *argv[]){
	int n,port_num;
	char* dir, *cur, buffer[BUFSIZE];
	pthread_t tid[THREADSIZE], recv;
	struct sockaddr_in cli, sin;
	FILE* log_fp;
	int clientlen=sizeof(cli);

	if(argc<3){
		//perror(argc);
		printf("No sufficient argument\n");
		exit(1);
	}
	
	dir = (char*)malloc(sizeof(char)*strlen(argv[1]));
	dir = getcwd(NULL, BUFSIZ);
	int i;
	for(i=0;argv[1][i+1]!='\0';i++)
		argv[1][i]=argv[1][i+1];
	argv[1][i]='\0';
	chdir(argv[1]);
	dir = getcwd(NULL, BUFSIZE);
	port_num = atoi(argv[2]);
	//save server directory and port num

	log_fp = fopen("log.txt", "w");
	if(log_fp==NULL)
		perror("log_fp");
	fclose(log_fp);

	printf("server directory = %s\nPort num = %d\n",dir,port_num);
	
	if((sd = socket(AF_INET, SOCK_STREAM, 0))==-1){
		perror("socket");
		exit(1);
	}
	memset((char*)&sin, '\0', sizeof(sin));
	signal(SIGINT, keycontrol);

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
	fclose(log_fp);
	return 0;
}

void keycontrol(int sig){
	if(sig==SIGINT){
		for(int i=0;i<t_size;i++)
			close(*ns[t_size]);
		close(sd);
		printf("Shutdown Server\n");
		exit(1);
	}

}

void *recvGetRequest(void *arg){
	int *sock = (int*)arg;
	char buffer[BUFSIZE];
	char cli_ip[20];
	int str_len = 0,index=0, port_num=0;

	while(str_len>=0){
		char* file_name=NULL;
		int type;
		if((str_len=recv(*sock, buffer, BUFSIZE, 0))==-1){

			printf("No Receive");
			return (void*) NULL;
		}

		buffer[str_len]='\0';

		file_name = getClientInfo(buffer, cli_ip, &port_num);
		openFile(file_name, &type);
		responseHTTP(sock, file_name, type);
		puts(file_name);
		fputs(buffer, stdout);
	}
}

void responseHTTP(int* sock, char* file, int type){
	FILE* fp=NULL;
	char content[BUFSIZE];
	char respHeader[BUFSIZE];
	char *dir;
	
	puts(file);
	puts(getcwd(NULL,BUFSIZE));
	fp = fopen(file, "r");

	if(fp==NULL){
		perror("fp");
		return;
	}
printf("type: %d\n",type);
	if(type==-1){
		strcpy(respHeader, "HTTP/1.1 404 Not Found\r\n"); 
		write(*sock, respHeader, strlen(respHeader));
	}
	else if(type==0){	//if file is .html
		puts(respHeader);
		strcpy(respHeader, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Security-Policy: default-src https:\r\n"); 
		puts(respHeader);
		write(*sock, respHeader, strlen(respHeader));
		while(fgets(content, BUFSIZE, fp)){
			//puts(content);
			write(*sock, content, strlen(content));
		}
	}
	else if(type==1){
		strcpy(respHeader, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"); 
		puts(respHeader);
		write(*sock, respHeader, strlen(respHeader));

	}

	fclose(fp);
}

char* openFile(char* file, int* type){
	FILE* fp = NULL;
	char content[BUFSIZE];
	char buffer[BUFSIZE];

	fp = fopen(file,"r");
	if(fp==NULL){
		fp=fopen("index.html","r");
		if(fp==NULL){
			printf("Not found");
			*type=-1;
			return NULL;
		}
		*type=0;
		return "index.html";
	}else
		*type=0;

	if(strncmp(file+strlen(file)-4, ".gif",4)==0){
			*type=1;
	}

	if(strncmp(file,"total.cgi",9)==0){
		//total cgi 	
	}

	
	fclose(fp);
	return NULL; 
}

char* getClientInfo(char* file, char* cli_ip, int* port_num){
	char HTTP[11][BUFSIZE];
	char *html_file;
	int index=0,j=0;
	
	char* ptr = strtok(file, "\n");
	while(ptr != NULL){
		if(index>7)break;
		strcpy(HTTP[index],ptr);
		printf("%s\n", HTTP[index]);
			
		ptr=strtok(NULL,"\n");
		index++;
	}

	char* fptr = strtok(HTTP[0], " ");
	fptr=strtok(NULL," ");
	fptr++;

	char* p = strtok(HTTP[1]," :");
	while(p != NULL){
		if(j>2)break;
		if(j==1)strcpy(cli_ip,p);
		else if(j==2)*port_num=atoi(p);
		p=strtok(NULL," :");
		j++;
	}

	saveLogfile(cli_ip, "index.html", 0);
	printf("client ip is : %s, port number is :%d\n\n",cli_ip, *port_num);
	return fptr;
}

void saveLogfile(char* cli_ip, char* file, int size){
	FILE* log_fp = fopen("log.txt", "a");

	if(log_fp==NULL){
		perror("log_fp");
		return;
	}

	fprintf(log_fp, "%s %s %d\n",cli_ip, file, size);

	fclose(log_fp);
	return;
}


