#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <string.h>
#include <arpa/inet.h>

#include <sys/epoll.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/wait.h>

#define ISspace(x) isspace((int)(x))
#define SERVER_STRING "Server: crkhttp/0.1.0\r\n"
#define MAXNUM 2048

void error_occur(const char *str)
{
    perror(str);
    exit(EXIT_FAILURE); //EXIT_FAILURE需要stdlib
}

//创建并初始化监听套接字
int create_listenfd(int port)
{
    int listenfd = 0;
    listenfd = socket(AF_INET,SOCK_STREAM,0);
    if(listenfd == -1)
        error_occur("socket");

    //设置端口复用
    int opt = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof (opt));

    //初始化地址结构
    struct sockaddr_in addr;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if(bind(listenfd,(struct sockaddr *)&addr,sizeof (addr)) < 0)
        error_occur("bind");

    if(listen(listenfd,5) < 0)   //设置同一时刻建立连接的上限数
        error_occur("listen");

    return listenfd;
}

//处理连接请求
void do_accept(int httpfd,int epofd)
{
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof (addr);

    //接受连接请求并打印客户端地址信息
    int cfd = 0;  //socket for communication
    cfd = accept(httpfd,(struct sockaddr *)&addr,&addr_len);
    if(cfd == -1)
        error_occur("accpet");
    printf("New connection... IP: %s , Port: %d\n",inet_ntoa(addr.sin_addr),ntohs(addr.sin_port));

    //epoll边缘触发模式
    struct epoll_event temp;
    temp.data.fd = cfd;
    temp.events = EPOLLIN | EPOLLET;
    //设置非阻塞
    int flag = fcntl(cfd,F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd,F_SETFL,flag);

    //插入
    int res = epoll_ctl(epofd,EPOLL_CTL_ADD,cfd,&temp);
    if(res == -1)
        error_occur("epoll_ctl");

}

//断开连接时，关闭通信套接字并从epoll监听对象中移除
void disconnect(int cfd,int epofd)
{
    int res = epoll_ctl(epofd,EPOLL_CTL_DEL,cfd,NULL);
    if(res == -1)
        error_occur("epoll_ctl for delete");
    close(cfd);
}

//501没有相应实现（不支持的方法）
void unimplemented(int cfd)
{
     char buf[1024];
     sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
     send(cfd, buf, strlen(buf), 0);
     sprintf(buf, SERVER_STRING);
     send(cfd, buf, strlen(buf), 0);
     sprintf(buf, "Content-Type: text/html\r\n");
     send(cfd, buf, strlen(buf), 0);
     sprintf(buf, "\r\n");
     send(cfd, buf, strlen(buf), 0);
     sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
     send(cfd, buf, strlen(buf), 0);
     sprintf(buf, "</TITLE></HEAD>\r\n");
     send(cfd, buf, strlen(buf), 0);
     sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
     send(cfd, buf, strlen(buf), 0);
     sprintf(buf, "</BODY></HTML>\r\n");
     send(cfd, buf, strlen(buf), 0);
}

//400语法错误
void bad_request(int cfd)
{
    char buf[1024];
    //发送400
    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(cfd, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(cfd, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(cfd, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(cfd, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(cfd, buf, sizeof(buf), 0);
}

//404页面错误
void not_found(int cfd)
{
    char buf[1024];
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(cfd, buf, strlen(buf), 0);
}

//500无法执行（属于服务器内部错误）
void cannot_execute(int cfd)
{
    char buf[1024];
    //发送500
    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(cfd, buf, strlen(buf), 0);
}

//读取一行http报文
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size - 1) && (c != '\n'))
    {
        n = recv(sock, &c, 1, 0);
        if (n > 0)
        {
            if (c == '\r')
            {
                n = recv(sock, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';
    return i;
}

//发送响应头部
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  // could use filename to determine file type  目前只支持text/html
    //发送HTTP头
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);

    sprintf(buf,"Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);

    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);

}

//读取文件
void cat(int cfd, FILE *resource)
{
    //发送文件的内容
    char buf[1024];
    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(cfd, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
        /*fget会覆盖buf之前的内容，而且短的能覆盖长的，相当于重写*/
    }
}

//如果不是CGI文件，也就是静态文件，直接读取文件返回给请求的http客户端即可
void serve_file(int cfd, const char *filename)
{
     FILE *resource = NULL;
     int numchars = 1;
     char buf[1024];
     buf[0] = 'H';  //任意一个字符但不能是'\n'，确保进入下面循环
     buf[1] = '\0'; //组成一个完整字符串与buf比较
     while ((numchars > 0) && strcmp("\n", buf))    //读取丢弃缓冲区不需要的内容（请求头部）
         numchars = get_line(cfd, buf, sizeof(buf));

     //打开文件
     resource = fopen(filename, "r");
     if (resource == NULL)
         not_found(cfd);
     else
     {
         headers(cfd, filename);
         cat(cfd, resource);
     }
     fclose(resource);//关闭文件句柄
}

void execute_cgi(int cfd, const char *path,const char *method, const char *query_string)
{
    //printf("\n in founction execute cig\n");

    char buf[1024];
    int cgi_output[2];
    int cgi_input[2];

    pid_t pid;
    int status;

    int i;
    char c;

    int numchars = 1;
    int content_length = -1;
    //默认字符
    buf[0] = 'A';
    buf[1] = '\0';
    if (strcasecmp(method, "GET") == 0)

        while ((numchars > 0) && strcmp("\n", buf))
            numchars = get_line(cfd, buf, sizeof(buf));

    else
    {
        numchars = get_line(cfd, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
        {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));

            numchars = get_line(cfd, buf, sizeof(buf));
        }

        if (content_length == -1)
        {
            bad_request(cfd);
            return;
        }
    }
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(cfd, buf, strlen(buf), 0);
    if (pipe(cgi_output) < 0)
    {
        cannot_execute(cfd);
        return;
    }
    if (pipe(cgi_input) < 0)
    {
        cannot_execute(cfd);
        return;
    }
    if ( (pid = fork()) < 0 )
    {
        cannot_execute(cfd);
        return;
    }
    if (pid == 0)  /* 子进程: 运行CGI 脚本 */
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        dup2(cgi_output[1], 1); //重定向到标准输出
        dup2(cgi_input[0], 0);  //重定向到标准输入

        close(cgi_output[0]);//关闭了cgi_output中的读通道
        close(cgi_input[1]);//关闭了cgi_input中的写通道


        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);

        if (strcasecmp(method, "GET") == 0)
        {
            //存储QUERY_STRING
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            fprintf(stderr,"query_string:%s	query_env:%s\n",query_string,query_env);
            putenv(query_env);
        }
        else
        {   /* POST */
            //存储CONTENT_LENGTH
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        fprintf(stderr,"\n path: %s",path);	//不能用printf，因为管道重定向到了标准输出
        execl(path, path, NULL);//执行CGI脚本
        exit(0);
    }
    else
    {
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(method, "POST") == 0)

            for (i = 0; i < content_length; i++)
            {
                recv(cfd, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        //读取cgi脚本返回数据

        while (read(cgi_output[0], &c, 1) > 0)//发送给浏览器
            send(cfd, &c, 1, 0);

        //运行结束关闭
        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0);
    }
}

//读取并解析请求行
void do_read(int cfd,int epofd)
{
    char buf[1024];
    char method[255];
    char url[255];
    char protocol[12];
    char path[512];
    //size_t i, j;
    struct stat st;
    int cgi = 0;
    char *query_string = NULL;

    int len = get_line(cfd, buf, sizeof(buf));
    if(len == 0)
    {
        printf("server disconnect...\n");
        disconnect(cfd,epofd);
    }

//    i = 0;
//    j = 0;
//    while (!ISspace(buf[j]) && (i < sizeof(method) - 1))
//    {
//        //提取其中的请求方式
//        method[i] = buf[j];
//        i++;
//        j++;
//    }
//    method[i] = '\0';

    sscanf(buf,"%[^ ] %[^ ] %[^ ]",method,url,protocol);    //正则表达式
    //printf("method = %s, url = %s, protocol = %s\n",method,url,protocol);

    //不支持的方法
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))    //返回非0即true
    {
        unimplemented(cfd);
        return;
    }
    if (strcasecmp(method, "POST") == 0)
        cgi = 1;
    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;

        //如果有?说明有参数,使用cgi处理 */
        if (*query_string == '?')
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }
//    i = 0;
//    while (ISspace(buf[j]) && (j < sizeof(buf)))
//        j++;


//    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf)))
//    {
//        url[i] = buf[j];
//        i++; j++;
//    }
//    url[i] = '\0';



    sprintf(path, "httpdocs%s", url);   //设置文件目录位置
    //printf("path is :%s \n",path);

    if (path[strlen(path) - 1] == '/')  //路径是目录，则默认访问test页面
        strcat(path, "test.html");

    //根据路径获取文件状态，状态存在st结构体中
    if (stat(path, &st) == -1)          //获取不到文件状态（找不到文件）
    {
        while ((len > 0) && strcmp("\n", buf))  //读取并丢弃输入缓冲区剩余内容
        {
            //printf("return strcmp:%d\n",strcmp("\n", buf));
            len = get_line(cfd, buf, sizeof(buf));
        }
        not_found(cfd);
    }
    else
    {
        //文件可执行
        if ((st.st_mode & S_IXUSR) ||
                (st.st_mode & S_IXGRP) ||
                (st.st_mode & S_IXOTH))
            //S_IXUSR:文件所有者具可执行权限
            //S_IXGRP:用户组具可执行权限
            //S_IXOTH:其他用户具可读取权限
            cgi = 1;

        if (!cgi)
            serve_file(cfd, path);
        else
            execute_cgi(cfd, path, method, query_string);
    }
    disconnect(cfd,epofd);
    printf("connection close...\n");
}


void epoll_run(int httpfd)
{
    int epofd = 0;
    epofd = epoll_create(MAXNUM);   //创建epoll实例（红黑树根节点），返回句柄，MAXNUM为最大监听数的参考值，不够可以自动扩容
    if(epofd == -1)
        error_occur("epoll create");


    struct epoll_event temp,event[MAXNUM];
    temp.events = EPOLLIN;          //构造监听套接字的节点
    temp.data.fd = httpfd;

    int res = epoll_ctl(epofd,EPOLL_CTL_ADD,httpfd,&temp);  //插入监听套接字节点
    if(res == -1)
        error_occur("epoll_ctl");

    int nready; //记录事件触发个数
    for(;;)
    {
        nready = epoll_wait(epofd,event,MAXNUM,-1);     //被触发的event被放入event数组,并返回触发个数
        if(nready == -1)
            error_occur("epoll_wait");
        for(int i = 0;i < nready; i++)
        {
            if(!(event[i].events & EPOLLIN))    //只接收请求连接的可读事件
                continue;
            if(event[i].data.fd == httpfd)
                do_accept(httpfd,epofd);
            else
                do_read(event[i].data.fd,epofd);    //读取浏览器（客户端）发送的http协议消息

        }
    }
}

int main()
{
    int port = 8888;
    int httpfd =  create_listenfd(port);
    epoll_run(httpfd);
    close(httpfd);
    return 0;
}
