#include "calibrationdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QDoubleValidator>
#include <QSettings>
#include <QMessageBox>
#include <QLocale>

CalibrationDialog::CalibrationDialog(QWidget *parent) : QDialog(parent)
{
    // Remove standard Windows borders and set size
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setFixedSize(304, 166);
    setStyleSheet("QDialog { background-color: #000000; border: 1px solid #555; }");

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

    QLabel *lblTitle = new QLabel("Calibration", this);
    lblTitle->setStyleSheet("color: black; font-weight: bold; font-size: 14px;");

    QPushButton *btnMin = new QPushButton("_", this);
    QPushButton *btnClose = new QPushButton("X", this);

    QString tbBtnStyle = "QPushButton { background-color: transparent; font-weight: bold; font-size: 14px; border: none; } "
                         "QPushButton:hover { color: white; }";
    btnMin->setStyleSheet(tbBtnStyle);
    btnClose->setStyleSheet(tbBtnStyle);
    btnMin->setFixedSize(25, 25);
    btnClose->setFixedSize(25, 25);

    titleLayout->addWidget(lblTitle);
    titleLayout->addStretch();
    titleLayout->addWidget(btnMin);
    titleLayout->addWidget(btnClose);

    // ==========================================
    // 2. BODY CONTROLS
    // ==========================================
    QFrame *body = new QFrame(this);
    QVBoxLayout *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setAlignment(Qt::AlignCenter);
    bodyLayout->setSpacing(10);

    QLabel *lblPrompt = new QLabel("Enter Diameter in mm", this);
    lblPrompt->setStyleSheet("color: white; font-weight: bold; font-size: 12px;");
    lblPrompt->setAlignment(Qt::AlignCenter);

    txtCalib = new QLineEdit(this);
    txtCalib->setFixedSize(100, 26);
    txtCalib->setStyleSheet("background-color: black; color: white; border: 1px solid #FF8000; font-size: 12px;");
    txtCalib->setAlignment(Qt::AlignCenter);

    // Qt Validator: Replaces your C# KeyPress event. Only allows numbers and one decimal point!
    QDoubleValidator *validator = new QDoubleValidator(1.0, 100.0, 3, this);
    validator->setNotation(QDoubleValidator::StandardNotation);
    validator->setLocale(QLocale::c());
    txtCalib->setValidator(validator);

    // Load last used calibration value
    QSettings settings("MetricScope", "Settings");
    txtCalib->setText(settings.value("calibval", "10.0").toString());

    QPushButton *btnSave = new QPushButton(this);
    btnSave->setFixedSize(119, 37);
    btnSave->setStyleSheet("QPushButton { border-image: url(:/images/SAVE.jpg); background-color: transparent; } "
                           "QPushButton:pressed { background-color: #555; }");

    bodyLayout->addWidget(lblPrompt, 0, Qt::AlignCenter);
    bodyLayout->addWidget(txtCalib, 0, Qt::AlignCenter);
    bodyLayout->addWidget(btnSave, 0, Qt::AlignCenter);

    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(body);

    // ==========================================
    // 3. SIGNAL CONNECTIONS
    // ==========================================
    connect(btnClose, &QPushButton::clicked, this, &CalibrationDialog::reject);
    connect(btnMin, &QPushButton::clicked, this, &CalibrationDialog::showMinimized);
    connect(btnSave, &QPushButton::clicked, this, &CalibrationDialog::onSaveClicked);
}

void CalibrationDialog::onSaveClicked()
{
    bool ok = false;
    const double diameterMm = txtCalib->text().toDouble(&ok);
    if (!ok || diameterMm < 1.0 || diameterMm > 100.0) {
        QMessageBox::warning(this, "Invalid diameter",
                             "Enter the calibration circle diameter from 1 to 100 mm.");
        txtCalib->setFocus();
        txtCalib->selectAll();
        return;
    }

    QSettings settings("MetricScope", "Settings");
    settings.setValue("calibval", diameterMm);
    settings.sync();

    // Tell the main window/camera thread to execute the visual calibration
    emit calibrationRequested();

    this->accept(); // Closes the dialog successfully
}

// Replaces SendMessage/ReleaseCapture for draggable frameless window
void CalibrationDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        // Use globalPos() for Qt 5.x compatibility
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void CalibrationDialog::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        // Use globalPos() for Qt 5.x compatibility
        move(event->globalPos() - m_dragPosition);
        event->accept();
    }
}

