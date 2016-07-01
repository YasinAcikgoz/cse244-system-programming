/************************************
*									*
*	         Yasin Acikgoz 			*
*			   131044070			*
*    	  grepfromDirParalel.c 		*
*    	     CSE 244 HW03			*
*									*
*************************************/
/*includes*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

/*defines*/
#ifndef PATH_MAX
	#define PATH_MAX 255
#endif
#define LOG_FILE "gfDP.log"
#define SLEEP_OFF
#define SLEEP_TIME 500000 /*yarim saniyelik bekleme*/

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
int countWords(const char sTarget[], const char sFilename[], int fd[], FILE *fLogFilePtr);
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
int getPaths(const char sHeadPath[PATH_MAX], const char sTarget[], int fd[], FILE *fLogFilePtr);
/*
*	CTRL-C komutunu yakalamaya yardim eden static degisken
*/
static volatile sig_atomic_t doneflag = 0;

static void setdoneflag(int signo) {
	doneflag = 1;
}
/*
*	ana processin pid'sini tutan static degisken
*/
static pid_t mainpid;

/*		START_OF_MAIN		*/
int main(int argc, char const *argv[]){
	/*degiskenler*/
	struct 	sigaction act;
	char 	sPath[PATH_MAX];
	FILE 	*fLogFilePtr;
	DIR 	*dRootDirPtr;
	int 	fd[2],
			iPipeFirstValue = 0,
			iResult = 0;
	
	/* signal handler olustur*/
	act.sa_handler = setdoneflag; 	
	act.sa_flags = 0;
	if ((sigemptyset(&act.sa_mask) == -1) || sigaction(SIGINT, &act, NULL) == -1) {
		err_sys("Failed to set CTRL-C handler\n");
	}
	mainpid=getpid();
	strcpy(sPath,argv[1]);

	if(argc!=3)
        usage("Please enter 3 command line arguments: <executable_file> <text_file> <target_word>");

    if(isDirectory(sPath)==false || strlen(argv[1])>PATH_MAX)
    	usage("Please enter a correct path.");
    if((dRootDirPtr = opendir(sPath)) == NULL)
		usage("Can't open directory.");
	closedir(dRootDirPtr);
    /* log dosyasini ac */
	fLogFilePtr=fopen(LOG_FILE, "a");
	if(fLogFilePtr==NULL)
		err_sys("opendir");
	/*pipe olustur*/
	if(pipe(fd)==-1)
		err_sys("Failed to create the pipe.\n");
	/*pipe'a ilk deger olarak 0 yaz ve pipe'i kapat*/
	write(fd[1], &iPipeFirstValue, sizeof(int));

	/*recursive fonksiyonu cagir*/
   	getPaths(sPath, argv[2], fd, fLogFilePtr);
   	/*toplam kelime sayisini pipe'tan oku ve pipe'i kapat*/
	read(fd[0], &iResult, sizeof(int));
	close(fd[0]);
	close(fd[1]);
	/*prompt*/
	printf("\n'%s' iterates %d times in '%s'.\n", argv[2], iResult, sPath);
	/*dosyayi kapat*/
	fclose(fLogFilePtr);
	return 0;
}
/*		END_OF_MAIN		*/
int getPaths(const char sHeadPath[PATH_MAX], const char sTarget[], int fd[], FILE *fLogFilePtr){
	/*degiskenler*/
	pid_t 	pid;
	DIR 	*dir;
	int		iCounterText=0,
			iTotal=0,
			iChildStatus=0,
			fifo;
	struct 	dirent *stDirent;
	char 	sPath[PATH_MAX],
		 	sBuffer[26];

	if (NULL!=(dir=opendir(sHeadPath))){
		while ((stDirent = readdir(dir)) != NULL){
			/*	CTRL-C komutunun handle edilip edilmedigini
			  	kontrol etmek icin sleep komutunu calistir.*/
			#ifdef SLEEP_ON
				usleep(SLEEP_TIME);
			#endif
			/* 	Signal handler */
			if(doneflag){
				time_t timer;
			    struct tm* tm_info;
			    time(&timer);
			    tm_info = localtime(&timer);
			    strftime(sBuffer, 26, "%d.%m.%Y, saat %H:%M:%S", tm_info);
			    /*	Log dosyasina CTRL-C bilgisini bas*/
				if(mainpid==getpid()){
					fprintf(fLogFilePtr, "\n________________________________________________________\n");
					fprintf(fLogFilePtr, "%s%s", sBuffer, " itibariyle CTRL-C'ye basildi...\n");
					fprintf(fLogFilePtr, "Junk dosya ve processler temizleniyor...\nProgram kapatildi.\n");
					fprintf(fLogFilePtr, "________________________________________________________\n");
				}
				close(fd[0]);
				close(fd[1]);
				fclose(fLogFilePtr);
				exit(0);
			}
			if((strcmp(stDirent->d_name,"..") != 0) && (strcmp(stDirent->d_name,".") != 0)){
				sprintf(sPath, "%s/%s", sHeadPath, stDirent->d_name);
				if(isDirectory(sPath)){ /*directory ise*/
					pid=fork();
					if(pid<0)
						err_sys("fork");
					if(pid==0){
						getPaths(sPath, sTarget, fd, fLogFilePtr);
						exit(0);
					}
				}
				else if(sPath[strlen(sPath)-1]!='~'){ /*text dosyasi ise*/
					pid = fork();
					if(pid<0)
						err_sys("fork");
					if(pid==0){
						/*total degerini pipe'tan oku*/
						read(fd[0], &iTotal, sizeof(int));
						close(fd[0]);
						/*kelime sayan fonksiyonun dondurdugu degeri total ile toplayip pipe'a yaz*/
						iCounterText=countWords(sTarget, sPath, fd, fLogFilePtr);
						iTotal+=iCounterText;
						write(fd[1], &iTotal, sizeof(int));
						close(fd[1]);
						exit(0);
					}
				}
			}
		}
		closedir (dir);
	}
	/*child processleri oldur.*/
	while(wait(&iChildStatus)>0){
		if(iChildStatus!=0){
			usage("Child process error.");
		}
	}
}
int countWords(const char sTarget[], const char sFilename[], int fd[], FILE *fLogFilePtr){
	/*degiskenler*/
	FILE* 	fTextFilePtr;
	int 	i=0, 
			iColumn = 0, 
			iLine = 1, 
			iCounter=0, 
			iNumberOfChar=0;
	char 	*sText,
			sBuffer[10];

	/*dosyayi ac. gerekli durumda hata basip programi sonlandir*/
	fTextFilePtr=fopen(sFilename, "r");
	if(fTextFilePtr==NULL)
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
			++iCounter;
			if(iCounter==1)
				fprintf(fLogFilePtr, "\n\n%s -> %s\n", sFilename, sTarget); /*bulunan kelimeleri log dosyasina yaz*/
			fprintf(fLogFilePtr, "%s    line: %d,    column: %d\n", findNum(iCounter, sBuffer), iLine, iColumn);
		}
		++i;
	}
	if(iCounter==0)
		fprintf(stdout, "\n\n'%s' didn't found in %s.\n", sTarget, sFilename);
	free(sText);
	fclose(fLogFilePtr);
	return iCounter;
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