#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QWidget>
#include <QListWidgetItem>
#include <QMap>
#include <QJsonArray>

namespace Ui {
class ChatWindow;
}

class ContactItemWidget; // 前向声明

class ChatWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ChatWindow(const QString& myUser, QWidget *parent = nullptr);
    ~ChatWindow();

private slots:
    void onContactClicked(QListWidgetItem* item);
    void onSendClicked();
    void onNewMessagesReceived(const QJsonArray& msgs);

private:
    void loadFriends();
    void addConversation(const QString& user);
    void updateConversation(const QString& user, const QString& lastMsg, int unreadDelta);
    void appendMessage(const QString& from, const QString& content, bool isMine);
    void renderHistory(const QJsonArray& msgs);

    Ui::ChatWindow *ui;
    QString m_myUser;
    QString m_currentPeer;

    QMap<QString, ContactItemWidget*> m_conversations; // 好友列表控件
    QMap<QString, int> m_unreadCounts;                 // 未读计数
    QMap<QString, QJsonArray> m_historyCache;          // 历史记录缓存
};

#endif // CHATWINDOW_H
