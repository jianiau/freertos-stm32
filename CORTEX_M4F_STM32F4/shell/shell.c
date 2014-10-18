#include "shell.h"
#include <stddef.h>
#include "clib.h"
#include <string.h>
#include "fio.h"
#include "filesystem.h"
#include "romfs.h"
#include "hash-djb2.h"

#include "FreeRTOS.h"
#include "task.h"
#include "host.h"
#include "bufbomb.h"

extern const unsigned char _sromfs;
extern const unsigned char _sromind;
extern uint32_t pwd_hash; // /romfs

typedef struct {
	const char *name;
	cmdfunc *fptr;
	const char *desc;
} cmdlist;

void ls_command(int, char **);
void pwd_command(int, char **);
void man_command(int, char **);
void cat_command(int, char **);
void ps_command(int, char **);
void host_command(int, char **);
void help_command(int, char **);
void host_command(int, char **);
void mmtest_command(int, char **);
void test_command(int, char **);
void pwd_command(int, char **);
void cd_command(int, char **);
int pwd (uint32_t hash, char *buf);
int ls (uint32_t hash);
int get_full_path (char *path, char *buf);
int sum (int num);
int sum_loop (int num);
void bufbomb_command(int, char **);

#define MKCL(n, d) {.name=#n, .fptr=n ## _command, .desc=d}

cmdlist cl[]={
	MKCL(ls, "List directory"),
	MKCL(man, "Show the manual of the command"),
	MKCL(cat, "Concatenate files and print on the stdout"),
	MKCL(ps, "Report a snapshot of the current processes"),
	MKCL(host, "Run command on host"),
	MKCL(mmtest, "heap memory allocation test"),
	MKCL(help, "help"),
	MKCL(test, "test new function"),
	MKCL(pwd, "print name of current/working directory"),
	MKCL(cd, "change working directory"),
	MKCL(bufbomb, "buffer overflow attack bomb")
};

int parse_command(char *str, char *argv[]){
	int b_quote=0, b_dbquote=0;
	int i;
	int count=0, p=0;
	for(i=0; str[i]; ++i){
		if(str[i]=='\'')
			++b_quote;
		if(str[i]=='"')
			++b_dbquote;
		if(str[i]==' '&&b_quote%2==0&&b_dbquote%2==0){
			str[i]='\0';
			argv[count]=&str[p];
			if (str[i-1]!='\0')
			    count++;
			p=i+1;
		}
	}

	/* last one */
	argv[count++]=&str[p];

	return count;
}


void ls_command(int n, char *argv[]){
	fio_printf(1, "\r\n");
    ls(pwd_hash);
    return ;
	int i,count;
	uint32_t h,len;
	char buf[128];
	char tempchar;
	int fd=fs_open("/romfs/INDEX", 0, O_RDONLY);

	if(fd==OPENFAIL)
		return ;

	fio_printf(1, "\r\n");

	i=0;
	while((count=fio_read(fd, &tempchar, 1))>0){
		if (tempchar != '\n') {
			buf[i++]=tempchar;
		} else {
			buf[i]='\0';
			h = hash_djb2((const uint8_t *) buf, -1);
			romfs_get_file_by_hash(&_sromfs,  h,  &len);
			fio_printf(1, "/romfs/%s\thash=%X\tlen=%u\r\n",buf,h,len);
			i=0;
			strcpy(buf, "");
		}
	}
	fio_close(fd);
}


int filedump(const char *filename){
	char buf[128];
	int fd=fs_open(filename, 0, O_RDONLY);

	if(fd==OPENFAIL)
		return 0;

	fio_printf(1, "\r\n");

	int count;
	while((count=fio_read(fd, buf, sizeof(buf)))>0){
		fio_write(1, buf, count);
	}

	fio_close(fd);
	return 1;
}

void ps_command(int n, char *argv[]){
	signed char buf[1024];
	vTaskList(buf);
        fio_printf(1, "\n\rName          State   Priority  Stack  Num\n\r");
        fio_printf(1, "*******************************************\n\r");
	fio_printf(1, "%s\r\n", buf + 2);
}

void cat_command(int n, char *argv[]){
	char buf[256]="/romfs/";
	char buf2[256]={0};
	if(n==1){
		fio_printf(2, "\r\nUsage: cat <filename>\r\n");
		return;
	}
	get_full_path (argv[1],buf2);
	strcat(buf,buf2);
	if(!filedump(buf))
		fio_printf(2, "\r\n%s no such file or directory.\r\n", argv[1]);
}

void man_command(int n, char *argv[]){
	if(n==1){
		fio_printf(2, "\r\nUsage: man <command>\r\n");
		return;
	}

	char buf[128]="/romfs/manual/";
	strcat(buf, argv[1]);

	if(!filedump(buf))
		fio_printf(2, "\r\nManual not available.\r\n");
}

void host_command(int n, char *argv[]){
    int i, len = 0, rnt;
    char command[80] = {0};
    if(n>1){
        for(i = 1; i < n; i++) {
            memcpy(&command[len], argv[i], strlen(argv[i]));
            len += (strlen(argv[i]) + 1);
            command[len - 1] = ' ';
        }
        command[len - 1] = '\0';
        rnt=host_action(SYS_SYSTEM, command);
        fio_printf(1, "\r\nfinish with exit code %d.\r\n", rnt);
    }
    else {
        fio_printf(2, "\r\nUsage: host 'command'\r\n");
    }
}

void help_command(int n,char *argv[]){
	int i;
	fio_printf(1, "\r\n");
	for(i=0;i<sizeof(cl)/sizeof(cl[0]); ++i){
		fio_printf(1, "%s - %s\r\n", cl[i].name, cl[i].desc);
	}
}



void test_command(int n, char *argv[]) {
    int handle;
    int error;
    char buffer[128]={'\0'};

    fio_printf(1, "\r\n");
	if (host_action(SYS_SYSTEM, "mkdir -p output")) {
		fio_printf(1, "mkdir error!\n\r");
		// this function will fail in gdb mode
        //return;
	}
    handle = host_action(SYS_OPEN, "output/syslog", 8);
    if(handle == -1) {
        fio_printf(1, "Open file error!\n\r");
        return;
    }

//	fio_printf(1,"\r\nsum(5)=%d\r\n",sum(5));
//	fio_printf(1,"sum(10)=%d\r\n",sum_loop(10));

    sprintf(buffer,"sum(10)=%d\n",sum(10));
    error = host_action(SYS_WRITE, handle, (void *)buffer, strlen(buffer));
	sprintf(buffer,"sum_loop(10)=%d\n",sum_loop(10));
    error = host_action(SYS_WRITE, handle, (void *)buffer, strlen(buffer));
    if(error != 0) {
        fio_printf(1, "Write file error! Remain %d bytes didn't write in the file.\n\r", error);
        host_action(SYS_CLOSE, handle);
        return;
    }

    host_action(SYS_CLOSE, handle);
}

void bufbomb_command(int n, char *argv[]) {
	bufbomb();
}

cmdfunc *do_command(const char *cmd){

	int i;

	for(i=0; i<sizeof(cl)/sizeof(cl[0]); ++i){
		if(strcmp(cl[i].name, cmd)==0)
			return cl[i].fptr;
	}
	return NULL;
}


void pwd_command(int n, char *argv[]){
    char buf[128];
    fio_printf(1, "\r\n");
    if (pwd(pwd_hash,buf)) {
        fio_printf(1, "/romfs/%s\r\n",buf);
    } else {
        fio_printf(1, "/romfs%s\r\n",buf);
    }
}

void cd_command(int n, char *argv[]){
    const uint8_t * file;
    char buf[256]={0};
    uint32_t hash,leng;
    fio_printf(1, "\r\n");
    leng=get_full_path (argv[1],buf);
    if (leng) {
		buf[leng]='/';
		buf[leng+1]='\0';
    }

	hash = hash_djb2((const uint8_t *) buf, -1);
	file =romfs_get_file_by_hash(&_sromind, hash, NULL);
    if (file) {
        pwd_hash=hash;
    } else {
        fio_printf(2, "%s no directory.\r\n", argv[1]);
    }

	return;
}

int sum (int num) {
	if (num<=0) {
		return 0;
	}
	return num+sum(num-1);
}

int sum_loop (int num) {
	int sum=0;
	if (num<=0) {
		return 0;
	}
	while (num) {
		sum=sum+num;
		num--;
	}
	return sum;
}
