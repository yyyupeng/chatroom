#include <stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<string.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<errno.h>
#include"my_recv.h"

#define INVALLD_USERINFO 'n'    
#define VALLD_USERINFO 'y'

//获取用户输入
int get_userinfo(char *buf, int len)
{
    int i;
    char c;

    if(buf == NULL)
        return -1;

    i = 0;
    while(((c = getchar()) != '\n') && (c != EOF) && (i < len - 2))
        buf[i++] = c;
    buf[i++] = '\n';
    buf[i++] = '\0';

    return 0;
}

//输入用户名，通过fd发送
void input_userinfo(int sock_fd, const char *string)
{
    char input_buf[32];
    char recv_buf[1024];
    char flag_userinfo;

    do
    {
        printf("%s: ",string);
        if(get_userinfo(input_buf,32) < 0)
        {
            printf("error return from get_userinfo\n");
            exit(1);
        }

        if(send(sock_fd,input_buf,strlen(input_buf),0) < 0)
            my_err("send",__LINE__);

        //从连接套接字上读取一次数据
        //if(recv(sock_fd,recv_buf,sizeof(recv_buf),0) < 0)
        if(my_recv(sock_fd,recv_buf,sizeof(recv_buf)) < 0)
        {
            printf("data is too long\n");
            exit(1);
        }

        if(recv_buf[0] == VALLD_USERINFO)
            flag_userinfo = VALLD_USERINFO;
        else
        {
            printf("%s error,input again,",string);
            flag_userinfo = INVALLD_USERINFO;
        }
    }while(flag_userinfo == INVALLD_USERINFO);
}

int main(int argc, char *argv[])
{
    int i;
    int ret;
    int sock_fd;
    int serv_port;
    struct sockaddr_in serv_addr;
    char recv_buf[BUFSIZE];

    //检查参数个数
    if(argc != 5)
    {
        printf("Usage: [-p] [serv_port] [-a] [serv_address]\n");
        exit(1);
    }

    //初始化服务器端地址
    memset(&serv_addr,0,sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;

    //获取服务器端端口和地址
    for(i = 1; i < argc; i++)
    {
        if(strcmp("-p",argv[i]) == 0)
        {
            serv_port = atoi(argv[i + 1]);
            if(serv_port < 0 || serv_port > 65535)
            {
                printf("invalid serv_addr.sin_port\n");
                exit(1);
            }
            else
                serv_addr.sin_port = htons(serv_port);
            
            continue;
        }
        if(strcmp("-a",argv[i]) == 0)
        {
            if(inet_aton(argv[i + 1],&serv_addr.sin_addr) == 0)
            {
                printf("invalid server ip address\n");
                exit(1);
            }

            continue;
        }
    }

    if(serv_addr.sin_port == 0 || serv_addr.sin_addr.s_addr == 0)
    {
        printf("Usage: [-p] [serv_port] [-a] [serv_address]\n");
        exit(1);
    }
    
    //创建TCP套接字
    sock_fd = socket(AF_INET,SOCK_STREAM,0);
    if(sock_fd < 0)
        my_err("socket",__LINE__);

    //向服务器端发送连接请求
    if(connect(sock_fd,(struct sockaddr *)&serv_addr,sizeof(struct sockaddr_in)) < 0)
        my_err("connect",__LINE__);

    //输入用户名和密码
    input_userinfo(sock_fd,"username");
    input_userinfo(sock_fd,"password");

    //if((ret = recv(sock_fd,recv_buf,sizeof(recv_buf),0)) < 0)
    if((ret = my_recv(sock_fd,recv_buf,sizeof(recv_buf))) < 0)
    {
        printf("data is too long\n");
        exit(1);
    }

    for(i = 0; i < ret; i++)
        printf("%c",recv_buf[i]);
    printf("\n");

    close(sock_fd);

    return 0;
}

