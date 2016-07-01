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
#include <sys/msg.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <limits.h>
#include <string.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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

int socketFD;

int id;

int flagWrite=0;

int flagRead=0;

pthread_t tid;

pthread_mutex_t mutex;

int getFileSize(char filename[255]);
/*	usage'i ekrana basan fonksiyon */

void usage(char *x){ 
	fprintf(stderr, "usage: %s\n", x);
	exit(-1);
}
/* error bilgisini ekrana basan fonksiyon */
void err_sys(const char *x){ 
	perror(x);
	exit(-1);
}
void menu(){
	fprintf(stderr,"> listLocal\t-> list the local files in the directory.\n");
	fprintf(stderr,"> listServer\t-> list the files in the current scope of the server-client.\n");
	fprintf(stderr,"> lsClients\t-> lists the clients currently connected to the server.\n");
	fprintf(stderr,"> sendFile <filename> <clientid>\t-> send the file <filename> from local directory to the client.\n");
}
static void signalHandler(int signo){
	request_t r;
	sprintf(r.command, "%s", "kill-me");
	r.clientID=id;
	int i=0;
	if(signo == SIGINT){
		while(flagWrite==1 || flagRead==1){
			if(i==0)
				printf("Waiting end of the file transfer process for program termination.\n");
			++i;
		}
		if(write(socketFD, &r, sizeof(r))<0)
			err_sys("write");
		close(socketFD);
	//	pthread_join(tid, NULL);
		fprintf(stderr,"\nProgram terminated.\n");
		exit(0);
	}
}
void *readFromServer(void *flag);

int callSocket(char *hostname, unsigned short portnum);

void printIPAddress(struct ifaddrs *ifa);

int main(int argc, char *argv[]){
	int 	listenfd,
			i=0, 
			iPort,
			fd,
			err,
			status;
	char 	client[MAX_CANON],
			command[MAX_CANON],
			ip[IP_MAX],
			*saveptr,
			*sPort,
			*sIp,
			a,
			*c;
	struct 	dirent *direntp;
	DIR 	*dirp;
	struct	sigaction sigIntHandler;
	struct ifaddrs *myaddrs, *ifa;
	request_t request, request2;
	flag_t 	flag;
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);

	sigIntHandler.sa_handler = signalHandler;

	if(argc != 2)
		usage("2 arguman girmelisiniz\t./client <server IP>:<server port number>");

	else{
		if (sigemptyset(&sigIntHandler.sa_mask) == -1 || sigaction(SIGINT, &sigIntHandler, NULL) || sigaction(SIGUSR1, &sigIntHandler, NULL))
			err_sys("signal handler");
		sigIntHandler.sa_flags = 0;
		saveptr=command;
		sIp=strtok_r(argv[1], ":", &saveptr);
		sPort=strtok_r(NULL, ":", &saveptr);
		printf("%s\n", argv[1]);
		if(sPort!=NULL)
			iPort=atoi(sPort);
		else
			usage("./client <server IP>:<server port number>");

		socketFD = callSocket(sIp, iPort);
		if(socketFD==-1)
			err_sys("connection error");

		getcwd(request2.path, PATH_MAX);

		if(write(socketFD, &request2, sizeof(request2))<0)
			err_sys("write");

		if(read(socketFD, &id, sizeof(int))<0)
			err_sys("read");

		pthread_mutex_init(&mutex, NULL);
		if (err = pthread_create(&tid, NULL, (void *)readFromServer, &c)<0)
			perror("pthread_create");

		status = getifaddrs(&myaddrs);
		if (status != 0)
			err_sys("getifaddrs failed!");

		freeifaddrs(myaddrs);

		system("clear");
		printf("Port Number: %d ",iPort);
		for (ifa = myaddrs; ifa != NULL; ifa = ifa->ifa_next)
			if ((ifa->ifa_flags & IFF_UP) != 0 && NULL != ifa->ifa_addr)
				printIPAddress(ifa);
	
		fprintf(stderr, "\nPlease enter a command.\nEnter 'help' for display the available commands and their usage.\n");

		while(1){
			printf("\nClient ID: %d\n  >\t", id);

		 	fgets(command, 50, stdin);
		 	printf("\n");
		 	command[strlen(command)-1]='\0';

		 	if(strcmp(command, "help") == 0)
		 		menu();
		 	else if(strcmp(command,"listLocal") == 0){
		 		char cwd[255];
		 		getcwd(cwd, 255);
				if((dirp = opendir(cwd)) == NULL)
					err_sys("opendir");

				printf("List of local files in %s\n", cwd);
				while((direntp = readdir(dirp)) != NULL) {
					if(strcmp(direntp->d_name,".") != 0 && strcmp(direntp->d_name,"..") != 0){
						fprintf(stderr, "%s\n", direntp->d_name);
					}
				}
		 	}

		 	else if(strcmp(command,"lsClients") == 0){
				sprintf(request.command, "%s", "lsClients");
				if(write(socketFD, &request, sizeof(request))<0)
					perror("write");
		 	}

		 	else if(strncmp(command,"sendFile", 8) == 0){
		 		bool status=true;
				char *fileName, *id1, *saveptr;
				saveptr=command;
				strtok_r(command, " ", &saveptr);

				fileName = strtok_r(NULL, " " , &saveptr);
				id1 = strtok_r(NULL, " ", &saveptr);

					if(fileName==NULL){
						fprintf(stderr, "Please enter a current filename.\n");
						status=false;
					}
					else{
						sprintf(request.file.name, "%s", fileName);
						if((fd = open(fileName, O_RDONLY))<0){
							fprintf(stderr, "%s isn't a current file name. Try again!\n", fileName);
							request.status=-3;
							status=false;
						}
					}
					if(id1==NULL) {	/*dosyayi sadece server'a yolla. */
						request.status=-1;
						request.clientID=id;
						printf("nULL\n");
						if((fd = open(fileName, O_RDONLY))<0){
							fprintf(stderr, "%s isn't a current file name. Try again!\n", fileName);
							status=false;
							printf("Status: %d\n", status);
						}
					}
					else{
						request.status=1;
						request.clientID=atoi(id1);
					}

					struct stat st;
					stat(fileName, &st);
					request.file.size = st.st_size;
							
					strcpy(request.command,"sendFile");
					if(status){
						flagWrite=1;
						pthread_sigmask(SIG_BLOCK, &set, NULL);
						if(write(socketFD, &request, sizeof(request))<0)
							err_sys("write");
						else{
							int j=0;
							while(j<request.file.size){
								if(read(fd, &a, sizeof(char))<0)
									perror("read1");
								write(socketFD, &a, sizeof(char));
								++j;
							}
							close(fd);
						flagWrite=0;
						pthread_sigmask(SIG_UNBLOCK, &set, NULL);
					}
				}
			}
			else if(strcmp(command,"listServer") == 0){
				sprintf(request.command, "%s", "listServer");
				if(write(socketFD, &request, sizeof(request))<0)
					perror("write");
			}
			
			else
				fprintf(stderr, "Please enter a current command.\n");
		}
	}
}
void *readFromServer(void *flag){
	int i=0, fd;
	bool status=false;

	flag_t *flags;
	flags=(flag_t*)flag;
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, SIGINT);

	while(1){
    	pthread_mutex_lock(&mutex);
		if(read(socketFD, flags, sizeof(flag_t))<0)
			err_sys("read");

		if(flags->status==LSCLIENTS){
			if(read(socketFD, &allClients, sizeof(allClients))<0)
				perror("read");
			fprintf(stderr, "\n");
			for(i=0; i<MAX_NUMBER_OF_CLIENTS; ++i){
				if(allClients[i].status)
					printf("Client ID: %d, IP: %s\nPath: %s\n\n", allClients[i].id, allClients[i].IP, allClients[i].cwd);
			}
			fprintf(stderr, "\n  >\t");
			i=0;
		}
		else if(flags->status==SENDFILE){
			char a;
			fprintf(stderr, "\n");
			pthread_sigmask(SIG_BLOCK, &set, NULL);
			flagRead=1;
			if(i==0){
				if((fd = open(flags->file.name, O_RDWR|O_CREAT|O_TRUNC, 0777)) <0)
					err_sys("open");
			}
			while(i<flags->file.size){
				if(read(socketFD, &a, sizeof(char))<0)
					err_sys("read");
				++i;
				if(write(fd, &a, sizeof(char))<0)
					err_sys("write");
			}
			close(fd);
			flagRead=0;
			pthread_sigmask(SIG_UNBLOCK, &set, NULL);
			i=0;
		}
		else if(flags->status==LISTSERVER){
			char c;
			fprintf(stderr, "\n");
			while(1){
				if(read(socketFD, &c, sizeof(char))<0)
					err_sys("read");
			}
		}
		else if(flags->status==DEACTIVECLIENT){
			fprintf(stderr, "Client ID is wrong!\n\n  >\t");
		}
		else if(flags->status==KILLME){
			fprintf(stderr, "Server CTRL-C\n");
			exit(1);
		}
   		pthread_mutex_unlock(&mutex);
	}
    close(socketFD);
}
int getFileSize(char filename[255]){
	struct stat st;
	stat(filename, &st);
	return (int)st.st_size;
}
/* dersin slaytlarindan aldim. */
int callSocket(char *hostname, unsigned short portnum){
	struct sockaddr_in sa;
	struct hostent *hp;
	int a, s;
	
	if (( hp= gethostbyname(hostname)) == NULL)
		err_sys("gethostbyname");

	memset(&sa,0,sizeof(sa));
	sa.sin_family= AF_INET;
	sa.sin_port= htons((u_short)portnum);
	inet_pton(AF_INET, hostname, &sa.sin_addr.s_addr);
	
	if ((s = socket(AF_INET,SOCK_STREAM,0)) < 0)
		return -1;

	if (connect(s, (struct sockaddr *)&sa,sizeof sa) < 0){
		close(s);
		return -1;
	}
	return(s);
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