#include "traysettingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDatabase>
#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>
#include <QMouseEvent>
#include <QHeaderView>
#include <QDebug>
#include <cstdlib>

TraySettingsDialog::TraySettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    showFullScreen();
    setStyleSheet("QDialog { background-color: #000000; border: 1px solid #555; color: white; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ==========================================
    // 1. CUSTOM ORANGE TITLE BAR
    // ==========================================
    QFrame *titleBar = new QFrame(this);
    titleBar->setFixedHeight(32);
    titleBar->setStyleSheet("background-color: #FF8000;");

    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(10, 0, 5, 0);

    QLabel *lblTitle = new QLabel("Tray Settings", this);
    lblTitle->setStyleSheet("color: white; font-weight: bold; font-size: 12pt; background: transparent; border: none;");

    QPushButton *btnMin = new QPushButton("_", this);
    btnClose = new QPushButton("X", this);
    QString tbBtnStyle = "QPushButton { background-color: transparent; font-weight: bold; font-size: 11pt; border: none; color: black; } "
                         "QPushButton:hover { color: white; }";
    btnMin->setStyleSheet(tbBtnStyle);
    btnClose->setStyleSheet(tbBtnStyle);
    btnMin->setFixedSize(23, 23);
    btnClose->setFixedSize(23, 23);

    titleLayout->addWidget(lblTitle);
    titleLayout->addStretch();
    titleLayout->addWidget(btnMin);
    titleLayout->addWidget(btnClose);

    // ==========================================
    // 2. BODY CONTROLS & TABLE VIEW USING LAYOUTS
    // ==========================================
    QFrame *body = new QFrame(this);
    body->setStyleSheet("background-color: black;");

    QVBoxLayout *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(15, 12, 15, 15);
    bodyLayout->setSpacing(10);

    cmbFile = new QComboBox(body);
    cmbFile->setFixedWidth(180);
    cmbFile->setStyleSheet("QComboBox { background-color: black; color: white; border: 1px solid #FF8000; font-weight: bold; font-size: 9pt; min-height: 25px; }"
                           "QComboBox QAbstractItemView { background: black; color: white; selection-background-color: #FF8000; }");
    bodyLayout->addWidget(cmbFile);

    tableView = new QTableView(body);
    tableView->setStyleSheet("QTableView { background-color: black; color: white; gridline-color: #444; border: 1px solid #333; font-size: 9pt; }"
                             "QHeaderView::section { background-color: #222; color: white; font-weight: bold; border: 1px solid #444; }"
                             "QTableView::item:selected { background-color: #FF8000; color: white; }");
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    bodyLayout->addWidget(tableView, 1); // Expand table to fill available space

    // Footer Buttons Row
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QString btnStyle = "QPushButton { background-color: #FF8000; color: white; font-weight: bold; border-radius: 4px; font-size: 9pt; border: none; min-height: 38px; } "
                       "QPushButton:pressed { background-color: #cc6600; }";

    btnTextUpdate = new QPushButton("Save", body);
    btnTextUpdate->setStyleSheet(btnStyle);

    btnTextAdd = new QPushButton("Add Profile", body);
    btnTextAdd->setStyleSheet(btnStyle);

    btnTextDelete = new QPushButton("Clear Row", body);
    btnTextDelete->setStyleSheet(btnStyle);

    btnDeleteFile = new QPushButton("Delete File", body);
    btnDeleteFile->setStyleSheet(btnStyle);

    btnLayout->addWidget(btnTextUpdate);
    btnLayout->addWidget(btnTextAdd);
    btnLayout->addWidget(btnTextDelete);
    btnLayout->addWidget(btnDeleteFile);

    bodyLayout->addLayout(btnLayout);

    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(body, 1); // Forces body to fill screen

    // ==========================================
    // 3. MODEL INITIALIZATION & CONNECTIONS
    // ==========================================
    initializeDatabaseTable();
    populateFileComboBox();

    connect(cmbFile, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged), this, &TraySettingsDialog::onFileChanged);
    connect(btnTextUpdate, &QPushButton::clicked, this, &TraySettingsDialog::onSaveClicked);
    connect(btnTextAdd, &QPushButton::clicked, this, &TraySettingsDialog::onAddProfileClicked);
    connect(btnTextDelete, &QPushButton::clicked, this, &TraySettingsDialog::onDeleteRowClicked);
    connect(btnDeleteFile, &QPushButton::clicked, this, &TraySettingsDialog::onDeleteProfileClicked);
    connect(btnClose, &QPushButton::clicked, this, &TraySettingsDialog::onCloseClicked);
    connect(btnMin, &QPushButton::clicked, this, &TraySettingsDialog::showMinimized);
}

void TraySettingsDialog::initializeDatabaseTable()
{
    QSqlDatabase db = QSqlDatabase::contains("qt_sql_default_connection")
                      ? QSqlDatabase::database("qt_sql_default_connection")
                      : QSqlDatabase::addDatabase("QSQLITE");

    if (!db.isOpen()) {
        db.setDatabaseName("History.db");
        if (!db.open()) {
            qDebug() << "Error: Failed to open database History.db";
            return;
        }
    }

    QSqlQuery query(db);
    QString sql = R"(
        CREATE TABLE IF NOT EXISTS TrayRules (
            Id INTEGER PRIMARY KEY AUTOINCREMENT,
            FileName TEXT NOT NULL,
            Number INTEGER NOT NULL,
            FromLength REAL,
            ToLength REAL,
            FromWidth REAL,
            ToWidth REAL
        );
    )";
    if (!query.exec(sql)) {
        qDebug() << "TrayRules table creation failed:" << query.lastError().text();
    }
}

void TraySettingsDialog::populateFileComboBox()
{
    cmbFile->clear();
    QSqlQuery query("SELECT DISTINCT FileName FROM TrayRules;");
    bool hasDefault = false;

    while (query.next()) {
        QString fName = query.value(0).toString();
        cmbFile->addItem(fName);
        if (fName == "Default_Profile") hasDefault = true;
    }

    if (!hasDefault) {
        cmbFile->insertItem(0, "Default_Profile");
    }

    QSettings settings("MetricScope", "Settings");
    QString lastFile = settings.value("cmbFile", "Default_Profile").toString();
    int idx = cmbFile->findText(lastFile);
    if (idx != -1) cmbFile->setCurrentIndex(idx);
    else cmbFile->setCurrentIndex(0);

    loadRulesForFile(cmbFile->currentText());
}

void TraySettingsDialog::loadRulesForFile(const QString &fileName)
{
    QSqlQuery checkQuery;
    checkQuery.prepare("SELECT COUNT(*) FROM TrayRules WHERE FileName = :fileName;");
    checkQuery.bindValue(":fileName", fileName);
    checkQuery.exec();

    if (checkQuery.next() && checkQuery.value(0).toInt() == 0) {
        QSqlQuery insertQuery;
        for (int i = 1; i <= 20; ++i) {
            insertQuery.prepare("INSERT INTO TrayRules (FileName, Number, FromLength, ToLength, FromWidth, ToWidth) VALUES (:file, :num, 0, 0, 0, 0);");
            insertQuery.bindValue(":file", fileName);
            insertQuery.bindValue(":num", i);
            insertQuery.exec();
        }
    }

    tableModel = new QStandardItemModel(20, 5, this);
    tableModel->setHeaderData(0, Qt::Horizontal, "Number");
    tableModel->setHeaderData(1, Qt::Horizontal, "From Length");
    tableModel->setHeaderData(2, Qt::Horizontal, "To Length");
    tableModel->setHeaderData(3, Qt::Horizontal, "From Width");
    tableModel->setHeaderData(4, Qt::Horizontal, "To Width");

    QSqlQuery selectQuery;
    selectQuery.prepare("SELECT Number, FromLength, ToLength, FromWidth, ToWidth FROM TrayRules WHERE FileName = :fileName ORDER BY Number ASC;");
    selectQuery.bindValue(":fileName", fileName);
    selectQuery.exec();

    int row = 0;
    while (selectQuery.next() && row < 20) {
        QStandardItem *itemNum = new QStandardItem(selectQuery.value("Number").toString());
        QStandardItem *itemFL  = new QStandardItem(selectQuery.value("FromLength").toString());
        QStandardItem *itemTL  = new QStandardItem(selectQuery.value("ToLength").toString());
        QStandardItem *itemFW  = new QStandardItem(selectQuery.value("FromWidth").toString());
        QStandardItem *itemTW  = new QStandardItem(selectQuery.value("ToWidth").toString());

        // Lock Number column (index 0) so it cannot be edited
        itemNum->setEditable(false);

        // Explicitly enable editing for length and width cells (indices 1 to 4)
        itemFL->setEditable(true);
        itemTL->setEditable(true);
        itemFW->setEditable(true);
        itemTW->setEditable(true);

        tableModel->setItem(row, 0, itemNum);
        tableModel->setItem(row, 1, itemFL);
        tableModel->setItem(row, 2, itemTL);
        tableModel->setItem(row, 3, itemFW);
        tableModel->setItem(row, 4, itemTW);

        row++;
    }

    tableView->setModel(tableModel);
}

void TraySettingsDialog::onFileChanged(const QString &fileName)
{
    QSettings settings("MetricScope", "Settings");
    settings.setValue("cmbFile", fileName);
    loadRulesForFile(fileName);
}

void TraySettingsDialog::saveCurrentGridView()
{
    QString currentFile = cmbFile->currentText();
    QSqlQuery query;

    for (int i = 0; i < tableModel->rowCount(); ++i) {
        int number = tableModel->item(i, 0)->text().toInt();
        double fromLen = tableModel->item(i, 1)->text().toDouble();
        double toLen = tableModel->item(i, 2)->text().toDouble();
        double fromWid = tableModel->item(i, 3)->text().toDouble();
        double toWid = tableModel->item(i, 4)->text().toDouble();

        query.prepare(R"(
            UPDATE TrayRules
            SET FromLength = :fl, ToLength = :tl, FromWidth = :fw, ToWidth = :tw
            WHERE FileName = :file AND Number = :num;
        )");
        query.bindValue(":fl", fromLen);
        query.bindValue(":tl", toLen);
        query.bindValue(":fw", fromWid);
        query.bindValue(":tw", toWid);
        query.bindValue(":file", currentFile);
        query.bindValue(":num", number);
        query.exec();
    }
}

void TraySettingsDialog::onSaveClicked()
{
    saveCurrentGridView();
    QMessageBox::information(this, "Success", "All tray rules saved successfully!");
}

void TraySettingsDialog::onAddProfileClicked()
{
    bool ok;
    QString newFile = QInputDialog::getText(this, "Add Profile", "Enter new profile name:", QLineEdit::Normal, "", &ok);
    if (!ok || newFile.trimmed().isEmpty()) return;

    newFile = newFile.trimmed();
    if (newFile.compare("Default_Profile", Qt::CaseInsensitive) == 0) {
        QMessageBox::warning(this, "Warning", "Default_Profile already exists.");
        return;
    }

    int existingIdx = cmbFile->findText(newFile, Qt::MatchExactly);
    if (existingIdx == -1) {
        cmbFile->addItem(newFile);
        cmbFile->setCurrentText(newFile);
    } else {
        cmbFile->setCurrentIndex(existingIdx);
    }

    loadRulesForFile(newFile);

    QSettings settings("MetricScope", "Settings");
    settings.setValue("cmbFile", newFile);

    QMessageBox::information(this, "Success", QString("Profile '%1' created and loaded successfully!").arg(newFile));
}

void TraySettingsDialog::onDeleteRowClicked()
{
    QModelIndex idx = tableView->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::warning(this, "Selection Error", "Please select a row in the grid to clear.");
        return;
    }

    int row = idx.row();

    // Keep the existing Number item at column 0 intact, only reset columns 1 to 4 to "0.0"
    QStandardItem *itemNum = tableModel->item(row, 0);
    QString numStr = itemNum ? itemNum->text() : QString::number(row + 1);

    QStandardItem *newItemNum = new QStandardItem(numStr);
    newItemNum->setEditable(false);

    tableModel->setItem(row, 0, newItemNum);
    tableModel->setItem(row, 1, new QStandardItem("0.0"));
    tableModel->setItem(row, 2, new QStandardItem("0.0"));
    tableModel->setItem(row, 3, new QStandardItem("0.0"));
    tableModel->setItem(row, 4, new QStandardItem("0.0"));

    // Ensure edited items are explicitly marked editable
    tableModel->item(row, 1)->setEditable(true);
    tableModel->item(row, 2)->setEditable(true);
    tableModel->item(row, 3)->setEditable(true);
    tableModel->item(row, 4)->setEditable(true);

    saveCurrentGridView();
    QMessageBox::information(this, "Success", "Row values cleared successfully.");
}

void TraySettingsDialog::onDeleteProfileClicked()
{
    QString currentFile = cmbFile->currentText();
    if (currentFile.compare("Default_Profile", Qt::CaseInsensitive) == 0) {
        QMessageBox::warning(this, "Error", "'Default_Profile' cannot be deleted.");
        return;
    }

    if (QMessageBox::question(this, "Confirm Deletion", QString("Are you sure you want to delete profile '%1'?").arg(currentFile)) == QMessageBox::Yes) {
        QSqlQuery query;
        query.prepare("DELETE FROM TrayRules WHERE FileName = :file;");
        query.bindValue(":file", currentFile);
        query.exec();

        populateFileComboBox();
        QMessageBox::information(this, "Success", "Profile deleted successfully.");
    }
}

void TraySettingsDialog::onCloseClicked()
{
    saveCurrentGridView();
    accept();
}

void TraySettingsDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void TraySettingsDialog::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    }
}
