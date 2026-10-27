#include "logindialog.h"
#include "ui_logindialog.h"
#include "httpmgr.h" // 包含网络类
#include <QMessageBox>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);

    QPixmap pix(":/res/qrcode.png");
    ui->label->setPixmap(pix.scaled(140, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->label->setScaledContents(true);

    // 点击登录 → 发请求
    connect(ui->pushButton, &QPushButton::clicked, this, [=]{
        QString user = ui->user_edit->text().trimmed();
        QString pass = ui->pass_edit->text();
        if (user.isEmpty() || pass.isEmpty()) {
            QMessageBox::warning(this, "提示", "用户名或密码不能为空");
            return;
        }
        ui->pushButton->setEnabled(false); // 防抖
        HttpMgr::instance().postLogin(user, pass);
    });

    // 接收网络层的信号
    connect(&HttpMgr::instance(), &HttpMgr::loginSuccess, this, [this](const QString& user, const QString& token){
        ui->pushButton->setEnabled(true);
        emit loginOk(user, token); // 发信号给主窗口
    });

    connect(&HttpMgr::instance(), &HttpMgr::loginFailed, this, [this](const QString& msg){
        ui->pushButton->setEnabled(true);
        QMessageBox::warning(this, "登录失败", msg);
    });

    connect(ui->pushButton_2, &QPushButton::clicked, this, &LoginDialog::goToRegister);
}

LoginDialog::~LoginDialog()
{
    delete ui;
}
