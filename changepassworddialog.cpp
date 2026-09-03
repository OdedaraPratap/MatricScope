#include "changepassworddialog.h"
#include "passwordmanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QMessageBox>
#include <QMouseEvent>

ChangePasswordDialog::ChangePasswordDialog(QWidget *parent) : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setFixedSize(300, 180);
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

    QLabel *lblTitle = new QLabel("Change Password", this);
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
    QGridLayout *grid = new QGridLayout(body);
    grid->setContentsMargins(20, 15, 20, 15);
    grid->setVerticalSpacing(10);

    QLabel *lblOld = new QLabel("Old Password:", body);
    lblOld->setStyleSheet("color: white;");
    txtOldPassword = new QLineEdit(body);
    txtOldPassword->setEchoMode(QLineEdit::Password);
    txtOldPassword->setStyleSheet("background-color: #1E1E1E; color: white; border: 1px solid #555; padding: 4px;");

    QLabel *lblNew = new QLabel("New Password:", body);
    lblNew->setStyleSheet("color: white;");
    txtNewPassword = new QLineEdit(body);
    txtNewPassword->setEchoMode(QLineEdit::Password);
    txtNewPassword->setStyleSheet("background-color: #1E1E1E; color: white; border: 1px solid #555; padding: 4px;");

    QPushButton *btnSave = new QPushButton("Save", body);
    btnSave->setFixedSize(80, 30);
    btnSave->setStyleSheet("QPushButton { background-color: #FF8000; color: black; font-weight: bold; border: none; border-radius: 3px; } "
                           "QPushButton:pressed { background-color: #cc6600; }");

    grid->addWidget(lblOld, 0, 0);
    grid->addWidget(txtOldPassword, 0, 1);
    grid->addWidget(lblNew, 1, 0);
    grid->addWidget(txtNewPassword, 1, 1);
    grid->addWidget(btnSave, 2, 1, Qt::AlignRight);

    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(body);

    connect(btnClose, &QPushButton::clicked, this, &ChangePasswordDialog::reject);
    connect(btnSave, &QPushButton::clicked, this, &ChangePasswordDialog::onSaveClicked);
}

void ChangePasswordDialog::onSaveClicked()
{
    if (PasswordManager::verifyPassword(txtOldPassword->text())) {
        if (!txtNewPassword->text().trimmed().isEmpty()) {
            PasswordManager::setPassword(txtNewPassword->text());
            QMessageBox::information(this, "Success", "Password changed successfully!");
            accept();
        } else {
            QMessageBox::warning(this, "Warning", "New password cannot be empty.");
        }
    } else {
        QMessageBox::critical(this, "Error", "Old password is incorrect.");
    }
}

void ChangePasswordDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}
void ChangePasswordDialog::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    }
}
