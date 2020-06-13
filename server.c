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
#define THREADSIZE  1000

int *ns[THREADSIZE], t_size;
int sd;
char* dir, *svc;
pthread_mutex_t m_lock;

void keycontrol(int sig);
void *recvGetRequest(void *arg);
void responseHTTP(int* sock, char* file, size_t* size, int type);
char* getClientInfo(char* file, char* cli_ip, int* port_num, int* type);
int openFile(char* file);
void saveLogfile(char* cli_ip, char* file, int size);

int main(int argc, char *argv[]){
	int n,port_num;						//Port number
	pthread_t tid[THREADSIZE], recv;	//Thread for multiple client
	struct sockaddr_in cli, sin;		//Web socket
	FILE* log_fp;						//"log.txt" file
	int clientlen=sizeof(cli);			//Client length

	if(argc<3){
		// argument error
		printf("No sufficient argument\n");
		exit(1);
	}
	
	dir = (char*)malloc(sizeof(char)*BUFSIZE);	//current directory(server)
	svc = (char*)malloc(sizeof(char)*BUFSIZE);	//service directory(argv[1])
	dir = getcwd(NULL, BUFSIZ);
	int i;
	/*
	for(i=0;argv[1][i+1]!='\0';i++)
		argv[1][i]=argv[1][i+1];
	argv[1][i]='\0';
	*/
	chdir(argv[1]);				//move to service dir
	svc = getcwd(NULL, BUFSIZE);
	printf("Service Directory = %s\n", svc);
	port_num = atoi(argv[2]);		//assign port number
	//save server directory and port num

	log_fp = fopen("log.txt", "w");
	if(log_fp==NULL)
		perror("log_fp");
	fclose(log_fp);
	//initialize existing content by opening "log.txt" with "w" option

	printf("server directory = %s\nPort num = %d\n\n",dir,port_num);
	
	if((sd = socket(AF_INET, SOCK_STREAM, 0))==-1){
		perror("socket");
		exit(1);
	}	//socket create
	memset((char*)&sin, '\0', sizeof(sin));
	signal(SIGINT, keycontrol);

	sin.sin_family=AF_INET;
	sin.sin_port=htons(port_num);
	sin.sin_addr.s_addr=inet_addr("0.0.0.0");
	//assign socket information

	if(bind(sd,(struct sockaddr*)&sin, sizeof(sin))==-1){
		perror("bind");
		exit(1);
	}	//binding socket

	if(listen(sd,5)==-1){	//wait client connect
		perror("listen");
		exit(1);
	}

	if(pthread_mutex_init(&m_lock, NULL)!=0){
		perror("Mutex Init Failure");
		exit(1);
	}	//mutex initilize for multiple client

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
		//Call function with creating a thread
		t_size++;
	}

	
	for(int i=0;i<t_size;i++)
		close(*ns[t_size]);
	close(sd);
	fclose(log_fp);
	//Close sockets and files
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
	//To handle when a new access request is received on the server
	int *sock = (int*)arg;
	char buffer[BUFSIZE];
	char cli_ip[20];
	int str_len = 0,index=0, port_num=0;
	size_t size=0;

	while(str_len>=0){
		char* file_name=NULL;
		int type=-1;
		if((str_len=recv(*sock, buffer, BUFSIZE, 0))==-1)
			return (void*) NULL;
		buffer[str_len]='\0';
		//Save client request in buffer
		
		file_name = getClientInfo(buffer, cli_ip, &port_num, &type);
		type = openFile(file_name);
		responseHTTP(sock, file_name, &size, type);
		saveLogfile(cli_ip, file_name, (int)size);
		close(*sock);
	}
}

void responseHTTP(int* sock, char* file, size_t* size, int type){
	//Responds http depending on type
	FILE* fp=NULL;
	char content[BUFSIZE];
	char respHeader[BUFSIZE];
	char *dir;
	printf("\ntype: %d\n",type);

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
		}	//Tokenize each number from and to behind
		for(int i=s;i<=f;i++)
			sum+=i;
		//"from" to "to" and stored in sum

		strcpy(respHeader, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"); 
		send(*sock, respHeader, strlen(respHeader),0);
		//Header Setting	
		
		sprintf(content, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\r\n<html>\r\n<head>\r\n<title>total.cgi</title>\r\n<meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\">\r\n</head>\r\n<body>\r\n<h1>sum of %d to %d = %d</h1>\r\n</body>\r\n</html>\r\n", s, f, sum);
		//Total.cgi html content 
		send(*sock, content, strlen(content), 0);
		//request content
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
	//calculate file size

	if(type==0){	//if file type is .html
		strcpy(respHeader, "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"); 
		puts(respHeader);
		write(*sock, respHeader, strlen(respHeader));
				
		while(fgets(content, BUFSIZE, fp)){
			//puts(content);
			send(*sock, content, strlen(content), 0);
		}	//Read the contents of the html file one by one and respond 
	}
	else if(type==1){	//if the file is image
		size_t fsize=0, nsize=0;
		strcpy(respHeader, "HTTP/1.1 200 OK\r\nContent-Type: image/webp; charset=utf-8\r\n\r\n"); 
		puts(respHeader);
		write(*sock, respHeader, strlen(respHeader));
		//Setting header to image type
		
		while(nsize != *size){
			memset(content, 0x00, sizeof(content));
			fsize = fread(content, 1, BUFSIZE-1, fp);
			nsize+= fsize;
			//puts(content);
			send(*sock, content, fsize, 0);
		}	//Respond by reading the binary file as much as BUFSIZE

	}else if(type==3){		//if the file is compressed
		size_t fsize=0, nsize=0;
		sprintf(respHeader, "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: %d\r\nContent-Type: text/html; charset=utf-8\r\n\r\n", (int)*size); 
		puts(respHeader);
		write(*sock, respHeader, strlen(respHeader));
		//In the header, state that the file is compressed in gzip format
		
		while(nsize != *size){
			memset(content, 0x00, sizeof(content));
			fsize = fread(content, 1, BUFSIZE-1, fp);
			nsize+= fsize;
			//puts(content);
			send(*sock, content, fsize, 0);
		}	//Respond by reading the binary file as much as BUFSIZE

	}
	fclose(fp);
}

int openFile(char* file){
	//Open file and image
	//Read the path of the file to find the type of path
	
	FILE* fp = NULL, *gz=NULL;
	char content[BUFSIZE];
	char gz_file[BUFSIZE];
	char buffer[BUFSIZE];
	int type=-1;
	
	if(strncmp(file,"total.cgi",9)==0){
		//If the link is total.cgi -> type = 2
		return 2;
	}

	strcpy(gz_file, file);
	strcat(gz_file, ".gz");
	gz = fopen(gz_file, "r");
	if(gz!=NULL){
		strcpy(file, gz_file);		
		fclose(gz);
		return 3;
	}

	fp = fopen(file,"r");
	if(fp==NULL){	//if the file doesn't exist, open "index.html"
		fp=fopen("index.html","r");
		if(fp==NULL){	//if "index.html" doesn't exist, Not found
			printf("Not found");
			return -1;
		}
		strcpy(file, "index.html");	//overwrite non-existent files with "index.html"
		return 0;
	}else
		type = 0;	//type 0 is about html file

	if(strncmp(file+strlen(file)-4, ".gif",4)==0
			||strncmp(file+strlen(file)-4, ".jpg",4)==0){
			type=1;
	}	//type 1 is about image file
	
	fclose(fp);
	return type; 
}

char* getClientInfo(char* file, char* cli_ip, int* port_num, int* type){
	//After receiving http request, obtain connection ip, port number and connection type
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
	//Get the connection URL behind "GET"

	char* p = strtok(HTTP[1]," :");
	while(p != NULL){
		if(j>2)break;
		if(j==1)strcpy(cli_ip,p);
		else if(j==2)*port_num=atoi(p);
		p=strtok(NULL," :");
		j++;
	}
	//Get the connection IP and port number with "Host"

	p = strtok(HTTP[3], " :,");
	p = strtok(NULL, " :,");
	//printf("Accept: %s\n", p);
	if(strcmp(p, "image/webp")==0)
		*type = 1;
	else if(strcmp(p, "text/html")==0)
		*type = 0;
	//Extract the string behind the accept to distinguish whether the request page is html or image file


	return fptr;
}

void saveLogfile(char* cli_ip, char* file, int size){
	//Storing server connection logs
	FILE* log_fp = NULL;
	char buffer[BUFSIZE];

	chdir(dir);			// move to the directory where the server program is located
	log_fp = fopen("log.txt", "a");
	if(log_fp==NULL){
		perror("log_fp");
		return;
	}
	//open the "log.txt" files with "a" option(additional)

	sprintf(buffer, "%s %s %d\n", cli_ip, file, size);
	fputs(buffer, log_fp);
	//write a log file consisting of client ip, file name and file size

	chdir(svc);
	//return to service directory

	fclose(log_fp);
	return;
}


