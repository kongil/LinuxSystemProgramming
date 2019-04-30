#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <regex.h>
#include <string.h>
#include <utime.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

#define TMP_SIZE 1024
#ifdef PATH_MAX
static int pathmax = PATH_MAX;
#else
static int pathmax = 0;
#endif

#define MAX_PATH_GUESSED 1024
#define DIRECTORY_SIZE MAXNAMLEN
#define BUFFER_SIZE 1024

const char * path = ".";
char *call_pathname;							//호출 작업 디렉토리 저장

int if_exist(char *name);
int is_file_or_directory(char *name);
void print_usage();
void get_cwd(char *dir_name);
void find_backslash(char *last);
void if_dir(char *dir_name);
void real_copy(char *source, char *target);
void modify_copy(char *source, char *target);
void copy_all_in_directory(char *source_path, char *target_path);
void copy_all_in_directory_process(char *source_path, char *target_path, int d_N);
void print_fileinfo();
void make_symfile(char *source, char *target);
void print_source_target(char *source, char *target);
int if_exist_overwrite(char *target);
int if_exist_no_overwrite(char *target);//0존재 1 없음


int main(int argc, char * argv[])
{
	int is_file = 0, is_dir = 0;
	int flag_s = 0;										//file
	int flag_r = 0, flag_d =0, flag_N = 0;				//directory			//r, d cannot both exist
	int flag_i = 0, flag_n = 0, flag_l =0, flag_p = 0;	//file+directroy	//i, n cannot both exist
	char * source;
	char * target;
	int c;
	int d_N;
	int do_not_fork = 0;

	if(pathmax == 0){
		if((pathmax = pathconf("/", _PC_PATH_MAX)) < 0){
			pathmax = MAX_PATH_GUESSED;
		}
		else{
			pathmax++;
		}
	}
	source = (char *)malloc(sizeof(char*)*pathmax);
	target = (char *)malloc(sizeof(char*)*pathmax);

	if(argc < 3){
		print_usage();
		exit(1);
	}
	else if(argc == 3){
		source = argv[1];
		target = argv[2];
	}
	else if(argc == 4){
		source = argv[2];
		target = argv[3];
	}
	else if(argc == 5){
		source = argv[3];
		target = argv[4];
	}

//	find_backslash(source);
//	find_backslash(target);

	while((c = getopt(argc, argv, "srd:inlp")) != -1){
		switch(c){
			case 's':
				flag_s++;
				break;
			case 'r':
				flag_r++;
				break;
			case 'd':
				flag_d++;
				d_N = atoi(optarg);
				break;
			case 'i':
				flag_i++;
				break;
			case 'n':
				flag_n++;
				break;
			case 'l':
				flag_l++;
				break;
			case 'p':
				flag_p++;
				break;
		}
	}

	if(flag_s&&(flag_r || flag_d)){
		fprintf(stderr, "You cannot use this both option\n");
		exit(1);
	}
	if(flag_s>1||flag_r>1||flag_d>1||flag_i>1||flag_n>1||flag_l>1||flag_p>1){
		fprintf(stderr, "option overlapped!\n");
		exit(1);
	}
	if(if_exist(source) != 1){
		print_source_target(source, target);
		fprintf(stderr, "ssu_cp: %s: No such file or directory\n", source);
		printf("ssu_cp error\n");
		print_usage();
		exit(1);
	}

	if(flag_s == 1){
		printf("s option is on!\n");
		make_symfile(source, target);
		print_source_target(source, target);
		exit(0);
	}

	if(flag_i == 1){
		printf("i option is on!\n");
		print_source_target(source, target);
		printf("%s\n", source);
		if(if_exist_overwrite(target)){
			real_copy(source, target);
			//overwrite
		}
	}

	if(flag_l == 1){
		printf("l option is on!\n");
		print_source_target(source, target);
		real_copy(source,target);
		modify_copy(source, target);
	}


	if(flag_n == 1){
		printf("n option is on!\n");
		if(if_exist_no_overwrite(target)){	//존재하지 않을 경우
		}
		else{
			exit(0);	//	존재할 경우
		}
	}
	if(flag_p == 1){	/* p option */
		print_source_target(source, target);
		printf("p option is on!\n");
		print_fileinfo(source);
	}
	if(flag_r == 1){	/* r option */
		print_source_target(source, target);
		printf("r option is on!\n");
		call_pathname = malloc(PATH_MAX);
		if(getcwd(call_pathname, PATH_MAX) == NULL){
			fprintf(stderr, "getcwd error\n");
			exit(1);
		}

		char *source_pathname;
		char *target_pathname;
		source_pathname = malloc(PATH_MAX);
		target_pathname = malloc(PATH_MAX);

		if(chdir(source)<0){
			fprintf(stderr,"?");
			exit(1);
		}
		getcwd(source_pathname, PATH_MAX);

		chdir(call_pathname);//원래 작업 디렉토리

		if(mkdir(target, 0777)<0){
		//	fprintf(stderr, "mkdir error or already exist\n");
			//exit(1);
		}
		if(chdir(target)<0){
			fprintf(stderr,"?");
			exit(1);
		}
		getcwd(target_pathname, PATH_MAX);

		copy_all_in_directory(source_pathname, target_pathname);

		chdir(call_pathname);//원래 작업 디렉토리
	}

	if(flag_d == 1){	/* d option */
		int count;
		int i; 
		int d_count = 0;
		struct dirent **namelist;
		if(d_N > 10 || d_N < 0){
			exit(1);
		}

		print_source_target(source, target);
		printf("d option is on!\n");
		call_pathname = malloc(PATH_MAX);
		if(getcwd(call_pathname, PATH_MAX) == NULL){
			fprintf(stderr, "getcwd error\n");
			exit(1);
		}
//
		if((count = scandir(source, &namelist, NULL, alphasort)) == -1){
			fprintf(stderr, "%s Director Scan Error : %s\n", path, strerror(errno));
			exit(1);
		}
		chdir(source);
		for(i = 0 ; i < count; i++){
			struct stat dirbuf;
			if(stat(namelist[i]->d_name, &dirbuf) == -1){
				fprintf(stderr, "%s stat error\n", namelist[i]->d_name);
			}
			if(S_ISDIR(dirbuf.st_mode)){
				d_count++;
			}
		}
		chdir(call_pathname);

		if(d_N < (d_count-2))
			do_not_fork = 1;
			//do not fork
//
		char *source_pathname;
		char *target_pathname;
		source_pathname = malloc(PATH_MAX);
		target_pathname = malloc(PATH_MAX);

		if(chdir(source)<0){
			fprintf(stderr,"?");
			exit(1);
		}
		getcwd(source_pathname, PATH_MAX);

		chdir(call_pathname);//원래 작업 디렉토리

		if(mkdir(target, 0777)<0){
			//fprintf(stderr, "mkdir error or already exist\n");
			//exit(1);
		}
		if(chdir(target)<0){
			fprintf(stderr,"?");
			exit(1);
		}

		getcwd(target_pathname, PATH_MAX);

		if(do_not_fork == 1){
			copy_all_in_directory(source_pathname, target_pathname);
		}
		else{
			copy_all_in_directory_process(source_pathname, target_pathname, d_N);
		}
		chdir(call_pathname);
	}

	if(flag_s||flag_r||flag_d||flag_i||flag_n||flag_l){
		exit(0);
		//옵션 하나라도 있는 경우
	}
	else{

		if(flag_p==0)
			print_source_target(source, target);
		
		if(is_file_or_directory(source)==0){//file
	//		printf("file!");
			real_copy(source, target);
		}
		else if(is_file_or_directory(source)==1){//dir
	//		printf("dir!");
			if(mkdir(target, 0777) < 0){
		//		printf("already exist\n");
			}
		}
	}

	
	exit(0);
}

void find_backslash(char last[]){
	regex_t preg;
	char *pattern = "\\$";
	char *string = last;
	int error_code;
	char buf[BUFSIZ];
	char tmp[TMP_SIZE];

	if((error_code = regcomp(&preg, pattern, REG_EXTENDED)) != 0){
		regerror(error_code, &preg, buf, BUFSIZ);
		fprintf(stderr, "regcomp error\n");
		exit(1);
	}

	if((error_code = regexec(&preg, string, 0, NULL, 0)) != 0){
		regerror(error_code, &preg, buf, BUFSIZ);
		return;
	}
	else{
		printf("\n> ");
		scanf("%s\n", tmp);
		find_backslash(tmp);
		strcat(last, tmp);
	}
	return;
}

int if_exist(char *name){
	if(access(name, F_OK) != 0){
		return 0;
	}
	return 1;
}

void print_usage(){
		fprintf(stderr, "usage :in case of file\n"
						" cp [-i/n][-l][-p]		[source][target]\n"
						" or cp [-s][source][target]\n"
						" in case of directory cp [-i/n][-l][-p][-r][-d][N]\n");
}

int is_file_or_directory(char *source){	// file->0 dir->1
	struct stat statbuf;
	if(lstat(source, &statbuf) < 0){
		fprintf(stderr, "stat error\n");
		return -1;
	}
	if(S_ISDIR(statbuf.st_mode)){
		return 1;
	}
	else if(S_ISREG(statbuf.st_mode)||S_ISBLK(statbuf.st_mode)||S_ISCHR(statbuf.st_mode)||S_ISFIFO(statbuf.st_mode)||S_ISSOCK(statbuf.st_mode)||S_ISLNK(statbuf.st_mode)){
		return 0;
	}
	else{
		printf("not file or directory error!\n");
		return -1;
	}
	
}
void real_copy(char *source, char *target){
	int fd_source, fd_target;
	int length = 0;
	char buf[BUFSIZ];
	struct stat statbuf;
	if((fd_source = open(source, O_RDONLY)) < 0){
		fprintf(stderr, "open source error\n");
		exit(1);
	}

	if((fd_target = creat(target, 0666)) < 0){
		fprintf(stderr, "creat target error\n");
		exit(1);
	}

	while((length = read(fd_source, buf, BUFSIZ)) > 0){
		write(fd_target, buf, length);
	}
}

void modify_copy(char *source, char *target){
	struct utimbuf time_buf;
	struct stat statbuf;
	
	if(stat(source, &statbuf) < 0){
		fprintf(stderr, "stat error\n");
		return;
	}
	
	time_buf.actime = statbuf.st_atime;
	time_buf.modtime = statbuf.st_mtime;

	if(utime(target, &time_buf) < 0){
		fprintf(stderr, "utimes error\n");
		return;
	}

	if(chmod(target, statbuf.st_mode) < 0){
		fprintf(stderr, "chmod error\n");
		return;
	}

	if(chown(target, statbuf.st_uid, statbuf.st_gid) < 0){
		fprintf(stderr, "chown error\n");
		return;
	}

}

void copy_all_in_directory(char *source_path, char *target_path){		//source, target_dir 절대경로
	struct dirent **namelist;
	int count;
	int i;

	if((count = scandir(source_path, &namelist, NULL, alphasort)) == -1){
		fprintf(stderr, "%s Director Scan Error : %s\n", path, strerror(errno));
		return;
	}

	if(count <= 2){
		for(i = 0; i < count; i++)
			free(namelist[i]);
		free(namelist);
		return;
	}

	for(i = 0; i < count; i++){
		char *source_pathname, *target_pathname;
		int fd_source, fd_target;
		int length = 0;
		int exe= 0;
		char buf[BUFSIZ];
		struct stat dirbuf;
		
		chdir(source_path);			//source파일로 들어감
		if(stat(namelist[i]->d_name, &dirbuf) == -1){
			fprintf(stderr, "%s stat error\n", namelist[i]->d_name);
			return;			
		}

		if(S_ISDIR(dirbuf.st_mode)){		//dir
			if(strcmp(namelist[i]->d_name, ".")!=0&&strcmp(namelist[i]->d_name, "..")!=0){
				if(chdir(namelist[i]->d_name)<0){
					fprintf(stderr, "!");
					return;
				};
				chdir(namelist[i]->d_name);
				source_pathname = malloc(PATH_MAX);
				getcwd(source_pathname, PATH_MAX);
		//		printf("%s\n", source_pathname);

				chdir(target_path);
				mkdir(namelist[i]->d_name, 0777);
				chdir(namelist[i]->d_name);
				target_pathname = malloc(PATH_MAX);
				getcwd(target_pathname, PATH_MAX);
		//		printf("%s\n", target_pathname);

				copy_all_in_directory(source_pathname, target_pathname);

			}
		}
		else{							//file
			chdir(source_path);			//원본 있는 곳
			//
			if(access(namelist[i]->d_name, X_OK) == 0){	//실행파일인가?
				exe = 1;	
			}
			//

			if((fd_source = open(namelist[i]->d_name, O_RDONLY)) < 0){
				fprintf(stderr," open error\n");
				return;
			}
			chdir(call_pathname);//..

			if(chdir(target_path) < 0){	//복사할 곳
				fprintf(stderr, "chdir error\n");	//없을 시 mkdir
				return;
			}		
			if(exe == 0){
				if((fd_target = creat(namelist[i]->d_name, 0666)) < 0){
					fprintf(stderr, "creat error\n");
					return;
				}
			}
			else{
				if((fd_target = creat(namelist[i]->d_name, 0777)) < 0){
					fprintf(stderr, "creat error\n");
					return;
				}

			}
			
			while((length = read(fd_source, buf, BUFSIZ)) > 0){
				write(fd_target, buf, length);
			}

		}

		free(source_pathname);
		free(target_pathname);

	}

	for(i = 0; i < count; i++)
		free(namelist[i]);
	free(namelist);

}

void copy_all_in_directory_process(char * source_path, char * target_path, int d_N){
	struct dirent **namelist;
	int count;
	int i;
	int *pid;
	pid = (int *)malloc(sizeof(int)*d_N);
	int pid_count = 0;

	if((count = scandir(source_path, &namelist, NULL, alphasort)) == -1){
		fprintf(stderr, "%s Director Scan Error : %s\n", path, strerror(errno));
		return;
	}
	

	if(count <= 2){
		printf("%s empty directroy\n", source_path);
		for(i = 0; i < count; i++)
			free(namelist[i]);
		free(namelist);
		return;
	}

	for(i = 0; i < count; i++){
		char *source_pathname, *target_pathname;
		int fd_source, fd_target;
		int length = 0;
		int exe = 0;
		char buf[BUFSIZ];
		struct stat dirbuf;
	
		chdir(source_path);			//source파일로 들어감
		if(stat(namelist[i]->d_name, &dirbuf) == -1){
			fprintf(stderr, "%s stat error\n", namelist[i]->d_name);
			return;			
		}

		if(S_ISDIR(dirbuf.st_mode)){		//dir
			if(strcmp(namelist[i]->d_name, ".")!=0&&strcmp(namelist[i]->d_name, "..")!=0){
				
				if((pid[pid_count] = fork()) < 0){
					printf("cannot fork!\n");
				}

				if(pid[pid_count] == 0){
					printf("======================\n");
					if(chdir(namelist[i]->d_name)<0){
						fprintf(stderr, "!");
						return;
					};
					chdir(namelist[i]->d_name);
					source_pathname = malloc(PATH_MAX);
					getcwd(source_pathname, PATH_MAX);
			//		printf("%s\n", source_pathname);
		
					chdir(target_path);
					mkdir(namelist[i]->d_name, 0777);
					chdir(namelist[i]->d_name);
					target_pathname = malloc(PATH_MAX);
					getcwd(target_pathname, PATH_MAX);
					printf("%s\n", target_pathname);
				
					copy_all_in_directory(source_pathname, target_pathname);
					
					printf("=====pid: %d ppid : %d====\n", getpid(), getppid());
					exit(0);
					
				}
				else if(pid[pid_count] > 0){
					int status;
					printf("wait child(%d)\n", pid[pid_count]);
					wait(&status);
					printf("chidl is exit(%d)\n", status);
				}

				pid_count++;

			}

		}
		else{							//file
			chdir(source_path);			//원본 있는 곳

			if((fd_source = open(namelist[i]->d_name, O_RDONLY)) < 0){
				fprintf(stderr," open error\n");
				return;
			}
			//
			if(access(namelist[i]->d_name, X_OK) == 0){	//실행파일인가?
				exe = 1;	
			}
			//

			chdir(call_pathname);//..

			if(chdir(target_path) < 0){	//복사할 곳
				fprintf(stderr, "chdir error\n");	//없을 시 mkdir
				return;
			}		

			if(exe == 0){
				if((fd_target = creat(namelist[i]->d_name, 0666)) < 0){
					fprintf(stderr, "creat error\n");
					return;
				}
			}
			else{
				if((fd_target = creat(namelist[i]->d_name, 0777)) < 0){
					fprintf(stderr, "creat error\n");
					return;
				}
			}

			while((length = read(fd_source, buf, BUFSIZ)) > 0){
				write(fd_target, buf, length);
			}

		}

		free(source_pathname);
		free(target_pathname);

	}


	for(i = 0; i < count; i++)
		free(namelist[i]);
	free(namelist);

}

void print_fileinfo(char * source){
	
	struct stat statbuf;
	struct tm *t;
	struct passwd *user_info;
	struct group *group_info;

	if(stat(source, &statbuf) < 0){
		fprintf(stderr, "stat error\n");
	}
	
	printf("*********************file info**********************\n");
	printf("파일 이름 : %s\n", source);
	printf("데이터의 마지막 읽은 시간: ");
	t = localtime(&statbuf.st_atime);
	printf("%04d.%02d.%02d %02d:%02d:%02d\n", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	printf("데이터의 마지막 수정 시간: ");
	t = localtime(&statbuf.st_mtime);
	printf("%04d.%02d.%02d %02d:%02d:%02d\n", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	printf("데이터의 마지막 변경 시간: ");
	t = localtime(&statbuf.st_ctime);
	printf("%04d.%02d.%02d %02d:%02d:%02d\n", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	user_info = getpwuid(statbuf.st_uid);
	printf("OWNER : %s\n", user_info->pw_name);
	group_info = getgrgid(statbuf.st_gid);
	printf("GROUP : %s\n", group_info->gr_name);
	printf("file size : %ld\n",	statbuf.st_size);

	return;
}
void make_symfile(char * source, char * target){
	if(is_file_or_directory(source) == 0){	//file
		if(symlink(source, target) < 0){
			fprintf(stderr, "symlink error\n");
			exit(1);
		 }
		else{

		}
	}
}

void print_source_target(char *source, char *target){
	printf("target : %s\nsrc : %s\n", target, source);
}

int if_exist_overwrite(char *target){	// 1 overwrite or write 0 no
	char answer;
	if(access(target, F_OK) == 0){
		printf("overwrite %s (y/n)?\n", target);
		scanf("%c", &answer);
		if(answer == 'y'){
			
			return 1;
		}
		else{
			return 0;
		}
	}
	else{
		return 1;
	}
}

int if_exist_no_overwrite(char *target){
	if(access(target, F_OK) == 0){
		return 0;
	}
	return 1;
}
