#include <stdio.h>
#include<stdlib.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<string.h>
#include<time.h>
#include<pthread.h>
#include<fcntl.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<errno.h>
#include"chat.h"

#define SERV_PORT 9527

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

#define PASSIVE 0
#define ACTIVE 1

void *get_back(void *arg); //接受服务器的反馈
void Menu();            //主菜单
void Menu_friends();    //好友管理
void Menu_groups();     //群管理
void Menu_message();    //聊天记录
void Menu_mes_box();    //消息盒子
int login_menu();       //登陆菜单
int get_choice(char *choice_t); //为避免输入时出现的意外错误，进行字符串解析
int login();            //登陆
void registe();         //注册
void check_fri();       //查看好友列表
void add_fri();         //添加好友
void del_fri();         //删除好友
void shi_fri();         //屏蔽好友
void cre_grp();         //创建群
void add_grp();         //加群
void out_grp();         //退群
void power_grp_menu();  //群管理权限
void del_grp();         //解散群
void set_grp_adm();     //设置管理员
void kick_grp();        //踢人
void check_grp_menu();  //查看群
void check_grp();       //查看所加群
void check_mem_grp();   //查看群中成员
void chat_one();        //私聊
void chat_many();       //群聊
void check_mes_fri();   //查看与好友聊天记录
void check_mes_grp();   //查看群组聊天记录
void send_pack(int type, char *send_name, char *recv_name, char *mes);

int sock_fd;
char user[MAX_CHAR];    //当前登陆的账号名称
FRI_INFO fri_info;      //好友列表信息
GROUP_INFO grp_info;    //群列表信息
RECORD_INFO rec_info[100];  //聊天记录

//来自外部的请求——消息盒子
char name[100][MAX_CHAR];    
char mes_box[100][MAX_CHAR];
int mes_box_inc[100];
int sign;
int sign_ive[100];

pthread_mutex_t mutex;
pthread_cond_t cond;

int main(int argc, char *argv[])
{
    struct sockaddr_in serv_addr;
    pthread_t thid;

    //初始化服务器端地址
    memset(&serv_addr,0,sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERV_PORT);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    //创建TCP套接字
    sock_fd = socket(AF_INET,SOCK_STREAM,0);
    if(sock_fd < 0)
        my_err("socket",__LINE__);

    //向服务器端发送连接请求
    if(connect(sock_fd,(struct sockaddr *)&serv_addr,sizeof(struct sockaddr_in)) < 0)
        my_err("connect",__LINE__);
    
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    
    if(login_menu() == 0)   //判断是否登陆成功
    {
        close(sock_fd);
        return 0;
    }

    pthread_create(&thid, NULL, get_back, NULL);
    
    Menu();

    close(sock_fd);

    return 0;
}

void *get_back(void *arg)
{
    pthread_mutex_t mutex_t;
    pthread_mutex_init(&mutex_t, NULL);
    while(1)
    {
        pthread_mutex_lock(&mutex_t);
        int flag;
        PACK recv_pack;
        if(recv(sock_fd, &recv_pack, sizeof(PACK), 0) < 0)
            my_err("recv", __LINE__);
 
        switch(recv_pack.type)
        {
        case GET_FRI_STA:
            flag = recv_pack.data.mes[0] - '0';
            if(flag == 0)
                printf("\t\t        ***%s***\n",recv_pack.data.recv_name);
            else if(flag == 1)
                printf("\t\t\e[1;32m        ***%s***\e[0m\n",recv_pack.data.recv_name);
            
            pthread_cond_signal(&cond);
            break;

        case ADD_FRI:
            printf("\n\t\t\e[1;33m您有新消息啦!\e[0m\n");
            flag = recv_pack.data.mes[0] - '0';
            if(flag == 0)
            {
                sign_ive[sign] = PASSIVE;
                sprintf(name[sign], "%s", recv_pack.data.send_name);
                mes_box_inc[sign] = ADD_FRI;
                sprintf(mes_box[sign], "%s请求加你为好友(y/n): ", recv_pack.data.send_name);
                sign++;
            }
            else if(flag == 1)
            {
                sign_ive[sign] = ACTIVE;
                sprintf(mes_box[sign], "%s已同意请求", recv_pack.data.send_name);
                sign++;
            }
            else if(flag == 2)
            {
                sign_ive[sign] = ACTIVE;
                sprintf(mes_box[sign], "%s拒绝了你的请求", recv_pack.data.send_name);
                sign++;
            }
            else if(flag == 3)
            {
                sign_ive[sign] = ACTIVE;
                sprintf(mes_box[sign], "%s账号不存在", recv_pack.data.send_name);
                sign++;
            }
            else if(flag == 4)
            {
                sign_ive[sign] = ACTIVE;
                sprintf(mes_box[sign], "%s已是你的好友", recv_pack.data.send_name);
                sign++;
            }
            break;

        case DEL_FRI:
            flag = recv_pack.data.mes[0] - '0';
            if(flag == 0)
                printf("\n\t\t他不是你的好友!\n");
            else if(flag == 1)
                printf("\n\t\t删除成功!\n");
            
            pthread_cond_signal(&cond);
            break;

        case SHI_FRI:
            flag = recv_pack.data.mes[0] - '0';
            if(flag == 0)
                printf("\n\t\t他不是你的好友!\n");
            else if(flag == 1)
                printf("\n\t\t屏蔽成功!\n");
            
            pthread_cond_signal(&cond);
            break;

        case CRE_GRP:
            flag = recv_pack.data.mes[0] - '0';
            if(flag == 0)
                printf("\n\t\t该群名已被注册!\n");
            else if(flag == 1)
                printf("\n\t\t创建成功!\n");
            pthread_cond_signal(&cond);
            break;
        
        case ADD_GRP:
            flag = recv_pack.data.mes[0] - '0';
            if(flag == 0)
                printf("\n\t\t该群不存在!\n");
            else if(flag == 1)
                printf("\n\t\t加群成功!\n");
            pthread_cond_signal(&cond);
            break;

        case OUT_GRP:
            flag = recv_pack.data.mes[0] - '0';
            if(flag == 0)
                printf("\n\t\t该群不存在!\n");
            else if(flag == 1)
                printf("\n\t\t退群成功!\n");
            pthread_cond_signal(&cond);
            break;

        case DEL_GRP:
            flag = recv_pack.data.mes[0] - '0';
            if(flag == 0)
                printf("\n\t\t该群不存在!\n");
            else if(flag == 1)
                printf("\n\t\t解散群成功!\n");
            else if(flag == 2)
                printf("\n\t\t只有群主可以解散群!\n");
            pthread_cond_signal(&cond);
            break;

        case SET_GRP_ADM:
            flag = recv_pack.data.mes[0] - '0';
            if(flag == 0)
                printf("\n\t\t该群不存在!\n");
            else if(flag == 1)
                printf("\n\t\t设置管理员成功!\n");
            else if(flag == 2)
                printf("\n\t\t只有群主可以设置管理员!\n");
            else if(flag == 3)
                printf("\n\t\t此用户不在群中!\n");
            pthread_cond_signal(&cond);
            break;

        case KICK_GRP:
            flag = recv_pack.data.mes[0] - '0';
            if(flag == 0)
                printf("\n\t\t该群不存在!\n");
            else if(flag == 1)
                printf("\n\t\t踢人成功!\n");
            else if(flag == 2)
                printf("\n\t\t只有群主/管理员可以踢人!\n");
            else if(flag == 3)
                printf("\n\t\t此用户不在群中!\n");
            pthread_cond_signal(&cond);
            break;

        case CHAT_ONE:
            flag = recv_pack.data.mes[0] - '0';
            if(flag == 0)
                printf("\n\t\t该用户不存在!\n");
            else if(flag == 1)
                printf("\n\t\t\e[1;33m好友%s想要与你一起探讨人生...\e[0m\n",recv_pack.data.send_name);
            else if(flag == 2)
                printf("\n\t\t该用户不在线!\n");
            else if(flag == 3)
                printf("\n\t\t该好友已被屏蔽!\n");
            else
                printf("\n\t\t\e[1;34m%s:\e[0m %s\n", recv_pack.data.send_name, recv_pack.data.mes);
            break;

        case CHAT_MANY:
            flag = recv_pack.data.mes[0] - '0';
            if(flag == 0)
                printf("\n\t\t该群不存在!\n");
            else if(flag == 1)
                printf("\n\t\t\e[1;33m群%s有新消息🌶\e[0m\n",recv_pack.data.send_name);
            else
                printf("\n\t\t\e[1;34m%s:\e[0m %s\n", recv_pack.data.send_name, recv_pack.data.mes);
            break;

        default:
            break;
        }
        pthread_mutex_unlock(&mutex_t);
    }
}

//登陆菜单
int login_menu()
{
    char choice_s[100];
    int choice;
    do
    {
        printf("\n\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m           1.登陆          \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m           2.注册          \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m           0.退出          \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t请选择：");
        scanf("%s",choice_s);
        choice = get_choice(choice_s);
        
        switch(choice)
        {
        case 1:
            if(login() == 1)
                return 1;
            break;
        case 2:
            registe();
            break;
        default:
            break;
        }
        
    }while(choice != 0);
    return 0;
}

//为避免输入时出现的意外错误，进行字符串解析
int get_choice(char *choice_t)
{
    int choice = 0;
    for(int i = 0; i < strlen(choice_t); i++)
        if(choice_t[i] < '0' || choice_t[i] > '9')
            return -1;
    for(int i = 0; i < strlen(choice_t); i++)
    {
        int t = 1;
        for(int j = 1; j < strlen(choice_t)-i; j++)
        {
            t *= 10;
        }
        choice += t*(int)(choice_t[i] - 48);
    }
    return choice;
}


//注册
void registe()
{
    int flag = REGISTE;
    char registe_name[MAX_CHAR];
    char registe_passwd[MAX_CHAR];
    PACK recv_registe;
    int recv_registe_flag;

    printf("\t\t输入你想要注册账号的名称：");
    scanf("%s",registe_name);
    printf("\t\t输入你想要注册账号的密码：");
    scanf("%s",registe_passwd);
    
    send_pack(flag, registe_name, "server", registe_passwd);
    if(recv(sock_fd, &recv_registe, sizeof(PACK), 0) < 0)
        my_err("recv", __LINE__);
    recv_registe_flag = recv_registe.data.mes[0] - '0';

    if(recv_registe_flag == 1)
        printf("\t\t注册成功!\n");
    else if(recv_registe_flag == 0)
        printf("\t\t该用户名已存在，请重新选择!\n");
}

//登陆
int login()
{
    int flag = LOGIN;
    char login_name[MAX_CHAR];
    char login_passwd[MAX_CHAR];
    PACK recv_login;
    int recv_login_flag;

    printf("\t\t请输入账号名称：");
    scanf("%s",login_name);
    printf("\t\t请输入账号密码：");
    scanf("%s",login_passwd);

    send_pack(flag, login_name, "server", login_passwd);
    if(recv(sock_fd, &recv_login, sizeof(PACK), 0) < 0)
        my_err("recv", __LINE__);
    recv_login_flag = recv_login.data.mes[0] - '0';

    if(recv_login_flag == 1)
    {
        printf("\t\t登陆成功!\n");
        strncpy(user, login_name, strlen(login_name));
        return 1;
    }
    else if(recv_login_flag == 0)
        printf("\t\t该用户账号不存在!\n");
    else if(recv_login_flag == 2)
        printf("\t\t该用户已在线!\n");
    return 0;
}

//主菜单
void Menu()
{
    char choice_s[100];
    int choice;
    int flag;
    do
    {
        printf("\n\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         1.好友管理        \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         2.群管理          \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         3.聊天记录        \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         4.消息盒子        \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         0.注销            \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t请选择：");
        scanf("%s",choice_s);
        choice = get_choice(choice_s);
        
        switch(choice)
        {
        case 1:
            Menu_friends();
            break;

        case 2:
            Menu_groups();
            break;
           
        case 3:
            Menu_message();
            break;

        case 4:
            Menu_mes_box();
            break;
        
        default:
            break;
        }
    }while(choice != 0);
    flag = EXIT;
    send_pack(flag, user, "server", " ");
}

//好友管理
void Menu_friends()
{
    char choice_s[100];
    int choice;
    do
    {
        printf("\n\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         1.查看好友        \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         2.添加好友        \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         3.删除好友        \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         4.屏蔽好友        \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         5.私聊            \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         0.返回            \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t请选择：");
        scanf("%s",choice_s);
        choice = get_choice(choice_s);
        
        switch(choice)
        {
        case 1:
            check_fri();
            break;

        case 2:
            add_fri();
            break;
           
        case 3:
            del_fri();
            break;

        case 4:
            shi_fri();
            break;

        case 5:
            chat_one();
            break;

        default:
            break;
        }
    }while(choice != 0);
}

//查看好友列表
void check_fri()
{
    int flag = CHECK_FRI;
    char mes[MAX_CHAR];
    memset(mes, 0, sizeof(mes));
    memset(&fri_info, 0, sizeof(fri_info));
    int i;

    pthread_mutex_lock(&mutex);
    send_pack(flag, user, "server", mes);
    if(recv(sock_fd, &fri_info, sizeof(FRI_INFO), 0) < 0)
        my_err("recv", __LINE__);
        
    printf("\n\t\t\e[1;34m***********friends***********\e[0m\n");
    if(fri_info.friends_num == 0)
        printf("\t\t暂无好友!\n");
    else
    {
        for(i = 0; i < fri_info.friends_num; i++)
        {
            if(fri_info.friends_status[i] == 1)
            {
                flag = GET_FRI_STA;
                send_pack(flag, fri_info.friends[i], "server", mes);
                pthread_cond_wait(&cond, &mutex);
            }
            else if(fri_info.friends_status[i] == 2)
                printf("\t\t\e[1;31m        ***%s***\e[0m\n",fri_info.friends[i]);
        }
    }
    pthread_mutex_unlock(&mutex);
}

//添加好友
void add_fri()
{
    int i;
    int flag = ADD_FRI;
    pthread_mutex_lock(&mutex);
    char friend_add[MAX_CHAR];
    printf("\t\t你想要添加的好友名称：");
    scanf("%s",friend_add);
    send_pack(flag, user, friend_add, "0");
    pthread_mutex_unlock(&mutex);
}

//删除好友
void del_fri()
{
    int flag = DEL_FRI;
    char friend_del[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("\t\t你想要删除的好友名称：");
    scanf("%s",friend_del);
    send_pack(flag, user, "server", friend_del);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

//屏蔽好友
void shi_fri()
{
    int flag = SHI_FRI;
    char friend_shi[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("\t\t你想要屏蔽的好友名称：");
    scanf("%s",friend_shi);
    send_pack(flag, user, "server", friend_shi);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

//私聊
void chat_one()
{
    int flag = CHAT_ONE;
    char chat_name[MAX_CHAR];
    char mes[MAX_CHAR];
    int i = 0;
    memset(mes, 0, sizeof(mes));
    memset(&rec_info, 0, sizeof(rec_info));
    rec_info[0].message[0] = '0';
    pthread_mutex_lock(&mutex);
    printf("\n\t\t你想要和谁聊天呢? ");
    scanf("%s",chat_name);
    mes[0] = '1';
    send_pack(flag, user, chat_name, mes);
    if(recv(sock_fd, &rec_info, sizeof(rec_info), 0) < 0)
        my_err("recv", __LINE__);
    printf("\n\t\t\e[1;34m***********off_Message***********\e[0m\n");
    if(rec_info[0].message[0] == '0')
        printf("\t\t暂无未读消息\n");
    while(rec_info[i].message[0] != '0')
    {
        printf("\t\t\e[1;35m%s-->%s: \e[0m%s\n",rec_info[i].name1, rec_info[i].name2, rec_info[i].message);
        i++;
    }
    char choice_s[100];
    int choice;
    do
    {
        printf("\n\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         1.发送消息        \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         2.发送文件        \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         0.返回            \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        //printf("\t\t请选择：");
        scanf("%s",choice_s);
        choice = get_choice(choice_s);
        
        memset(mes, 0, sizeof(mes));
        switch(choice)
        {
        case 1:
            printf("\t\t\e[1;34m%s:\e[0m ", user);
            scanf("%s", mes);
            send_pack(flag, user, chat_name, mes);
            break;

        case 2:
            
            break;

        default:
            break;
        }
    }while(choice != 0);

    mes[0] = '0';
    send_pack(flag, user, "server", mes);
    pthread_mutex_unlock(&mutex);
}

//群管理
void Menu_groups()
{
    char choice_s[100];
    int choice;
    do
    {
        printf("\n\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         1.查看群          \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         2.创建群          \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         3.加群            \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         4.退群            \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         5.power           \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         6.群聊            \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         0.返回            \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t请选择：");
        scanf("%s",choice_s);
        choice = get_choice(choice_s);
        
        switch(choice)
        {
        case 1:
            check_grp_menu();           
            break;

        case 2:
            cre_grp();
            break;
           
        case 3:
            add_grp();
            break;

        case 4:
            out_grp();
            break;

        case 5:
            power_grp_menu();
            break;

        case 6:
            chat_many();
            break;

        default:
            break;
        }
    }while(choice != 0);
}

//查看群
void check_grp_menu()
{
    char choice_s[100];
    int choice;
    do
    {
        printf("\n\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         1.查看所加群      \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         2.查看群中成员    \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         0.返回            \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t请选择：");
        scanf("%s",choice_s);
        choice = get_choice(choice_s);
        
        switch(choice)
        {
        case 1:
            check_grp();
            break;

        case 2:
            check_mem_grp();
            break;

        default:
            break;
        }
    }while(choice != 0);
}

//查看所加群
void check_grp()
{
    int flag = CHECK_GRP;
    char mes[MAX_CHAR];
    memset(mes, 0, sizeof(mes));
    memset(&grp_info, 0, sizeof(grp_info));
    int i;

    pthread_mutex_lock(&mutex);
    send_pack(flag, user, "server", mes);
    if(recv(sock_fd, &grp_info, sizeof(GROUP_INFO), 0) < 0)
        my_err("recv", __LINE__);
        
    printf("\n\t\t\e[1;34m***********groups***********\e[0m\n");
    if(grp_info.grp_num == 0)
        printf("\t\t暂无加入群聊!\n");
    else
    {
        for(i = 0; i < grp_info.grp_num; i++)
        {
            printf("\t\t        ***%s***\n",grp_info.groups[i]);
        }
    }
    pthread_mutex_unlock(&mutex);
}

//查看群中成员
void check_mem_grp()
{
    int flag = CHECK_MEM_GRP;
    char mes[MAX_CHAR];
    int i;

    pthread_mutex_lock(&mutex);
    printf("\n\t\t你想要查看那个群中的成员信息：");
    scanf("%s",mes);
    for(i = 0; i < grp_info.grp_num; i++)
    {
        if(strcmp(grp_info.groups[i], mes) == 0)
            break;
    }
    if(i >= grp_info.grp_num)
        printf("\t\t你没有加入此群!\n");
    else
    {
        memset(&fri_info, 0, sizeof(fri_info));
        send_pack(flag, user, "server", mes);
        if(recv(sock_fd, &fri_info, sizeof(FRI_INFO), 0) < 0)
            my_err("recv", __LINE__);
        printf("\n\t\t\e[1;34m***********%s***********\e[0m\n",mes);
        if(fri_info.friends_num == 0)
            printf("该群中暂无成员!\n");
        else
        {
            for(i = 0; i < fri_info.friends_num; i++)
                printf("\t\t        ***%s***\n", fri_info.friends[i]);
        }
    }
    pthread_mutex_unlock(&mutex);
}

//创建群
void cre_grp()
{
    int flag = CRE_GRP;
    char grp_cre[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("\t\t你想要创建的群名称：");
    scanf("%s",grp_cre);
    send_pack(flag, user, "server", grp_cre);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

//加群
void add_grp()
{
    int flag = ADD_GRP;
    char grp_add[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("\t\t你想要加入的群名称：");
    scanf("%s",grp_add);
    send_pack(flag, user, "server", grp_add);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

//退群
void out_grp()
{
    int flag = OUT_GRP;
    char grp_out[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("\t\t你想要退出的群名称：");
    scanf("%s",grp_out);
    send_pack(flag, user, "server", grp_out);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

//群管理权限
void power_grp_menu()
{
    printf("\n\t\t\e[1;33m身为群主/管理员，你有以下特权：\e[0m\n");
    printf("\t\t\e[1;33m群主——1，2，3\e[0m\n");
    printf("\t\t\e[1;33m管理员——3\e[0m\n");
    char choice_s[100];
    int choice;
    do
    {
        printf("\n\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         1.解散群          \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         2.设置管理员      \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         3.踢人            \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         0.返回            \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t请选择：");
        scanf("%s",choice_s);
        choice = get_choice(choice_s);
        
        switch(choice)
        {
        case 1:
            del_grp();
            break;

        case 2:
            set_grp_adm();
            break;
           
        case 3:
            kick_grp();
            break;

        default:
            break;
        }
    }while(choice != 0);
}

//解散群
void del_grp()
{
    int flag = DEL_GRP;
    char grp_del[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("\t\t你想要解散的群名称：");
    scanf("%s",grp_del);
    send_pack(flag, user, "server", grp_del);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

//设置管理员
void set_grp_adm()
{
    int flag = SET_GRP_ADM;
    char grp_set_1[MAX_CHAR];
    char grp_set_2[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("\t\t你想要在那个群设置谁为管理员：");
    scanf("%s",grp_set_1);
    scanf("%s",grp_set_2);
    send_pack(flag, user, grp_set_1, grp_set_2);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

//踢人
void kick_grp()
{
    int flag = KICK_GRP;
    char grp_set_1[MAX_CHAR];
    char grp_set_2[MAX_CHAR];
    pthread_mutex_lock(&mutex);
    printf("\t\t你想要在那个群将谁踢出：");
    scanf("%s",grp_set_1);
    scanf("%s",grp_set_2);
    send_pack(flag, user, grp_set_1, grp_set_2);
    pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
}

//群聊
void chat_many()
{
    int flag = CHAT_MANY;
    char chat_name[MAX_CHAR];
    char mes[MAX_CHAR];
    int i = 0;
    memset(mes, 0, sizeof(mes));
    memset(&rec_info, 0, sizeof(rec_info));
    rec_info[0].message[0] = '0';
    pthread_mutex_lock(&mutex);
    printf("\n\t\t你想要在那个群中聊天呢? ");
    scanf("%s",chat_name);
    mes[0] = '1';
    send_pack(flag, user, chat_name, mes);
    if(recv(sock_fd, &rec_info, sizeof(rec_info), 0) < 0)
        my_err("recv", __LINE__);
    printf("\n\t\t\e[1;34m***********off_Message***********\e[0m\n");
    if(rec_info[0].message[0] == '0')
        printf("\t\t暂无未读消息\n");
    while(rec_info[i].message[0] != '0')
    {
        printf("\t\t\e[1;35m%s-->%s: \e[0m%s\n",rec_info[i].name1, rec_info[i].name2, rec_info[i].message);
        i++;
    }
    char choice_s[100];
    int choice;
    do
    {
        printf("\n\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         1.发送消息        \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         2.发送文件        \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         0.返回            \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        //printf("\t\t请选择：");
        scanf("%s",choice_s);
        choice = get_choice(choice_s);
        
        memset(mes, 0, sizeof(mes));
        switch(choice)
        {
        case 1:
            printf("\t\t\e[1;34m%s:\e[0m ", user);
            scanf("%s", mes);
            send_pack(flag, user, chat_name, mes);
            break;

        case 2:
            
            break;

        default:
            break;
        }
    }while(choice != 0);

    mes[0] = '0';
    send_pack(flag, user, "server", mes);
    pthread_mutex_unlock(&mutex);
}

//聊天记录
void Menu_message()
{
    char choice_s[100];
    int choice;
    do
    {
        printf("\n\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         1.好友记录        \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         2.群记录          \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t\e[1;32m|\e[0m         0.返回            \e[1;32m|\e[0m\n");
        printf("\t\t\e[1;32m-----------------------------\e[0m\n");
        printf("\t\t请选择：");
        scanf("%s",choice_s);
        choice = get_choice(choice_s);
        
        switch(choice)
        {
        case 1:
            check_mes_fri();
            break;

        case 2:
            //check_mes_grp();
            break;
           
        default:
            break;
        }
    }while(choice != 0);
}

//与好友聊天记录
void check_mes_fri()
{
    int i = 0;
    int flag = CHECK_MES_FRI;
    char mes_fri[MAX_CHAR];
    memset(&rec_info, 0, sizeof(rec_info));
    rec_info[0].message[0] = '0';
    pthread_mutex_lock(&mutex);
    printf("\n\t\t你想要查看与谁的聊天记录? ");
    scanf("%s",mes_fri);
    send_pack(flag, user, "server", mes_fri);
    if(recv(sock_fd, &rec_info, sizeof(rec_info), 0) < 0)
        my_err("recv", __LINE__);
    printf("\n\t\t\e[1;34m***********Message***********\e[0m\n");
    if(rec_info[0].message[0] == '0')
        printf("\t\t暂无历史记录\n");
    while(rec_info[i].message[0] != '0')
    {
        printf("\t\t\e[1;35m%s-->%s: \e[0m%s\n",rec_info[i].name1, rec_info[i].name2, rec_info[i].message);
        i++;
    }
    pthread_mutex_unlock(&mutex);
}

//消息盒子
void Menu_mes_box()
{
    int i;
    char ch[5];
    pthread_mutex_lock(&mutex);
    printf("\n\t\t您有%d条消息未读\n", sign);
    for(i = 0; i < sign; i++)
    {
        if(sign_ive[i] == PASSIVE)
        {
            printf("\t\tNO.%d: %s", i + 1, mes_box[i]);
            scanf("%s", ch);
            send_pack(mes_box_inc[i], user, name[i], ch);
        }
        else if(sign_ive[i] == ACTIVE)
            printf("\t\tNO.%d: %s\n", i + 1, mes_box[i]);
    }
    sign = 0;
    pthread_mutex_unlock(&mutex);
}

//发送信息
void send_pack(int type, char *send_name, char *recv_name, char *mes)
{
    PACK pack_send;
    pack_send.type = type;
    pack_send.data.recv_fd = sock_fd;
    strcpy(pack_send.data.send_name, send_name);
    strcpy(pack_send.data.recv_name, recv_name);
    strcpy(pack_send.data.mes, mes);
    if(send(sock_fd, &pack_send, sizeof(PACK), 0) < 0)
        my_err("send",__LINE__);
}
