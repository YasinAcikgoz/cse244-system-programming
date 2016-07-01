/************************************
*									*
*	         Yasin Acikgoz 			*
*			   131044070			*
*    		grepfromDir.c 			*
*    	     CSE 244 HW02 			*
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

/*defines*/
#define LOG_FILE "gfD.log"
#define DIR_RESULT_FILE "directory_results.txt"
#define TEXT_RESULT_FILE "text_results.txt"
#define PATH_MAX 255
#define DEBUG_MODE_OFF

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
int countWords(const char sTarget[], const char sFilename[], FILE* fLogFilePtr);
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
int getPaths(const char sHeadPath[PATH_MAX], const char sTarget[], FILE *fLogFilePtr);

/*		START_OF_MAIN		*/
int main(int argc, char const *argv[]){
	/*degiskenler*/
	char 	sPath[PATH_MAX];
	FILE 	*fLogFilePtr;
	strcpy(sPath,argv[1]);

	if(argc!=3)
        usage("Please enter 3 command line arguments: <executable_file> <text_file> <target_word>");
    if(isDirectory(sPath)==false || strlen(argv[1])>PATH_MAX){
    	usage("Please enter a correct path.");
    }
	fLogFilePtr=fopen(LOG_FILE, "w");
	if(fLogFilePtr==NULL)
		err_sys("opendir");

	printf("\n'%s' iterates %d times in '%s'.\n", argv[2], getPaths(sPath, argv[2], fLogFilePtr), sPath);
	/*dosyayi kapat*/
	fclose(fLogFilePtr);			
	/*temp dosyayi sil*/	
	remove(DIR_RESULT_FILE);
	return 0;
}
/*		END_OF_MAIN		*/
int getPaths(const char sHeadPath[PATH_MAX], const char sTarget[], FILE* fLogFilePtr){
	/*degiskenler*/
	pid_t 	pidDir;
	pid_t 	pidText;		
	DIR 	*dir;
	FILE 	*fTextFilePtr;
	FILE 	*fDirFilePtr;
	int 	iCounterDir=0, 
			iTotalText=0, 
			iCounterText=0;
	struct 	dirent *stDirent;
	char 	sPath[PATH_MAX];

	/*directory pointer NULL iken fonksiyonu tekrar cagir, 
	bu sayede tum klasorlerde gezerek directory ve text file'lari belirle*/
	if (NULL!=(dir=opendir(sHeadPath))){
		while ((stDirent = readdir(dir)) != NULL) {
			/*ust klasor ve gecerli klasoru es gec*/
			if((strcmp(stDirent->d_name,"..") != 0) && (strcmp(stDirent->d_name,".") != 0)){
				/*Klasor ve dosyalarin Path'ini olustur*/
				sprintf(sPath, "%s/%s", sHeadPath, stDirent->d_name);
				/*eger dosya text ise ve gizli dosya degilse fork yap ve kelimeleri sayan fonksiyonu cagir*/
				if(isDirectory(sPath)){
					/*fonksiyonu tekrar cagir*/
					pidDir=fork();
					/*child process'te recursive fonksiyonu cagir ve text kismindan donen sonucu dosyaya yaz.
					parent process'te donen degerleri oku ve topla. */
					if(pidDir<0)/*fork olmadiysa hata bas*/
						err_sys("fork");
					if(pidDir==0){
						/*dosyayi ac. gerekli durumda hata basip programi sonlandir*/
						fDirFilePtr=fopen(DIR_RESULT_FILE, "w");
						if(fDirFilePtr==NULL)
							err_sys("opendir");
						iCounterDir = getPaths(sPath, sTarget, fLogFilePtr);
						fprintf(fDirFilePtr, "%d\n", iCounterDir);
						fclose(fDirFilePtr);
						/*dosyayi kapat*/
						exit(0);
					}
					else{
						wait(NULL);
						/*dosyayi ac. gerekli durumda hata basip programi sonlandir*/
						fDirFilePtr=fopen(DIR_RESULT_FILE, "r");
						if(fDirFilePtr==NULL)
							err_sys("opendir");
						fscanf(fDirFilePtr, "%d\n", &iCounterDir);
						iTotalText+=iCounterDir;
						/*dosyayi kapat*/
						fclose(fDirFilePtr);
					}
				}/*text dosyasi ise */
				else if(sPath[strlen(sPath)-1]!='~'){
					pidText = fork();
					/*child process'te kelimeleri sayan fonksiyonu cagir ve sonucu dosyaya yaz.
					parent process'te dosyadan donen degeri oku ve topla. */
					if(pidText<0)/*fork olmadiysa hata bas*/
						err_sys("fork");
					if(pidText==0){
						/*dosyayi ac. gerekli durumda hata basip programi sonlandir*/
						fTextFilePtr=fopen(TEXT_RESULT_FILE, "w");
						if(fTextFilePtr == NULL)						
							err_sys("opendir");
						/*kelime sayisini veren fonksiyonu cagir*/
						iCounterText = countWords(sTarget, sPath, fLogFilePtr);
						fprintf(fTextFilePtr, "%d\n", iCounterText);
						/*dosyayi kapat*/
						fclose(fTextFilePtr);
						exit(0);
					}
					else{
						wait(NULL);
						/*dosyayi ac. gerekli durumda hata basip programi sonlandir*/
						fTextFilePtr=fopen(TEXT_RESULT_FILE, "r");
						if(fTextFilePtr==NULL)
							err_sys("opendir");
						else{
							fscanf(fTextFilePtr, "%d\n", &iCounterText);
							iTotalText+=iCounterText;
						}
						/*dosyayi kapat*/
						fclose(fTextFilePtr);
					}
				}
			}
		}
		/*dizini kapat*/
		closedir (dir);
	}			
	/*temp dosyayi sil*/
	remove(TEXT_RESULT_FILE);
	/*toplami return et*/
	return iTotalText;
}
int countWords(const char sTarget[], const char sFilename[], FILE *fLogFilePtr){
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
			#ifdef DEBUG_MODE_ON
			if(iCounter==1)
				printf("\n\n%s -> %s\n", sFilename, sTarget); /*bulunan kelimeleri komut satirina yaz*/
			printf("%s    line: %d,    column: %d\n", findNum(iCounter, sBuffer), iLine, iColumn);
			#endif
		}
		++i;
	}
	if(iCounter==0)
		fprintf(fLogFilePtr, "\n\n'%s' didn't found in %s.\n", sTarget, sFilename);
	/*dosyayi kapat*/
	fclose(fLogFilePtr);
	free(sText);
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