#include "chatwindow.h"
#include "ui_chatwindow.h"
#include "httpmgr.h"
#include <QMessageBox>
#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextCursor>
#include <QScrollBar>
#include <QTimer>

// ================= 自定义会话列表项控件 =================
class ContactItemWidget : public QWidget {
public:
    QLabel* nameLabel;
    QLabel* msgLabel;
    QLabel* unreadLabel;

    ContactItemWidget(const QString& name, QWidget* parent = nullptr) : QWidget(parent) {
        // 强制背景透明
        this->setStyleSheet("background-color: transparent;");

        QHBoxLayout* mainLayout = new QHBoxLayout(this);
        mainLayout->setContentsMargins(5, 5, 5, 5);

        QVBoxLayout* textLayout = new QVBoxLayout;
        textLayout->setSpacing(2);

        nameLabel = new QLabel(name);
        nameLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #000000; background: transparent;");

        msgLabel = new QLabel("暂无消息");
        msgLabel->setStyleSheet("color: #666666; font-size: 12px; background: transparent;");

        textLayout->addWidget(nameLabel);
        textLayout->addWidget(msgLabel);

        unreadLabel = new QLabel("");
        unreadLabel->setStyleSheet("background-color: #ff3b30; color: white; border-radius: 8px; padding: 2px 6px; font-size: 11px; font-weight: bold;");
        unreadLabel->hide();

        mainLayout->addLayout(textLayout, 1);
        mainLayout->addWidget(unreadLabel, 0, Qt::AlignTop | Qt::AlignRight);
    }

    void setLastMsg(const QString& msg) {
        QString disp = msg;
        if (disp.length() > 10) disp = disp.left(10) + "...";
        msgLabel->setText(disp);
    }

    void setUnread(int count) {
        if (count > 0) {
            unreadLabel->setText(count > 99 ? "99+" : QString::number(count));
            unreadLabel->show();
        } else {
            unreadLabel->hide();
        }
    }
};
// ========================================================

ChatWindow::ChatWindow(const QString& myUser, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChatWindow)
    , m_myUser(myUser)
{
    ui->setupUi(this);
    setWindowTitle("MiniChat - " + m_myUser);

    ui->chatDisplay->setAcceptRichText(true);

    connect(ui->contactList, &QListWidget::itemClicked, this, &ChatWindow::onContactClicked);
    connect(ui->sendBtn, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
    connect(ui->inputEdit, &QLineEdit::returnPressed, this, &ChatWindow::onSendClicked);
    connect(&HttpMgr::instance(), &HttpMgr::newMessagesReceived, this, &ChatWindow::onNewMessagesReceived);

    loadFriends();
}

ChatWindow::~ChatWindow()
{
    delete ui;
}

void ChatWindow::loadFriends()
{
    HttpMgr::instance().getAllUsers([this](QJsonArray usersArray){
        for (auto v : usersArray) {
            QString user = v.toString();
            if (user != m_myUser) {
                addConversation(user);
            }
        }
    });
}

void ChatWindow::addConversation(const QString& user)
{
    // 如果已经存在，直接返回，绝不重复添加
    if (m_conversations.contains(user)) return;

    QListWidgetItem* item = new QListWidgetItem(ui->contactList);
    item->setData(Qt::UserRole, user); // 核心：把用户名存在这里，千万别用文本去判断
    item->setSizeHint(QSize(150, 55));

    ContactItemWidget* widget = new ContactItemWidget(user);
    ui->contactList->setItemWidget(item, widget);

    m_conversations[user] = widget;
    m_unreadCounts[user] = 0;
}

void ChatWindow::updateConversation(const QString& user, const QString& lastMsg, int unreadDelta)
{
    if (!m_conversations.contains(user)) {
        addConversation(user);
    }

    ContactItemWidget* widget = m_conversations[user];
    widget->setLastMsg(lastMsg);

    m_unreadCounts[user] += unreadDelta;
    widget->setUnread(m_unreadCounts[user]);
}

void ChatWindow::onContactClicked(QListWidgetItem* item)
{
    if (!item) return;
    QString user = item->data(Qt::UserRole).toString();
    m_currentPeer = user;

    ui->chatDisplay->clear();
    m_unreadCounts[m_currentPeer] = 0;
    if (m_conversations.contains(m_currentPeer)) {
        m_conversations[m_currentPeer]->setUnread(0);
    }

    // 如果有缓存，直接渲染
    if (m_historyCache.contains(m_currentPeer)) {
        renderHistory(m_historyCache[m_currentPeer]);
    } else {
        ui->chatDisplay->setText("<div style='color:gray; text-align:center; margin-top:20px;'>正在加载...</div>");
        HttpMgr::instance().getHistory(m_currentPeer, [this, user](QJsonArray msgs){
            m_historyCache[user] = msgs;
            if (m_currentPeer == user) {
                ui->chatDisplay->clear();
                renderHistory(msgs);
            }
        });
    }
}

// 确保 renderHistory 里的判断是正确的
void ChatWindow::renderHistory(const QJsonArray& msgs)
{
    for (auto v : msgs) {
        QJsonObject msg = v.toObject();
        QString from = msg["from"].toString();
        QString content = msg["content"].toString();

        // 这里：如果 from 是当前登录的人（我），那就是 isMine = true
        appendMessage(from, content, from == m_myUser);
    }
}

void ChatWindow::onSendClicked()
{
    QString content = ui->inputEdit->text().trimmed();
    if (content.isEmpty() || m_currentPeer.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择会话并输入内容");
        return;
    }

    ui->inputEdit->clear();

    // 1. 发送网络请求
    HttpMgr::instance().postSendMessage(m_currentPeer, content);

    // 2. 本地立刻回显（解决看不见自己消息的问题）
    appendMessage(m_myUser, content, true);

    // 3. 存入历史缓存，防止切换联系人后消息消失
    QJsonObject msgObj;
    msgObj["from"] = m_myUser;
    msgObj["content"] = content;
    m_historyCache[m_currentPeer].append(msgObj);

    // 更新左侧列表
    updateConversation(m_currentPeer, content, 0);
}

void ChatWindow::onNewMessagesReceived(const QJsonArray& msgs)
{
    for (auto v : msgs) {
        QJsonObject msg = v.toObject();
        QString from = msg["from"].toString();
        QString content = msg["content"].toString();

        // 【关键修复】如果是自己发的消息，因为本地已经回显过了，直接跳过！
        if (from == m_myUser) {
            continue;
        }

        addConversation(from);
        m_historyCache[from].append(msg);

        if (from == m_currentPeer) {
            appendMessage(from, content, false);
            updateConversation(from, content, 0);
        } else {
            updateConversation(from, content, 1);
        }
    }
}

void ChatWindow::appendMessage(const QString& from, const QString& content, bool isMine)
{
    QString bgColor = isMine ? "#95ec69" : "#ffffff";       // 微信绿 和 白底
    QString border  = isMine ? "0" : "1";                   // 自己发的不带边框，对方带边框
    QString bColor  = isMine ? "#95ec69" : "#dcdcdc";       // 边框颜色
    QString align   = isMine ? "right" : "left";

    QString safeContent = content.toHtmlEscaped();
    safeContent.replace("\n", "<br>");

    // 【绝对稳定版】抛弃所有可能崩溃的 CSS，只用原生 Table 属性
    QString html = QString(
                       "<table width='100%' cellspacing='0' cellpadding='0' style='margin-bottom: 10px;'>"
                       "  <tr>"
                       "    <td align='%1'>"
                       "      <table border='%2' bordercolor='%3' cellspacing='0' cellpadding='8' style='background-color: %4;'>"
                       "        <tr>"
                       "          <td><font color='#333333' size='3'>%5</font></td>"
                       "        </tr>"
                       "      </table>"
                       "    </td>"
                       "  </tr>"
                       "</table>"
                       ).arg(align, border, bColor, bgColor, safeContent);

    // 使用 append 插入
    ui->chatDisplay->append(html);

    // 自动滚动到底部
    QScrollBar* bar = ui->chatDisplay->verticalScrollBar();
    bar->setValue(bar->maximum());
}