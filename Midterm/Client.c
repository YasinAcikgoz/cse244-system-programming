#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

/*boolean type*/
typedef int bool;
#define true 1
#define false 0

//#define DEBUG
#define INPUT_MAX 3
#define PID_MAX 7
#define ALARM_TIME 15
#define SERVER_LOG "server.log"
#define DEBUG_RESULT
/*function prototypes*/

/* error bilgisini ekrana basan fonksiyon */
void err_sys(const char *x){ 
	perror(x);
	exit(-1);
}
/*	usage'i ekrana basan fonksiyon */
void usage(char *x){ 
	fprintf(stderr, "usage: %s\n", x);
	exit(-1);
}
/* prompter*/
void prompt(char *x){
	fprintf(stderr, "%s", x);
}

void generateLogFile(char fi[], char fj[], char op);

bool controlFunctionParams(char *f);


bool controlOperation(char *f);

/* global degiskenler */
int mainFifo;
char sMainFifo[10];

int resultFifo;
char sResultFifo[PID_MAX];

char logPath[BUFSIZ];

char pid[PID_MAX];

FILE *fLogFilePtr;

FILE *fServerLogFilePtr;

struct Client{
	char fi[INPUT_MAX];
	char fj[INPUT_MAX];
	double timeInt;
	char op;
	char pid[PID_MAX];
};

static void signalHandler(int signo){
	struct Client clientInfo;
	sprintf(logPath, "%d.log", getpid());
	fLogFilePtr=fopen(logPath, "a");
	fServerLogFilePtr=fopen(SERVER_LOG, "a");

	/*	gelen sinyale gore log dosyasinin sonuna
		sinyali bas ve programi sonlandir. */
	if(signo == SIGINT){
   		mainFifo = open(sMainFifo, O_RDWR);
   		sprintf(clientInfo.pid, "%s", pid);
   		if(mainFifo>0)
   			write(mainFifo, &clientInfo, sizeof(clientInfo));
   		else
			err_sys("Server aktif degil. Client islem yapamaz");
		prompt("\n\nCTRL-C'ye basildi...\nJunk dosya ve processler temizlendi.\nClient kapatildi.\n");
		fprintf(stderr, "Daha fazla detay icin %s dosyasina bakiniz.\n", logPath);
		fprintf(fLogFilePtr, "Client CTRL-C yaptigi icin program kapatildi.\n");
		fprintf(fServerLogFilePtr, "%d pid'li client CTRL-C ile kapatildi.", getpid());
		close(mainFifo);
	}else if(signo == SIGUSR1){
		prompt("\n\nServer tarafindan CTRL-C'ye basildi...\nJunk dosya ve processler temizlendi.\nClient kapatildi.\n");
		fprintf(stderr, "Daha fazla detay icin %s dosyasina bakiniz.\n", logPath);
		fprintf(fLogFilePtr, "Server CTRL-C yaptigi icin client kapatildi.\n");
		fprintf(fServerLogFilePtr, "%d pid'li client, server tarafindan CTRL-C ile kapatildi.", getpid());
	}else if(signo == SIGUSR2){
		prompt("\n\nMaksimum client sayisina ulasildi.\nDaha sonra tekrar deneyiniz.\n");
		fprintf(fLogFilePtr, "Server dolu oldugu icin client server'a baglanamadi.\n");
		fprintf(fServerLogFilePtr, "%d pid'li client server dolu oldugu icin baglanamadi", getpid());
	}else if(signo == SIGALRM){
		fprintf(fLogFilePtr, "\n\nServer CTRL-Z yapti\nClient %d saniye boyunca bekledigi icin kapandi.\n", ALARM_TIME);
		fprintf(stderr, "Server CTRL-Z yaptigi icin client kapatildi.\n");
		fprintf(fServerLogFilePtr, "%d pid'li client, server CTRL-Z yaptigi icin kapatildi.", getpid());
	}
	fprintf(fServerLogFilePtr, "\n________________________________________________________\n");
	fprintf(fLogFilePtr, "________________________________________________________\n");
	close(mainFifo);
	close(resultFifo);
	unlink(sResultFifo);
	fclose(fLogFilePtr);
	fclose(fServerLogFilePtr);
	exit(1);
}
int main(int argc, char *argv[]){
	int 	numberOfIteration=0;
	double	result=0,
			timeInterval;
	char	fi[INPUT_MAX], 
			fj[INPUT_MAX];
	struct 	Client clientInfo;
	struct 	sigaction sigIntHandler;  

	/*client icin ctrl-c signal handleri*/
	sigIntHandler.sa_handler = signalHandler;
	if (sigemptyset(&sigIntHandler.sa_mask) == -1 || sigaction(SIGINT, &sigIntHandler, NULL) || 
		sigaction(SIGUSR1, &sigIntHandler, NULL) || sigaction(SIGUSR2, &sigIntHandler, NULL) || sigaction(SIGALRM, &sigIntHandler, NULL))
		err_sys("Signal handler olusturma hatasi");
	sigIntHandler.sa_flags = 0;

	if(argc!=5)
		usage("5 parametre girmelisiniz.\t./Client -<fi> -<fj> -<time interval> -<operation>");
	sprintf(sMainFifo, "%s",  "mainFifo");
	sprintf(pid, "%d", getpid());

	timeInterval=atof(argv[3]);

	/*	gelen parametreleri kontrol et */
	if(controlOperation(argv[4]) && controlFunctionParams(argv[1]) && controlFunctionParams(argv[2]));

	sprintf(clientInfo.fi, "%s",argv[1]);
	sprintf(clientInfo.fj, "%s",argv[2]);

   	mainFifo = open(sMainFifo, O_RDWR);

	if(mainFifo==-1)
		err_sys("Server aktif degil. Client islem yapamaz");
	sprintf(sResultFifo, "%s", pid);
	if(mkfifo(sResultFifo, 0666)>0)
		err_sys("result fifo hatasi.");
	clientInfo.op=argv[4][0];
	clientInfo.timeInt=timeInterval;
	
	sprintf(clientInfo.pid, "%s", pid);

	#ifdef DEBUG
	fprintf(stderr, "fi: %s, fj: %s, timeInterval: %f, operation: %c\n", clientInfo.fi, clientInfo.fj, clientInfo.timeInt, clientInfo.op);
	#endif
	/* log dosyasini olustur */
	if(write(mainFifo, &clientInfo, sizeof(clientInfo))>0)
		generateLogFile(clientInfo.fi, clientInfo.fj, clientInfo.op);
	
	#ifdef DEBUG
	perror("write_mainFifo");
	#endif

	resultFifo = open(sResultFifo, O_RDWR);
	if(resultFifo==-1)
		err_sys("result fifo olusmadi");
	else 
		sprintf(logPath, "%s.log", pid);

	while(1){
	    struct timeval start, end;
	    double mtime, stime, min;    
		long seconds, useconds;
		fLogFilePtr=fopen(logPath, "a");
		/* server'in ctrl-z yapma ihtimaline karsilik alarm olustur */
		alarm(ALARM_TIME);
		++numberOfIteration;
		if(numberOfIteration==1)
			fprintf(fLogFilePtr, "No\t\tResult\t\t\tElapsed Time\n");
	    gettimeofday(&start, NULL);
		read(resultFifo, &result, sizeof(double));
		#ifdef DEBUG
		perror("read_resultFifo");
		#endif
		gettimeofday(&end, NULL);
		seconds  = end.tv_sec  - start.tv_sec;
		useconds = end.tv_usec - start.tv_usec;
		mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
		/* log dosyasina sonucu yaz */
		fprintf(fLogFilePtr, "%d\t\t%.4f\t\t%f\n", numberOfIteration, result, mtime);
		#ifdef DEBUG_RESULT
			fprintf(stderr, "%f\n",result);
		#endif
		fclose(fLogFilePtr);
	}
	return 0;
}
int controlFunctionParams(char *f){
	int num = -1;
	if(strlen(f)==2 && f[0]=='f')
		num=f[1]-'0';
	else
		usage("Fonksiyon adini yanlis girdiniz.\t<fi/fj>");
	if(num>6 || num<1)
		usage("fi/fj degeri 1 ile 6 arasinda olmali.\t i, j ε [1,2,3,4,5,6]");
	else
		return true;
}
bool controlOperation(char *f){
	if(strlen(f)==1 && (f[0]=='+' || f[0]=='-' || f[0]=='x' || f[0]=='/'))
		return true;
	else
		usage("Aritmetik operasyonu yanlis girdiniz.\t< +, -, x, / >");
}
void generateLogFile(char fi[], char fj[], char op){
	char sBuffer[BUFSIZ];
	sprintf(logPath, "%s.log", pid);
	fLogFilePtr=fopen(logPath, "a");
	time_t timer;
    struct tm* tm_info;
    time(&timer);
    tm_info = localtime(&timer);
    strftime(sBuffer, 26, "%d.%m.%Y, %H:%M:%S", tm_info);
	fprintf(fLogFilePtr, "\n________________________________________________________\n");
	fprintf(fLogFilePtr, "PID: %s\n", pid);
	fprintf(fLogFilePtr, "Connection Time: %s\n", sBuffer);
	fprintf(fLogFilePtr, "Aritmetical operation: %s %c %s\n", fi, op, fj);
	fclose(fLogFilePtr);
}