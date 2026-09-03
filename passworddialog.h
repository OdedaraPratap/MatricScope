#ifndef PASSWORDDIALOG_H
#define PASSWORDDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QPoint>

class PasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PasswordDialog(QWidget *parent = nullptr);
    bool isAuthenticated() const { return m_authenticated; }

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void onSubmitClicked();
    void onChangePasswordClicked();

private:
    QLineEdit *txtPassword;
    QPushButton *btnSubmit;
    QPushButton *btnChangePassword;
    bool m_authenticated;
    QPoint m_dragPosition;
};

#endif // PASSWORDDIALOG_H
