#ifndef REGISTERDIALOG_H
#define REGISTERDIALOG_H

#include <QDialog>

namespace Ui {
class RegisterDialog;
}

class RegisterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisterDialog(QWidget *parent = nullptr);
    ~RegisterDialog();

signals:
    void backToLogin();                      // 点"已有账号"时发
    void registerOk(const QString &user);    // 注册成功时发

private slots:
    void showError(const QString &msg);      // 显示错误提示
    void clearError();                       // 清除错误提示

private:
    Ui::RegisterDialog *ui;
};

#endif // REGISTERDIALOG_H
