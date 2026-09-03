#include "passworddialog.h"
#include "changepassworddialog.h"
#include "passwordmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QMessageBox>
#include <QMouseEvent>

PasswordDialog::PasswordDialog(QWidget *parent)
    : QDialog(parent), m_authenticated(false)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setFixedSize(300, 150);
    setStyleSheet("QDialog { background-color: #2D2D30; color: white; border: 1px solid #555; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Custom Orange Title Bar
    QFrame *titleBar = new QFrame(this);
    titleBar->setFixedHeight(30);
    titleBar->setStyleSheet("background-color: #FF8000;");

    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(10, 0, 0, 0);

    QLabel *lblTitle = new QLabel("Enter Password", this);
    lblTitle->setStyleSheet("color: black; font-weight: bold; font-size: 10pt; background: transparent; border: none;");

    QPushButton *btnClose = new QPushButton("X", this);
    btnClose->setFixedSize(30, 30);
    btnClose->setStyleSheet("QPushButton { background-color: #FF8000; color: black; font-weight: bold; border: none; } "
                            "QPushButton:hover { background-color: red; color: white; }");

    titleLayout->addWidget(lblTitle);
    titleLayout->addStretch();
    titleLayout->addWidget(btnClose);

    // Body Form Layout
    QFrame *body = new QFrame(this);
    body->setStyleSheet("background-color: #2D2D30; border: none;");

    txtPassword = new QLineEdit(body);
    txtPassword->setGeometry(30, 25, 240, 28);
    txtPassword->setEchoMode(QLineEdit::Password);
    txtPassword->setStyleSheet("background-color: #1E1E1E; color: white; border: 1px solid #555; padding: 4px;");

    btnChangePassword = new QPushButton("Change Password", body);
    btnChangePassword->setGeometry(30, 75, 120, 30);
    btnChangePassword->setStyleSheet("QPushButton { background-color: #3C3C3C; color: white; font-weight: bold; border: none; border-radius: 3px; } "
                                     "QPushButton:pressed { background-color: #555; }");

    btnSubmit = new QPushButton("Login", body);
    btnSubmit->setGeometry(190, 75, 80, 30);
    btnSubmit->setStyleSheet("QPushButton { background-color: #FF8000; color: black; font-weight: bold; border: none; border-radius: 3px; } "
                             "QPushButton:pressed { background-color: #cc6600; }");

    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(body);

    connect(btnClose, &QPushButton::clicked, this, &PasswordDialog::reject);
    connect(btnSubmit, &QPushButton::clicked, this, &PasswordDialog::onSubmitClicked);
    connect(btnChangePassword, &QPushButton::clicked, this, &PasswordDialog::onChangePasswordClicked);

    // Pressing Enter triggers Login
    connect(txtPassword, &QLineEdit::returnPressed, this, &PasswordDialog::onSubmitClicked);
}

void PasswordDialog::onSubmitClicked()
{
    if (PasswordManager::verifyPassword(txtPassword->text())) {
        m_authenticated = true;
        accept();
    } else {
        QMessageBox::critical(this, "Error", "Incorrect Password!");
        txtPassword->clear();
    }
}

void PasswordDialog::onChangePasswordClicked()
{
    ChangePasswordDialog changeDlg(this);
    changeDlg.exec();
}

void PasswordDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}
void PasswordDialog::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    }
}
