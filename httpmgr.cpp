#include "httpmgr.h"
#include <QDebug>
#include <QUrl>
#include <QNetworkProxy>

const QString BASE_URL = "http://127.0.0.1:9006";

HttpMgr::HttpMgr()
{
    // 强制不走系统代理
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
}

void HttpMgr::postJson(const QString& url, const QJsonObject& body, std::function<void(QJsonObject)> callback)
{
    QNetworkRequest req(QUrl(BASE_URL + url));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Connection", "close");

    QNetworkReply* reply = m_mgr.post(req, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            callback(QJsonObject{{"code", -1}, {"msg", reply->errorString()}});
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            callback(QJsonObject{{"code", -1}, {"msg", "Invalid JSON"}});
            return;
        }
        callback(doc.object());
    });
}

void HttpMgr::postLogin(const QString& user, const QString& passwd)
{
    QJsonObject body{{"user", user}, {"passwd", passwd}};
    postJson("/api/login", body, [this, user](QJsonObject resp) {
        if (resp["code"].toInt() == 0) {
            QString token = resp["data"].toObject()["token"].toString();
            emit loginSuccess(user, token);
        } else {
            emit loginFailed(resp["msg"].toString());
        }
    });
}

void HttpMgr::postRegister(const QString& user, const QString& passwd)
{
    QJsonObject body{{"user", user}, {"passwd", passwd}};
    postJson("/api/register", body, [this](QJsonObject resp) {
        if (resp["code"].toInt() == 0) {
            emit registerSuccess();
        } else {
            emit registerFailed(resp["msg"].toString());
        }
    });
}

void HttpMgr::startPoll(const QString& token)
{
    m_token = token;
    m_lastMsgId = "0"; // 重置 since
    m_polling = true;
    doPoll(); // 启动死循环
}

void HttpMgr::stopPoll()
{
    m_polling = false;
}

void HttpMgr::doPoll()
{
    if (!m_polling || m_token.isEmpty()) return;

    QString url = QString("%1/api/poll?token=%2&since=%3").arg(BASE_URL, m_token, m_lastMsgId);
    QNetworkRequest req((QUrl(url)));
    req.setRawHeader("Connection", "close"); // 强制断开防死连接

    QNetworkReply* reply = m_pollMgr.get(req);
    QTimer::singleShot(30000, reply, &QNetworkReply::abort);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (!m_polling) { reply->deleteLater(); return; }

        if (reply->error() != QNetworkReply::NoError) {
            reply->deleteLater();
            QTimer::singleShot(1000, this, &HttpMgr::doPoll); // 出错等1秒再连
            return;
        }

        QByteArray data = reply->readAll();
        reply->deleteLater();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj["code"].toInt() == 0) {
                QJsonObject dataObj = obj["data"].toObject();
                QJsonArray msgs = dataObj["messages"].toArray();

                if (!msgs.isEmpty()) {
                    // 必须找到最大的 ID 推进游标
                    long long maxId = m_lastMsgId.toLongLong();
                    for (auto v : msgs) {
                        long long id = v.toObject()["id"].toVariant().toLongLong();
                        if (id > maxId) maxId = id;
                    }
                    m_lastMsgId = QString::number(maxId);

                    emit newMessagesReceived(msgs);
                }
            }
        }
        // 成功拉取后，立刻发起下一次请求（不加延时，反应最快）
        QTimer::singleShot(0, this, &HttpMgr::doPoll);
    });
}

void HttpMgr::postSendMessage(const QString& to, const QString& content)
{
    QJsonObject body{{"token", m_token}, {"to", to}, {"content", content}};
    postJson("/api/send", body, [](QJsonObject resp) {
        if (resp["code"].toInt() != 0) {
            qDebug() << "发送失败:" << resp["msg"].toString();
        }
    });
}

void HttpMgr::getOnlineUsers(std::function<void(QJsonArray)> callback)
{
    QNetworkRequest req(QUrl(BASE_URL + "/api/online"));
    QNetworkReply* reply = m_mgr.get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
            callback(obj["data"].toObject()["online"].toArray());
        } else {
            callback(QJsonArray());
        }
    });
}

void HttpMgr::getHistory(const QString& peer, std::function<void(QJsonArray)> callback)
{
    // 假设后端支持获取所有历史（这里为了简单，先拉取和 peer 的聊天记录）
    QString url = QString("%1/api/history?token=%2&peer=%3").arg(BASE_URL, m_token, peer);
    QNetworkRequest req((QUrl(url)));
    QNetworkReply* reply = m_mgr.get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
            callback(obj["data"].toObject()["messages"].toArray());
        } else {
            callback(QJsonArray());
        }
    });
}

void HttpMgr::getAllUsers(std::function<void(QJsonArray)> callback)
{
    QNetworkRequest req(QUrl(BASE_URL + "/api/users"));
    req.setRawHeader("Connection", "close"); // 防止复用死连接

    QNetworkReply* reply = m_mgr.get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, callback]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
            callback(obj["data"].toObject()["users"].toArray());
        } else {
            callback(QJsonArray());
        }
    });
}