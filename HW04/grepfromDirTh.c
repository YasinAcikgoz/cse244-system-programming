/****************************************
*					*
*	    Yasin Acikgoz 		*
*		131044070		*
*    	  grepfromDirParalel.c 		*
*    	     CSE 244 HW04		*
*					*
****************************************/
/*includes*/
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>

/*defines*/
#ifndef PATH_MAX
	#define PATH_MAX 255
#endif
#define LOG_FILE "gfD.log"
#define SIZEMAX 250

/*boolean type*/
typedef int bool;
#define true 1
#define false 0
/*function prototypes*/

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
/*
*	Kelimeleri sayip dosyaya yazan fonksioyn
*/
//int countWords(const char sTarget[], const char sFilename[], FILE *fLogFilePtr);
/*
*	Dosyadaki karakter sayisini bulan fonksiyon
*/
int getNumberOfChar(const char *str);
/*
*	Dosyanin bos olup olmadigini kontrol eden fonksiyon
*/
bool isEmptyFile(FILE *fTextFilePtr);
/*
*	Kelimelerin sirasini bir stringe yazip return eden fonksiyon	
*/
char *findNum(int iCounter, char sBuffer[]);
/*
*	Verilen path'in dosya olup olmadigini bulan fonksiyon
*/
bool isDirectory(char sHeadPath[PATH_MAX]);
/*
*	Verilen path'in icinde gezinen fonksiyon
*/
int getPaths(char sHeadPath[PATH_MAX], const char sTarget[], int fd[]);

int countDir(char sPath[PATH_MAX], char fifoNames[SIZEMAX][SIZEMAX]);

int countText(char sPath[PATH_MAX]);

void* countWords(void *thread);

int fd[2];

int fifoFD;

/*
*	ana processin pid'sini tutan static degisken
*/
pid_t mainpid;

struct info
{
	char sTarget[PATH_MAX];
	char sFilename[PATH_MAX];
}info_t;

int iNumberOfDir=0;

int iNumberOfText=0;

int iThreadCount=0;

int iCounterDir=0;

char allFifos[SIZEMAX][SIZEMAX];

/* 	Signal handler */
static void signalHandler(int signo) {
	FILE* fLogFilePtr;
	char sBuffer[26];
	fLogFilePtr=fopen(LOG_FILE, "a");
	if(fLogFilePtr==NULL)
		err_sys("opendir");
	time_t timer;
    struct tm* tm_info;
    time(&timer);
    tm_info = localtime(&timer);
    strftime(sBuffer, 26, "%d.%m.%Y, saat %H:%M:%S", tm_info);
    /*	Log dosyasina CTRL-C bilgisini bas 	*/
	if(mainpid==getpid()){
		fprintf(fLogFilePtr, "\n________________________________________________________\n");
		fprintf(fLogFilePtr, "%s%s", sBuffer, " itibariyle CTRL-C'ye basildi...\n");
		fprintf(fLogFilePtr, "Junk dosya ve processler temizleniyor...\nProgram kapatildi.\n");
		fprintf(fLogFilePtr, "________________________________________________________\n");
		int i=0;
		/*fifolari unlink et*/
		for(i=0; i<iCounterDir; ++i)
			unlink(allFifos[i]);
		printf("CTRL-C'ye basildi.\n");
	}
	fclose(fLogFilePtr);
	exit(0);
}
/*		START_OF_MAIN		*/
int main(int argc, char const *argv[]){
	/*degiskenler*/
	struct 	sigaction act;
	char 	sPath[PATH_MAX];
	int 	iResult = 0;
	DIR*	dRootDirPtr;
	
	/* signal handler olustur*/
	act.sa_handler = signalHandler; 	
	act.sa_flags = 0;
	if ((sigemptyset(&act.sa_mask) == -1) || sigaction(SIGINT, &act, NULL) == -1)
		err_sys("Failed to set Signal handler\n");

	mainpid=getpid();
	strcpy(sPath,argv[1]);

	if(argc!=3)
        usage("Please enter 3 command line arguments: <executable_file> <text_file> <target_word>");

    if(isDirectory(sPath)==false || strlen(argv[1])>PATH_MAX)
    	usage("Please enter a correct path.");
    if((dRootDirPtr = opendir(sPath)) == NULL)
		usage("Can't open directory.");
	closedir(dRootDirPtr);

	/*recursive fonksiyonu cagir*/
   	iResult = getPaths(sPath, argv[2], fd);
	printf("'%s' iterates %d times in '%s'.\n", argv[2], iResult, sPath);
	return 0;
}
/*		END_OF_MAIN		*/
int getPaths(char sHeadPath[PATH_MAX], const char sTarget[], int fd[]){
	/*degiskenler*/
	pid_t 	pid;
	DIR 	*dir;
	int		iCounterText=0,
			iChildStatus=0;
	struct 	dirent *stDirent;
	char 	sPath[PATH_MAX];
	int iNumberOfDir, iNumberOfText;
	int i=0;
	char fifoNames[SIZEMAX][SIZEMAX];
	int iParentCount=0, iChildCount=0;

	iNumberOfDir=countDir(sHeadPath, fifoNames);	
	iNumberOfText=countText(sHeadPath);

	struct info infos[SIZEMAX];
	pthread_t threads[SIZEMAX];

	if(pipe(fd)==-1)
		err_sys("pipe error");
	if(write(fd[1], &iChildCount, sizeof(int))<0)
		err_sys("pipe write error");
	if (NULL!=(dir=opendir(sHeadPath))){
		while ((stDirent = readdir(dir)) != NULL){
			/*	CTRL-C komutunun handle edilip edilmedigini
			  	kontrol etmek icin sleep komutunu calistir.*/
			#ifdef SLEEP
				usleep(SLEEP_TIME);
			#endif
			if((strcmp(stDirent->d_name,"..") != 0) && (strcmp(stDirent->d_name,".") != 0)){
				sprintf(sPath, "%s/%s", sHeadPath, stDirent->d_name);
				if(isDirectory(sPath)){
					pid=fork();
					if(pid<0)
						err_sys("fork");
					if(pid==0){
						char fifoName[PATH_MAX];
						sprintf(fifoName, "%d_%s", getppid(), stDirent->d_name);
						iParentCount += getPaths(sPath, sTarget, fd);
						fifoFD = open(fifoName, O_WRONLY);
						if(write(fifoFD, &iParentCount, sizeof(int))<0)
							err_sys("fifo write error");
						close(fifoFD);
						exit(0);
					}
				}
				else if(sPath[strlen(sPath)-1]!='~'){ /*text dosyasi ise*/
					strcpy(infos[iThreadCount].sFilename, sPath);
					strcpy(infos[iThreadCount].sTarget, sTarget);
					int siblingTextCount;
					if(read(fd[0], &siblingTextCount, sizeof(int))<0)
						err_sys("pipe read error");

					pthread_create(&threads[iThreadCount], NULL, countWords, &infos[iThreadCount]);
				//	perror("pthread_create");
					pthread_join(threads[iThreadCount], (void**)&iCounterText);
				//	perror("pthread_join");

					iChildCount=siblingTextCount+iCounterText;
					if(write(fd[1], &iChildCount, sizeof(int))<0)
						err_sys("pipe write error");
					++iThreadCount;
				}
			}
		}
		closedir (dir);
	}
	int subTotals=0;
	i=0;
	for(i=0; i<iNumberOfDir; ++i){
		fifoFD = open(fifoNames[i], O_RDONLY);
		if(read(fifoFD, &subTotals, sizeof(int))<0)
			err_sys("pipe read error");
		iParentCount+=subTotals;
		unlink(fifoNames[i]);
	}

	while(wait(&iChildStatus)!=-1)
		if(iChildStatus!=0)
			err_sys("Child process error.");
	int sum;		
	if(read(fd[0], &sum, sizeof(int))<0)
		err_sys("pipe read error");
	iParentCount+=sum;

	return iParentCount;
}
void* countWords(void *thread){
	
	struct info *ThreadData;
	ThreadData = (struct info*)thread;
	FILE* 	fTextFilePtr;
	FILE* 	fLogFilePtr;
	
	int 	i=0, 
			iColumn = 0, 
			iLine = 1, 
			*iCounter,
			iNumberOfChar=0;

	char 	*sText,
			sBuffer[10],
			sFilename[PATH_MAX],
			sTarget[PATH_MAX];

	iCounter = (int*)malloc(sizeof(int));

	*iCounter=0;
	sprintf(sFilename, "%s", ThreadData->sFilename);
	sprintf(sTarget, "%s", ThreadData->sTarget);


	/*dosyayi ac. gerekli durumda hata basip programi sonlandir*/
	fTextFilePtr=fopen(sFilename, "r");
	if(fTextFilePtr==NULL)
		err_sys("opendir");

	fLogFilePtr=fopen(LOG_FILE, "a");
	if(fLogFilePtr==NULL)
		err_sys("opendir");
	/*dosyadaki karakter sayisi kadar yer al*/
	iNumberOfChar=getNumberOfChar(sFilename);
   	sText=(char*)malloc(sizeof(char)*iNumberOfChar);
   	rewind(fTextFilePtr);

    /*text stringini olustur*/
    while(!feof(fTextFilePtr)){
    	fscanf(fTextFilePtr, "%c", &sText[i]);
    	++i;
    }
    i=0;
    fclose(fTextFilePtr);
    /*kelimeleri say ve log dosyasini olustur*/
	while(i<iNumberOfChar){
		if(sText[i]!='\n')
			++iColumn;
		else{
			++iLine;
			iColumn=0;
		}
		if(strncmp(&sText[i], sTarget, strlen(sTarget))==0){
			++(*iCounter);
			if(*iCounter==1)
				fprintf(fLogFilePtr, "\n\n%s -> %s\n", sFilename, sTarget); 
			fprintf(fLogFilePtr, "%s    line: %d,    column: %d\n", findNum(*iCounter, sBuffer), iLine, iColumn);
		}
		++i;
	}
	if(*iCounter==0)
		fprintf(fLogFilePtr, "\n\n'%s' didn't found in %s.\n", sTarget, sFilename);
	free(sText);
	fclose(fLogFilePtr);
	return *iCounter;
}
int getNumberOfChar(const char *str){
	/*degiskenler*/
	FILE 	*fTextFilePtr;
	int 	iLength=0;
	char 	c;
	/*dosyayi ac*/
	fTextFilePtr=fopen(str,"r");
	if(fTextFilePtr==NULL)
		err_sys("opendir");
	/*dosyayi sonuna kadar oku*/
	while(!feof(fTextFilePtr)){
		fscanf(fTextFilePtr, "%c", &c);
		++iLength;
	}
	/*dosyayi kapat*/
	fclose(fTextFilePtr);
	/*toplam karakter sayisini return et*/
	return iLength;
} 
char *findNum(int iCounter, char sBuffer[]){
	if(iCounter==1)
		sprintf(sBuffer, "%dst", iCounter);
	else if(iCounter==2)
		sprintf(sBuffer, "%dnd", iCounter);
	else if(iCounter==3)
		sprintf(sBuffer, "%drd", iCounter);
	else 
		sprintf(sBuffer, "%dth", iCounter);
	return sBuffer;
}
bool isDirectory(char sHeadPath[PATH_MAX]){
	struct 	stat statbuf;
	if(stat(sHeadPath, &statbuf)==-1)
		return false;
	else
		return S_ISDIR(statbuf.st_mode);
}
int countDir(char sPath[PATH_MAX], char fifoNames[SIZEMAX][SIZEMAX]){
	DIR *dir;
	struct 	dirent *stDirent;
	struct 	stat statbuf;
	char path[100];
	int d=0;
	if (NULL!=(dir=opendir(sPath))){
		while ((stDirent = readdir(dir)) != NULL){
			sprintf(path, "%s/%s", sPath, stDirent->d_name);
			if((strcmp(stDirent->d_name,"..") != 0) && (strcmp(stDirent->d_name,".") != 0)){
				if(stat(path, &statbuf)==0){
					if(S_ISDIR(statbuf.st_mode)){
						sprintf(fifoNames[d], "%d_%s", getpid(), stDirent->d_name);
						mkfifo(fifoNames[d], 0666);
						sprintf(allFifos[iCounterDir], "%s", fifoNames[d]);
						++d;
						++iCounterDir;
					}
				}
			}
		}
	}
	closedir(dir);
	return d;
}
int countText(char sPath[PATH_MAX]){
	DIR *dir;
	struct 	dirent *stDirent;
	struct 	stat statbuf;
	char path[100];
	int d=0;
	if (NULL!=(dir=opendir(sPath))){
		while ((stDirent = readdir(dir)) != NULL){
			sprintf(path, "%s/%s", sPath, stDirent->d_name);
			if((strcmp(stDirent->d_name,"..") != 0) && (strcmp(stDirent->d_name,".") != 0)){
				if(!isDirectory(path)){
					++d;
				}
			}
		}
	}
	closedir(dir);
	return d;
}
