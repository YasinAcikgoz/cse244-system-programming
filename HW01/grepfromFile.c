/************************************
*									*
*	         Yasin Acikgoz 			*
*			   131044070			*
*    		grepfromFile.c 			*
*    	     CSE 244 HW01 			*
*									*
*************************************/
 
//includes
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//defines
#define LOGFILE "gfF.log"

//boolean type
typedef int bool;
#define true 1
#define false 0

//function prototypes
void findWords(FILE* fPtr, FILE* lPtr, const char target[], char *text, const char filename[], int numberOfChar);

int getBiggestLine(FILE *fPtr);

int getNumberOfChar(FILE *fPtr);

bool isEmptyFile(FILE *fPtr);

char *findNum(int counter, char buffer[]);

int main(int argc, char const *argv[]){
	/*File pointerlar*/
	FILE* fPtr;
	FILE* lPtr;
	/*Degiskenler*/
	int i=1, biggestLine=0, numberOfChar;
    char *temp, *text;

	if(argc!=3){
        printf("Usage: Command line error. Please enter 3 command line arguments.\n");
        exit(1);
    }
    /*dosyayi read modunda ac*/
    fPtr=fopen(argv[1], "r"); 
    /*dosyanin acilip acilmadigini kontrol edip hata mesaji bas ve programi kapat*/
    if(fPtr==NULL){
        perror("Usage: Error openning file.\n");
        exit(1);
    }
    //en uzun satiri bulan fonksiyonu cagir
    biggestLine=getBiggestLine(fPtr);

    numberOfChar=getNumberOfChar(fPtr);
    rewind(fPtr);
    temp=(char*)malloc(sizeof(char)*numberOfChar);
    text=(char*)malloc(sizeof(char)*numberOfChar);
    //text dosyasinin aktarilacagi karakter dizisini temizle
    strcpy(text, "\0");

    //dosyanin bos olup olmadigini kontrol et bos degilse dosyanin icini satir satir oku
    if(!isEmptyFile(fPtr)){
    	//imleci basa getir
    	rewind(fPtr);
	    while(!feof(fPtr)){
	    	fgets(temp, biggestLine, fPtr);
	    	//printf("%d. satir: %s\n", i, temp);
	    	strcat(text, temp);
	    	strcpy(temp,"");
	    	++i;
	    }
	    //kelimeleri bulan fonksiyonu cagir
    	findWords(fPtr, lPtr, argv[2], text, argv[1], numberOfChar);
	}//dosya bossa hata basip programi sonlandir.
	else{
        printf("Usage: %s is empty.", argv[1]);
        exit(1);
	}
	free(text);
	free(temp);
	fclose(fPtr);
	return 0;
}
void findWords(FILE* fPtr, FILE* lPtr, const char target[], char *text, const char filename[], int numberOfChar){
	int i=0, j=0, k=0, tempCol = 1, column = 0, line = 1, counter=0, lenght=0;
	char temp[100], buffer[10];
	//hedef stringin uzunlugunu bul.
    lenght=strlen(target);
	
	//log dosyasini append modda ac
	lPtr=fopen(LOGFILE, "a");
	
	//acilip acilmadigini kontrol et
	if(lPtr==NULL){
		perror("Usage: Error openning file.\n");
		exit(1);
	}
	while(i<numberOfChar){
		for(k=0; k<lenght; ++k)
			temp[k]=text[i+k];
		temp[k]='\0';
		if(strcmp(temp, target)==0){
			column=tempCol;
			++counter;
			if(counter==1)	//dosyanin adini log dosyasina yaz
				fprintf(lPtr, "\n\n%s -> %s\n", filename, target); //bulunan kelimeleri log dosyasina yaz
			fprintf(lPtr, "%s    line: %d,    column: %d\n", findNum(counter, buffer), line, column);
		}/*Line ve column hesapla*/
		if(text[i]=='\n'){
			++line;
			tempCol=0;
		}
		if(text[i]==target[j]){
			if(j==0)
				column=tempCol;
			++j;
			if(j==lenght){
				j=0;
			}
		}
		else
			j=0;
		++i;
		++tempCol;
	}
	printf("'%s' iterates %d times in '%s'.\n", target, counter, filename);
	fclose(lPtr);
}
int getBiggestLine(FILE *fPtr){
	char ch;
	int counter=0, numberOfLine=0, biggestLine=0, c=0;
	do{
		fscanf(fPtr, "%c", &ch);
		if(ch=='\n')
			numberOfLine+=numberOfLine;
		else
			++c;
	}while(!feof(fPtr));
    /*dosyanin sonuna kadar karakter oku ve en uzun satiri bul*/
    if(numberOfLine!=0){
    	rewind(fPtr);
	    do{
			fscanf(fPtr, "%c", &ch);
			if(ch!='\n'){
				++counter;
			}
			else{
				if(counter>biggestLine)
					biggestLine=counter+1; 
				counter=0;
			}
		}while(!feof(fPtr));
	}
	else
		biggestLine=c;
	/*en uzun satirin karakter sayisini return et*/
	return biggestLine;
} //dosyadaki karakter sayisini hesaplayan fonksiyon
int getNumberOfChar(FILE *fPtr){
	int lenght=0;
	char c;
	rewind(fPtr);
	while(!feof(fPtr)){
		fscanf(fPtr, "%c", &c);
		++lenght;
	}
	return lenght;
} 
char *findNum(int counter, char buffer[]){
	if(counter==1)
		sprintf(buffer, "%dst", counter);
	else if(counter==2)
		sprintf(buffer, "%dnd", counter);
	else if(counter==3)
		sprintf(buffer, "%drd", counter);
	else 
		sprintf(buffer, "%dth", counter);
	return buffer;
} //dosyanin bos olup olmadigini kontrol eden fonksiyon
bool isEmptyFile(FILE *fPtr){
	char ch;
	if(fscanf(fPtr,"%c",&ch)==EOF)
    	return true;
	else
		return false;
}
