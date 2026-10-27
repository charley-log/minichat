#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <limits.h>
#include <memory>
#include <string>

#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"
#include "./timer/timer_service.h"
#include "./utils/utils.h"
#include "./server_manager/connection_manager.h"
#include "./server_manager/acceptor.h"
#include "./server_manager/epoller.h"
#include "./service/poll_manager.h"

const int MAX_FD = 65536;
const int MAX_EVENT_NUMBER = 10000;
const int TIMESLOT = 5;

class WebServer
{
public:
    WebServer();
    ~WebServer();

    // 禁止拷贝
    WebServer(const WebServer &) = delete;
    WebServer &operator=(const WebServer &) = delete;

    void init(int port, std::string user, std::string passWord, std::string databaseName,
              int log_write, int opt_linger, int trigmode, int sql_num,
              int thread_num, int close_log, int actor_model);

    void thread_pool();
    void sql_pool();
    void log_write();
    void trig_mode();
    void eventListen();
    void eventLoop();

    bool dealclientdata();
    bool dealwithsignal(bool &timeout, bool &stop_server);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);

public:
    // 基础
    int m_port = 0;
    std::string m_root;
    int m_log_write = 0;
    int m_close_log = 0;
    int m_actormodel = 0;

    int m_pipefd[2] = {-1, -1};

    // 数据库
    connection_pool *m_connPool = nullptr;
    std::string m_user;
    std::string m_passWord;
    std::string m_databaseName;
    int m_sql_num = 8;

    // epoll
    Epoller m_epoller;

    int m_listenfd = -1;
    int m_OPT_LINGER = 0;
    int m_TRIGMode = 0;
    int m_LISTENTrigmode = 0;
    int m_CONNTrigmode = 0;

    // 定时器
    Utils utils;
    TimerService m_timer;

    // 依赖顺序（声明顺序 => 析构逆序）：
    //   m_acceptor 先析构，接着 m_pool，再 m_timer，最后 m_conn_mgr
    // 保证 worker / timer 线程停止时 conn_mgr 仍然有效
    std::unique_ptr<ConnectionManager> m_conn_mgr;
    std::unique_ptr<threadpool> m_pool;
    std::unique_ptr<Acceptor> m_acceptor;

    int m_thread_num = 8;
};

#endif