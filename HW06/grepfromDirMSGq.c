/************************************
*									*
*	         Yasin Acikgoz 			*
*			   131044070			*
*    	   grepfromDirMSGq.c 	 	*
*    	     CSE 244 HW06			*
*									*
*************************************/

/*includes*/
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/*defines*/

#ifndef PATH_MAX
	#define PATH_MAX 255
#endif
#define LOG_FILE "gfD.log"
#define SIZEMAX 256
#define PERMS (S_IRUSR | S_IWUSR)

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
/*
*	Alt directory'lerdeki klasor sayisini bulan fonksiyon
*/
int countDir(char sPath[PATH_MAX]);
/*
*	Alt directory'lerdeki text sayisini bulan fonksiyon
*/
int countText(char sPath[PATH_MAX]);

void* countWords(void *thread);

/*	ana processin pid'sini tutan static degisken */
pid_t mainpid;

/*	pipe file descriptor	*/
int fd[2];

/*	semaphore	*/
sem_t sem;

typedef struct info
{
	char sTarget[PATH_MAX];
	char sFilename[PATH_MAX];
	int id;
}info_t;

typedef struct msg{
	int counter;
	long mtype;
}msg_t;

/* toplam directory sayisi 	*/
int iCounterDir=0;

int *iSubDirTotal;

int shm_id;

key_t shm_key = 6166529;

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
		/*	fifolari unlink et */
		sem_destroy(&sem);
		printf("CTRL-C'ye basildi.\n");
		fclose(fLogFilePtr);
	}
	exit(0);
}
/*		START_OF_MAIN		*/
int main(int argc, char const *argv[]){
	/*degiskenler*/
	struct 	sigaction act;
	char 	sPath[PATH_MAX];
	int 	iResult = 0;
	DIR*	dRootDirPtr;
	
	if(argc!=3)
        usage("Please enter 3 command line arguments: <executable_file> <text_file> <target_word>");
	/* signal handler olustur*/
	act.sa_handler = signalHandler; 	
	act.sa_flags = 0;
	if ((sigemptyset(&act.sa_mask) == -1) || sigaction(SIGINT, &act, NULL) == -1)
		err_sys("Failed to set Signal handler\n");

	mainpid=getpid();


	shm_id = shmget(shm_key, sizeof(int), IPC_CREAT | 0660);
	if(shm_id==-1)
		err_sys("shmget");

	sprintf(sPath, "%s", argv[1]);

    if(isDirectory(sPath)==false || strlen(argv[1])>PATH_MAX)
    	usage("Please enter a correct path.");
    if((dRootDirPtr = opendir(sPath)) == NULL)
		usage("Can't open directory.");

	closedir(dRootDirPtr);
	countDir(sPath);
	sem_init(&sem, 0, 1);
	/*recursive fonksiyonu cagir*/
   	iResult = getPaths(sPath, argv[2], fd);
	printf("\n'%s' iterates %d times in '%s'.\n", argv[2], iResult, sPath);

	sem_destroy(&sem);
	shmdt(iSubDirTotal);
	shmctl(shm_id, IPC_RMID, 0);

	return 0;
}
/*		END_OF_MAIN		*/
int getPaths(char sHeadPath[PATH_MAX], const char sTarget[], int fd[]){
	/*degiskenler*/
	pid_t 	pid;
	key_t 	key;
	pthread_t threads[SIZEMAX];

	DIR 	*dir;

	int		iCounterText=0,
			iChildStatus=0,
			i=0, 
			iTotal=0, 
			iChildCount=0, 
			iSiblingCount=0, 
			iThreadCount=0,
			msqid;

	struct 	msg mymsg;
	struct 	dirent *stDirent;
	struct 	info infos[SIZEMAX];
	char 	sPath[PATH_MAX];

	/* shared memory olustur */
	if(mainpid==getpid()){
		iSubDirTotal = (int*) shmat(shm_id, NULL, 0);
		if(iSubDirTotal==(int*)-1)
			err_sys("shmat");
	}
	/* key olustur */
	if((key = ftok(sHeadPath, 1)) == (key_t)-1)
		perror("ftok");

	/* message queue olustur */
	if ((msqid = msgget(key, IPC_CREAT | 0666)) == -1)
		err_sys("msgget");

	if (NULL!=(dir=opendir(sHeadPath))){
		while ((stDirent = readdir(dir)) != NULL){
			if((strcmp(stDirent->d_name,"..") != 0) && (strcmp(stDirent->d_name,".") != 0)){
				sprintf(sPath, "%s/%s", sHeadPath, stDirent->d_name);
				if(isDirectory(sPath)){ /*	directory ise 	*/
					pid=fork();
					if(pid<0)
						err_sys("fork");
					if(pid==0){
						iTotal += getPaths(sPath, sTarget, fd);
						exit(0);
					}
				}
				else if(sPath[strlen(sPath)-1]!='~'){ /*	text dosyasi ise 	*/

					sprintf(infos[iThreadCount].sFilename, "%s", sPath);
					sprintf(infos[iThreadCount].sTarget,"%s", sTarget);
					infos[iThreadCount].id=msqid;

					/*	thread olustur */
					pthread_create(&threads[iThreadCount], NULL, countWords, &infos[iThreadCount]);
					++iThreadCount;	/*thread counter attir */
				}
			}
		}
		closedir (dir);
	}

	/* Join threads */
	for(i=0; i<iThreadCount; ++i){
		pthread_join(threads[i], NULL);
		if(msgrcv(msqid, &mymsg, sizeof(mymsg), 0, 0)<0)
			perror("msgrcv");
		iSiblingCount+=mymsg.counter;
	}

	iTotal=+iSiblingCount;
	/* wait childs */
	while(wait(&iChildStatus)!=-1);


	/****************** start of critical section *******************/
	sem_wait(&sem);
	/* toplami shared memory'ye ekle */
	if(mainpid!=getpid())
		(*iSubDirTotal)+=iTotal;

	sem_post(&sem);
	/****************** exit section ********************************/

	/*	root klasor ise shared memory'deki degeri toplama yaz. */
	if(mainpid==getpid()){
		iTotal+=*iSubDirTotal;
		msgctl(msqid, IPC_RMID, NULL);
	}
	return iTotal;
}
void* countWords(void *thread){
	struct msg mymsg;		
	struct info *ThreadData;
	ThreadData = (struct info*)thread;

	FILE* 	fTextFilePtr;
	FILE* 	fLogFilePtr;
	
	int 	i=0, 
			iColumn = 0, 
			iLine = 1, 
			iCounter=0,
			iNumberOfChar=0,
			iSiblingCount=0,
			id;

	char 	*sText,
			sBuffer[10],
			sFilename[PATH_MAX],
			sTarget[PATH_MAX];


	sprintf(sFilename, "%s", ThreadData->sFilename);
	sprintf(sTarget, "%s", ThreadData->sTarget);
	id=ThreadData->id;


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
			++(iCounter);
			if(iCounter==1)
				fprintf(fLogFilePtr, "\n\n%s -> %s\n", sFilename, sTarget); 
			fprintf(fLogFilePtr, "%s    line: %d,    column: %d\n", findNum(iCounter, sBuffer), iLine, iColumn);
		}
		++i;
	}
	if(iCounter==0)
		fprintf(fLogFilePtr, "\n\n'%s' didn't found in %s.\n", sTarget, sFilename);
	free(sText);
	fclose(fLogFilePtr);

	mymsg.mtype=1;
	mymsg.counter=iCounter;

	/* text dosyasindan gelen sonucu message queue'ya yaz.*/
	if(msgsnd(id, &mymsg, sizeof(mymsg), 0)<0)
		perror("msgsnd");

	return 0;
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
int countDir(char sPath[PATH_MAX]){
	DIR *dir;
	struct 	dirent *stDirent;
	struct 	stat statbuf;
	char path[PATH_MAX];
	if (NULL!=(dir=opendir(sPath))){
		while ((stDirent = readdir(dir)) != NULL){
			sprintf(path, "%s/%s", sPath, stDirent->d_name);
			if((strcmp(stDirent->d_name,"..") != 0) && (strcmp(stDirent->d_name,".") != 0)){
				if(stat(path, &statbuf)==0){
					if(S_ISDIR(statbuf.st_mode)){
						iCounterDir+=countDir(path);
						++iCounterDir;
					}
				}
			}
		}
	}
	closedir(dir);
}