#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>


#define _CRT_SECURE_NO_WARNINGS
#define BUF_SIZE 1024
#define MK_SIZE 128
#define MAX_STR_LEN 4000
#define MACRO_SIZE 1024
#define MKFILE_SIZE 1024
#define DEP_SIZE 1024
#define COMMAND_SIZE 1024

int entry = 0;
int entry_c = 0;
int entry_m = 0;
int first_m = 0;
int depend_count = 0;
int depend_default[5] = {0};
int do_not_print = 0;
int do_exec = 0;
char result[1024];
int fd;
char *fname;
char *pathname;
char *tmp[100];//메모리용 포인터 배열 선언 - 비효율적인데 좋은 방법 없을까..?
int ex_target[MKFILE_SIZE];	// target이 한번 들려졌는가
char *error_string;
int prent[100] = {0};
int row_length = 0;

struct {
	char *mac;
	char *real;
}macro[MACRO_SIZE];

struct {
	char * target;
	char * dependency[DEP_SIZE];
	char * command[COMMAND_SIZE];
} mkfile[MKFILE_SIZE];

char *replaceAll(char *s, const char * olds, const char *news);
void mk_main(char * string, char * pattern);
void mk_command(char * string, char * pattern);
void mk_target_dependency(char * string, char * pattenr);
void mk_macro_1(char *string, char *pattern);
void mk_macro_2(char *string, char *pattern);
void mk_include(char *string, char *pattern);
int depend(int i);
void print();
void print_help();
void print_macro();
void depend_all();//dependency초기화
char*  trim(char *s); // 문자열 좌우 공백 모두 삭제 함수
char* ltrim(char *s); // 문자열 좌측 공백 제거 함수
char* rtrim(char* s); // 문자열 우측 공백 제거 함수
void trim_all();
void exec();
void print_t(int pwd, int row_len, int vacant);

int main(int argc, char *argv[])
{
	depend_all();
	int l;
	for(l = 0; l < MKFILE_SIZE; l++){
		ex_target[l] = -1;
	}
	int flag_f = 0, flag_c = 0, flag_s = 0, flag_h = 0, flag_m = 0, flag_t = 0;

	if(argc < 2){
		fname = "Makefile";
		if((fd = open(fname, O_RDONLY)) < 0){
			fprintf(stderr, "make: %s: No such file or directory\n", fname);
			exit(1);
		}else{
			
		}
	}
	else{
		
		int c;
		
		while((c = getopt(argc, argv, "f:c:shmt")) != -1){	
			
			switch(c){
				case 'f':
					flag_f =1;
					fname = optarg;
					break;
				case 'c':
					flag_c =1;
					pathname = optarg;
					break;
				case 's':
					flag_s =1;
					break;
				case 'h':
					flag_h =1;
					break;
				case 'm':
					flag_m =1;
					break;
				case 't':
					flag_t =1;
					break;
			}
		}
		
		if(flag_c){
			if(chdir(pathname) < 0){
				fprintf(stderr, "chdir error\n");
			}
			else{
			}
		}
		if(flag_f){
		}
		if(flag_s){
			do_not_print = 1;
		}
		if(flag_h){
			print_help();
		}
		if(flag_m){ // 밖으로 나감
		}
		if(flag_t){
		}
	}
	
	exec();
	//진짜 실행

	if(entry == 0){
		fprintf(stderr, "make: *** No targets. Stop.\n");
		exit(1);
	}
	int s;
	int u;
	int c;

	//trim
	trim_all();

//`	for(s = 0; s < entry; s++)
//`	{
//`		printf("target: [%s]\n", mkfile[s].target);
//`		u = 0;
//`		c = 0;
//`		while(mkfile[s].dependency[u] !=NULL)
//`		{
//`			printf("dep : [%s]\n", mkfile[s].dependency[u]);
//`			u++;
//`		}
//`		while(mkfile[s].command[c] != NULL){
//`			printf("com : [%s]\n", mkfile[s].command[c]);
//`			c++;
//`		}
//`	}
//	for(s = 0; s < entry_m; s++){
//		printf("mac %d : [%s]\n", s, macro[s].mac);
//		printf("macr %d : [%s]\n", s, macro[s].real);
//	}


	//macro , target default
	int i;

	for(i = optind; i < argc; i++){ 
		if(islower(argv[i][0]) != 0){
			char * ar = argv[i];
			int j;
			for(j = 0; j < entry; j++){
				if(strcmp(argv[i], mkfile[j].target) == 0){
					depend_default[depend_count++] = j;
					break;
				}
			}
			if(j == entry){
				depend_default[depend_count++] = -1;
				error_string = argv[i];

				//
				break;
				//
				//	printf("make: *** No rule to make target '%s'. Stop\n", argv[i]);
				//	exit(1);
			}
		}
		else{
			mk_macro_1(argv[i], "=");
		}
	}
	//macr가 아닐 경우

	// replace 내부 매크로 치환

	char *internal_target = "$@";
	char *internal_target2 = "$*";
	int k = 0;
	char *remove_exe;
	for(s = 0; s < entry; s++)
	{
		k = 0;
		char x[MAX_STR_LEN];
		strcpy(x, mkfile[s].target);
		char substitute[MAX_STR_LEN] = "$(";
		int m = 0;

		while(mkfile[s].command[k] != NULL){
			mkfile[s].command[k] = replaceAll(mkfile[s].command[k], internal_target, x);
			remove_exe = strtok(x, ".");
			mkfile[s].command[k] = replaceAll(mkfile[s].command[k], internal_target2, remove_exe);
			for(m = 0; m < entry_m ; m++){
				strcat(substitute, macro[m].mac);
				strcat(substitute, ")");
				mkfile[s].command[k] = replaceAll(mkfile[s].command[k], substitute, macro[m].real); 
			}
			k++;
		}
	}
//
	if(flag_m){
		print_macro();
		exit(0);
	}
	if(flag_h){
		exit(0);
	}
	if(flag_t){
		for(i = 0; i < entry; i++){
			int j;
			for(j = 0; j < entry; j++){
				prent[j] = 0;
			}
			print_t(i, 0, 0);
		}
		exit(0);
	}
		
	for(i = 0; i < depend_count; i++){
		if(depend_default[i] != -1){
			depend(depend_default[i]);
		}else{
			printf("make: *** No rule to make target '%s'. Stop\n", error_string);
			exit(1);
		}
			
	}
	if(depend_count == 0){
		depend(depend_count);
	}

	exit(0);
	
	
}

struct stat statbuf;
struct stat statbuf1;
int depend(int i){//성공시 0 실패시 -1
    int j = 0, z, n = i;
	
	ex_target[i] = 0;
	
	if(stat(mkfile[i].target, &statbuf)!=0){}

    while(mkfile[i].dependency[j] != NULL){

    	for(z = 0; z < entry; z++){	//	dependency와 같은 타겟 찾기
	        if(strcmp(mkfile[i].dependency[j], mkfile[z].target) == 0){
				if(ex_target[z] == 0 ){ // dependency와 같은 target이 이미 호출되었던 경우 
					printf("make: Circular %s <- %s dependency dropped.\n", mkfile[i].target, mkfile[z].target); 
					break;
				}	
	            else{ //  dependency와 같은 target이 있는 경우, 나머지 실패하는 경우
					if(access(mkfile[z].target, F_OK) == 0){
						if(stat(mkfile[z].target, &statbuf1)!=0){	
						}
						else{
							if(statbuf1.st_mtime <= statbuf.st_mtime){
								printf("make: '%s'is up to date\n", mkfile[i].target);
								return 0;
							}
						}
					}
					else if(depend(z) == 0){
						break;
					}else{
						fprintf(stderr, "make: No rule to make target '%s', needed by '%s'. stop\n", mkfile[i].dependency[j], mkfile[i].target);
						exit(1);//	return -1;
					}
	            }
	        }
	    }
		
		if(z == entry){ // 타겟에 존재하지 않을 때,
			if(access(mkfile[i].dependency[j], F_OK) != 0){//디렉토리에 존재하지 않을 때! //z=6
				fprintf(stderr, "make: No rule to make target '%s', needed by '%s'. stop\n", mkfile[i].dependency[j], mkfile[i].target);
				exit(1); //return -1;
			}
		}
	    j++; //	 dependency -> 
	}

	int c = 0;
	if(do_not_print == 0){
		while(mkfile[i].command[c] != NULL){
			printf("%s\n", mkfile[i].command[c]);
			c++;
		}
	}
	c = 0;
	while(mkfile[i].command[c] != NULL){
		system(mkfile[i].command[c]);
		c++;
	}
	return 0;
}
  
void print_macro(){//미완
	int j;
	printf("-----------------------------macro list-------------------------------\n");
	for(j = 0; j < entry_m; j++){
		printf("%s-> %s\n", macro[j].mac, macro[j].real);
	}
}

void print_help(){
	printf( "Usage : ssu_make [Target] [Option] [Macro]\n"
		    "Option : \n"
			"-f <file>	Use <file> as a makefile.\n"
			"-c <directory>	Change to directory <directory> before reading the makefiles.\n"
			"-s		Do not print the commands as they are executedt\n"
			"-h		print usage\n"
			"-m		print macro list\n"
			"-t		print tree\n"
		  );
}
			
void mk_command(char *string, char *pattern){
	
	int ch;
	int ndx;
	int space = 0;
	for (ndx= 0; ndx < strlen(string); ndx++)
	{
	    ch = string[ndx];
	    if (isspace(ch)==0)
			space = 1;
    }
	if(space == 1){	
		mkfile[entry-1].command[entry_c++] = strtok(string, pattern);//command
	}
}

void mk_target_dependency(char *string, char *pattern){
	int i = 0;
	
	mkfile[entry].target = strtok(string, pattern);//target

	while((mkfile[entry].dependency[i] = strtok(NULL, " +[\t]+")) != NULL){//dependency[]
		i++;
	}
	
	entry++;//entry = target개수
	entry_c = 0;//command 개수 0으로 초기화
}

void mk_macro_1(char *string, char *pattern){
	int i = first_m;	
	char *mac_ = strtok(string, pattern);//mac임시
	char *real_ = strtok(NULL, pattern);//real임시
	
	for(i = first_m; i < entry_m; i++){
		if(strcmp(macro[i].mac, mac_) == 0){
			macro[i].real = real_;
			return;
		}
	}
	
	if(i == entry_m){
		macro[entry_m].mac = mac_;
		macro[entry_m].real = real_;
		entry_m++;
	}
}

void mk_macro_2(char *string, char *pattern){
	int i = 0;	
	char *mac_  = strtok(string, pattern);//mac임시
	char *real_ = strtok(NULL, pattern);//real임시
	
	for(i = 0; i < entry_m; i++){
		if(strcmp(macro[i].mac, mac_) == 0){
			return;
		}
	}
	
	if(i == entry_m){
		macro[entry_m].mac = mac_;
		macro[entry_m].real = real_;
		entry_m++;
	}
}

void mk_include(char *string, char *pattern){
	int fd;
	strtok(string, " ");
	char *fname = strtok(NULL, " ");
	
	int i;
	int row_count = 0;// 줄 수
	int length;
	int k = 0;
	char *str[100];//진짜로 들어갈 공간 mkfile 보다 큰 한 줄씩 읽힘.
	char buf[BUF_SIZE];//에러용
	char buf2[BUF_SIZE];//파일용
	
	if((fd = open(fname, O_RDONLY)) < 0){
		fprintf(stderr, "open error\n");
		exit(1);
	}

	if((length = read(fd, buf2, BUF_SIZE)) > 0){
		str[row_count] = strtok(buf2, "\n");

		while(str[row_count] != NULL){
			row_count++;
			str[row_count] = strtok(NULL, "\n");
		}
		//
		row_count--; // 공백이 있을경우엔?
	}//=============================================================================줄수 나누기

	regex_t preg_tab;
	regex_t preg_include;
	regex_t preg_etc;
	regex_t preg_macro_1;
	regex_t preg_macro_2;
	regex_t preg_target_dependency;
	regex_t preg_slash;
	char *pattern_tab = "^\t";
	char *pattern_include = "^(include)";
	char *pattern_etc = "^#";
	char *pattern_macro_1 = "=";
	char *pattern_macro_2 = "[?]=";
	char *pattern_target_dependency = ":";
	char *pattern_slash = "[\\]$";
	int error_code;

	if((error_code = regcomp(&preg_tab, pattern_tab, REG_EXTENDED)) != 0){
		regerror(error_code, &preg_tab, buf, BUFSIZ);
		fprintf(stderr, "recomp error : %s\n", buf);
		exit(1);
	}
	if((error_code = regcomp(&preg_include, pattern_include, REG_EXTENDED)) != 0){
		regerror(error_code, &preg_include, buf, BUFSIZ);
		fprintf(stderr, "recomp error : %s\n", buf);
		exit(1);
	}
	if((error_code = regcomp(&preg_etc, pattern_etc, REG_EXTENDED)) != 0){
		regerror(error_code, &preg_etc, buf, BUFSIZ);
		fprintf(stderr, "recomp error : %s\n", buf);
		exit(1);
	}
	if((error_code = regcomp(&preg_macro_1, pattern_macro_1, REG_EXTENDED)) != 0){
		regerror(error_code, &preg_macro_1, buf, BUFSIZ);
		fprintf(stderr, "recomp error : %s\n", buf);
		exit(1);
	}
	if((error_code = regcomp(&preg_macro_2, pattern_macro_2, REG_EXTENDED)) != 0){
		regerror(error_code, &preg_macro_2, buf, BUFSIZ);
		fprintf(stderr, "recomp error : %s\n", buf);
		exit(1);
	}
	if((error_code = regcomp(&preg_target_dependency, pattern_target_dependency, REG_EXTENDED)) != 0){
		regerror(error_code, &preg_target_dependency, buf, BUFSIZ);
		fprintf(stderr, "recomp error : %s\n", buf);
		exit(1);
	}
	if((error_code = regcomp(&preg_slash, pattern_slash, REG_EXTENDED)) != 0){
	         regerror(error_code, &preg_slash, buf, BUFSIZ);
	         fprintf(stderr, "recomp error : %s\n", buf);
	         exit(1);
	}

	// 역슬래쉬 붙여주기
	 
    for(i = 0; i < row_count; i++){
         char *string = str[i];
         if((error_code = regexec(&preg_slash, string, 0, NULL, 0)) != 0){
             regerror(error_code, &preg_slash, buf, BUFSIZ);
         }
         else{
             tmp[i] = (char *)malloc(200);
             str[i] = strtok(string, pattern_slash);
             strcpy(tmp[i], str[i]);
             strcat(tmp[i], str[i+1]);
             str[i] = tmp[i];
             int j;
             for(j = i+1; j < row_count-1; j++){
                 str[j] = str[j+1];
             }
             row_count--;
         }
	}


	for(i = 0; i < row_count; i++){
		char *string = str[i];	
		int flag = -1;// 0->target_dependency, command; 1->macro_1; 2->macro_2; 3->include; 4->주석
		if((error_code = regexec(&preg_tab, string, 0, NULL, 0)) != 0){//왜 str이 바뀔까?
			regerror(error_code, &preg_tab, buf, BUFSIZ);
		}
		else{
			flag = 5;
		}

		if((error_code = regexec(&preg_include, string, 0, NULL, 0)) != 0){
			regerror(error_code, &preg_include, buf, BUFSIZ);
		}
		else{
			flag = 3;
		}
		
		if((error_code = regexec(&preg_etc, string, 0, NULL, 0)) != 0){
			regerror(error_code, &preg_etc, buf, BUFSIZ);
		}
		else{
			flag = 4;
		}

		if((error_code = regexec(&preg_macro_1, string, 0, NULL, 0)) != 0){
			regerror(error_code, &preg_macro_1, buf, BUFSIZ);
		}
		else{
			flag = 1;
		}

		if((error_code = regexec(&preg_macro_2, string, 0, NULL, 0)) != 0){
			regerror(error_code, &preg_macro_2, buf, BUFSIZ);
		}
		else{
			flag = 2;
		}

		if((error_code = regexec(&preg_target_dependency, string, 0, NULL, 0)) != 0){
			regerror(error_code, &preg_target_dependency, buf, BUFSIZ);
		}
		else{
			flag = 0;
		}
		
		if(flag < 0){
			fprintf(stderr, "%s:%d: *** missing seperator. Stop.\n", fname, i + 1);
			exit(1);
		}


		switch(flag){
			case 0:
				mk_target_dependency(str[i], pattern_target_dependency);
				break;
			case 1:
				mk_macro_1(str[i], pattern_macro_1);
				break;
			case 2:
				mk_macro_2(str[i], pattern_macro_2);
				break;
			case 3:
				mk_include(str[i], pattern_include);
				break;
			case 4:	//아무것도 안해주면 된다.
				break;
			case 5:
				mk_command(str[i], pattern_tab);
			default:
				break;
		}
	}
	
	regfree(&preg_tab);
	regfree(&preg_include);
	regfree(&preg_etc);
	regfree(&preg_macro_1);
	regfree(&preg_macro_2);
	regfree(&preg_target_dependency);
	regfree(&preg_slash);

}
void print(){
	int j;
	int c = 0;
	if(do_not_print == 0){
		for(j = 0; j < entry_c; j++){
			c = 0;
			while(mkfile[j].command[c] != NULL)
			{
				printf("command : %s\n", mkfile[j].command[c]);
				c++;
			}
		}
	}
}

//
void depend_all(){
	int i;
	int j;
	for(i = 0; i < MKFILE_SIZE; i++){
		for(j = 0; j < DEP_SIZE; j++){
			mkfile[i].dependency[j] = NULL;
		}
	}
}

// 문자열 우측 공백문자 삭제 함수
char* rtrim(char* s) {
      char t[MAX_STR_LEN];
      char *end;

      strcpy(t, s);
      end = t + strlen(t) - 1;
      while (end != t && isspace(*end))
           end--;
      *(end + 1) = '\0';
      strcpy(s, t);

return s;
} 


// 문자열 좌측 공백문자 삭제 함수
char* ltrim(char *s) {
	  char* begin;
	  begin = s;

	  while (*begin != '\0') {
	      if (isspace(*begin))
			 begin++;
	      else {
		     s = begin;
		     break;
		  }
	  }
	  return s;
}


// 문자열 앞뒤 공백 모두 삭제 함수
char* trim(char *s) {
	  return rtrim(ltrim(s));
}

void trim_all(){
	//trim
	int z;
	int x = 0;
	int c = 0;
	
	for(z = 0; z < entry; z++){
		mkfile[z].target = trim(mkfile[z].target);
		x = 0;
		while(mkfile[z].dependency[x] != NULL){
		 	mkfile[z].dependency[x] = trim(mkfile[z].dependency[x]);
			x++;
		}
		while(mkfile[z].command[c] != NULL){
			mkfile[z].command[c] = trim(mkfile[z].command[c]);
			c++;
		}
	}
	for(z = 0; z < entry_m; z++){
		macro[z].real = trim(macro[z].real);
		macro[z].mac = trim(macro[z].mac);
	}
}


void exec(){
	//진짜 실행
	if((fd = open(fname, O_RDONLY)) < 0){
		fprintf(stderr, "make: %s: No such file or directory\n", fname);
		exit(1);
	}
		
	//읽기
	int i;
	int row_count = 0;// 줄 수
	int length;
	int k = 0;
	char *str[100];//진짜로 들어갈 공간 mkfile 보다 큰 한 줄씩 읽힘.
	char buf[BUF_SIZE] ="";//에러용
	char buf2[BUF_SIZE] = "";//파일용

	if((length = read(fd, buf2, BUF_SIZE)) > 0){
		str[row_count] = strtok(buf2, "\n");
		while((str[++row_count] = strtok(NULL, "\n")) != NULL){
		}
	}//=============================================================================줄수 나누기

	regex_t preg_tab;
	regex_t preg_include;
	regex_t preg_etc;
	regex_t preg_macro_1;
	regex_t preg_macro_2;
	regex_t preg_target_dependency;
	regex_t preg_slash;
	char *pattern_tab = "^\t";
	char *pattern_include = "^(include)";
	char *pattern_etc = "^#";
	char *pattern_macro_1 = "=";
	char *pattern_macro_2 = "[?]=";
	char *pattern_target_dependency = ":";
	char *pattern_slash = "[\\]$";
	int error_code;

	if((error_code = regcomp(&preg_tab, pattern_tab, REG_EXTENDED)) != 0){
		regerror(error_code, &preg_tab, buf, BUFSIZ);
		fprintf(stderr, "recomp error : %s\n", buf);
		exit(1);
	}
	if((error_code = regcomp(&preg_include, pattern_include, REG_EXTENDED)) != 0){
		regerror(error_code, &preg_include, buf, BUFSIZ);
		fprintf(stderr, "recomp error : %s\n", buf);
		exit(1);
	}
	if((error_code = regcomp(&preg_etc, pattern_etc, REG_EXTENDED)) != 0){
		regerror(error_code, &preg_etc, buf, BUFSIZ);
		fprintf(stderr, "recomp error : %s\n", buf);
		exit(1);
	}
	if((error_code = regcomp(&preg_macro_1, pattern_macro_1, REG_EXTENDED)) != 0){
		regerror(error_code, &preg_macro_1, buf, BUFSIZ);
		fprintf(stderr, "recomp error : %s\n", buf);
		exit(1);
	}
	if((error_code = regcomp(&preg_macro_2, pattern_macro_2, REG_EXTENDED)) != 0){
		regerror(error_code, &preg_macro_2, buf, BUFSIZ);
		fprintf(stderr, "recomp error : %s\n", buf);
		exit(1);
	}
	if((error_code = regcomp(&preg_target_dependency, pattern_target_dependency, REG_EXTENDED)) != 0){
		regerror(error_code, &preg_target_dependency, buf, BUFSIZ);
		fprintf(stderr, "recomp error : %s\n", buf);
		exit(1);
	}
	if((error_code = regcomp(&preg_slash, pattern_slash, REG_EXTENDED)) != 0){
		regerror(error_code, &preg_slash, buf, BUFSIZ);
		fprintf(stderr, "recomp error : %s\n", buf);
		exit(1);
	}
	
	// 역슬래쉬 붙여주기
	
	for(i = 0; i < row_count; i++){
		char *string = str[i];
		if((error_code = regexec(&preg_slash, string, 0, NULL, 0)) != 0){
			regerror(error_code, &preg_slash, buf, BUFSIZ);
		}
		else{
			tmp[i] = (char *)malloc(200);
			str[i] = strtok(string, pattern_slash);
			strcpy(tmp[i], str[i]);
			strcat(tmp[i], str[i+1]);
			str[i] = tmp[i];
			int j;
			for(j = i+1; j < row_count-1; j++){
				str[j] = str[j+1];
			}
			row_count--;
		}
	}
	
	for(i = 0; i < row_count; i++){
		char *string = str[i];	
		int flag = -1;// 0->target_dependency, command; 1->macro_1; 2->macro_2; 3->include; 4->주석
		if((error_code = regexec(&preg_tab, string, 0, NULL, 0)) != 0){//왜 str이 바뀔까?
			regerror(error_code, &preg_tab, buf, BUFSIZ);
		}
		else{
			flag = 5;
		}

		if((error_code = regexec(&preg_include, string, 0, NULL, 0)) != 0){
			regerror(error_code, &preg_include, buf, BUFSIZ);
		}
		else{
			flag = 3;
		}
		
		if((error_code = regexec(&preg_etc, string, 0, NULL, 0)) != 0){
			regerror(error_code, &preg_etc, buf, BUFSIZ);
		}
		else{
			flag = 4;
		}

		if((error_code = regexec(&preg_macro_1, string, 0, NULL, 0)) != 0){
			regerror(error_code, &preg_macro_1, buf, BUFSIZ);
		}
		else{
			flag = 1;
		}

		if((error_code = regexec(&preg_macro_2, string, 0, NULL, 0)) != 0){
			regerror(error_code, &preg_macro_2, buf, BUFSIZ);
		}
		else{
			flag = 2;
		}

		if((error_code = regexec(&preg_target_dependency, string, 0, NULL, 0)) != 0){
			regerror(error_code, &preg_target_dependency, buf, BUFSIZ);
		}
		else{
			flag = 0;
		}
		
		if(flag < 0){
			fprintf(stderr, "%s:%d: *** missing seperator. Stop.\n", fname, i + 1);
			exit(1);
		}


		switch(flag){
			case 0:
				mk_target_dependency(str[i], pattern_target_dependency);
				break;
			case 1:
				mk_macro_1(str[i], pattern_macro_1);
				break;
			case 2:
				mk_macro_2(str[i], pattern_macro_2);
				break;
			case 3:
				mk_include(str[i], pattern_include);
				break;
			case 4:	//아무것도 안해주면 된다.
				break;
			case 5:
				mk_command(str[i], pattern_tab);
				break;
			default:
				break;
		}
	}

	regfree(&preg_tab);
	regfree(&preg_include);
	regfree(&preg_etc);
	regfree(&preg_macro_1);
	regfree(&preg_macro_2);
	regfree(&preg_target_dependency);
	regfree(&preg_slash);

}

char *replaceAll(char *s, const char *olds, const char *news) {
    char *result, *sr;
    size_t i, count = 0;
    size_t oldlen = strlen(olds); if (oldlen < 1) return s;
    size_t newlen = strlen(news);
    if (newlen != oldlen) {
        for (i = 0; s[i] != '\0';) {
            if (memcmp(&s[i], olds, oldlen) == 0) count++, i += oldlen;
            else i++;
        }
    } else i = strlen(s);
    result = (char *) malloc(i + 1 + count * (newlen - oldlen));
    if (result == NULL) return NULL;
    sr = result;
    while (*s) {
        if (memcmp(s, olds, oldlen) == 0) {
            memcpy(sr, news, newlen);
            sr += newlen;
            s  += oldlen;
        } else *sr++ = *s++;
    }
    *sr = '\0';
    return result;
}
int vacant = 0;
void print_t(int pwd, int row_len, int vacant){
	int i, j, k;
	int first = 1;
	int target_found = 0;
	int present;
	row_length = row_len;
	row_length	+= (strlen(mkfile[pwd].target)+1);
	present = row_length;
	if(vacant == 1){
		for(i = 0; i < row_len; i++)
			printf(" ");
	}

	if(prent[pwd] == 0){//자기 자신 출력
		printf("-%s", mkfile[pwd].target);
		prent[pwd] = 1;
	}
	else{
		printf("\n");
		return;
	}
	j = 0;
	while(mkfile[pwd].dependency[j] != NULL){
		for(i = 0; i < entry; i++){
			if(strcmp(mkfile[i].target, mkfile[pwd].dependency[j]) == 0){
				if(first == 1){ // 첫번째 타겟
					first = 0;
					print_t(i, present, 0);
					target_found = 1;
				}else{ // 두번째부터의 타겟
					print_t(i, present, 1);
					target_found = 1;
				}
			}
		}
		if(target_found == 0)//맨 끝라인 dependency
			if(first == 1){
				printf("-%s\n", mkfile[pwd].dependency[j]);
				first = 0;
			}
			else{
				for(k = 0; k < row_length; k++)
					printf(" ");
				printf("-%s\n", mkfile[pwd].dependency[j]);

			}
		j++;
	}
}
