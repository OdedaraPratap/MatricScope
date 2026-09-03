#ifndef CHANGEPASSWORDDIALOG_H
#define CHANGEPASSWORDDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QPoint>

class ChangePasswordDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChangePasswordDialog(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void onSaveClicked();

private:
    QLineEdit *txtOldPassword;
    QLineEdit *txtNewPassword;
    QPoint m_dragPosition;
};

#endif // CHANGEPASSWORDDIALOG_H
