#include "camerasettingdialog.h"
#include "traysettingsdialog.h"
#include "passworddialog.h"
#include "variationdialog.h"
#include "historydialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QSettings>
#include <QMouseEvent>
#include <QMessageBox>

CameraSettingsDialog::CameraSettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setFixedSize(544, 220);
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

    QLabel *lblTitle = new QLabel("Camera Setting", this);
    lblTitle->setStyleSheet("color: white; font-weight: bold; font-size: 12px;");

    QPushButton *btnClose = new QPushButton("X", this);
    btnClose->setStyleSheet("QPushButton { background-color: transparent; font-weight: bold; font-size: 14px; border: none; color: black; } "
                            "QPushButton:hover { color: white; }");
    btnClose->setFixedSize(23, 23);

    titleLayout->addWidget(lblTitle);
    titleLayout->addStretch();
    titleLayout->addWidget(btnClose);

    // ==========================================
    // 2. SETTINGS BODY & CONTROLS
    // ==========================================
    QFrame *body = new QFrame(this);
    body->setStyleSheet("background-color: black;");

    QSettings settings("MetricScope", "Settings");

    // --- Master Gain (Hikrobot typically uses 0.0 to 20.0) ---
    QLabel *lblGain = new QLabel("Master Gain", body);
    lblGain->setGeometry(25, 15, 100, 20);
    lblGain->setStyleSheet("color: white; font-weight: bold; font-size: 12px;");

    gainSlider = new QSlider(Qt::Horizontal, body);
    gainSlider->setGeometry(126, 15, 195, 25);
    gainSlider->setRange(0, 200); // 0 to 200 represents 0.0 to 20.0

    gainSpin = new QDoubleSpinBox(body);
    gainSpin->setGeometry(328, 15, 96, 26);
    gainSpin->setRange(0.0, 20.0);
    gainSpin->setSingleStep(0.1);
    gainSpin->setValue(settings.value("trackBarGainMaster1", 10.0).toDouble());
    gainSpin->setStyleSheet("background: black; color: white; border: 1px solid #555; font-weight: bold;");
    gainSlider->setValue(gainSpin->value() * 10);

    // --- Exposure (Hikrobot uses microseconds: 20us to 100,000us) ---
    QLabel *lblExp = new QLabel("Exposure (μs)", body);
    lblExp->setGeometry(28, 55, 90, 20);
    lblExp->setStyleSheet("color: white; font-weight: bold; font-size: 12px;");

    expSlider = new QSlider(Qt::Horizontal, body);
    expSlider->setGeometry(126, 55, 195, 25);
    expSlider->setRange(20, 100000);

    expSpin = new QSpinBox(body);
    expSpin->setGeometry(328, 55, 96, 26);
    expSpin->setRange(20, 100000);
    expSpin->setSingleStep(500);
    expSpin->setValue(settings.value("trackBarExposure1", 10000).toInt());
    expSpin->setStyleSheet("background: black; color: white; border: 1px solid #555; font-weight: bold;");
    expSlider->setValue(expSpin->value());

    // --- Gamma (Usually 0.1 to 4.0) ---
    QLabel *lblGamma = new QLabel("Gamma", body);
    lblGamma->setGeometry(28, 95, 71, 20);
    lblGamma->setStyleSheet("color: white; font-weight: bold; font-size: 12px;");

    gammaSlider = new QSlider(Qt::Horizontal, body);
    gammaSlider->setGeometry(123, 95, 195, 25);
    gammaSlider->setRange(10, 400); // represents 0.1 to 4.0

    gammaSpin = new QDoubleSpinBox(body);
    gammaSpin->setGeometry(329, 95, 96, 26);
    gammaSpin->setRange(0.1, 4.0);
    gammaSpin->setDecimals(2);
    gammaSpin->setSingleStep(0.1);
    gammaSpin->setValue(settings.value("trackBarGamma", 1.0).toDouble());
    gammaSpin->setStyleSheet("background: black; color: white; border: 1px solid #555; font-weight: bold;");
    gammaSlider->setValue(gammaSpin->value() * 100);

    // --- Checkboxes ---
    chkPrint = new QCheckBox("Auto Print", body);
    chkPrint->setGeometry(7, 145, 98, 24);
    chkPrint->setStyleSheet("color: white; font-size: 12px;");
    chkPrint->setChecked(settings.value("chkPrint", false).toBool());

    chkSave = new QCheckBox("Auto Save", body);
    chkSave->setGeometry(106, 145, 102, 24);
    chkSave->setStyleSheet("color: white; font-size: 12px;");
    chkSave->setChecked(settings.value("chkSave", false).toBool());

    // --- Action Buttons ---
    btnMesure = new QPushButton(body);
    btnMesure->setGeometry(209, 140, 109, 37);
    btnMesure->setStyleSheet("border-image: url(:/images/TRAY SETTING.jpg); background-color: #FF8000; border-radius: 4px;");

    btnHistory = new QPushButton(body);
    btnHistory->setGeometry(328, 140, 108, 36);
    btnHistory->setStyleSheet("border-image: url(:/images/HISTORY.jpg); background-color: #FF8000; border-radius: 4px;");

    btnVariation = new QPushButton(body);
    btnVariation->setGeometry(448, 133, 63, 48);
    btnVariation->setStyleSheet("border-image: url(:/images/Callibrationin.jpg); background-color: #FF8000; border-radius: 4px;");

    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(body);

    // ==========================================
    // 3. SYNCHRONIZATION SIGNALS
    // ==========================================

    // Gain (Integer slider to Double spinbox mapped by 10)
    connect(gainSlider, &QSlider::valueChanged, this, [this](int val) { gainSpin->setValue(val / 10.0); });
    connect(gainSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double val) { gainSlider->setValue(val * 10); });

    // Exposure (Direct 1:1 mapping)
    connect(expSlider, &QSlider::valueChanged, expSpin, &QSpinBox::setValue);
    connect(expSpin, QOverload<int>::of(&QSpinBox::valueChanged), expSlider, &QSlider::setValue);

    // Gamma (Integer slider to Double spinbox mapped by 100)
    connect(gammaSlider, &QSlider::valueChanged, this, [this](int val) { gammaSpin->setValue(val / 100.0); });
    connect(gammaSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, [this](double val) { gammaSlider->setValue(val * 100); });

    connect(btnClose, &QPushButton::clicked, this, &CameraSettingsDialog::onSaveClicked);
    connect(chkPrint, &QCheckBox::toggled, this, &CameraSettingsDialog::onAutoPrintToggled);
    connect(chkSave, &QCheckBox::toggled, this, &CameraSettingsDialog::onAutoSaveToggled);

    connect(btnMesure, &QPushButton::clicked, this, &CameraSettingsDialog::onTraySettingsClicked);
    connect(btnHistory, &QPushButton::clicked, this, &CameraSettingsDialog::onHistoryClicked);
    connect(btnVariation, &QPushButton::clicked, this, &CameraSettingsDialog::onVariationClicked);
}

void CameraSettingsDialog::onSaveClicked()
{
    QSettings settings("MetricScope", "Settings");

    // Save raw Double and Int values rather than slider positions so CameraWorker reads them correctly
    settings.setValue("trackBarGainMaster1", gainSpin->value());
    settings.setValue("trackBarExposure1", expSpin->value());
    settings.setValue("trackBarGamma", gammaSpin->value());

    emit settingsUpdated();
    this->accept();
}

void CameraSettingsDialog::onAutoPrintToggled(bool checked) {
    QSettings settings("MetricScope", "Settings");
    settings.setValue("chkPrint", checked ? "true" : "false");
}

void CameraSettingsDialog::onAutoSaveToggled(bool checked) {
    QSettings settings("MetricScope", "Settings");
    settings.setValue("chkSave", checked ? "true" : "false");
}

void CameraSettingsDialog::onTraySettingsClicked() {
    TraySettingsDialog dlg(this);
    dlg.exec();
}

void CameraSettingsDialog::onHistoryClicked() {
    HistoryDialog dlg(this);
    dlg.exec();
}

void CameraSettingsDialog::onVariationClicked() {
    PasswordDialog pwdDlg(this);
    if (pwdDlg.exec() == QDialog::Accepted && pwdDlg.isAuthenticated()) {
        VariationDialog varDlg(this);
        varDlg.exec();
    }
}

void CameraSettingsDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void CameraSettingsDialog::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    }
}
