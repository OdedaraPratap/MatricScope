#ifndef HISTORYDIALOG_H
#define HISTORYDIALOG_H

#include <QDialog>
#include <QTableView>
#include <QComboBox>
#include <QCheckBox>
#include <QDateTimeEdit>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QSqlTableModel>
#include <QPoint>

// Tiny helper class to make QLabel clickable
class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ClickableLabel(QWidget *parent = nullptr) : QLabel(parent) {}

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override {
        emit clicked();
        QLabel::mousePressEvent(event);
    }
};

class HistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HistoryDialog(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void applyFilters();
    void onSelectionChanged();
    void onPreviewClicked();
    void onDeleteSelectedClicked();
    void onDeleteAllClicked();
    void onCloseClicked();

private:
    QTableView *tableView;
    QSqlTableModel *tableModel;

    QComboBox *cmbFilterShape;
    QDateTimeEdit *dtpFilterDate;
    QCheckBox *chkUseDateFilter;
    QLineEdit *txtFilterLength;
    QLineEdit *txtFilterWidth;

    ClickableLabel *picPreview; // <--- Changed from QLabel to ClickableLabel
    QString m_currentImagePath;

    QPushButton *btnDelete;
    QPushButton *button1;
    QPushButton *button3;
    QPushButton *btnMin;

    QPoint m_dragPosition;

    void loadData();
    void populateShapeFilter();
};

#endif // HISTORYDIALOG_H
