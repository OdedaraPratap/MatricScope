#ifndef TRAYSETTINGSDIALOG_H
#define TRAYSETTINGSDIALOG_H

#include <QDialog>
#include <QTableView>
#include <QComboBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QPoint>

class TraySettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TraySettingsDialog(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void onFileChanged(const QString &fileName);
    void onSaveClicked();
    void onAddProfileClicked();
    void onDeleteRowClicked();
    void onDeleteProfileClicked();
    void onCloseClicked();

private:
    QTableView *tableView;
    QComboBox *cmbFile;
    QStandardItemModel *tableModel;

    QPushButton *btnTextUpdate; // Save
    QPushButton *btnTextAdd;    // Add Profile
    QPushButton *btnTextDelete; // Clear Row
    QPushButton *btnDeleteFile; // Delete Profile
    QPushButton *btnClose;      // Close / Exit

    QPoint m_dragPosition;

    void initializeDatabaseTable();
    void populateFileComboBox();
    void loadRulesForFile(const QString &fileName);
    void saveCurrentGridView();
};

#endif // TRAYSETTINGSDIALOG_H
