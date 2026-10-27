#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QIcon>
#include <QFile>        // QFile
#include <QString>      // QString
#include <QDebug>       // qDebug()
#include <QStyleFactory>
#include <QPalette>

#include "logindialog.h"
#include "registerdialog.h"

QT_BEGIN_NAMESPACE
/******************************************************************************
 *
 * @file       mainwindow.h
 * @brief      主界面
 *
 * @author     刘国康
 * @date       2026/10/14
 * @history
 *****************************************************************************/
namespace Ui { class MainWindow; }
class QStackedWidget;
QT_END_NAMESPACE

class LoginDialog;
class RegisterDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    Ui::MainWindow *ui;
    QStackedWidget *_stack = nullptr;
    LoginDialog *_login_dlg = nullptr;
    RegisterDialog *_reg_dlg = nullptr;
};

#endif // MAINWINDOW_H
