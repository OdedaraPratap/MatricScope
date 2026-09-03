#include "historydialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QTableView>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QMouseEvent>
#include <QHeaderView>
#include <algorithm>

HistoryDialog::HistoryDialog(QWidget *parent) : QDialog(parent)
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

    QLabel *lblTitle = new QLabel("History", this);
    lblTitle->setStyleSheet("color: white; font-weight: bold; font-size: 12pt; background: transparent; border: none;");

    btnMin = new QPushButton("_", this);
    button3 = new QPushButton("X", this);
    QString tbBtnStyle = "QPushButton { background-color: transparent; font-weight: bold; font-size: 11pt; border: none; color: black; } "
                         "QPushButton:hover { color: white; }";
    btnMin->setStyleSheet(tbBtnStyle);
    button3->setStyleSheet(tbBtnStyle);
    btnMin->setFixedSize(23, 23);
    button3->setFixedSize(23, 23);

    titleLayout->addWidget(lblTitle);
    titleLayout->addStretch();
    titleLayout->addWidget(btnMin);
    titleLayout->addWidget(button3);

    // ==========================================
    // 2. BODY LAYOUT (Optimized for 800x480 via Layouts)
    // ==========================================
    QFrame *body = new QFrame(this);
    body->setStyleSheet("background-color: black;");

    QVBoxLayout *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(10, 10, 10, 10);
    bodyLayout->setSpacing(8);

    // Filter Controls Row
    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(8);

    // Shape Filter
    QVBoxLayout *vShape = new QVBoxLayout();
    QLabel *lblShape = new QLabel("Shape", body);
    lblShape->setStyleSheet("color: white; font-size: 8pt;");
    cmbFilterShape = new QComboBox(body);
    cmbFilterShape->setFixedWidth(110);
    cmbFilterShape->setStyleSheet("QComboBox { background-color: black; color: white; border: 1px solid #FF8000; font-weight: bold; font-size: 9pt; height: 25px; }"
                                  "QComboBox QAbstractItemView { background: black; color: white; selection-background-color: #FF8000; }");
    vShape->addWidget(lblShape);
    vShape->addWidget(cmbFilterShape);

    // Date Filter
    QVBoxLayout *vDate = new QVBoxLayout();
    QLabel *lblDate = new QLabel("Date", body);
    lblDate->setStyleSheet("color: white; font-size: 8pt;");
    dtpFilterDate = new QDateTimeEdit(QDate::currentDate(), body);
    dtpFilterDate->setFixedWidth(120);
    dtpFilterDate->setDisplayFormat("yyyy-MM-dd");
    dtpFilterDate->setStyleSheet("background: black; color: white; border: 1px solid #555; font-size: 9pt; height: 25px;");
    vDate->addWidget(lblDate);
    vDate->addWidget(dtpFilterDate);

    // Length Filter
    QVBoxLayout *vLen = new QVBoxLayout();
    QLabel *lblLen = new QLabel("Length", body);
    lblLen->setStyleSheet("color: white; font-size: 8pt;");
    txtFilterLength = new QLineEdit(body);
    txtFilterLength->setFixedWidth(70);
    txtFilterLength->setStyleSheet("background: black; color: white; border: 1px solid #FF8000; font-size: 9pt; height: 25px;");
    vLen->addWidget(lblLen);
    vLen->addWidget(txtFilterLength);

    // Width Filter
    QVBoxLayout *vWid = new QVBoxLayout();
    QLabel *lblWid = new QLabel("Width", body);
    lblWid->setStyleSheet("color: white; font-size: 8pt;");
    txtFilterWidth = new QLineEdit(body);
    txtFilterWidth->setFixedWidth(70);
    txtFilterWidth->setStyleSheet("background: black; color: white; border: 1px solid #FF8000; font-size: 9pt; height: 25px;");
    vWid->addWidget(lblWid);
    vWid->addWidget(txtFilterWidth);

    // Date Toggle Checkbox
    chkUseDateFilter = new QCheckBox("Use Date", body);
    chkUseDateFilter->setStyleSheet("color: white; font-size: 9pt;");
    chkUseDateFilter->setChecked(false);

    filterLayout->addLayout(vShape);
    filterLayout->addLayout(vDate);
    filterLayout->addLayout(vLen);
    filterLayout->addLayout(vWid);
    filterLayout->addWidget(chkUseDateFilter);
    filterLayout->addStretch();

    bodyLayout->addLayout(filterLayout);

    // Center Section: Table View (Left) + Image Preview (Right)
    QHBoxLayout *centerLayout = new QHBoxLayout();
    centerLayout->setSpacing(10);

    tableView = new QTableView(body);
    tableView->setStyleSheet("QTableView { background-color: black; color: white; gridline-color: #444; border: 1px solid #333; font-size: 9pt; }"
                             "QHeaderView::section { background-color: #222; color: white; font-weight: bold; border: 1px solid #444; }"
                             "QTableView::item:selected { background-color: #FF8000; color: white; }");
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    picPreview = new ClickableLabel(body);
    picPreview->setFixedSize(190, 270);
    picPreview->setStyleSheet("background-color: #111; border: 2px dashed #444;");
    picPreview->setAlignment(Qt::AlignCenter);
    picPreview->setText("No Image Selected");

    centerLayout->addWidget(tableView, 1); // Table expands dynamically
    centerLayout->addWidget(picPreview);

    bodyLayout->addLayout(centerLayout, 1);

    // Bottom Action Buttons Row
    QHBoxLayout *actionLayout = new QHBoxLayout();
    QString actionBtnStyle = "QPushButton { background-color: #FF8000; color: white; font-weight: bold; border-radius: 4px; border: none; font-size: 9pt; min-height: 35px; }"
                             "QPushButton:pressed { background-color: #cc6600; }";

    btnDelete = new QPushButton("Delete Selected", body);
    btnDelete->setIcon(QIcon(":/images/DELETE.png"));
    btnDelete->setStyleSheet(actionBtnStyle);
    btnDelete->setFixedWidth(130);

    button1 = new QPushButton("Delete All", body);
    button1->setIcon(QIcon(":/images/DELETE_ALL.png"));
    button1->setStyleSheet(actionBtnStyle);
    button1->setFixedWidth(130);

    actionLayout->addWidget(btnDelete);
    actionLayout->addWidget(button1);
    actionLayout->addStretch();

    bodyLayout->addLayout(actionLayout);

    // Assemble Main Layout
    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(body, 1); // Crucial: Forces body to fill entire remaining vertical space

    // ==========================================
    // 3. INITIALIZATION & CONNECTIONS
    // ==========================================
    loadData();
    populateShapeFilter();

    connect(cmbFilterShape, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &HistoryDialog::applyFilters);
    connect(dtpFilterDate, &QDateTimeEdit::dateTimeChanged, this, &HistoryDialog::applyFilters);
    connect(chkUseDateFilter, &QCheckBox::toggled, this, &HistoryDialog::applyFilters);
    connect(txtFilterLength, &QLineEdit::textChanged, this, &HistoryDialog::applyFilters);
    connect(txtFilterWidth, &QLineEdit::textChanged, this, &HistoryDialog::applyFilters);

    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &HistoryDialog::onSelectionChanged);
    connect(picPreview, &ClickableLabel::clicked, this, &HistoryDialog::onPreviewClicked);

    connect(btnDelete, &QPushButton::clicked, this, &HistoryDialog::onDeleteSelectedClicked);
    connect(button1, &QPushButton::clicked, this, &HistoryDialog::onDeleteAllClicked);
    connect(button3, &QPushButton::clicked, this, &HistoryDialog::onCloseClicked);
    connect(btnMin, &QPushButton::clicked, this, &HistoryDialog::showMinimized);
}

void HistoryDialog::loadData()
{
    tableModel = new QSqlTableModel(this);
    tableModel->setTable("Records");
    tableModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    tableModel->select();

    tableView->setModel(tableModel);

    int imgCol = tableModel->fieldIndex("Image");
    if (imgCol != -1) {
        tableView->setColumnHidden(imgCol, true);
    }
}

void HistoryDialog::populateShapeFilter()
{
    cmbFilterShape->clear();
    cmbFilterShape->addItem("-- All Shapes --");

    QSqlQuery query("SELECT DISTINCT Shape FROM Records WHERE Shape IS NOT NULL AND Shape != '' ORDER BY Shape ASC;");
    while (query.next()) {
        cmbFilterShape->addItem(query.value(0).toString());
    }
    cmbFilterShape->setCurrentIndex(0);
}

void HistoryDialog::applyFilters()
{
    QString filterStr = "1=1";

    if (cmbFilterShape->currentIndex() > 0) {
        QString shape = cmbFilterShape->currentText().replace("'", "''");
        filterStr += QString(" AND Shape = '%1'").arg(shape);
    }

    if (chkUseDateFilter->isChecked()) {
        QString dateStr = dtpFilterDate->date().toString("yyyy-MM-dd");
        filterStr += QString(" AND Date LIKE '%%1%'").arg(dateStr);
    }

    bool okLen;
    double minLen = txtFilterLength->text().toDouble(&okLen);
    if (okLen && !txtFilterLength->text().trimmed().isEmpty()) {
        filterStr += QString(" AND Length >= %1").arg(minLen);
    }

    bool okWid;
    double minWid = txtFilterWidth->text().toDouble(&okWid);
    if (okWid && !txtFilterWidth->text().trimmed().isEmpty()) {
        filterStr += QString(" AND Width >= %1").arg(minWid);
    }

    tableModel->setFilter(filterStr);
    tableModel->select();
}

void HistoryDialog::onSelectionChanged()
{
    QModelIndexList selected = tableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;

    int row = selected.first().row();
    int imgCol = tableModel->fieldIndex("Image");
    if (imgCol == -1) return;

    m_currentImagePath = tableModel->data(tableModel->index(row, imgCol)).toString();

    if (!m_currentImagePath.isEmpty() && QFile::exists(m_currentImagePath)) {
        QPixmap pix(m_currentImagePath);
        picPreview->setPixmap(pix.scaled(picPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        picPreview->clear();
        picPreview->setText("Image Not Found");
    }
}

void HistoryDialog::onPreviewClicked()
{
    if (!m_currentImagePath.isEmpty() && QFile::exists(m_currentImagePath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentImagePath));
    } else {
        QMessageBox::warning(this, "Asset Error", "No image file is currently loaded or the source file was moved.");
    }
}

void HistoryDialog::onDeleteSelectedClicked()
{
    QModelIndexList selected = tableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "Selection Required", "Please select at least one record row to delete.");
        return;
    }

    if (QMessageBox::question(this, "Confirm Delete", QString("Are you sure you want to permanently delete %1 selected record(s)?").arg(selected.size())) == QMessageBox::Yes) {
        QSqlDatabase::database().transaction();

        QList<int> rowsToDelete;
        for (const auto &idx : selected) rowsToDelete.append(idx.row());
        std::sort(rowsToDelete.begin(), rowsToDelete.end(), std::greater<int>());

        for (int row : rowsToDelete) {
            tableModel->removeRow(row);
        }

        tableModel->submitAll();
        QSqlDatabase::database().commit();

        picPreview->clear();
        picPreview->setText("No Image Selected");
        populateShapeFilter();
        QMessageBox::information(this, "Deleted", "Selected records cleared cleanly from database.");
    }
}

void HistoryDialog::onDeleteAllClicked()
{
    if (QMessageBox::warning(this, "Delete All Shapes", "This will permanently delete ALL shape records and history data.\n\nAre you sure?", QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    QSqlQuery query("DELETE FROM Records;");
    tableModel->select();
    picPreview->clear();
    picPreview->setText("No Image Selected");
    populateShapeFilter();

    QMessageBox::information(this, "Completed", "All history records have been deleted.");
}

void HistoryDialog::onCloseClicked()
{
    accept();
}

void HistoryDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void HistoryDialog::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    }
}
