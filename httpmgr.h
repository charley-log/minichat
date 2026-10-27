#ifndef HTTPMGR_H
#define HTTPMGR_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>
#include <QMessageBox>
#include "singleton.h"

class HttpMgr : public QObject, public Singleton<HttpMgr>
{
    Q_OBJECT
    friend class Singleton<HttpMgr>;

public:
    void postLogin(const QString& user, const QString& passwd);
    void postRegister(const QString& user, const QString& passwd);

    // 开启/停止长轮询
    void startPoll(const QString& token);
    void stopPoll();

    void postSendMessage(const QString& to, const QString& content);
    void getOnlineUsers(std::function<void(QJsonArray)> callback);
    void getHistory(const QString& peer, std::function<void(QJsonArray)> callback);
    void getAllUsers(std::function<void(QJsonArray)> callback); //获取所有用户

signals:
    // 登录注册信号
    void loginSuccess(const QString& user, const QString& token);
    void loginFailed(const QString& msg);
    void registerSuccess();
    void registerFailed(const QString& msg);

    // 收到新消息信号
    void newMessagesReceived(const QJsonArray& messages);

private:
    HttpMgr();

    // 发送 POST 请求的通用封装
    void postJson(const QString& url, const QJsonObject& body, std::function<void(QJsonObject)> callback);

    // 长轮询核心函数
    void doPoll();

    QString m_token;
    QString m_lastMsgId = "0"; // 记录收到的最后一条消息 ID
    bool m_polling = false;    // 当前是否处于轮询状态
    QNetworkAccessManager m_mgr;       // 用于普通请求 (Login, Send, History)
    QNetworkAccessManager m_pollMgr;   // 专门用于长轮询
};

#endif // HTTPMGR_H
