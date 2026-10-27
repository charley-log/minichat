#include "registerdialog.h"
#include "ui_registerdialog.h"
#include "httpmgr.h"

#include <QRegularExpression>

RegisterDialog::RegisterDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RegisterDialog)
{
    ui->setupUi(this);

    ui->etip_label->setVisible(false);

    // 取消按钮 → 发信号回登录页
    connect(ui->cancel_btn, &QPushButton::clicked, this, &RegisterDialog::backToLogin);

    // 确认按钮 → 注册逻辑
    connect(ui->confirm_btn, &QPushButton::clicked, this, [this]{
        QString user = ui->user_edit->text().trimmed();
        QString mail = ui->email_edit->text().trimmed();  // 假设邮箱输入框叫 mail_edit
        QString pwd  = ui->pass_edit->text();             // 假设密码输入框叫 pwd_edit
        QString pwd2 = ui->verify_edit->text();            // 假设确认密码输入框叫 pwd2_edit

        // 用户名不能为空
        if (user.isEmpty()) {
            showError("请输入用户名");
            ui->user_edit->setProperty("error", true);
            return;
        }

        // 邮箱格式校验
        QRegularExpression re("[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}");
        if (!re.match(mail).hasMatch()) {
            showError("邮箱格式不正确");
            ui->email_edit->setProperty("error", true);
            return;
        }

        // 密码长度不能少于6位
        if (pwd.size() < 6) {
            showError("密码长度不能少于6位");
            ui->pass_edit->setProperty("error", true);
            return;
        }

        // 两次密码不一致
        if (pwd != pwd2) {
            showError("两次输入的密码不一致");
            ui->pass_edit->setProperty("error", true);
            ui->verify_edit->setProperty("error", true);
            return;
        }

        // 全部校验通过 → 清除错误状态，发送信号
        clearError();
        ui->confirm_btn->setEnabled(false);

        // 发送真实注册请求
        HttpMgr::instance().postRegister(user, pwd);
    });

    // 在构造函数末尾接住网络层信号
    connect(&HttpMgr::instance(), &HttpMgr::registerSuccess, this, [this]{
        ui->confirm_btn->setEnabled(true);
        QMessageBox::information(this, "成功", "注册成功，请登录！");
        emit backToLogin(); // 切回登录页
    });

    connect(&HttpMgr::instance(), &HttpMgr::registerFailed, this, [this](const QString& msg){
        ui->confirm_btn->setEnabled(true);
        showError(msg);
    });

    // 获取验证码按钮
    connect(ui->get_code, &QPushButton::clicked, this, [this]{
        // 这里写获取验证码逻辑
    });

    // 输入框内容变化时自动清除错误提示，避免用户改了还一直红着
    connect(ui->user_edit, &QLineEdit::textChanged, this, [this]{
        clearError();
        ui->user_edit->setProperty("error", false);
    });
    connect(ui->email_edit, &QLineEdit::textChanged, this, [this]{
        clearError();
        ui->email_edit->setProperty("error", false);
    });
    connect(ui->pass_edit, &QLineEdit::textChanged, this, [this]{
        clearError();
        ui->pass_edit->setProperty("error", false);
        ui->verify_edit->setProperty("error", false);  // 密码改了，确认密码的红框也清掉
    });
    connect(ui->verify_edit, &QLineEdit::textChanged, this, [this]{
        clearError();
        ui->verify_edit->setProperty("error", false);
    });
}

RegisterDialog::~RegisterDialog()
{
    delete ui;
}

// ========== 错误提示函数实现 ==========

void RegisterDialog::showError(const QString &msg)
{
    ui->etip_label->setText(msg);
    ui->etip_label->setVisible(true);
}

void RegisterDialog::clearError()
{
    ui->etip_label->clear();
    ui->etip_label->setVisible(false);
}
