#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<string.h>
#include<pthread.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<errno.h>
#include<signal.h>
#include<mysql.h>
#include<sys/epoll.h>
#include"chat.h"
#include"thread_pool.h"

#define SERV_PORT 9527  //服务器端口号
#define LISTENQ 10      //连接请求队列的最大长度
#define MAX_EVENTS 1000

#define EXIT 0
#define REGISTE 1
#define LOGIN 2
#define CHECK_FRI 3
#define GET_FRI_STA 4
#define ADD_FRI 5
#define DEL_FRI 6
#define SHI_FRI 7
#define CRE_GRP 8
#define ADD_GRP 9
#define OUT_GRP 10
#define DEL_GRP 11
#define SET_GRP_ADM 12
#define KICK_GRP 13
#define CHECK_GRP 14
#define CHECK_MEM_GRP 15
#define CHAT_ONE 16
#define CHAT_MANY 17
#define CHECK_MES_FRI 18
#define CHECK_MES_GRP 19

#define FRIEND 1
#define FRI_BLK 2
#define GRP 3
#define GRP_OWN 4
#define GRP_ADM 5

#define OFFLINE 0
#define ONLINE 1
#define ONE_CHAT 2
#define MANY_CHAT 3

User *U_read();                     //读取用户信息表
Relation *R_read();                 //读取关系表
Recordinfo *RC_read();              //读取消息记录
void Insert(User *pNew);            //注册——加入链表
void Insert_R(Relation *pNew);      //加入关系表
void Insert_RC(Recordinfo *pNew);   //加入聊天记录
void Delete_R(Relation *pNew);      //删除出关系表
void DeleteLink();                  //销毁		
void DeleteLink_R();                //销毁		
void DeleteLink_RC();               //销毁		
void *Menu(void *recv_pack_t);      //处理函数
void Exit(PACK *recv_pack);         //注销
void registe(PACK *recv_pack);      //注册
void login(PACK *recv_pack);        //登陆
void check_fri(PACK *recv_pack);    //查看好友列表
void get_fri_sta(PACK *recv_pack);  //获取好友状态
void add_fri(PACK *recv_pack);      //添加好友
void del_fri(PACK *recv_pack);      //删除好友
void shi_fri(PACK *recv_pack);      //屏蔽好友
void cre_grp(PACK *recv_pack);      //创建群
void add_grp(PACK *recv_pack);      //加群
void out_grp(PACK *recv_pack);      //退群
void del_grp(PACK *recv_pack);      //解散群
void set_grp_adm(PACK *recv_pack);  //设置管理员
void kick_grp(PACK *recv_pack);     //踢人
void check_grp(PACK *recv_pack);    //查看群列表       
void check_mem_grp(PACK *recv_pack);//查看群中成员
void chat_one(PACK *recv_pack);     //私聊
void chat_many(PACK *recv_pack);    //群聊
void check_mes_fri(PACK *recv_pack);//查看与好友聊天记录
void check_mes_grp(PACK *recv_pack);//查看群组聊天记录
void send_pack(int fd, PACK *recv_pack, char *ch);
void send_more(int fd, int flag, PACK *recv_pack, char *mes);

MYSQL mysql;
pthread_mutex_t mutex;
pthread_cond_t cond;
User *pHead = NULL;
Relation *pStart = NULL;
Recordinfo *pRec = NULL;

PACK Mex_Box[100];
int sign;
int book;

int flag_happen;
void handler_sigint(int signo)
{
    flag_happen = 1;
}

int main()
{
    int sock_fd,conn_fd;
    int optval;
    socklen_t len;
    struct sockaddr_in cli_addr,serv_addr;
    
    PACK recv_t;
    PACK *recv_pack;
    int ret;

    int epfd;   //epoll的文件描述符
    struct epoll_event ev, events[MAX_EVENTS];  //存放从内核读取的事件
    int ret_event;  //epoll_wait()的返回值

    int i = 0;
    len = sizeof(struct sockaddr_in);

    //信号处理函数
    if(signal(SIGINT, handler_sigint) == SIG_ERR)
        my_err("signal", __LINE__);

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    
    if (mysql_init(&mysql) == NULL) 
    {
        printf("mysql_init(): %s\n", mysql_error(&mysql));
        return -1;
    }

    if (mysql_real_connect(&mysql,NULL,"root","Lyp20000617","chat",0,NULL,0) == NULL) 
    {
        printf("mysql_real_connect(): %s\n", mysql_error(&mysql));
        return -1;
    }

    printf("服务器启动中...\n");

    //创建TCP套接字
    sock_fd = socket(AF_INET,SOCK_STREAM,0);
    if(sock_fd < 0)
        my_err("socket",__LINE__);
    printf("创建套接字...\n");

    //设置该套接字使之可以重新绑定端口
    optval = 1;
    if(setsockopt(sock_fd,SOL_SOCKET,SO_REUSEADDR,(void *)&optval,sizeof(int)) < 0)
        my_err("setsockopt",__LINE__);

    //初始化服务器端地址结构
    memset(&serv_addr,0,len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_PORT);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //将套接字绑定到本地端口
    if(bind(sock_fd,(struct sockaddr *)&serv_addr,len) < 0)
        my_err("bind",__LINE__);
    printf("绑定端口...\n");

    //将套接字转化为监听套接字
    if(listen(sock_fd,LISTENQ) < 0)
        my_err("listen",__LINE__);
    printf("侦听套接字...\n");

    //多路复用
    epfd = epoll_create(MAX_EVENTS);    //创建句柄
    ev.data.fd = sock_fd;               //设置与要处理事件相关的文件描述符
    ev.events = EPOLLIN;                //设置要处理的事件类型
    epoll_ctl(epfd, EPOLL_CTL_ADD, sock_fd, &ev);//注册epoll事件 
    
    printf("创建线程池...\n");
    pool_init();
    sleep(1);

    printf("服务器启动成功！\n");

    //读取信息表
    pHead = U_read();
    User *t = pHead;
    pStart = R_read();
    pRec = RC_read();

    while(1)
    {
        ret_event = epoll_wait(epfd, events, MAX_EVENTS, 1000);    //等待事件到来

        for(i = 0; i < ret_event; i++)
        {
            if(events[i].data.fd == sock_fd)
            {
                conn_fd = accept(sock_fd, (struct sockaddr *)&cli_addr, &len);
                printf("Connected: %s, fd is %d\n",inet_ntoa(cli_addr.sin_addr), conn_fd);
                ev.data.fd = conn_fd;               //设置与要处理事件相关的文件描述符
                ev.events = EPOLLIN;                //设置要处理的事件类型
                epoll_ctl(epfd, EPOLL_CTL_ADD, conn_fd, &ev);   //注册epoll事件
            }
            else if(events[i].events & EPOLLIN)
            {
                ret = recv(events[i].data.fd, &recv_t, sizeof(PACK), 0);
                recv_t.data.send_fd = events[i].data.fd;

                if(ret < 0)
                {
                    close(events[i].data.fd);
                    perror("revc");
                    continue;
                }
                else if(ret == 0)
                {
                    ev.data.fd = events[i].data.fd;
                    while(t)
                    {
                        if(strcmp(t->name, recv_t.data.send_name) == 0)
                        {
                            t->statu_s = OFFLINE;
                            break;
                        }
                        t = t->next;
                    }
                    printf("log off(fd): %d\n",ev.data.fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, &ev);
                    close(events[i].data.fd);
                    continue;
                }
                
                //输出收到的包信息
                printf("\n\e[1;34m****PACK****\e[0m\n");
                printf("\e[1;34m*\e[0m type      : %d\n", recv_t.type);
                printf("\e[1;34m*\e[0m send_fd   : %d\n", recv_t.data.send_fd);
                printf("\e[1;34m*\e[0m recv_fd   : %d\n", recv_t.data.recv_fd);
                printf("\e[1;34m*\e[0m send_name : %s\n", recv_t.data.send_name);
                printf("\e[1;34m*\e[0m recv_name : %s\n",recv_t.data.recv_name);
                printf("\e[1;34m*\e[0m mes       : %s\n", recv_t.data.mes);
                printf("\e[1;34m*************\e[0m\n\n");
                
                recv_pack = (PACK *)malloc(sizeof(PACK));
                memcpy(recv_pack, &recv_t, sizeof(PACK));

                //开启线程
                pool_add(Menu, (void *)recv_pack);        
                
            }
        }
        if(flag_happen == 1)
        {
            printf("\n服务器关闭...\n");
            break;
        }
    }

    //销毁资源
    pool_destroy();
    DeleteLink();
    DeleteLink_R();
    DeleteLink_RC();
    free(recv_pack);
    close(sock_fd);
    close(epfd);

    return 0;
}

//读取用户信息表
User *U_read()
{
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char query_str[1000];
    int rows;
    int fields;

    User *pEnd, *pNew;

    sprintf(query_str, "select * from userinfo");
    mysql_real_query(&mysql, query_str, strlen(query_str));
    res = mysql_store_result(&mysql);
    rows = mysql_num_rows(res);
    fields = mysql_num_fields(res);

    while(row = mysql_fetch_row(res))
    {
        pNew = (User *)malloc(sizeof(User));
        strcpy(pNew->name, row[0]);
        strcpy(pNew->passwd, row[1]);
        pNew->statu_s = OFFLINE;
        pNew->next = NULL;
        if(pHead == NULL)
            pHead = pNew;
        else
            pEnd->next = pNew;
        pEnd = pNew;
    }
    return pHead;
}

//读取关系表
Relation *R_read()
{
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char query_str[1000];
    int rows;
    int fields;

    Relation *pEnd, *pNew;

    sprintf(query_str, "select * from relationinfo");
    mysql_real_query(&mysql, query_str, strlen(query_str));
    res = mysql_store_result(&mysql);
    rows = mysql_num_rows(res);
    fields = mysql_num_fields(res);

    while(row = mysql_fetch_row(res))
    {
        pNew = (Relation *)malloc(sizeof(Relation));
        strcpy(pNew->name1, row[0]);
        strcpy(pNew->name2, row[1]);
        pNew->statu_s = row[2][0] - '0';
        pNew->next = NULL;
        if(pStart == NULL)
            pStart = pNew;
        else
            pEnd->next = pNew;
        pEnd = pNew;
    }
    return pStart;
}

//读取消息记录
Recordinfo *RC_read()
{
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char query_str[1000];
    int rows;
    int fields;

    Recordinfo *pEnd, *pNew;

    sprintf(query_str, "select * from recordinfo");
    mysql_real_query(&mysql, query_str, strlen(query_str));
    res = mysql_store_result(&mysql);
    rows = mysql_num_rows(res);
    fields = mysql_num_fields(res);

    while(row = mysql_fetch_row(res))
    {
        pNew = (Recordinfo *)malloc(sizeof(Recordinfo));
        strcpy(pNew->name1, row[0]);
        strcpy(pNew->name2, row[1]);
        strcpy(pNew->message, row[2]);
        pNew->next = NULL;
        if(pRec == NULL)
            pRec = pNew;
        else
            pEnd->next = pNew;
        pEnd = pNew;
    }
    return pRec;
}

//处理函数
void *Menu(void *recv_pack_t)
{
    PACK *recv_pack = (PACK *)recv_pack_t;
    switch(recv_pack->type)
    {
    case EXIT:
        Exit(recv_pack);
        break;

    case REGISTE:
        registe(recv_pack);            
        break;

    case LOGIN:
        login(recv_pack);
        break;

    case CHECK_FRI:
        check_fri(recv_pack);
        break;

    case GET_FRI_STA:
        get_fri_sta(recv_pack);
        break;

    case ADD_FRI:
        add_fri(recv_pack);
        break;

    case DEL_FRI:
        del_fri(recv_pack);
        break;

    case SHI_FRI:
        shi_fri(recv_pack);
        break;
        
    case CRE_GRP:
        cre_grp(recv_pack);
        break;

    case ADD_GRP:
        add_grp(recv_pack);
        break;

    case OUT_GRP:
        out_grp(recv_pack);
        break;

    case DEL_GRP:
        del_grp(recv_pack);
        break;

    case SET_GRP_ADM:
        set_grp_adm(recv_pack);
        break;

    case KICK_GRP:
        kick_grp(recv_pack);
        break;

    case CHECK_GRP:
        check_grp(recv_pack);
        break;

    case CHECK_MEM_GRP:
        check_mem_grp(recv_pack);
        break;

    case CHAT_ONE:
        chat_one(recv_pack);
        break;

    case CHAT_MANY:
        chat_many(recv_pack);
        break;

    case CHECK_MES_FRI:
        check_mes_fri(recv_pack);
        break;

    default:
        break;
    }
}

//注销
void Exit(PACK *recv_pack)
{
    User *t = pHead;
    while(t)
    {
        if(strcmp(t->name, recv_pack->data.send_name) == 0)
        {
            t->statu_s = OFFLINE;
            break;
        }
        t = t->next;
    }
    close(recv_pack->data.send_fd);
}

//注册
void registe(PACK *recv_pack)
{
    char query_str[1000];

    int a;
    char ch[5];
    int fd = recv_pack->data.send_fd;

    User *t = pHead;
    int flag = 0;
    User *pNew = (User *)malloc(sizeof(User));
    while(t)
    {
        if(strcmp(t->name, recv_pack->data.send_name) == 0)
        {
            flag = 1;
            break;
        }
        t = t->next;
    }

    //添加到数据库中并发送信息给客户端
    if(flag == 0)
    {
        strcpy(pNew->name, recv_pack->data.send_name);
        strcpy(pNew->passwd, recv_pack->data.mes);
        pNew->statu_s = OFFLINE;
        Insert(pNew);

        memset(query_str, 0, strlen(query_str));
        sprintf(query_str, "insert into userinfo values('%s', '%s')", recv_pack->data.send_name, recv_pack->data.mes);
        mysql_real_query(&mysql, query_str, strlen(query_str));
        ch[0] = '1';
    }
    else
        ch[0] = '0';
    
    ch[1] = '\0';
    send_pack(fd, recv_pack, ch);
    free(pNew);
    pNew = NULL;
}

//注册——加入链表
void Insert(User *pNew)
{
    User *t = pHead;
    while(t && t->next != NULL)
        t = t->next;
    t->next = pNew;
    pNew->next = NULL;
}

//登陆
void login(PACK *recv_pack)
{
    char ch[5];
    int fd = recv_pack->data.send_fd;
    int i;

    User *t = pHead;
    int flag = 0;
    while(t)
    {
        if(strcmp(t->name, recv_pack->data.send_name) == 0 && strcmp(t->passwd, recv_pack->data.mes) == 0)
        {
            flag = 1;
            break;
        }
        t = t->next;
    }

    if(flag == 0)
        ch[0] = '0';
    else
    {
        if(t->statu_s == OFFLINE)
        {
            ch[0] = '1';
            t->statu_s = ONLINE;
            t->fd = recv_pack->data.send_fd;
        }
        else 
            ch[0] = '2';
    }
    ch[1] = '\0';
    
    send_pack(fd, recv_pack, ch);
    
    for(i = 0; i < sign; i++)
    {
        if(ch[0] == '1' && strcmp(recv_pack->data.send_name, Mex_Box[i].data.recv_name) == 0)
        {
            //开启线程执行离线任务
            pool_add(Menu, (void *)&Mex_Box[i]);  
            memset(&Mex_Box[i], 0, sizeof(PACK));
            book++;
        }
    }
    if(book == sign)
        sign = book = 0;
}

//查看好友列表
void check_fri(PACK *recv_pack)
{
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char query_str[700];
    int rows;
    int i;

    int fd = recv_pack->data.send_fd;
    int statu_s;

    FRI_INFO fri_info;

    memset(query_str, 0, strlen(query_str));
    sprintf(query_str, "select * from relationinfo where name1='%s' or name2='%s'", recv_pack->data.send_name, recv_pack->data.send_name);
    mysql_real_query(&mysql, query_str, strlen(query_str));
    
    res = mysql_store_result(&mysql);
    
    rows = mysql_num_rows(res); //行数

    if(rows == 0)
        fri_info.friends_num = 0;
    else
    {
        i = 0;
        while(row = mysql_fetch_row(res))
        {
            if(strcmp(row[0], recv_pack->data.send_name) == 0)
            {
                strcpy(fri_info.friends[i], row[1]);
                statu_s = row[2][0] - '0';
                fri_info.friends_status[i] = statu_s;
                i++;
            }
            else if(strcmp(row[1], recv_pack->data.send_name) == 0)
            {
                strcpy(fri_info.friends[i], row[0]);
                statu_s = row[2][0] - '0';
                fri_info.friends_status[i] = statu_s;
                i++;
            }   
        }
        fri_info.friends_num = i;
    }
    if(send(fd, &fri_info, sizeof(FRI_INFO), 0) < 0)
        my_err("send", __LINE__);
}

//获取好友状态
void get_fri_sta(PACK *recv_pack)
{
    int flag = GET_FRI_STA;
    char ch[5];
    int fd = recv_pack->data.send_fd;

    User *t = pHead;
    int flag_2 = 0;
    while(t)
    {
        if(strcmp(t->name, recv_pack->data.send_name) == 0)
        {
            flag_2 = 1;
            break;
        }
        t = t->next;
    }

    if(t->statu_s == OFFLINE)
        ch[0] = '0';
    else 
        ch[0] = '1';
    ch[1] = '\0';

    send_more(fd, flag, recv_pack, ch);
}

//添加好友
void add_fri(PACK *recv_pack)
{
    char query_str[1700];

    int flag = ADD_FRI;
    int fd = recv_pack->data.send_fd;
    char ch[5];
    char ss[MAX_CHAR];

    User *t = pHead;
    int flag_2 = 0;
    Relation *q = pStart;
    int flag_3 = 0;
    Relation *pNew = (Relation *)malloc(sizeof(Relation));
    while(q)
    {
        if((strcmp(q->name1, recv_pack->data.recv_name) == 0 && strcmp(q->name2, recv_pack->data.send_name) == 0) || (strcmp(q->name1, recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.recv_name) == 0))
        {
            flag_3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag_3 == 1)
    {
        ch[0] = '4';
        send_more(fd, flag, recv_pack, ch);
        free(pNew);
        pNew = NULL;
        return;
    }
    else
    {
        while(t)
        {
            if(strcmp(t->name, recv_pack->data.recv_name) == 0)
            {
                flag_2 = 1;
                break;
            }
            t = t->next;
        }
        
        //该用户不存在
        if(flag_2 == 0)
        {
            ch[0] = '3';
            send_more(fd, flag, recv_pack, ch);
            free(pNew);
            pNew = NULL;
            return;
        }
        else
        {
            if(t->statu_s != OFFLINE)
            {
                fd = t->fd;
                if(recv_pack->data.mes[0] == '0')
                    ch[0] = '0';
                else if(recv_pack->data.mes[0] == 'y')
                {
                    ch[0] = '1';
                    strcpy(pNew->name1, recv_pack->data.recv_name);
                    strcpy(pNew->name2, recv_pack->data.send_name);
                    pNew->statu_s = FRIEND;
                    Insert_R(pNew);

                    memset(query_str, 0, strlen(query_str));
                    sprintf(query_str, "insert into relationinfo values('%s', '%s', %d)", recv_pack->data.recv_name, recv_pack->data.send_name, FRIEND);
                    mysql_real_query(&mysql, query_str, strlen(query_str));
                }
                else if(recv_pack->data.mes[0] == 'n')
                    ch[0] = '2';
                
                strcpy(ss,recv_pack->data.recv_name);
                strcpy(recv_pack->data.recv_name, recv_pack->data.send_name);
                strcpy(recv_pack->data.send_name, ss);
                send_more(fd, flag, recv_pack, ch);
            }
            else if(t->statu_s == OFFLINE)
            {
                memcpy(&Mex_Box[sign++], recv_pack, sizeof(PACK));       
            }
            free(pNew);
            pNew = NULL;
        }
    }
}

//加入关系表
void Insert_R(Relation *pNew)
{
    Relation *t = pStart;
    while(t && t->next != NULL)
        t = t->next;
    t->next = pNew;
    pNew->next = NULL;
}

//删除好友
void del_fri(PACK *recv_pack)
{
    char query_str[1700];

    int flag = DEL_FRI;
    char ch[5];
    int fd = recv_pack->data.send_fd;

    Relation *q = pStart;
    int flag_3 = 0;
    while(q)
    {
        if((strcmp(q->name1, recv_pack->data.mes) == 0 && strcmp(q->name2, recv_pack->data.send_name) == 0) || (strcmp(q->name1, recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.mes) == 0))
        {
            flag_3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag_3 == 0)
        ch[0] = '0';
    else
    {
        Delete_R(q);

        memset(query_str, 0, strlen(query_str));
        sprintf(query_str, "delete from relationinfo where (name1='%s' and name2='%s') or (name1='%s' and name2='%s')", recv_pack->data.send_name, recv_pack->data.mes, recv_pack->data.mes, recv_pack->data.send_name);
        mysql_real_query(&mysql, query_str, strlen(query_str));
        ch[0] = '1';
    }
    send_more(fd, flag, recv_pack, ch);
}

//删除出关系表
void Delete_R(Relation *pNew)
{
    Relation *t = pStart;
    Relation *ptr;
    while(t)
    {
        if((strcmp(t->name1, pNew->name1) == 0 && strcmp(t->name2, pNew->name2) == 0) || (strcmp(t->name1, pNew->name2) == 0 && strcmp(t->name2, pNew->name2) == 0))
        {
            if(pStart == t)
            {
                pStart = t->next;
                free(t);
                return;
            }
            ptr->next = t->next;
            free(t);
            return;
        }
        ptr = t;
        t = t->next;
    }
}

//屏蔽好友
void shi_fri(PACK *recv_pack)
{
    char query_str[1700];

    int flag = SHI_FRI;
    char ch[5];
    int fd = recv_pack->data.send_fd;

    Relation *q = pStart;
    int flag_3 = 0;
    while(q)
    {
        if((strcmp(q->name1, recv_pack->data.mes) == 0 && strcmp(q->name2, recv_pack->data.send_name) == 0) || (strcmp(q->name1, recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.mes) == 0))
        {
            flag_3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag_3 == 0)
        ch[0] = '0';
    else
    {
        q->statu_s = FRI_BLK;
        memset(query_str, 0, strlen(query_str));
        sprintf(query_str, "update relationinfo set status=%d where (name1='%s' and name2='%s') or (name1='%s' and name2='%s')", FRI_BLK, recv_pack->data.send_name, recv_pack->data.mes, recv_pack->data.mes, recv_pack->data.send_name);
        mysql_real_query(&mysql, query_str, strlen(query_str));
        ch[0] = '1';
    }
    send_more(fd, flag, recv_pack, ch);

}

//创建群
void cre_grp(PACK *recv_pack)
{
    char query_str[1000];

    int flag = CRE_GRP;
    int fd = recv_pack->data.send_fd;
    char ch[5];

    Relation *q = pStart;
    int flag_3 = 0;
    Relation *pNew = (Relation *)malloc(sizeof(Relation));
    while(q)
    {
        if(strcmp(q->name2, recv_pack->data.mes) == 0)
        {
            flag_3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag_3 == 1)
    {
        ch[0] = '0';
    }
    else
    {
        ch[0] = '1';
        strcpy(pNew->name1, recv_pack->data.send_name);
        strcpy(pNew->name2, recv_pack->data.mes);
        pNew->statu_s = GRP_OWN;
        Insert_R(pNew);

        memset(query_str, 0, strlen(query_str));
        sprintf(query_str, "insert into relationinfo values('%s', '%s', %d)", recv_pack->data.send_name, recv_pack->data.mes, GRP_OWN);
        mysql_real_query(&mysql, query_str, strlen(query_str));
    }
    send_more(fd, flag, recv_pack, ch);
    free(pNew);
    pNew = NULL;
}

//加群
void add_grp(PACK *recv_pack)
{
    char query_str[1000];

    int flag = ADD_GRP;
    int fd = recv_pack->data.send_fd;
    char ch[5];

    Relation *q = pStart;
    int flag_3 = 0;
    Relation *pNew = (Relation *)malloc(sizeof(Relation));
    while(q)
    {
        if(strcmp(q->name2, recv_pack->data.mes) == 0)
        {
            flag_3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag_3 == 0)
    {
        ch[0] = '0';
    }
    else
    {
        ch[0] = '1';
        strcpy(pNew->name1, recv_pack->data.send_name);
        strcpy(pNew->name2, recv_pack->data.mes);
        pNew->statu_s = GRP;
        Insert_R(pNew);

        memset(query_str, 0, strlen(query_str));
        sprintf(query_str, "insert into relationinfo values('%s', '%s', %d)", recv_pack->data.send_name, recv_pack->data.mes, GRP);
        mysql_real_query(&mysql, query_str, strlen(query_str));
    }
    send_more(fd, flag, recv_pack, ch);
    free(pNew);
    pNew = NULL;
}

//退群
void out_grp(PACK *recv_pack)
{
    char query_str[1000];

    int flag = OUT_GRP;
    char ch[5];
    int fd = recv_pack->data.send_fd;

    Relation *q = pStart;
    int flag_3 = 0;
    while(q)
    {
        if(strcmp(q->name2, recv_pack->data.mes) == 0)
        {
            flag_3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag_3 == 0)
        ch[0] = '0';
    else
    {
        ch[0] = '1';
        Delete_R(q);

        memset(query_str, 0, strlen(query_str));
        sprintf(query_str, "delete from relationinfo where name1='%s' and name2='%s'", recv_pack->data.send_name, recv_pack->data.mes);
        mysql_real_query(&mysql, query_str, strlen(query_str));
    }
    send_more(fd, flag, recv_pack, ch);
}

//解散群
void del_grp(PACK *recv_pack)
{
    char query_str[1000];

    int flag = DEL_GRP;
    char ch[5];
    int fd = recv_pack->data.send_fd;

    Relation *q = pStart;
    int flag_3 = 0;
    int flag_3_3 = 0;
    while(q)
    {
        if(strcmp(q->name2, recv_pack->data.mes) == 0)
        {
            flag_3_3 = 1;
            break;
        }
        q = q->next;
    }

    q = pStart;
    while(q)
    {
        if(strcmp(q->name1, recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.mes) == 0 && (q->statu_s == GRP_OWN))
        {
            flag_3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag_3_3 == 0)
        ch[0] = '0';
    else if(flag_3 == 1 && flag_3_3 == 1)
    {
        ch[0] = '1';
        q = pStart;
        while(q)
        {
            if(strcmp(q->name2, recv_pack->data.mes) == 0)
                Delete_R(q);
            q = q->next;
        }
        memset(query_str, 0, strlen(query_str));
        sprintf(query_str, "delete from relationinfo where name2='%s'", recv_pack->data.mes);
        mysql_real_query(&mysql, query_str, strlen(query_str));
    }
    else if(flag_3 == 0 && flag_3_3 == 1)
        ch[0] = '2';
    send_more(fd, flag, recv_pack, ch);
}

//设置管理员
void set_grp_adm(PACK *recv_pack)
{
    char query_str[1000];

    int flag = SET_GRP_ADM;
    char ch[5];
    int fd = recv_pack->data.send_fd;

    Relation *q = pStart;
    int flag_3 = 0;
    int flag_3_3 = 0;
    int flag_3_3_3 = 0;
    while(q)
    {
        if(strcmp(q->name2, recv_pack->data.recv_name) == 0)
        {
            flag_3_3 = 1;
            break;
        }
        q = q->next;
    }

    q = pStart;
    while(q)
    {
        if(strcmp(q->name2, recv_pack->data.recv_name) == 0 && strcmp(q->name1, recv_pack->data.mes) == 0)
        {
            flag_3_3_3 = 1;
            break;
        }
        q = q->next;
    }

    q = pStart;
    while(q)
    {
        if(strcmp(q->name1, recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.recv_name) == 0 && q->statu_s == GRP_OWN)
        {
            flag_3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag_3 == 1 && flag_3_3 == 1 && flag_3_3_3 == 1)
    {
        ch[0] = '1';
        q = pStart;
        while(q)
        {
            if(strcmp(q->name1, recv_pack->data.mes) == 0 && strcmp(q->name2, recv_pack->data.recv_name) == 0)
            {
                q->statu_s = GRP_ADM;
                break;
            }
            q = q->next;
        }
        memset(query_str, 0, strlen(query_str));
        sprintf(query_str, "update relationinfo set status=%d where name1='%s' and name2='%s'", GRP_ADM, recv_pack->data.mes, recv_pack->data.recv_name);
        mysql_real_query(&mysql, query_str, strlen(query_str));
    }
    else if(flag_3 == 0 && flag_3_3 == 1 && flag_3_3_3 == 1)
        ch[0] = '2';
    else if(flag_3_3_3 == 0)
        ch[0] = '3';
    else if(flag_3_3 == 0)
        ch[0] = '0';
    send_more(fd, flag, recv_pack, ch);
}

//踢人
void kick_grp(PACK *recv_pack)
{
    char query_str[1000];

    int flag = KICK_GRP;
    char ch[5];
    int fd = recv_pack->data.send_fd;

    Relation *q = pStart;
    int flag_3 = 0;
    int flag_3_3 = 0;
    int flag_3_3_3 = 0;
    while(q)
    {
        if(strcmp(q->name2, recv_pack->data.recv_name) == 0)
        {
            flag_3_3 = 1;
            break;
        }
        q = q->next;
    }

    q = pStart;
    while(q)
    {
        if(strcmp(q->name2, recv_pack->data.recv_name) == 0 && strcmp(q->name1, recv_pack->data.mes) == 0)
        {
            flag_3_3_3 = 1;
            break;
        }
        q = q->next;
    }

    q = pStart;
    while(q)
    {
        if(strcmp(q->name1, recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.recv_name) == 0 && (q->statu_s == GRP_OWN || q->statu_s == GRP_ADM))
        {
            flag_3 = 1;
            break;
        }
        q = q->next;
    }

    if(flag_3 == 1 && flag_3_3 == 1 && flag_3_3_3 == 1)
    {
        ch[0] = '1';
        Delete_R(q);

        memset(query_str, 0, strlen(query_str));
        sprintf(query_str, "delete from relationinfo where name1='%s' and name2='%s'", recv_pack->data.mes, recv_pack->data.recv_name);
        mysql_real_query(&mysql, query_str, strlen(query_str));
    }
    else if(flag_3 == 0 && flag_3_3 == 1 && flag_3_3_3 == 1)
        ch[0] = '2';
    else if(flag_3_3_3 == 0)
        ch[0] = '3';
    else if(flag_3_3 == 0)
        ch[0] = '0';
    send_more(fd, flag, recv_pack, ch);
}

//查看所加群
void check_grp(PACK *recv_pack)
{
    GROUP_INFO grp_info;
    int fd = recv_pack->data.send_fd;
    Relation *q = pStart;
    int i = 0;

    while(q)
    {
        if(strcmp(q->name1, recv_pack->data.send_name) == 0 && (q->statu_s == GRP || q->statu_s == GRP_OWN || q->statu_s == GRP_ADM))
        {
            strcpy(grp_info.groups[i], q->name2);
            i++;
        }
        q = q->next;
    }
    grp_info.grp_num = i;

    if(send(fd, &grp_info, sizeof(GROUP_INFO), 0) < 0)
        my_err("send", __LINE__);
}

//查看群中成员
void check_mem_grp(PACK *recv_pack)
{
    FRI_INFO fri_info;
    int fd = recv_pack->data.send_fd;
    Relation *q = pStart;
    int i = 0;

    while(q)
    {
        if(strcmp(q->name2, recv_pack->data.mes) == 0 && (q->statu_s == GRP || q->statu_s == GRP_OWN || q->statu_s == GRP_ADM))
        {
            strcpy(fri_info.friends[i], q->name1);
            i++;
        }
        q = q->next;
    }
    fri_info.friends_num = i;

    if(send(fd, &fri_info, sizeof(FRI_INFO), 0) < 0)
        my_err("send", __LINE__);
}

//私聊
void chat_one(PACK *recv_pack)
{
    int flag = CHAT_ONE;
    char ch[5];
    int fd = recv_pack->data.send_fd;
    char ss[MAX_CHAR];
    
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char query_str[1500];
    int rows;
    int fields;
    RECORD_INFO rec_info[100];
    int i = 0,j;

    User *t = pHead;
    Relation *q = pStart;
    int flag_2 = 0;
    int flag_2_2 = 0;

    Recordinfo *pNew = (Recordinfo *)malloc(sizeof(Recordinfo));

    if(recv_pack->data.mes[0] == '0')
    {
        while(t)
        {
            if(strcmp(t->name, recv_pack->data.send_name) == 0)
            {
                t->statu_s = ONLINE;
                free(pNew);
                pNew = NULL;
                return;
            }
            t = t->next;
        }
    }

    while(q)
    {
        if(((strcmp(q->name1,recv_pack->data.send_name) == 0 && strcmp(q->name2, recv_pack->data.recv_name) == 0) || (strcmp(q->name2,recv_pack->data.send_name) == 0 && strcmp(q->name1, recv_pack->data.recv_name) == 0)) && (q->statu_s == FRI_BLK))
        {
            ch[0] = '3';
            send_more(fd, flag, recv_pack, ch);
            free(pNew);
            pNew = NULL;
            return;
        }
        q = q->next;
    }

    t = pHead;
    while(t)
    {
        if(strcmp(t->name, recv_pack->data.recv_name) == 0)
        {
            flag_2 = 1;
            break;
        }
        t = t->next;
    }

    if(flag_2 == 0)
    {
        ch[0] = '0';
        send_more(fd, flag, recv_pack, ch);
        free(pNew);
        pNew = NULL;
        return;
    }
    else
    {
        if(recv_pack->data.mes[0] == '1')
        {
            memset(query_str, 0, strlen(query_str));
            sprintf(query_str, "select * from off_recordinfo where name1='%s' and name2='%s'", recv_pack->data.recv_name, recv_pack->data.send_name);
            mysql_real_query(&mysql, query_str, strlen(query_str));
            res = mysql_store_result(&mysql);
            rows = mysql_num_rows(res);
            fields = mysql_num_fields(res);
            while(row = mysql_fetch_row(res))
            {
                strcpy(pNew->name1, row[0]);
                strcpy(pNew->name2, row[1]);
                strcpy(pNew->message, row[2]);
                Insert_RC(pNew);
                memset(query_str, 0, strlen(query_str));
                sprintf(query_str, "insert into recordinfo values('%s', '%s', '%s')", row[0], row[1], row[2]);
                mysql_real_query(&mysql, query_str, strlen(query_str));
                
                strcpy(rec_info[i].name1, row[0]);
                strcpy(rec_info[i].name2, row[1]);
                strcpy(rec_info[i].message, row[2]);
                i++;
                if(i > 100)
                    break;                          
            }
            rec_info[i].message[0] = '0';
            send(fd, &rec_info, sizeof(rec_info), 0);

            memset(query_str, 0, strlen(query_str));
            sprintf(query_str, "delete from off_recordinfo where name1='%s' and name2='%s'", recv_pack->data.recv_name, recv_pack->data.send_name);
            mysql_real_query(&mysql, query_str, strlen(query_str));
            
            t = pHead;
            while(t)
            {
                if(strcmp(t->name, recv_pack->data.send_name) == 0)
                {
                    t->statu_s = ONE_CHAT;
                    break;
                }
                t = t->next;
            }
            t = pHead;
            while(t)
            {
                if(strcmp(t->name, recv_pack->data.recv_name) == 0 && (t->statu_s != OFFLINE))
                {
                    flag_2_2 = 1;
                    break;
                }
                t = t->next;
            }
            if(flag_2_2 == 1)
            {
                ch[0] = '1';
                fd = t->fd;
                strcpy(ss,recv_pack->data.recv_name);
                strcpy(recv_pack->data.recv_name, recv_pack->data.send_name);
                strcpy(recv_pack->data.send_name, ss);
                send_more(fd, flag, recv_pack, ch);
                free(pNew);
                pNew = NULL;
                return;
            }
            else 
            {
                ch[0] = '2';
                send_more(fd, flag, recv_pack, ch);
                memcpy(&Mex_Box[sign++], recv_pack, sizeof(PACK));       
                free(pNew);
                pNew = NULL;
            }
        }
        else
        {
            t = pHead;
            while(t)
            {
                if(strcmp(t->name, recv_pack->data.recv_name) == 0 && (t->statu_s == ONE_CHAT))
                {
                    fd = t->fd;
                    strcpy(pNew->name1, recv_pack->data.send_name);
                    strcpy(pNew->name2, recv_pack->data.recv_name);
                    strcpy(pNew->message, recv_pack->data.mes);
                    Insert_RC(pNew);

                    memset(query_str, 0, strlen(query_str));
                    sprintf(query_str, "insert into recordinfo values('%s', '%s', '%s')", recv_pack->data.send_name, recv_pack->data.recv_name, recv_pack->data.mes);
                    mysql_real_query(&mysql, query_str, strlen(query_str));

                    strcpy(ss,recv_pack->data.recv_name);
                    strcpy(recv_pack->data.recv_name, recv_pack->data.send_name);
                    strcpy(recv_pack->data.send_name, ss);
                    send_more(fd, flag, recv_pack, recv_pack->data.mes);
                    free(pNew);
                    pNew = NULL;
                    return;
                }
                else if(strcmp(t->name, recv_pack->data.recv_name) == 0 && (t->statu_s != ONE_CHAT))
                {
                    memset(query_str, 0, strlen(query_str));
                    sprintf(query_str, "insert into off_recordinfo values('%s', '%s', '%s')", recv_pack->data.send_name, recv_pack->data.recv_name, recv_pack->data.mes);
                    mysql_real_query(&mysql, query_str, strlen(query_str));
                    free(pNew);
                    pNew = NULL;
                    return;
                }
                t = t->next;
            }
        }
    }
}

//加入聊天记录
void Insert_RC(Recordinfo *pNew)
{
    Recordinfo *p = pRec;
    while(p && p->next != NULL)
        p = p->next;
    p->next = pNew;
    pNew->next = NULL;
}

//群聊
void chat_many(PACK *recv_pack)
{
    int flag = CHAT_MANY;
    char ch[5];
    int fd = recv_pack->data.send_fd;
    char ss[MAX_CHAR];
    
    MYSQL_RES *res = NULL;
    MYSQL_ROW row;
    char query_str[1500];
    int rows;
    int fields;
    RECORD_INFO rec_info[100];
    int i = 0,j;

    User *t = pHead;
    Relation *q = pStart;
    int flag_2 = 0;

    Recordinfo *pNew = (Recordinfo *)malloc(sizeof(Recordinfo));

    if(recv_pack->data.mes[0] == '0')
    {
        while(t)
        {
            if(strcmp(t->name, recv_pack->data.send_name) == 0)
            {
                t->statu_s = ONLINE;
                free(pNew);
                pNew = NULL;
                return;
            }
            t = t->next;
        }
    }

    while(q)
    {
        if(strcmp(q->name2, recv_pack->data.recv_name) == 0 && (q->statu_s >= GRP))
        {
            flag_2 = 1;
            break;
        }
        q = q->next;
    }
    if(flag_2 == 0)
    {
        ch[0] = '0';
        send_more(fd, flag, recv_pack, ch);
        free(pNew);
        pNew = NULL;
        return;
    }
    else
    {
        if(recv_pack->data.mes[0] == '1')
        {
            memset(query_str, 0, strlen(query_str));
            sprintf(query_str, "select * from off_recordinfo where name2='%s'", recv_pack->data.recv_name);
            mysql_real_query(&mysql, query_str, strlen(query_str));
            res = mysql_store_result(&mysql);
            rows = mysql_num_rows(res);
            fields = mysql_num_fields(res);
            while(row = mysql_fetch_row(res))
            {
                strcpy(pNew->name1, row[0]);
                strcpy(pNew->name2, row[1]);
                strcpy(pNew->message, row[2]);
                Insert_RC(pNew);
                memset(query_str, 0, strlen(query_str));
                sprintf(query_str, "insert into recordinfo values('%s', '%s', '%s')", row[0], row[1], row[2]);
                mysql_real_query(&mysql, query_str, strlen(query_str));
                
                strcpy(rec_info[i].name1, row[0]);
                strcpy(rec_info[i].name2, row[1]);
                strcpy(rec_info[i].message, row[2]);
                i++;
                if(i > 100)
                    break;                          
            }
            rec_info[i].message[0] = '0';
            send(fd, &rec_info, sizeof(rec_info), 0);

            memset(query_str, 0, strlen(query_str));
            sprintf(query_str, "delete from off_recordinfo where name2='%s'", recv_pack->data.recv_name);
            mysql_real_query(&mysql, query_str, strlen(query_str));
            
            t = pHead;
            while(t)
            {
                if(strcmp(t->name, recv_pack->data.send_name) == 0)
                {
                    t->statu_s = MANY_CHAT;
                    break;
                }
                t = t->next;
            }
            q = pStart;
            while(q)
            {
                if(strcmp(q->name2, recv_pack->data.recv_name) == 0 && (q->statu_s >= GRP))
                {
                    if(strcmp(q->name1, recv_pack->data.send_name) == 0)
                        continue;

                    t = pHead;
                    while(t)
                    {
                        if(strcmp(q->name1, t->name) == 0 && (t->statu_s != OFFLINE))
                        {
                            ch[0] = '1';
                            fd = t->fd;
                            send_more(fd, flag, recv_pack, ch);
                            break;
                        }
                        else if(strcmp(q->name1, t->name) ==0 && (t->statu_s == OFFLINE))
                        {
                            memcpy(&Mex_Box[sign++], recv_pack, sizeof(PACK));       
                            free(pNew);
                            pNew = NULL;
                        }
                        t = t->next;
                    }
                }
                q = q->next;
            }
        }
        else
        {
            strcpy(pNew->name1, recv_pack->data.send_name);
            strcpy(pNew->name2, recv_pack->data.recv_name);
            strcpy(pNew->message, recv_pack->data.mes);
            Insert_RC(pNew);
            memset(query_str, 0, strlen(query_str));
            sprintf(query_str, "insert into recordinfo values('%s', '%s', '%s')", recv_pack->data.send_name, recv_pack->data.recv_name, recv_pack->data.mes);
            mysql_real_query(&mysql, query_str, strlen(query_str));

            q = pStart;
            while(q)
            {
                if(strcmp(q->name2, recv_pack->data.recv_name) == 0 && (q->statu_s >= GRP))
                {
                    if(strcmp(q->name1, recv_pack->data.send_name) == 0)
                        continue;

                    t = pHead;
                    while(t)
                    {
                        if(strcmp(q->name1, t->name) == 0 && (t->statu_s == MANY_CHAT))
                        {
                            fd = t->fd;

                            strcpy(ss,recv_pack->data.recv_name);
                            strcpy(recv_pack->data.recv_name, recv_pack->data.send_name);
                            strcpy(recv_pack->data.send_name, ss);
                            send_more(fd, flag, recv_pack, recv_pack->data.mes);
                            break;
                        }
                        else if(strcmp(q->name1, t->name) == 0 && (t->statu_s != MANY_CHAT))
                        {
                            memset(query_str, 0, strlen(query_str));
                            sprintf(query_str, "insert into off_recordinfo values('%s', '%s', '%s')", recv_pack->data.send_name, recv_pack->data.recv_name, recv_pack->data.mes);
                            mysql_real_query(&mysql, query_str, strlen(query_str));
                            break;
                        }
                        t = t->next;
                    }
                }
                q = q->next;
            }
        }
    }
}

//查看与好友聊天记录
void check_mes_fri(PACK *recv_pack)
{
    RECORD_INFO rec_info[100];
    int i = 0;
    int flag = CHECK_MES_FRI;
    char ch[5];
    int fd = recv_pack->data.send_fd;
    Recordinfo *p = pRec;
    while(p)
    {
        if((strcmp(p->name1, recv_pack->data.send_name) == 0 && strcmp(p->name2, recv_pack->data.mes) == 0) || (strcmp(p->name2, recv_pack->data.send_name) == 0 && strcmp(p->name1, recv_pack->data.mes) == 0))
        {
            strcpy(rec_info[i].name1, p->name1);
            strcpy(rec_info[i].name2, p->name2);
            strcpy(rec_info[i].message, p->message);
            i++;
            if(i > 100)
                break;
        }
        p = p->next;
    }
    rec_info[i].message[0] = '0';
    printf("%d\n",i);
    if(send(fd, &rec_info, sizeof(rec_info), 0) < 0)
        my_err("send", __LINE__);
}

void send_more(int fd, int type, PACK *recv_pack, char *mes)
{
    PACK pack_send;
    char ss[MAX_CHAR];
    memcpy(&pack_send, recv_pack, sizeof(PACK));
    strcpy(ss,pack_send.data.recv_name);
    
    pack_send.type = type;
    strcpy(pack_send.data.recv_name, pack_send.data.send_name);
    strcpy(pack_send.data.send_name, ss);
    strcpy(pack_send.data.mes, mes);
    pack_send.data.recv_fd = pack_send.data.send_fd;
    pack_send.data.send_fd = fd;

    if(send(fd, &pack_send, sizeof(PACK), 0) < 0)
        my_err("send", __LINE__);
}

//发送信息
void send_pack(int fd, PACK *recv_pack, char *ch)
{
    PACK pack_send;
    memcpy(&pack_send, recv_pack, sizeof(PACK));
    
    strcpy(pack_send.data.recv_name, pack_send.data.send_name);
    strcpy(pack_send.data.send_name, "server");
    strcpy(pack_send.data.mes, ch);
    pack_send.data.recv_fd = pack_send.data.send_fd;
    pack_send.data.send_fd = fd;

    if(send(fd, &pack_send, sizeof(PACK), 0) < 0)
        my_err("send", __LINE__);
}

//销毁链表
void DeleteLink()		
{
    User *q = pHead;
    if(pHead == NULL)
        return;
    while(pHead)
    {
        q = pHead->next;
        free(pHead);
        pHead = q;
    }
	pHead = NULL;
}

void DeleteLink_R()		
{
    Relation *q = pStart;
    if(pStart == NULL)
        return;
    while(pStart)
    {
        q = pStart->next;
        free(pStart);
        pStart = q;
    }
	pStart = NULL;
}

void DeleteLink_RC()
{
    Recordinfo *q = pRec;
    if(pRec == NULL)
        return;
    while(pRec)
    {
        q = pRec->next;
        free(pRec);
        pRec = q;
    }
	pRec = NULL;
}
