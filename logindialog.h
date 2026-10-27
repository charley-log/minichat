#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

signals:
    void goToRegister();            // 点"注册"跳转
    void loginOk(const QString &user, const QString &token);   // 登录成功

private:
    Ui::LoginDialog *ui;
};

#endif // LOGINDIALOG_H
