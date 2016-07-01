/******************************

	Yasin Acikgoz 
	131044070

	CSE244 System Programming

		Final
		fileShareServer.c

*******************************/
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/msg.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <limits.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <string.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <net/if.h>


/*boolean type*/
typedef int bool;
#define true 1
#define false 0

#ifndef PATH_MAX
	#define PATH_MAX 255
#endif
#ifndef MAX_CANON
	#define MAX_CANON 255
#endif
#define MAX_NUMBER_OF_CLIENTS 10
#define IP_MAX 20
#define MAX_COMMAND 50
#define KILLME -1
#define SENDFILE -2
#define LSCLIENTS -3
#define LISTSERVER -4
#define SENDYOURSELF -5
#define DEACTIVECLIENT -6

typedef struct {
	int size;
	char name[30];
}file_t;

typedef struct {
	char command[MAX_COMMAND];
	char path[PATH_MAX];
	int clientID;
	file_t file;
	int status;
}request_t;

typedef struct 
{
	bool status;
	int id;
	char IP[IP_MAX];
	int socketFD;
	char cwd[PATH_MAX];
	pthread_mutex_t mutex;
	pthread_t tid;
}client_t;

typedef struct 
{
	int status;
	char c;
	file_t file;
}flag_t;

client_t allClients[MAX_NUMBER_OF_CLIENTS];

int doneflag=0;

struct timeval start;

char ip[IP_MAX];

/* error bilgisini ekrana basan fonksiyon */
void err_sys(const char *x){ 
	perror(x);
	exit(-1);
}

static void ctrl_c_handler(int signo){
	int k=0, i=0;

	flag_t flag;
	flag.status=KILLME;
	if(signo==SIGINT){

		while(doneflag!=0){
			if(i==0)
				printf("Waiting end of the process for program termination.\n");
			++i;
		}
		for(k=0; k<MAX_NUMBER_OF_CLIENTS; ++k){
			if(allClients[k].status){
				if(pthread_mutex_lock(&allClients[k].mutex)<0)
					err_sys("Could not lock thread");
				if(write(allClients[k].socketFD, &flag, sizeof(flag))<0)
					err_sys("write");
				if(pthread_mutex_unlock(&allClients[k].mutex)<0)
					err_sys("Could not unlock thread");
				if(pthread_join(allClients[k].tid, NULL)<0)
					err_sys("Could not join thread");
				if(pthread_mutex_destroy(&allClients[k].mutex)<0){
					err_sys("Could not destroy mutex");
				}
			}
		}
		fprintf(stderr, "\nServer CTRL-C\nServer and active clients terminated.\n");
		exit(1);
	}
}

/*	usage'i ekrana basan fonksiyon */
void usage(char *x){ 
	fprintf(stderr, "usage: %s\n", x);
	exit(-1);
}
void *sendToClient(void *thread);

int getSocketFD(int id);

void fillClients();

bool controlClientID(int id);

int findFirstEmptyLocation();

bool removeClient(int id);

double getTimeDif(struct timeval start, struct timeval end);

int establish (unsigned short portnum);

void printIPAddress(struct ifaddrs *ifa);

int main(int argc, char *argv[]){
	char 	client[30];
	int 	listenfd,
			i=0,j=0, 
			err, 
			port,
			socketFD,
			counter=0,
			status, 
			temp=0;
	struct 	timeval  
			end;
	double 	timeDif;

	request_t request2;
	struct sigaction sigIntHandler;
	struct ifaddrs *myaddrs, *ifa;

	/*set time*/
	gettimeofday(&start, NULL);

	if(argc != 2)
		usage("2 parametre girmelisiniz\t./ fileShareServer  <port number>");
	else{
		/*set signal handler*/
		sigIntHandler.sa_handler = ctrl_c_handler;
		sigemptyset(&sigIntHandler.sa_mask);
		sigIntHandler.sa_flags = 0;
		sigaction(SIGINT, &sigIntHandler, NULL);

		port=atoi(argv[1]);

		system("clear");

		status = getifaddrs(&myaddrs);
		if (status != 0)
			err_sys("getifaddrs failed!");

		freeifaddrs(myaddrs);

		fprintf(stderr, "\nFile Share Server started...\nPort Number: %d ", port);
		for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
			if ((ifa->ifa_flags & IFF_UP) != 0 && NULL != ifa->ifa_addr)
				printIPAddress(ifa);
		

		if((listenfd = establish(port))<0)
			err_sys("socket");

		fillClients();
		/* accept allClients */
		while(1) {
			if((socketFD = accept(listenfd, NULL, NULL))<0)
				err_sys("accept");
			
			if(read(socketFD, &request2, sizeof(request2))<0)
				err_sys("read");

			allClients[i].socketFD=socketFD;
			sprintf(allClients[i].IP, "%s", ip);
			allClients[i].id=i;
			allClients[i].status=true;
			sprintf(allClients[i].cwd, "%s", request2.path);

			if(write(socketFD, &i, sizeof(int))<0)
				err_sys("read");
			

			gettimeofday(&end, NULL);
			fprintf(stderr,"Client ID: %d\tIP: %s\tConnection Time: %d ms.\n\n", allClients[i].id, allClients[i].IP, (int)getTimeDif(start, end));

			pthread_mutex_init(&allClients[i].mutex, NULL);

			if (err = pthread_create(&allClients[i].tid, NULL, (void *)sendToClient, &allClients[i])<0)
				err_sys("pthread_create");
			++i;
		}	
	close(port);
	}
}
void *sendToClient(void *thread){
	int 	k=0,
			fd;
	char 	a;
	DIR 	*dirp;
	struct 	dirent *direntp;
	struct 	timeval 
			start1, 
			end1;
	client_t *client;
	client = (client_t*)thread;
	flag_t flag;
	request_t request;

	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);

	while(read(client->socketFD, &request, sizeof(request))){
	//	printf("request id: %d\n", request.clientID);
		pthread_mutex_lock(&allClients[request.clientID].mutex);
		doneflag=1;
		pthread_sigmask(SIG_BLOCK, &set, NULL);
		if(strcmp(request.command, "lsClients") == 0){
			gettimeofday(&end1, NULL);
			fprintf(stderr,"\nClient %d request for lsClients operation: %d ms.\n", client->id, (int)getTimeDif(start, end1));
			flag.status=LSCLIENTS;
			if(write(client->socketFD, &flag, sizeof(flag))<0)
				err_sys("write");
			if(write(client->socketFD, &allClients, sizeof(allClients))<0)
				err_sys("write");
		}

	 	else if(strcmp(request.command,"kill-me") == 0){
			gettimeofday(&end1, NULL);
	 		allClients[request.clientID].status=false;
	 		fprintf(stderr, "Client %d disconnected: %d ms.\n", request.clientID, (int)getTimeDif(start, end1));
	 	}
		else if(strncmp(request.command,"sendFile", 8) == 0){
			bool sendToClient=false, sendToServer=false, sendYourself=false, sendToWrong=false;
			gettimeofday(&end1, NULL);
			gettimeofday(&start1, NULL);
		//	printf("request.clientID: %d, request.status: %d, size: %d, request.file.name: %s\n", request.clientID, request.status, request.file.size, request.file.name);
			if(request.clientID==client->id && request.status==1){
				fprintf(stderr, "A client cannot send a file to yourself: ");
				sendYourself=true;
			}
			else if(request.status==-1){
				fprintf(stderr,"\nClient %d request for sendFile operation to Server: ", client->id);
				sendToServer=true;
			}
			else if(controlClientID(request.clientID)) {
				fprintf(stderr,"\nClient %d request for sendFile operation to Client %d: ", client->id, request.clientID);
				sendToClient=true;
			}
			else{
				fprintf(stderr, "There isn't a client has Client ID '%d': ", request.clientID);
				flag.status=DEACTIVECLIENT;
				sendToWrong=true;
				if(write(client->socketFD, &flag, sizeof(flag))<0)
					err_sys("write");
			}
			fprintf(stderr, "%d ms.\n", (int)getTimeDif(start, end1));

			if((!sendYourself && !sendToWrong && (sendToServer || sendToClient))){
				bool status=true;
				if(sendToServer && request.file.size!=-1){
					if((fd = open(request.file.name, O_RDWR|O_CREAT|O_TRUNC, 0777)) <0){
						err_sys("open");	
					status=false;
					}
				}
				if(status){
					int socketFD = getSocketFD(request.clientID);
					flag.status=SENDFILE;
					sprintf(flag.file.name, "%s", request.file.name);
					flag.file.size=request.file.size;
					if(write(socketFD, &flag, sizeof(flag))<0)
						err_sys("write");

					while(k<request.file.size){
						if((read(client->socketFD, &a, sizeof(char)))<0)
							err_sys("read");
						
						if(sendToClient){
							if(write(socketFD, &a, sizeof(char))<0)
								err_sys("write");						
						}
						else if(sendToServer){
							if(write(fd, &a, sizeof(char))<0)
								err_sys("write");
						}
						++k;
					}
					close(fd);
					k=0;
				}
			}
		}
		else if(strcmp(request.command,"listServer") == 0){
			fprintf(stderr,"\nClient %d request for listServer operation: %d ms.\n", client->id, (int)getTimeDif(start, end1));
			char line[255];
			char cwd[255];
			int socketFD = getSocketFD(request.clientID);
			flag_t flag;
	 		getcwd(cwd, 255);
			if((dirp = opendir(cwd)) == NULL)
				err_sys("opendir");

			flag.status=LISTSERVER;
			if(write(client->socketFD, &flag, sizeof(flag))<0)
				err_sys("write");
			while((direntp = readdir(dirp)) != NULL) {
				if(strcmp(direntp->d_name,".") != 0 && strcmp(direntp->d_name,"..") != 0){
					sprintf(line, "%s\n", direntp->d_name);
					for(k=0; k<strlen(line); ++k){
						write(client->socketFD, &line[k], sizeof(char));
					}
				}
			}
			closedir(dirp);
			k=0;
		}
		pthread_sigmask(SIG_UNBLOCK, &set, NULL);
		if(request.clientID!=-2)
			pthread_mutex_unlock(&allClients[request.clientID].mutex);
		else
			pthread_mutex_lock(&allClients[client->socketFD].mutex);
		doneflag=0;
	}
}
bool controlClientID(int id){
	int k=0;
	bool status=false;
	for(k=0; k<MAX_NUMBER_OF_CLIENTS; ++k){
		if(allClients[k].id==id){
			status=true;
			break;
		}
	}
	return status;
}
void fillClients(){
	int k=0;
	for(k=0; k<MAX_NUMBER_OF_CLIENTS; ++k){
		allClients[k].status=false;
		allClients[k].id=-1;	
		allClients[k].socketFD=-1;
	}
}
int getSocketFD(int id){
	int k=0;
	for(k=0; k<MAX_NUMBER_OF_CLIENTS; ++k){
		if(allClients[k].status && allClients[k].id==id){
			return allClients[k].socketFD;
		}
	}
}
bool removeClient(int id){
	int k=0;
	for(k=0; k<MAX_NUMBER_OF_CLIENTS; ++k){
		if(allClients[k].id==id){
			allClients[k].status=false;
			allClients[k].id=-1;	
			allClients[k].socketFD=-1;
			sprintf(allClients[k].IP, "%c", '\0');
			sprintf(allClients[k].cwd, "%c", '\0');
			return true;
		}
	}
}
int findFirstEmptyLocation(){
	int k=0;
	for(k=0; k<MAX_NUMBER_OF_CLIENTS; ++k)
		if(allClients[k].status==false)
			return k;
}
double getTimeDif(struct timeval start, struct timeval end){
	double 	mtime;
	long 	seconds, 
			useconds;
	seconds  = end.tv_sec  - start.tv_sec;
	useconds = end.tv_usec - start.tv_usec;
	mtime = ((seconds*1000) + (useconds/1000.0)) + 0.5;
	return mtime;
}
int establish (unsigned short portnum){
	char myname[IP_MAX];
	int s;
	struct sockaddr_in serverAdress;
	struct hostent *hp;

	memset(&serverAdress, 0, sizeof(struct sockaddr_in));
	gethostname(myname,IP_MAX);

	serverAdress.sin_family= AF_INET;
	serverAdress.sin_port= htons(portnum);
	serverAdress.sin_addr.s_addr=htonl(INADDR_ANY);
 	
	if ((s= socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;
	if (bind(s,(struct sockaddr *)&serverAdress,sizeof(struct sockaddr_in)) < 0){
		close(s);
		return -1;
	}
	listen(s, 10);
	return s;
}
void printIPAddress( struct ifaddrs *ifa ){
	struct sockaddr_in *s4;
	/* ipv6 addresses have to fit in this buffer */
	char buf[64];

	if (AF_INET == ifa->ifa_addr->sa_family){
		s4 = (struct sockaddr_in *)(ifa->ifa_addr);
		if(NULL != inet_ntop(ifa->ifa_addr->sa_family, (void *)&(s4->sin_addr), buf, sizeof(buf)) && strcmp(ifa->ifa_name, "lo")!=0)
			printf("IP address: %s\n",buf);
	}
}