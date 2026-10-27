#include "webserver.h"

WebServer::WebServer()
    : m_epoller(MAX_EVENT_NUMBER),
      m_conn_mgr(std::make_unique<ConnectionManager>(MAX_FD, 3 * TIMESLOT))
{
    // 计算 root 目录
    char server_path[PATH_MAX] = {0};
    if (getcwd(server_path, sizeof(server_path)) != nullptr)
        m_root = std::string(server_path) + "/root";
    else
        m_root = "./root";
}

WebServer::~WebServer()
{
    // 1) 停定时器：不再触发连接回收
    m_timer.stop();

    // 2) 停线程池：join 所有 worker，之后不再访问 conn
    m_pool.reset();

    // 3) 关闭监听 fd 和信号管道
    if (m_listenfd >= 0)
    {
        close(m_listenfd);
        m_listenfd = -1;
    }
    if (m_pipefd[0] >= 0)
    {
        close(m_pipefd[0]);
        m_pipefd[0] = -1;
    }
    if (m_pipefd[1] >= 0)
    {
        close(m_pipefd[1]);
        m_pipefd[1] = -1;
    }

    // 4) m_acceptor / m_conn_mgr 由 unique_ptr 自动析构
}

void WebServer::init(int port,
                     std::string user, std::string passWord, std::string databaseName,
                     int log_write, int opt_linger, int trigmode,
                     int sql_num, int thread_num, int close_log, int actor_model)
{
    m_port = port;
    m_user = std::move(user);
    m_passWord = std::move(passWord);
    m_databaseName = std::move(databaseName);
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_log_write = log_write;
    m_OPT_LINGER = opt_linger;
    m_TRIGMode = trigmode;
    m_close_log = close_log;
    m_actormodel = actor_model;
}

void WebServer::trig_mode()
{
    switch (m_TRIGMode)
    {
    case 0:
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;
        break; // LT + LT
    case 1:
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
        break; // LT + ET
    case 2:
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 0;
        break; // ET + LT
    case 3:
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
        break; // ET + ET
    default:
        break;
    }
}

void WebServer::log_write()
{
    if (0 == m_close_log)
    {
        if (1 == m_log_write)
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 800);
        else
            Log::get_instance()->init("./ServerLog", m_close_log, 2000, 800000, 0);
    }
}

void WebServer::sql_pool()
{
    m_connPool = connection_pool::GetInstance();

    if (!m_connPool->init("localhost", m_user, m_passWord, m_databaseName,
                          3306, m_sql_num, m_close_log))
    {
        LOG_ERROR("%s", "init mysql connection pool failed");
        exit(1);
    }

    m_conn_mgr->get(0)->initmysql_result(m_connPool);
}

void WebServer::thread_pool()
{
    m_pool = std::make_unique<threadpool>(m_thread_num, 10000);
}

void WebServer::eventListen()
{
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    // 优雅关闭
    if (0 == m_OPT_LINGER)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (1 == m_OPT_LINGER)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    ret = bind(m_listenfd, reinterpret_cast<struct sockaddr *>(&address), sizeof(address));
    assert(ret >= 0);
    ret = listen(m_listenfd, 512);
    assert(ret >= 0);

    // Acceptor
    m_acceptor = std::make_unique<Acceptor>(m_listenfd, m_LISTENTrigmode);
    m_acceptor->setCloseLog(m_close_log);

    utils.setnonblocking(m_listenfd);
    m_epoller.add(m_listenfd,
                  Epoller::makeEvents(/*want_read=*/true, /*one_shot=*/false, m_LISTENTrigmode));
    http_conn::m_epoller = &m_epoller;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);

    utils.setnonblocking(m_pipefd[1]);
    utils.setnonblocking(m_pipefd[0]);
    m_epoller.add(m_pipefd[0],
                  Epoller::makeEvents(/*want_read=*/true, /*one_shot=*/false, 0));

    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    Utils::u_pipefd = m_pipefd;

    m_conn_mgr->setTimerService(&m_timer);
    m_conn_mgr->setEpoller(&m_epoller);
    m_conn_mgr->setCloseLog(m_close_log);

    m_timer.start(TIMESLOT);

    int poll_fd = PollManager::instance().getEventFd();
    m_epoller.add(poll_fd, Epoller::makeEvents(true, false, 0));
}

bool WebServer::dealclientdata()
{
    return m_acceptor->acceptAll(
        [this](int connfd, const sockaddr_in &addr) -> bool
        {
            if (http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                return false;
            }

            m_conn_mgr->add(connfd, addr,
                            &m_root[0], // std::string → char*
                            m_CONNTrigmode, m_close_log,
                            m_user, m_passWord, m_databaseName);
            return true;
        });
}

bool WebServer::dealwithsignal(bool &timeout, bool &stop_server)
{
    (void)timeout; // 定时器已移至独立线程，不再依赖此参数

    char signals[1024];
    int ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret <= 0)
        return false;

    for (int i = 0; i < ret; ++i)
    {
        if (signals[i] == SIGTERM)
            stop_server = true;
    }
    return true;
}

void WebServer::dealwithread(int sockfd)
{
    http_conn *conn = m_conn_mgr->get(sockfd);
    if (!conn)
        return;

    // ---------------- reactor ----------------
    if (1 == m_actormodel)
    {
        m_conn_mgr->touch(sockfd);

        bool enqueued = m_pool->enqueue([this, conn]
                                        {
            bool ok = conn->read_once();
            if (ok)
            {
                connectionRAII mysqlcon(&conn->mysql, m_connPool);
                if (conn->mysql != nullptr)
                    conn->process();
            }

            {
                std::lock_guard<std::mutex> g(conn->m_mutex);
                conn->improv = 1;
                if (!ok)
                    conn->timer_flag = 1;
            }
            conn->m_cond.notify_one(); });

        if (!enqueued)
        {
            m_conn_mgr->remove(sockfd);
            return;
        }

        // 等待 worker 完成
        bool need_remove = false;
        {
            std::unique_lock<std::mutex> lock(conn->m_mutex);
            conn->m_cond.wait(lock, [conn]
                              { return conn->improv == 1; });
            conn->improv = 0;

            if (conn->timer_flag)
            {
                conn->timer_flag = 0;
                need_remove = true;
            }
        }
        // 释放锁后再 remove，避免 remove 内部再访问 mutex
        if (need_remove)
            m_conn_mgr->remove(sockfd);
    }
    // ---------------- proactor ----------------
    else
    {
        if (conn->read_once())
        {
            LOG_INFO("deal with the client(%s)",
                     inet_ntoa(conn->get_address()->sin_addr));

            bool enqueued = m_pool->enqueue([this, conn]
                                            {
                connectionRAII mysqlcon(&conn->mysql, m_connPool);
                if (conn->mysql != nullptr)
                    conn->process(); });

            if (!enqueued)
            {
                m_conn_mgr->remove(sockfd);
                return;
            }

            m_conn_mgr->touch(sockfd);
        }
        else
        {
            m_conn_mgr->remove(sockfd);
        }
    }
}

void WebServer::dealwithwrite(int sockfd)
{
    http_conn *conn = m_conn_mgr->get(sockfd);
    if (!conn)
        return;

    // ---------------- reactor ----------------
    if (1 == m_actormodel)
    {
        m_conn_mgr->touch(sockfd);

        bool enqueued = m_pool->enqueue([conn]
                                        {
            bool ok = conn->write();

            {
                std::lock_guard<std::mutex> g(conn->m_mutex);
                conn->improv = 1;
                if (!ok)
                    conn->timer_flag = 1;
            }
            conn->m_cond.notify_one(); });

        if (!enqueued)
        {
            m_conn_mgr->remove(sockfd);
            return;
        }

        bool need_remove = false;
        {
            std::unique_lock<std::mutex> lock(conn->m_mutex);
            conn->m_cond.wait(lock, [conn]
                              { return conn->improv == 1; });
            conn->improv = 0;

            if (conn->timer_flag)
            {
                conn->timer_flag = 0;
                need_remove = true;
            }
        }
        if (need_remove)
            m_conn_mgr->remove(sockfd);
    }
    // ---------------- proactor ----------------
    else
    {
        if (conn->write())
        {
            LOG_INFO("send data to the client(%s)",
                     inet_ntoa(conn->get_address()->sin_addr));
            m_conn_mgr->touch(sockfd);
        }
        else
        {
            m_conn_mgr->remove(sockfd);
        }
    }
}

void WebServer::eventLoop()
{
    bool stop_server = false;
    bool timeout = false;

    while (!stop_server)
    {
        int number = m_epoller.wait(-1);
        if (number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i = 0; i < number; ++i)
        {
            int sockfd = m_epoller.dataFd(i);
            uint32_t ev = m_epoller.events(i);

            // 优先处理 eventfd，不让它掉进下面 socket 的处理逻辑里
            if (sockfd == PollManager::instance().getEventFd())
            {
                PollManager::instance().processWakeQueue();
                continue;
            }

            if (sockfd == m_listenfd)
            {
                dealclientdata();
            }
            else if (ev & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                m_conn_mgr->remove(sockfd);
            }
            else if (sockfd == m_pipefd[0] && (ev & EPOLLIN))
            {
                if (!dealwithsignal(timeout, stop_server))
                    LOG_ERROR("%s", "dealwithsignal failure");
            }
            else if (ev & EPOLLIN)
            {
                dealwithread(sockfd);
            }
            else if (ev & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
    }
}