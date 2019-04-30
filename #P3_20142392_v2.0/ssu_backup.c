#include <sys/stat.h>
#include <sys/types.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>
#include <utime.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>

#define STR_MAX 1024
#define PATH_MAXIMUM 255
#define LOG_SIZE 1024
#define BUF_SIZE 1000
#define MAX_COUNT 1024

typedef struct fileinfo{
	char file_path[PATH_MAX];
	char filename[STR_MAX];
	char pathname[PATH_MAX];	
	char origin_pathname[PATH_MAX];	
	char log_pathname[PATH_MAX];
	int i;
}fileinfo;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static int pid = 0;
static time_t ex_mtime = 0;
static time_t ex_mtimes[MAX_COUNT];
static time_t ex_ctime = 0;
const char *path = ".";
struct tm *t;
int flag_c = 0, flag_r = 0, flag_n = 0, flag_m = 0, flag_d = 0;
int repeated;

int ssu_daemon_init(void);
void ascii_to_hex(const char *ascii, char *hex);
void make_backup_log(const char *current_pathname, const char *fname, struct tm *backup_t);
void make_backup_log_dir(const char *current_pathname, const char *fname_path, struct tm *t_backup, int i);
void delete_log(const char *backup_pathname, const char *fname, struct tm *backup_t);
void copy_source_target(const char *pathname_source, const char *pathname_target);
void filename_to_pathname(const char *dirname, const char *filename, char *pathname);
void write_to_backup_log(const char *fname, const char *log);
void repeat_check(char *ssu_backup_path);
int getCmdLine(char *file, char *buf);
void when_signaled(int signo);
int if_modified(const char *target);	//modified 1 unmodified 0
void search_in_dir(char *pathname, char *target, int n_limit);
void remove_first_in_dir(char *pathname, struct tm *tm, int n_limit);
void call_diff(char *target, char *backup_dir_pathname);
void recovery();
void daemon_in_dir(char *pathname, char *origin_pathname);
void *make_thread(void *args);
void ex_directory(char *pathname);
void path_to_file(const char *pathname, char *filename);
void except_(const char *filename, char *fname_except);

int main(int argc, char *argv[])
{
	int c;
	int ssu_daemon_success = 1;
	char current_path[STR_MAX];				//	/home/kongil/project3						
	char *backup_filename[STR_MAX];
	char hex_target_pathname[PATH_MAXIMUM];	//	62931412315				
	char process_pathname[PATH_MAXIMUM];	//	./backup
	char backup_dir_pathname[PATH_MAXIMUM];	//	/home/kongil/project3/backup
	char target_pathname[PATH_MAXIMUM];		//	/home/kongil/project3/target.txt
	char hex_filename[PATH_MAXIMUM];		//	62931412315_123141234
	char hex_pathname[PATH_MAXIMUM];		//	/home/kongil/project3/backup/target.txt	
	char hex_pathname_backup[PATH_MAXIMUM];	//	/home/kongil/project3/backup/629314...
	char *target;
	int period = 0;
	int n_limit;
	char backup_time[15];
	struct timeval backup_t;
	int fd;
	char backup_log[LOG_SIZE];
	struct stat statbuf;

	while ((c = getopt(argc, argv, "crn:md")) != -1) {
		switch(c) {
			case 'c':
				flag_c = 1;
				break;
			case 'r':
				flag_r = 1;
				break;
			case 'n':
				flag_n = 1;
				n_limit = atoi(optarg);
				break;
			case 'm':
				flag_m = 1;
				break;
			case 'd':
				flag_d = 1;
				break;
			case '?':
				break;
		}
	}

	if(argc < 3) {
		fprintf(stderr, "usage : %s <filename> <option>\n", argv[0]);
		exit(1);
	}
	else if(flag_n) {
		target = argv[3];
		period = (atoi)(argv[4]);
	}
	else if(flag_m||flag_d) {
		target = argv[2];
		period = (atoi)(argv[3]);
	}
	else if(flag_c || flag_r){
		target = argv[2];
	}
	else {
		target = argv[1];
		period = (atoi)(argv[2]);
	}

	
	/*3<=period<=10*/
	if (((period < 3) || (period > 10)) && (flag_c != 1) && (flag_r != 1)) {
		fprintf(stderr, "period must be in 3~10\n");
		exit(1);
	}

	/*if get signal*/
	signal(SIGUSR1, when_signaled); 

	/*make target from path*/
	char *token;
	char tmp[PATH_MAX];
	char last[PATH_MAX];
	char path[PATH_MAX] = "";
	int cnt = 0;

	strcpy(tmp, target);
	if((token = strtok(tmp, "/"))) {
		if(strcmp(token, ".")==0) {	//sangdae
			getcwd(path, PATH_MAX);
			strcpy(last ,strtok(NULL, "/"));
			strcpy(target, last);
		}
		else {
			strcat(path, token);
			while ((token = strtok(NULL, "/"))) {
				cnt++;
				strcat(path, "/");
				strcat(path, token);
				strcpy(last, token);
			}
			if (cnt == 0) {
				strcpy(target, path);
				strcpy(path, "/");
			}
		}
	}

	/*get current ssu_backup directory pathname, target_pathname*/
	getcwd(current_path, PATH_MAXIMUM);		

	//filename_to_pathname(current_path, "ssu_backup", process_pathname);
	strcpy(process_pathname, argv[0]);

	filename_to_pathname(current_path, "backup", backup_dir_pathname);

	filename_to_pathname(current_path, target, target_pathname);

	ascii_to_hex(target_pathname, hex_target_pathname);

	/*if daemon repeated*/
	repeat_check(process_pathname);

	if(repeated == 0)
		printf("Daemon process initialization\n");

	/*flag_c*/
	if (flag_c) {
		call_diff(target, backup_dir_pathname);
	}
	/*flag_r*/
	if (flag_r) {
		recovery(target, backup_dir_pathname);
	}

	/*execute daemon process*/
	if (ssu_daemon_init() < 0) {
		ssu_daemon_success = 0;
		fprintf(stderr, "ssu_daemon_init fail\n");
		exit(1);
	}

	/*make backup directory*/
	if (mkdir(backup_dir_pathname, 0777) < 0) {
	}

	/*n option is on!*/
	if (flag_n) {
		search_in_dir(backup_dir_pathname, target, n_limit);
	}

	/*make log or syslog and make backupfile*/
	while (1) {
		if (ssu_daemon_success > 0) {
			/*get execute time*/
			gettimeofday(&backup_t, NULL);
			t = localtime(&backup_t.tv_sec);

			/*if dir*/
			if (flag_d) {
				if (stat(target_pathname, &statbuf) < 0) {
					fprintf(stderr, "stat error\n");
					exit(1);
				}
				if (!S_ISDIR(statbuf.st_mode)) {
					fprintf(stderr, "it's no directory!\n");
					exit(1);
				}
				else {
					char origin_pathname[PATH_MAX];
					strcpy(origin_pathname, target_pathname);
					ex_directory(origin_pathname);
					/*main in d option*/
					daemon_in_dir(target_pathname, origin_pathname);
				}
			}
			else
			{
				/*make hexfile*/
				strcpy(hex_filename, "");
				strcat(hex_filename, hex_target_pathname);
				/*_Time_stamp*/
				strcat(hex_filename, "_");
				sprintf(backup_time, "%02d%02d%02d%02d%02d", t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
				strcat(hex_filename, backup_time);
	
				/*if m option is not on*/
				if ((flag_n == 0) &&( (flag_m == 0) || if_modified(target_pathname))) {
					/*write log*/
					make_backup_log(current_path, target, t);
					
					/*copy hex_pathaname_backup & make backup file*/
					filename_to_pathname(backup_dir_pathname, hex_filename, hex_pathname_backup);
					copy_source_target(target_pathname, hex_pathname_backup);
				}
				/*if n option is on*/
				else if (flag_n) {
					/*remove ex_backup & write log*/
					remove_first_in_dir(backup_dir_pathname, t, n_limit);
	
					t = localtime(&backup_t.tv_sec);
					make_backup_log(current_path, target, t);
	
					/* & copy hex_pathaname_backup & make backup file*/
					filename_to_pathname(backup_dir_pathname, hex_filename, hex_pathname_backup);
					copy_source_target(target_pathname, hex_pathname_backup);
				}
			}

		}
		else {
			openlog("ssu_backup", LOG_PID, LOG_LPR);
			syslog(LOG_ERR, "open failed %m");
			exit(1);
		}

		sleep(period);

	}
	exit(0);
}

int ssu_daemon_init(void) {
	pid_t pid;
	int fd, maxfd;

	umask(0);

	if ((pid = fork()) < 0) {
		fprintf(stderr, "fork error\n");
		exit(1);
	}
	else if (pid != 0)
		exit(0);
	
	setsid();

	maxfd = getdtablesize(); /*can open socket num*/

	printf("process %d running as ssu_backup daemon\n", getpid());

	for (fd = 0; fd < maxfd; fd++)
		close(fd);

	fd = open("/dev/null", O_RDWR);
	dup(0);
	dup(0);

	chdir("/");

	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);


	return 0;
}

void ascii_to_hex(const char *ascii, char *hex) {
     char tmp[20];
	 int length = strlen(ascii);
     int i;
     int j = 0;
     for (i = 0; i < length; i++) {
         sprintf(tmp, "%.2x", ascii[i]);
         hex[j++] = tmp[0];
         hex[j++] = tmp[1];
     }
     hex[j] = '\0';
}

void hex_to_ascii(const char *hex, char *ascii) {
	char tmp[PATH_MAXIMUM];
	char *fname;
	char *day;

	strcpy(tmp, hex);
	fname = strtok(tmp, "_");
	day = strtok(NULL, "_");
}

void make_backup_log(const char *current_pathname, const char *fname_path, struct tm *t_backup) {
	struct stat statbuf;
	struct tm *t_mtime;
	struct timeval tv;	
	char log_tmp[LOG_SIZE];
	char log_tmp_2[LOG_SIZE];
	char fname[STR_MAX];
	int fd;
	int fsize;
	
	/*if /.././/./.././abc*/
	path_to_file(fname_path, fname); 

	if (chdir(current_pathname) < 0) {
		fprintf(stderr, " %s chdir error\n", current_pathname);
		//exit(1);
	}

	/*if flag_d*/
	gettimeofday(&tv, NULL);
	t_backup = localtime(&tv.tv_sec);
	
	sprintf(log_tmp, "[%02d%02d %02d:%02d:%02d] %s ",
			t_backup->tm_mon+1, t_backup->tm_mday, t_backup->tm_hour, t_backup->tm_min, t_backup->tm_sec, fname);

	/*if target deleted */
	if ((fd = open(fname_path, O_RDONLY)) < 0) {	
		strcat(log_tmp, "is deleted\n");
		write_to_backup_log("backup_log.txt", log_tmp);
		exit(1);
	}

	fsize = lseek(fd, 0, SEEK_END);

	if (stat(fname_path, &statbuf) < 0) {
		fprintf(stderr, "stat error\n");
	}

	/*if target modified */
	if (ex_mtime == 0) { //first
		ex_mtime = statbuf.st_mtime;
	}
	else if(ex_mtime == statbuf.st_mtime){									//changed
		ex_mtime = statbuf.st_mtime;
	}
	else {
		strcat(log_tmp, "is modified ");
		ex_mtime = statbuf.st_mtime;
	}
	
	t_mtime = localtime(&statbuf.st_mtime);

	sprintf(log_tmp_2, "[size:%d/mtime:%02d%02d %02d:%02d:%02d]\n",
		  fsize, t_mtime->tm_mon+1, t_mtime->tm_mday, t_mtime->tm_hour, t_mtime->tm_min, t_mtime->tm_sec);

	strcat(log_tmp, log_tmp_2);

	close(fd);
	
	write_to_backup_log("backup_log.txt", log_tmp);

}

void make_backup_log_dir(const char *current_pathname, const char *fname_path, struct tm *t_backup, int i) {
	struct stat statbuf;
	struct tm *t_mtime;
	struct timeval tv;	
	char log_tmp[LOG_SIZE];
	char log_tmp_2[LOG_SIZE];
	char fname[STR_MAX];
	int fd;
	int fsize;
	
	/*if /.././/./.././abc*/
	path_to_file(fname_path, fname); 

	if (chdir(current_pathname) < 0) {
		fprintf(stderr, " %s chdir error\n", current_pathname);
		//exit(1);
	}

	/*if flag_d*/
	gettimeofday(&tv, NULL);
	t_backup = localtime(&tv.tv_sec);
	
	sprintf(log_tmp, "[%02d%02d %02d:%02d:%02d] %s ",
			t_backup->tm_mon+1, t_backup->tm_mday, t_backup->tm_hour, t_backup->tm_min, t_backup->tm_sec, fname);

	/*if target deleted */
	if ((fd = open(fname_path, O_RDONLY)) < 0) {	
		fprintf(stderr, "here open error\n");
		strcat(log_tmp, "is deleted\n");
		write_to_backup_log("backup_log.txt", log_tmp);
		exit(1);
	}

	fsize = lseek(fd, 0, SEEK_END);

	if (stat(fname_path, &statbuf) < 0) {
		fprintf(stderr, "stat error\n");
	}
	else {
		ex_mtime = statbuf.st_mtime;
	}

	/*if target modified */
	if (ex_mtimes[i] == 0) { //first
		ex_mtimes[i] = statbuf.st_mtime;
	}
	else if (ex_mtimes[i] != statbuf.st_mtime) {									//changed
		strcat(log_tmp, "is modified ");
		ex_mtimes[i] = statbuf.st_mtime;
	}
	
	t_mtime = localtime(&ex_mtimes[i]);

	sprintf(log_tmp_2, "[size:%d/mtime:%02d%02d %02d:%02d:%02d]\n",
		  fsize, t_mtime->tm_mon+1, t_mtime->tm_mday, t_mtime->tm_hour, t_mtime->tm_min, t_mtime->tm_sec);

	strcat(log_tmp, log_tmp_2);

	close(fd);
	
	write_to_backup_log("backup_log.txt", log_tmp);

}

void delete_log(const char *backup_pathname, const char *fname, struct tm *t_backup) {
	struct stat statbuf;
	struct tm *t_mtime;
	struct timeval tv;
	char log_tmp[LOG_SIZE];
	char log_tmp_2[LOG_SIZE];
	int fd;
	int fsize;
	char current[2000];

	getcwd(current, 2000);

	sprintf(log_tmp, "[%02d%02d %02d:%02d:%02d] %s ",
			t_backup->tm_mon+1, t_backup->tm_mday, t_backup->tm_hour, t_backup->tm_min, t_backup->tm_sec, "Delete backup");

	if (chdir("backup") < 0) {
		fprintf(stderr, "backup chdir error\n");
		exit(1);
	}
	getcwd(current, 2000);
	if ((fd = open(fname, O_RDONLY)) < 0) {	
		fprintf(stderr, "delete_log_here open error\n");
	}

	fsize = lseek(fd, 0, SEEK_END);

	if (stat(fname, &statbuf) < 0) {
		fprintf(stderr, "stat error\n");
	}
	
	t_mtime = localtime(&statbuf.st_mtime);

	sprintf(log_tmp_2, "[size:%d/btime:%02d%02d %02d:%02d:%02d]\n",
		  fsize, t_mtime->tm_mon+1, t_mtime->tm_mday, t_mtime->tm_hour, t_mtime->tm_min, t_mtime->tm_sec);

	strcat(log_tmp, log_tmp_2);

	close(fd);
	chdir("..");
	getcwd(current, 2000);
	
	write_to_backup_log("backup_log.txt", log_tmp);

}

void copy_source_target(const char *pathname_source, const char *pathname_target) {
	int fd_source, fd_target;
	int length;
	char buf[BUF_SIZE];

	if ((fd_source = open(pathname_source, O_RDONLY)) < 0) {
		fprintf(stderr, "open error2\n");
		exit(1);
	}
	
	if ((fd_target = open(pathname_target, O_CREAT|O_TRUNC|O_RDWR, 0666)) < 0) {
		fprintf(stderr, "%s open error3\n", pathname_target);
		exit(1);
	}

	while((length = read(fd_source, buf, BUF_SIZE)) > 0) {
		if (write(fd_target, buf, length) < 0) {
			fprintf(stderr, "write error\n");
			exit(1);
		}
	}

	close(fd_source);
	close(fd_target);

}

void filename_to_pathname(const char *dirname, const char *filename, char *pathname) {
	strcpy(pathname, dirname);
	strcat(pathname, "/");
	strcat(pathname, filename);
}

void write_to_backup_log(const char *fname, const char *log) {
	int fd;

	if ((fd = open(fname, O_WRONLY| O_CREAT| O_APPEND, 0666)) < 0) {
		fprintf(stderr, "logopen2 error\n");
		exit(1);
	}
	if (write(fd, log, strlen(log)) < 0) {
		fprintf(stderr, "write error\n");
		exit(1);
	}
	close(fd);
}

void repeat_check(char *ssu_backup_path){
	DIR *dir;                       //  /proc/pid/ 를 가리킬 DIR* 변수
	struct dirent *entry;        // 각 파일의 inode를 통해 파일을 선택할 dirent 구조체
    struct stat fileStat;          // 파일의 정보를 담는 구조체
    int pid;                         // 프로세스는 /proc 디렉토리에 자신의 pid로 파일을 담아 둡니다.
	int ex_pid;
    char cmdLine[256];
    char tempPath[256];
	int count = 0;

    dir = opendir("/proc");   //  /proc이란 디렉토리 스트름이 대한 포인터가 반환되었습니다.
    while ((entry = readdir(dir)) != NULL) {   //   /proc에 존재하는 파일들을 차례대로 읽습니다.
	    lstat(entry->d_name, &fileStat);          // DIR*가 가리키는 파일의 state 정보를 가져온다.
										  
		if (!S_ISDIR(fileStat.st_mode))            // is dir? 디렉토리인지 확인한다.
	        continue;                                    // 프로세스는 /proc에 자신의 pid로 디렉토리를
	                                         // 만드는 점을 안다면 이해하실거라 생각합니다.

		pid = atoi(entry->d_name);          // 프로세스(디렉토리)인것을 확인하면, 숫자로 반환한다.
        if (pid <= 0) continue;                       // 숫자가 아니라면 다시 continue;
		    sprintf(tempPath, "/proc/%d/cmdline", pid); // cmdline :: 프로세스 이름이 적힌파일
        getCmdLine(tempPath, cmdLine);     // /proc/pid/cmdline에서 프로세스의 이름을
             // 가져오는 함수로 보냅니다. 아래에 정의되어있습니다.
        //printf("[%d] %s\n", pid, cmdLine);
		
		if (strcmp(cmdLine, ssu_backup_path) == 0) {
			count++;
			if (count == 1) {
				ex_pid = pid;
			}
			if (count == 2) {
				repeated = 1;
				printf("Send signal to ssu_backup process<%d>\n", ex_pid);
				kill(ex_pid, SIGUSR1);
				break;
			}
		}
	}
	closedir(dir);
}

int getCmdLine(char *file, char *buf) {
    FILE *srcFp;
    int i;
    srcFp = fopen(file, "r");            //   /proc/pid/cmdline에 이름이 있습니다.

    memset(buf, 0, sizeof(buf));
    fgets(buf, 256, srcFp);
    fclose(srcFp);
}

void when_signaled(int signo) {
	struct timeval signal_time;
	struct tm *t_backup;
	char *log_tmp;
	gettimeofday(&signal_time, NULL);
	t_backup = localtime(&signal_time.tv_sec);

	sprintf(log_tmp, "[%02d%02d %02d:%02d:%02d] ssu_backup<%d> exit\n",
			t_backup->tm_mon+1, t_backup->tm_mday, t_backup->tm_hour, t_backup->tm_min, t_backup->tm_sec, getpid());
	write_to_backup_log("backup_log.txt", log_tmp);	

	exit(0);
}

int if_modified(const char *target_path) {
	struct stat statbuf;
	int fd;
	if (stat(target_path, &statbuf) < 0) {
		fprintf(stderr, "%s if modified stat error\n", target_path);
		exit(1);
	}

	/*if target modified */
	if (ex_mtime == 0) { 
		ex_mtime = statbuf.st_mtime;
		return 0;
	}
	else if (ex_mtime == statbuf.st_mtime) {
		return 0;
	}
	else {
		return 1;
	}
	return -1;//abnormal
}

void search_in_dir(char *pathname, char *target, int n_limit) {
	struct dirent **namelist;
	int count = 0;
	int same_count = 0;
	int i;
	char call_path[PATH_MAX];
	char samearr[MAX_COUNT][PATH_MAX];
	struct tm *t;

	if (chdir(pathname) < 0){
		fprintf(stderr, "chdir error in search_indir\n");
		exit(1);
	}

	if ((count = scandir(".", &namelist, NULL, alphasort)) == -1)
	{
		fprintf(stderr, "%s Directory Scan Error : %s\n", path, strerror(errno));
		return;
	}

	for (i = 0 ; i < count; i++) {
		if(strcmp(target, namelist[i]->d_name) == 0) {
			strcpy(samearr[same_count], namelist[i]->d_name);
			same_count++;	
		}
	}
	if (same_count > n_limit) {
		for (i = 0; i < same_count - n_limit; i++) {
			printf("same : %s\n", samearr[i]);
			/*remove except n recent files by alphasort*/
			remove(samearr[i]);
		}
	}
	for(i = 0; i < count; i++)
		free(namelist[i]);
	free(namelist);

	if (chdir("/") < 0){
		fprintf(stderr, "chdir error in search_indir\n");
		exit(1);
	}
	return;
}

void remove_first_in_dir(char *pathname, struct tm *tm, int n_limit) {
	struct dirent **namelist;
	int count = 0;
	int same_count = 0;
	int i;
	char call_path[PATH_MAX];
	struct tm *t;

	if (chdir(pathname) < 0){
		fprintf(stderr, "chdir error in search_indir\n");
		exit(1);
	}

	if ((count = scandir(".", &namelist, NULL, alphasort)) == -1)
	{
		fprintf(stderr, "%s Directory Scan Error : %s\n", path, strerror(errno));
		return;
	}

	chdir("..");
	for (i = 2 ; i < count; i++) {
		/*remove except n recent files*/
		delete_log(".",namelist[i]->d_name, tm); 
		chdir(pathname);
		remove(namelist[i]->d_name);
		chdir("..");
	}
	for(i = 0; i < count; i++)
		free(namelist[i]);
	free(namelist);

	if (chdir("/") < 0){
		fprintf(stderr, "chdir error in search_indir\n");
		exit(1);
	}
	return;
}

void call_diff(char *target, char *backup_dir_pathname){
	pid_t pid;
	struct dirent **namelist;
	int count = 0;
	int i;
	char current_path[PATH_MAX];
	char target_path[PATH_MAX];
	char call_path[PATH_MAX];
	char hex[PATH_MAXIMUM];
	char *in_dir_file;
	char recent_file[PATH_MAXIMUM] = "";
	char recent_pathname[PATH_MAXIMUM] = "";
	char tmp[PATH_MAXIMUM];
	char *etc;
	char buf[BUF_SIZE];
	
	getcwd(current_path, PATH_MAX);
	filename_to_pathname(current_path, target, target_path);
	ascii_to_hex(target_path, hex);

	/*get recent backup file*/
	if (chdir(backup_dir_pathname) < 0){
		fprintf(stderr, "chdir error in search_indir\n");
		exit(1);
	}

	if ((count = scandir(".", &namelist, NULL, alphasort)) == -1)
	{
		fprintf(stderr, "%s Directory Scan Error : %s\n", path, strerror(errno));
		return;
	}

	for (i = 2 ; i < count; i++) {
		strcpy(tmp, namelist[i]->d_name);
		in_dir_file = strtok(tmp, "_");
		if(strcmp(in_dir_file, hex) == 0) {
			strcpy(recent_file, namelist[i]->d_name);
			etc = strtok(NULL, "_");
		}
	}


	for(i = 0; i < count; i++)
		free(namelist[i]);
	free(namelist);

	if(strcmp(recent_file, "") == 0){
		exit(0);
	}
	else {
		strcpy(recent_pathname, backup_dir_pathname);
		strcat(recent_pathname, "/");	
		strcat(recent_pathname, recent_file);
	}
	
	strcat(target, "_"); 
	strcat(target, etc); 

	printf("<Compare with backup file[%s]>\n", target);
	/*fork & exec*/
	char *args[] ={"diff", target_path, recent_pathname, NULL};
	if((pid = fork()) < 0){
		fprintf(stderr, "fork error\n");
		exit(1);
	}
	else if(pid == 0){
		if (execv("/usr/bin/diff", args) < 0) {
			fprintf(stderr, "exec error\n");
			exit(1);
		}
	}
	else {
		wait(0);
		exit(0);
	}
}

void recovery(char *target, char *backup_dir_pathname){
	struct dirent **namelist;
	int count = 0;
	int match_count = 0;
	int i, num;
	int fd;
	char current_path[PATH_MAX];
	char target_path[PATH_MAX];
	char call_path[PATH_MAX];
	char hex[PATH_MAXIMUM];
	char target_hex[PATH_MAXIMUM];
	char *in_dir_file;
	char recent_file[PATH_MAXIMUM] = "";
	char recent_pathname[PATH_MAXIMUM] = "";
	char recover_time[MAX_COUNT][STR_MAX];
	char hex_recover_time[MAX_COUNT][STR_MAX];
	int recover_fsize[MAX_COUNT];
	char tmp[PATH_MAXIMUM];
	char *etc;
	char buf[BUF_SIZE];

	getcwd(current_path, PATH_MAX);
	filename_to_pathname(current_path, target, target_path);
	ascii_to_hex(target_path, hex);

	if (chdir(backup_dir_pathname) < 0){
		fprintf(stderr, "chdir error in search_indir\n");
		exit(1);
	}

	if ((count = scandir(".", &namelist, NULL, alphasort)) == -1)
	{
		fprintf(stderr, "%s Directory Scan Error : %s\n", path, strerror(errno));
		return;
	}

	for (i = 2 ; i < count; i++) {
		strcpy(tmp, namelist[i]->d_name);
		in_dir_file = strtok(tmp, "_");
		if(strcmp(in_dir_file, hex) == 0) {
			etc = strtok(NULL, "_");
			match_count++;
			strcpy(recover_time[match_count], etc);
		}
	}
	strcpy(target_hex, hex);

	for(i = 0; i < count; i++)
		free(namelist[i]);
	free(namelist);


	strcpy(recent_pathname, backup_dir_pathname);
	strcat(recent_pathname, "/");	
	strcat(recent_pathname, recent_file);
	
	for (i = 1; i<= match_count; i++) {
		strcpy(tmp, recover_time[i]);
		strcpy(recover_time[i], target);
		strcat(recover_time[i], "_");
		strcat(recover_time[i], tmp);

		strcpy(hex_recover_time[i], target_hex);
		strcat(hex_recover_time[i], "_");
		strcat(hex_recover_time[i], tmp);
	}
	/*size*/
	for (i = 1; i <= match_count; i++) {
		int fd;
		if ((fd = open(hex_recover_time[i], O_RDONLY)) < 0) {
			char c[STR_MAX];
			getcwd(c, STR_MAX);
			printf("cwd : %s\n", c);
			fprintf(stderr, "%s open error\n", hex_recover_time[i]);
			exit(1);
		}
		recover_fsize[i] = lseek(fd, 0, SEEK_END);
		close(fd);
	}

	printf("<%s backup list>\n", target);
	printf("0. exit\n");
	for (i = 1; i <= match_count; i++) {
		printf("%d. %s [size:%d]\n", i, recover_time[i], recover_fsize[i]);
	}
	printf("input : ");
	scanf("%d", &num);
	if (num == 0)
		exit(0);
	else {
		printf("Recovery backup file...\n");
		printf("[%s]\n", target);
		if ((fd = open(hex_recover_time[num], O_RDONLY)) < 0) {
			fprintf(stderr, "open error\n");
			exit(1);
		}

		while (read(fd, buf, BUF_SIZE) > 0) {
			printf("%s", buf);
		}
		copy_source_target(hex_recover_time[num], target_path);
	}

	exit(0);
}

void daemon_in_dir(char *pathname, char *origin_pathname) {
	struct dirent **namelist;
	int count = 0;
	int i, j;
	int status;
	char call_path[PATH_MAX];
	char log_pathname[PATH_MAX];
	char file_pathname[PATH_MAXIMUM];	//	file path from root
	char tmp[PATH_MAXIMUM];
	struct tm *t;
	struct stat statbuf;
	struct fileinfo fi[200];
	pthread_t tid[MAX_COUNT];

	getcwd(call_path, 200);

	if ((count = scandir(pathname, &namelist, NULL, alphasort)) == -1)
	{
		fprintf(stderr, "%s Directory Scan Error : %s\n", path, strerror(errno));
		exit(1);
	}

	/*log_pathname*/
	strcpy(log_pathname, origin_pathname);
	//ex_directory(log_pathname);
	if(strcmp(pathname, "/"))
		strcat(log_pathname, "/");
	strcat(log_pathname, "backup_log.txt");


	for (i = 2 ; i < count; i++) {
		/*file_pathname*/
		strcpy(file_pathname, pathname);
		if(strcmp(pathname, "/"))
			strcat(file_pathname, "/");
		strcat(file_pathname, namelist[i]->d_name);

		/*make fileinfo*/
		strcpy(fi[i].pathname, pathname);
		strcpy(fi[i].filename, namelist[i]->d_name);
		strcpy(fi[i].file_path, file_pathname);
		strcpy(fi[i].log_pathname, log_pathname);
		strcpy(fi[i].origin_pathname, origin_pathname);
		fi[i].i = i;


		if (lstat(file_pathname, &statbuf) < 0){
			fprintf(stderr, "lstat error\n");
			exit(1);
		}
		if (S_ISDIR(statbuf.st_mode)) {
			daemon_in_dir(file_pathname, origin_pathname);
		}
		else{
			if (pthread_create(&tid[i], NULL, make_thread, (void *)(&fi[i])) != 0) {
				fprintf(stderr, "pthread error\n");
				exit(1);
			}
			/*
			for (j = 2; j < i; j++) {
				pthread_join(tid[i], (void *)&status);
			}
			*/
		}
	}
	for(i = 0; i < count; i++)
		free(namelist[i]);
	free(namelist);

	return;
}

void *make_thread(void *args) {
	pthread_t tid;
	tid = pthread_self();
	struct fileinfo *f = (args);
	struct fileinfo hex_file;
	char hex_filename[PATH_MAXIMUM];
	char current[PATH_MAXIMUM];
	char backup_time[STR_MAX];

	ascii_to_hex(f->file_path, hex_filename);
	/*_Time_stamp*/
	strcat(hex_filename, "_");
	sprintf(backup_time, "%02d%02d%02d%02d%02d", t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	strcat(hex_filename, backup_time);
	strcpy(hex_file.filename, hex_filename);
	strcpy(hex_file.pathname, f->origin_pathname);
	strcat(hex_file.pathname, "/backup");
	filename_to_pathname(hex_file.pathname, hex_file.filename, hex_file.file_path);
	strcpy(hex_file.log_pathname, f->log_pathname);
	hex_file.i = f->i;

	/*copy backup file*/
	copy_source_target(f->file_path, hex_file.file_path);
		
	ex_directory(hex_file.pathname);
	/*mutex thread lock*/
	pthread_mutex_lock(&mutex);
	make_backup_log_dir(hex_file.pathname, f->file_path, t, hex_file.i);
	/*mutex thread unlock*/
	pthread_mutex_unlock(&mutex);
}

void ex_directory(char *pathname) {
	char tmp[STR_MAX];
	char tmp2[STR_MAX];
    char target[STR_MAX];
    strcpy(tmp, pathname);
    strcpy(tmp2, pathname);
    char *token;
    token = strtok(tmp, "/");
    while ((token = strtok(NULL, "/")) != 0) {
         strcpy(target, token);
    }
 
    strcpy(pathname, "/");
    token = strtok(tmp2, "/");
    strcat(pathname, token);
    while ((token = strtok(NULL, "/")) != 0) {
	    if(strcmp(token, target))
        {
	        strcat(pathname, "/");
            strcat(pathname, token);
        }
    }
    return;
}

void path_to_file(const char *pathname, char *filename) {
	char tmp[PATH_MAX];
    char *token;
	char last[STR_MAX];
	int flag = 0;
	strcpy(tmp, pathname);

    token = strtok(tmp, "/");
    while ((token = strtok(NULL, "/")) != 0) {
		flag++;
		strcpy(last, token);
    }
	if(flag)
		strcpy(filename, last);
	else {
		token = strtok(tmp, "/");
		strcpy(filename, token);
	}
}

void compare_file(const char *path_source, char *path_target) {
}
void except_(const char *filename, char *fname_except) {
}
