#include "csapp.h"

/*
 * 小型的迭代web服务器
 *
 */

void doit(int fd);
void read_requesthdrs(rio_t *rp, int *length, int is_post);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
		 char *shortmsg, char *longmsg);
void post_dynamic(int fd, char *filename, int contentLength, rio_t *rio);

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
    int is_static, head = 0, post = 0, get = 0, contentLength = 0;
    struct stat sbuf;/*文件结构体*/
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    rio_readinitb(&rio, fd);
    rio_readlineb(&rio, buf, MAXLINE);/*从缓冲区读一行请求*/
    sscanf(buf, "%s %s %s", method, uri, version);
    //printf("%s %s %s\n", method, uri, version);/*返回原样请求行和报头*/
	
	/*判断请求方式*/
	if(!strcasecmp(method, "HEAD")) {
		head = 1;
	}else if (!strcasecmp(method, "POST")) {
		post = 1;
	}else if(!strcasecmp(method, "GET")){
		get = 1;
	}else {
        clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
        return;
    }

    //read_requesthdrs(&rio);/*get请求时：读取并忽略请求报头*/ 
	read_requesthdrs(&rio, &contentLength, post);

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
		if(post) {
			post_dynamic(fd, filename, contentLength, &rio);
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
void read_requesthdrs(rio_t *rp, int *length, int is_post)
{
    char buf[MAXLINE];
	char *c;

    rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        rio_readlineb(rp, buf, MAXLINE);
		if(is_post) {/*判断是否为post请求*/
			if(strcasecmp(buf, "Content-Length:") == 0) {/*获得Content-Length的值*/
				c = &buf[15];
				c +=  strspn(c, " \t");
				*length = atol(c);
			}
		}
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
        execve(filename, emptylist, environ);/*运行CGI程序*/
    }
    wait(NULL);/*父进程阻塞等待子进程终止*/
}

/*
 * 处理post请求
 * post请求的参数保存在请求主体中，不能直接从套接字流中读取，因为所有的数据已经缓冲在rio中
 * 采用管道的形式，将缓冲区的数据写入到管道写端，标准输入流重定向到管道读端-读出数据
 */

void post_dynamic(int fd, char *filename, int contentLength, rio_t *rp)
{
	char buf[MAXLINE], data[MAXLINE], length[32], *emptylist[] = {NULL};
	int pipe_fd[2];

	sprintf(length, "%d", contentLength);
	memset(data, 0, MAXLINE);


	/*创建管道*/
	if(pipe(pipe_fd) < 0) {
		return;
	}

	if(fork() == 0) {/*创建子进程*/
		close(pipe_fd[0]);/*关闭读端*/
		rio_readnb(rp, data, contentLength);/*从缓冲区读取请求报头*/

		rio_writen(pipe_fd[1], data, contentLength);/*将请求报头写入管道*/
		exit(0);/*退出该子进程*/
	}
	else {
		/*打印HTTP响应*/
		sprintf(buf, "HTTP/1.0 200 OK\r\n");
		sprintf(buf, "%sSever: Tiny Web Sever\r\n", buf);
		rio_writen(fd, buf, strlen(buf));

		if(fork() == 0) {

			close(pipe_fd[1]);/*关闭写端*/
			dup2(pipe_fd[0], STDIN_FILENO);/*将标准输入重新定向到管道读端，即数据将从管道中获取*/
			close(pipe_fd[0]);/*关闭管道读端*/
			setenv("CONTENT_LENGTH", length, 1);/*设置post环境变量*/
			dup2(fd, STDOUT_FILENO);/*将标准输出重新定向到客户端*/
			execve(filename, emptylist, environ);

		}
		else {
			close(pipe_fd[0]);
			close(pipe_fd[1]);
		}
	}

}
































