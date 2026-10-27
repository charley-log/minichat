#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "logindialog.h"
#include "registerdialog.h"
#include "chatwindow.h"

#include "httpmgr.h"
#include <QMessageBox>
#include <QStackedWidget>
#include <QIcon>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/res/minichat_icon.ico"));

    // 用页面栈管理登录页和注册页
    // 注意：不要反复调用 setCentralWidget，它会把原来的中心控件删除，
    // 导致指针悬空，后面再使用就会崩溃或破坏内存
    _stack = new QStackedWidget(this);

    _login_dlg = new LoginDialog(this);          // 传 this，交给 Qt 父子关系释放
    _reg_dlg = new RegisterDialog(this);

    _stack->addWidget(_login_dlg);               // 索引 0
    _stack->addWidget(_reg_dlg);                 // 索引 1
    _stack->setCurrentWidget(_login_dlg);        // 默认显示登录页

    setCentralWidget(_stack);                    // 只设置一次中心控件

    // 登录页 → 注册页
    connect(_login_dlg, &LoginDialog::goToRegister, this, [this]{
        _stack->setCurrentWidget(_reg_dlg);
    });

    // 注册页 → 回登录页
    connect(_reg_dlg, &RegisterDialog::backToLogin, this, [this]{
        _stack->setCurrentWidget(_login_dlg);
    });

    // 注册成功 → 切回登录
    connect(_reg_dlg, &RegisterDialog::registerOk, this, [this](const QString &user){
        Q_UNUSED(user);
        _stack->setCurrentWidget(_login_dlg);
    });

    // 登录成功 → 进主界面
    connect(_login_dlg, &LoginDialog::loginOk, this, [this](const QString &user, const QString &token){
        // 1. 开启网络长轮询
        HttpMgr::instance().startPoll(token);

        // 2. 创建聊天窗口
        ChatWindow* chatWin = new ChatWindow(user);
        chatWin->setAttribute(Qt::WA_DeleteOnClose); // 关闭时自动释放内存

        // 隐藏主窗口
        this->hide();

        // 关闭聊天窗口时，整个程序退出
        connect(chatWin, &QWidget::destroyed, this, &QWidget::close);

        chatWin->show();
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}
