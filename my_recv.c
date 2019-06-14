#include <stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<errno.h>
#include"my_recv.h"

//错误处理函数
void my_err(const char *err_string,int line)
{
    fprintf(stderr,"line:%d ",line);
    perror(err_string);
    exit(1);
}

//从套接字读取一次数据（以‘\n’为结束标志）
//出错返回-1，服务器端关闭连接返回0，成功返回读取的字节数
int my_recv(int conn_fd,char *data_buf,int len)
{
    static char recv_buf[BUFSIZE];  //自定义缓冲区
    static char *pread;             //指向下一次读取数据的位置
    static int len_remain = 0;      //自定义缓冲区中剩余字节数
    int i;

    if(len_remain <= 0)
    {
        if((len_remain = recv(conn_fd,recv_buf,sizeof(recv_buf),0)) < 0)
            my_err("recv",__LINE__);
        else if(len_remain == 0)
            return 0;   //目的计算机端的socket连接关闭

        pread = recv_buf;
    }

    for(i = 0; *pread != '\n'; i++)
    {
        if(i > len)
            return -1;

        data_buf[i] = *pread++;
        len_remain--;
    }

    len_remain--;
    pread++;

    return i;
}
