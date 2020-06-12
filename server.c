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
#define THREADSIZE  1000

int *ns[THREADSIZE], t_size;
int sd;
pthread_mutex_t m_lock;

void keycontrol(int sig);
void *recvGetRequest(void *arg);
void responseHTTP(int* sock, char* file, size_t* size, int type);
char* getClientInfo(char* file, char* cli_ip, int* port_num, int* type);
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

	printf("server directory = %s\nPort num = %d\n\n",dir,port_num);
	
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
	size_t size=0;

	while(str_len>=0){
		char* file_name=NULL;
		int type;
		if((str_len=recv(*sock, buffer, BUFSIZE, 0))==-1){
			//printf("No Receive");
			return (void*) NULL;
		}

		buffer[str_len]='\0';
		
		file_name = getClientInfo(buffer, cli_ip, &port_num, &type);
		openFile(file_name, &type);
		responseHTTP(sock, file_name, &size, type);
		saveLogfile(cli_ip, file_name, (int)size);
		close(*sock);
	}
}

void responseHTTP(int* sock, char* file, size_t* size, int type){
	FILE* fp=NULL;
	char content[BUFSIZE];
	char respHeader[BUFSIZE];
	char *dir;

	if(type==-1){	//if the page isn't exist
		strcpy(respHeader, "HTTP/1.1 404 Not Found\r\n\r\n"); 
		write(*sock, respHeader, strlen(respHeader));
		return;
	}
	else if(type==2){	//if link is total.cgi
		char *p = strtok(file, "?=&");
		int sum=0, s=0, f=0;
		while(p!=NULL){
			if(strcmp(p,"from")==0){
				p=strtok(NULL, "?=&");
				s=atoi(p);
			}else if(strcmp(p, "to")==0){
				p=strtok(NULL, "?=&");
				f=atoi(p);
			}
			p=strtok(NULL, "?=&");
		}
		for(int i=s;i<=f;i++)
			sum+=i;

		strcpy(respHeader, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"); 
		send(*sock, respHeader, strlen(respHeader),0);
		//Header Setting	
		
		sprintf(content, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\r\n<html>\r\n<head>\r\n<title>total.cgi</title>\r\n<meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\">\r\n</head>\r\n<body>\r\n<h1>sum of %d to %d = %d</h1>\r\n</body>\r\n</html>\r\n", s, f, sum);
		//Total.cgi html content 
		send(*sock, content, strlen(content), 0);
		return;
	}

	fp = fopen(file, "r");

	if(fp==NULL){	//if the file isn't exist
		perror("fp");
		return;
	}

	fseek(fp, 0, SEEK_END);
	*size = ftell(fp);
	printf("file size: %d\n",(int)*size);
	fseek(fp, 0, SEEK_SET);

	if(type==0){	//if file is .html
		strcpy(respHeader, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"); 
		puts(respHeader);
		write(*sock, respHeader, strlen(respHeader));
				
		while(fgets(content, BUFSIZE, fp)){
			//puts(content);
			send(*sock, content, strlen(content), 0);
		}
	}
	else if(type==1){	//if the file is image
		size_t fsize=0, nsize=0;
		strcpy(respHeader, "HTTP/1.1 200 OK\r\nContent-Type: image/webp; charset=utf-8\r\n\r\n"); 
		puts(respHeader);
		write(*sock, respHeader, strlen(respHeader));
		
		while(nsize != *size){
			memset(content, 0x00, sizeof(content));
			fsize = fread(content, 1, BUFSIZE-1, fp);
			nsize+= fsize;
			//puts(content);
			send(*sock, content, fsize, 0);
		}

	}
	fclose(fp);
}

char* openFile(char* file, int* type){
	//Open file and image
	
	FILE* fp = NULL;
	char content[BUFSIZE];
	char buffer[BUFSIZE];
	
	if(strncmp(file,"total.cgi",9)==0){
		//If the link is total.cgi -> type = 2
		*type=2;
		return NULL;
	}


	fp = fopen(file,"r");
	if(fp==NULL){	//if the file doesn't exist, open "index.html"
		fp=fopen("index.html","r");
		if(fp==NULL){	//if "index.html" doesn't exist, Not found
			printf("Not found");
			*type=-1;
			return NULL;
		}
		*type=0;
		strcpy(file, "index.html");
		return "index.html";
	}else
		*type=0;

	if(strncmp(file+strlen(file)-4, ".gif",4)==0
			||strncmp(file+strlen(file)-4, ".jpg",4)==0){
			*type=1;
	}	//image file
	
	fclose(fp);
	return NULL; 
}

char* getClientInfo(char* file, char* cli_ip, int* port_num, int* type){
	char HTTP[11][BUFSIZE];
	char *html_file;
	int index=0,j=0;
	
	/*
	 HTTP Header token
	 */
	char* ptr = strtok(file, "\n");
	while(ptr != NULL){
		if(index>7)break;
		strcpy(HTTP[index],ptr);
		printf("%s\n", HTTP[index]);
			
		ptr=strtok(NULL,"\n");
		index++;
	}

	char* fptr = strtok(HTTP[0], " ");
	if(strcmp(fptr,"GET")!=0) {
		return NULL;
	}
	fptr=strtok(NULL," ");
	fptr++;

	//Host Token(client IP, Port Number)
	char* p = strtok(HTTP[1]," :");
	while(p != NULL){
		if(j>2)break;
		if(j==1)strcpy(cli_ip,p);
		else if(j==2)*port_num=atoi(p);
		p=strtok(NULL," :");
		j++;
	}

	p = strtok(HTTP[3], " :,");
	p = strtok(NULL, " :,");
	//printf("Accept: %s\n", p);
	if(strcmp(p, "image/webp")==0){
		//printf("This is image\n\n");
		*type = 1;
	}
	else if(strcmp(p, "text/html")==0){
		//printf("This is page\n\n");
		*type = 0;
	}

	return fptr;
}

void saveLogfile(char* cli_ip, char* file, int size){
	FILE* log_fp = fopen("log.txt", "a");

	if(log_fp==NULL){
		perror("log_fp");
		return;
	}

	fprintf(log_fp, "%s %s %d\n",cli_ip, file, size);
	printf("%s %s %d\n",cli_ip, file, size);

	fclose(log_fp);
	return;
}


