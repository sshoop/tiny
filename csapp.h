/* define csapp */

#ifndef  __CSAPP_H__
#define  __CSAPP_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*  文件打开方式和权限*/
#define DEF_MODE   S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH
#define DEF_UMASK  S_IWGRP|S_IWOTH

/* sockaddr */
typedef struct sockaddr SA;

/*缓冲区结构*/
#define RIO_BUFSIZE 8192
typedef struct {
	int rio_fd;											/*描述符*/
	int rio_cnt;											/*未读的字节*/
	char *rio_bufptr;								/*下一个未读的字节地址*/
	char rio_buf[RIO_BUFSIZE];			/*内部缓冲区*/
}rio_t;

/*静态变量*/
#define MAXLINE 8192
#define MAXBUF 8192
#define LISTENQ 1024

/* 外部变量*/
extern char **environ;

/*声明函数*/



/*RIO*/
ssize_t rio_readn(int fd, void *usrbuf, size_t n);
ssize_t rio_writen(int fd, void *usrbuf, size_t n);
void rio_readinitb(rio_t *rp, int fd);
ssize_t	rio_readnb(rio_t *rp, void *usrbuf, size_t n);
ssize_t	rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);


/*客户端 服务器的封装函数*/
int open_clientfd(char *hostname, int portno);
int open_listenfd(int portno);



#endif






