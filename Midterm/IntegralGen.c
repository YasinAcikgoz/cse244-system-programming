#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <matheval.h>
#include <math.h>

/*boolean type*/
typedef int bool;
#define true 1
#define false 0
//#define DEBUG
#define INPUT_MAX 3
#define PID_MAX 8 
#define SERVER_LOG "server.log"
#define DEBUG_RESULT
/*
*	error bilgisini ekrana basan fonksiyon
*/
void err_sys(const char *x){ 
	perror(x);
	exit(-1);
}
/*
*	usage'i ekrana basan fonksiyon
*/
void usage(char *x){ 
	fprintf(stderr, "usage: %s\n", x);
	exit(-1);
}
void prompt(char *x){
	fprintf(stderr, "%s", x);
}
char *change(char f[], int size);
/* text dosyalarini okuyan fonksiyon*/
bool getTextFiles(char f1[], char f2[], char f3[], char f4[], char f5[], char f6[]);
/* matematiksel islemi yapip sonucu client'a gonderen fonksiyon*/
void mathServer(double t0, double resolution, double timeInterval, char operation);

void generateLogFileForServer(int maxClient);

void generateLogFile(double sec, char pid[]);

int mainFifo;
char sMainFifo[10];

int resultFifo;
char sResultFifo[PID_MAX];

int numberOfClients=0;

int counter=0;

static pid_t mainPid;

char clientPids[1000][PID_MAX];

void *func1, *func2;

FILE *fLogFilePtr;

struct Client{
	char fi[INPUT_MAX];
	char fj[INPUT_MAX];
	double timeInt;
	char op;
	char pid[PID_MAX];
};
/* 
*	server icin ctrl-c signalini handle eden fonksiyon
*	aktif clientlari kill eder ve child process'leri oldurur
*/
static void ctrl_c_handler(int signo){
	int i=0;
	if(mainPid==getpid())
		prompt("\nCTRL-C'ye basildi...\nJunk dosya ve processler temizlendi.\nServer ve aktif client'lar kapatildi.\n");

	fLogFilePtr=fopen(SERVER_LOG, "a");
	for(i=1; i<counter+1; ++i){
		if(strcmp(clientPids[i], "free")!=0){
			kill(atoi(clientPids[i]), SIGUSR1);
		}
	}
	while(wait(NULL)>0);
	fprintf(fLogFilePtr, "\nCTRL-C'ye basildi...\nJunk dosya ve processler temizlendi.\nServer ve aktif client'lar kapatildi.\n");
	close(mainFifo);
	close(resultFifo);
	unlink(sMainFifo);
	unlink(sResultFifo);
	fclose(fLogFilePtr);
	evaluator_destroy (func1);
	evaluator_destroy (func2);
	exit(1); 
}

int main(int argc, char *argv[]){
	pid_t 	pid;
	int 	maxNumberOfClients;
	double	resolution,
			result = 0;
	char 	f1[BUFSIZ], 
			f2[BUFSIZ],
			f3[BUFSIZ],
			f4[BUFSIZ],
			f5[BUFSIZ],
			f6[BUFSIZ];
    struct 	timeval start, end;

    gettimeofday(&start, NULL);
	system("clear");
	mainPid=getpid();
	if(argc!=3)
		usage("3 parametre girmelisiniz.\n./IntegralGen -<resolution> -<max # of clients>");

	/* signal handler olustur*/
	struct sigaction sigIntHandler;

	/*ctrl-c handler*/
	sigIntHandler.sa_handler = ctrl_c_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

	resolution=atof(argv[1]);
	maxNumberOfClients=atoi(argv[2]);

	if(resolution <= 0 || maxNumberOfClients < 0)
		usage("Arguman olarak girilen degerler yanlis.");

	sprintf(sMainFifo, "%s",  "mainFifo");
	#ifdef DEBUG
	printf("Main Server PID: %d\n", getpid());
	#endif
	/*main fifoyu olustur ve ac*/
	if(mkfifo(sMainFifo, 0666)<0)
		err_sys("Server zaten aktif.");
	else
		generateLogFileForServer(maxNumberOfClients);
	mainFifo = open(sMainFifo, O_RDWR);
	if(mainFifo==-1)
		err_sys("mainFifo olusmadi.");
	else if(getTextFiles(f1,f2,f3,f4,f5,f6)){ /*text dosyalarini oku*/
		prompt("Input dosyalari okundu.\nServer acildi.\nClient(lar) bekleniyor.\n");
	}

	/*ctrl-c sinyali gelene kadar calisacak olan sonsuz dongu*/
	while(1){
		struct Client clientInfo;
	    double mtime, t0;  
		long seconds, useconds;
		int i = 0, j = 0, count = 0;
		bool status=false;
		/*	client'tan matematiksel isleme dair bilgileri oku */
		if(read(mainFifo, &clientInfo, sizeof(clientInfo))>0){
			gettimeofday(&end, NULL);	/* t0 hesapla */
			seconds  = end.tv_sec  - start.tv_sec;
			useconds = end.tv_usec - start.tv_usec;
			mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
			t0 = mtime/1000.0;
			++counter;
			sprintf(clientPids[counter], "%s", clientInfo.pid);
		}
		/* 	global client pid arrayine ayni pid'nin 2 defa yazilip yazilmadigini kontrol et.
			eger bu durum gecerliyse client sayisini azalt ve client pid'sine free yaz */
		for(i=1; i<counter+1; ++i){
			for(j=i; j<counter+1; ++j){
				if(strcmp(clientPids[i], clientPids[j])==0 && strcmp(clientPids[i], "free")!=0 && strcmp(clientPids[j], "free")!=0){
					++count;
					if(count>1){
						strcpy(clientPids[i], "free");
						--numberOfClients;
						status=true;
					}
				}
			}
			count=0;
		}
		/*	maksimum client sayisi toplam client sayisina esitse server'a client alma ve client'a sinyal yolla*/
		if(maxNumberOfClients==numberOfClients)
			kill(atoi(clientInfo.pid), SIGUSR2);
		else if(!status){	/*toplam client sayisini arttir*/
			++numberOfClients;
			sprintf(sResultFifo, "%s", clientInfo.pid);
			generateLogFile(t0, clientInfo.pid);
		}
		#ifdef DEBUG
		perror("read_mainFifo");			
		printf("pid: %s, fi: %s, fj: %s, timeInterval: %d\noperation: %c, resolution: %f, maxNumberOfClients: %d\n", clientInfo.pid, clientInfo.fi, clientInfo.fj, clientInfo.timeInt, clientInfo.op, resolution, maxNumberOfClients);
		printf("numberOfClients: %d\n", numberOfClients);
		#endif

		/*	aritmetik islemleri olustur	*/
		if(strcmp(clientInfo.fi, "f1")==0)
			func1 = evaluator_create(f1);
		else if(strcmp(clientInfo.fi, "f2")==0)
			func1 = evaluator_create(f2);
		else if(strcmp(clientInfo.fi, "f3")==0)
			func1 = evaluator_create(f3);
		else if(strcmp(clientInfo.fi, "f4")==0)
			func1 = evaluator_create(f4);
		else if(strcmp(clientInfo.fi, "f5")==0)
			func1 = evaluator_create(f5);
		else if(strcmp(clientInfo.fi, "f6")==0)
			func1 = evaluator_create(f6);
		if(strcmp(clientInfo.fj, "f1")==0)
			func2 = evaluator_create(f1);
		else if(strcmp(clientInfo.fj, "f2")==0)
			func2 = evaluator_create(f2);
		else if(strcmp(clientInfo.fj, "f3")==0)
			func2 = evaluator_create(f3);
		else if(strcmp(clientInfo.fj, "f4")==0)
			func2 = evaluator_create(f4);
		else if(strcmp(clientInfo.fj, "f5")==0)
			func2 = evaluator_create(f5);
		else if(strcmp(clientInfo.fj, "f6")==0)
			func2 = evaluator_create(f6);

		pid=fork();
		if(pid<0)
			err_sys("fork");
		else if(pid==0) //MathServer
			mathServer(t0, resolution, clientInfo.timeInt, clientInfo.op);
	}
	return 0;
}
char * change(char f[], int size){
	int i=0;
	for(i=0; i<size; ++i){
		if(f[i]=='t')
			f[i]='x';
		if(f[i]=='\n')
			f[i]='\0';
	}
	return f;
}
bool getTextFiles(char f1[], char f2[], char f3[], char f4[], char f5[], char f6[]){
	FILE *f1Ptr, *f2Ptr, *f3Ptr, *f4Ptr, *f5Ptr, *f6Ptr;
	bool status=false;
	/* input dosyalarini ac */
	f1Ptr=fopen("f1.txt", "r");
	f2Ptr=fopen("f2.txt", "r");
	f3Ptr=fopen("f3.txt", "r");
	f4Ptr=fopen("f4.txt", "r");
	f5Ptr=fopen("f5.txt", "r");
	f6Ptr=fopen("f6.txt", "r");
	/* eksik dosya varsa hata basip cik*/
	if(f1Ptr==NULL || f2Ptr==NULL || f3Ptr==NULL || f4Ptr==NULL || f5Ptr==NULL || f6Ptr==NULL){
		usage("Input dosyalarinda eksik var. Lutfen kontrol ediniz.");
		unlink(sMainFifo);
	}
	else{
		/*	inputlari olustur */
		fgets(f1, BUFSIZ, f1Ptr);
		fgets(f2, BUFSIZ, f2Ptr);
		fgets(f3, BUFSIZ, f3Ptr);			
		fgets(f4, BUFSIZ, f4Ptr);
		fgets(f5, BUFSIZ, f5Ptr);			
		fgets(f6, BUFSIZ, f6Ptr);
		#ifdef DEBUG
			prompt("Input dosyalarindaki matematiksel islemler\n");
			printf("__________________________________________\n\n");
			printf("f1.txt: %s\n", f1);
			printf("f2.txt: %s\n", f2);
			printf("f3.txt: %s\n", f3);
			printf("f4.txt: %s\n", f4);
			printf("f5.txt: %s\n", f5);
			printf("f6.txt: %s\n", f6);
			printf("__________________________________________\n\n");
		#endif
		/*islemlerin icindeki t degiskenlerini x'e cevir.*/
		change(f1, strlen(f1));
		change(f2, strlen(f2));
		change(f3, strlen(f3));
		change(f4, strlen(f4));
		change(f5, strlen(f5));
		change(f6, strlen(f6));
		/* dosyalari kapat */
		fclose(f1Ptr);
		fclose(f2Ptr);
		fclose(f3Ptr);
		fclose(f4Ptr);
		fclose(f5Ptr);
		fclose(f6Ptr);
		status=true;
	}
	return status;
}
void mathServer(double t0, double resolution, double timeInterval, char operation){
	double i=0, first=0, second=0, result=0, tempTimeInterval=0;

	for(i=t0; ; i += resolution){
		/* fonksiyonun sonucunu hesapla client'tan 
		gelen aritmetik isleme gore sonuca ata	*/
		first=evaluator_evaluate_x(func1, t0);
		second=evaluator_evaluate_x(func2, t0);
		if(operation=='+')
			result+=(first+second)*resolution;
		else if(operation=='-')
			result+=(first-second)*resolution;
		else if(operation=='/')
			result+=(first/second)*resolution;
		else if(operation=='x')
			result+=(first*second)*resolution;
		if(i>=tempTimeInterval+t0){
			resultFifo = open(sResultFifo, O_RDWR);
			if(resultFifo==-1){
				close(resultFifo);
				unlink(sResultFifo);
				exit(1);
			}
			else{
				write(resultFifo, &result, sizeof(double));
				#ifdef DEBUG
				perror("write_resultFifo");
				#endif
				close(resultFifo);
			}
			tempTimeInterval+=timeInterval;
		}
	}
}
void generateLogFile(double sec, char pid[]){
	char sBuffer[BUFSIZ];
	time_t timer;
    struct tm* tm_info;
    time(&timer);
    tm_info = localtime(&timer);
    strftime(sBuffer, 26, "%d.%m.%Y, saat %H:%M:%S", tm_info);
	fLogFilePtr=fopen(SERVER_LOG, "a");
	fprintf(fLogFilePtr, "\n________________________________________________________\n");
	fprintf(fLogFilePtr, "PID: %s\n", pid);
	fprintf(fLogFilePtr, "Connection Time: %s\n", sBuffer);
	fprintf(fLogFilePtr, "Connection Second: %f\n", sec);

	fclose(fLogFilePtr);
}
void generateLogFileForServer(int maxClient){
	char sBuffer[BUFSIZ];
	time_t timer;
    struct tm* tm_info;
    time(&timer);
    tm_info = localtime(&timer);
    strftime(sBuffer, 26, "%d.%m.%Y, saat %H:%M:%S", tm_info);
	fLogFilePtr=fopen(SERVER_LOG, "a");
	fprintf(fLogFilePtr, "\t\tOpening Time: %s\n", sBuffer);
	fprintf(fLogFilePtr, "\t\tNumber Of Maximum Client: %d\n", maxClient);
	fprintf(fLogFilePtr, "________________________________________________________\n\n");

	fclose(fLogFilePtr);
}