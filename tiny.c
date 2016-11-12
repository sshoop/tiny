#include "csapp.h"

/*
 * 小型的迭代web服务器
 *
 */

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg);


int main(int argc, char **argv)
{
	int listenfd, connfd, port;
	socklen_t clientlen;
	struct sockaddr_in clientaddr;

	/*检查参数个数*/
	if (argc != 2 ) {
		fprintf(stderr, "usage: %s <port>\n" , argv[0]);
		exit(1);
	}
	port = atoi(argv[1]);

	listenfd = open_listenfd(port);
	while (1) {
		clientlen = sizeof(clientaddr);
		connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
		if(fork() == 0) {
			//基于进程的并发处理  共享文件表，但不共享地址空间，父子进程独立的地址空间不便于共享信息
			//父子进程的已连接描述符指向同一文件表表项，父进程不关闭的话，将导致该connfd永远不会关闭，发生内存泄露
			//
			close(listenfd); //子进程关闭监听描述符  
			doit(connfd);
			close(connfd);	//子进程关闭已连接描述符
			exit(0);        //退出子进程（重要）
		}
		close(connfd);		//父进程关闭已连接描述符
	}
	return 0;
}

/*
        处理HTTP事务
*/
void doit(int fd)
{
    int is_static;
    struct stat sbuf;/*文件结构体*/
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    rio_readinitb(&rio, fd);
    rio_readlineb(&rio, buf, MAXLINE);/*从缓冲区读一行请求*/
    sscanf(buf, "%s %s %s", method, uri, version);
    //printf("%s %s %s\n", method, uri, version);/*返回原样请求行和报头*/
    if(strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
        return;
    }
    read_requesthdrs(&rio);/*读取并忽略任何
    请求报头*/
    /*解析URI*/
    is_static = parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) {/*未找到文件*/
        clienterror(fd, filename, "404", "Not Found", "Tiny couldn't find this file");
        return;
    }

    if ( is_static) {/*静态资源处理*/
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {/*检查文件类型和读写权限*/
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read this file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size);
    }
    else {/*动态资源处理*/
        if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {/*检查文件类型和执行权限*/
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}

/*
    错误处理
*/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    /*HTTP响应的头部*/
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /*输出HTTP响应*/
    sprintf(buf, "HTTP/1.0 %s %s \r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content_type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content_length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
}

/*
    读取并忽略请求报头
*/
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

/*
    解析uri
    默认静态内容的目录就是当前主目录，可执行文件的目录是./cgi-bin
*/
int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {/*判断是否为子串 */
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        if (uri[strlen(uri) - 1] == '/')
            strcat(filename, "home.html");
        return 1;
    }
    else {/*动态资源*/
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        }
        else
            strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}

/*
    处理静态资源
*/
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    rio_writen(fd, buf, strlen(buf));
    /*将响应发送到客户端*/
    srcfd = open(filename, O_RDONLY, 0);
    srcp = mmap(0,filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    close(srcfd);
    rio_writen(fd, srcp, filesize);
    munmap(srcp, filesize);
}
/*
    从文件名中获取文件类型
*/
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpg");
    else
        strcpy(filetype, "text/plain");
}

/*
运行CGI动态程序
*/
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /*首先返回响应头*/
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    rio_writen(fd, buf, strlen(buf));

    /*派生子进程执行CGI程序*/
    if (fork() == 0) {
        setenv("QUERY_STRING", cgiargs, 1);
        dup2(fd, STDOUT_FILENO);/*将标准文件输出流重新定向到客户端*/
        //execve(filename, emptylist, environ);/*运行CGI程序*/
    }
    wait(NULL);/*父进程阻塞等待子进程终止*/
}
































