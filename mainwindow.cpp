#include "mainwindow.h"
#include "calibrationdialog.h"
#include "camerasettingdialog.h"
#include "customshapedialog.h"
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPixmap>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_activeMode(MeasurementMode::GeneralC)
{
    showFullScreen();
    setStyleSheet("QMainWindow { background-color: #000000; color: white; }");

    m_shapeGroup = new QButtonGroup(this);
    m_shapeGroup->setExclusive(true);

    setupUi();
    populateCustomShapesMenu();

    m_cameraWorker = new CameraWorker(this);
        connect(m_cameraWorker, &CameraWorker::frameReady, this, &MainWindow::updateCameraFeed);
        connect(m_cameraWorker, &CameraWorker::measurementResult, this, &MainWindow::handleMeasurement);
        connect(m_cameraWorker, &CameraWorker::statusUpdated, this, &MainWindow::updateMeasurementUI);
    m_cameraWorker->start();
}

MainWindow::~MainWindow() {
    if (m_cameraWorker) m_cameraWorker->stop();
}

void MainWindow::setupUi() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ==========================================
    // 1. LEFT PANEL (3-Tier Metric Display)
    // ==========================================
    QFrame *leftPanel = new QFrame(this);
    leftPanel->setFixedWidth(145);
    leftPanel->setStyleSheet("background-color: #111111; border-right: 2px solid #333333;");

    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(6, 6, 6, 6);
    leftLayout->setSpacing(4);

    lblLengthTitle = new QLabel("LENGTH", this);
    lblLengthTitle->setStyleSheet("color: white; font-weight: bold; font-size: 9pt; background: transparent; border: none;");

    lblLengthVal = new QLabel("0.00", this);
    lblLengthVal->setAlignment(Qt::AlignCenter);
    lblLengthVal->setStyleSheet("color: red; font-weight: bold; font-size: 14pt; background-color: #222222; border: 2px solid #FF8000; border-radius: 6px; min-height: 34px;");

    lblWidthTitle = new QLabel("WIDTH", this);
    lblWidthTitle->setStyleSheet("color: white; font-weight: bold; font-size: 9pt; background: transparent; border: none;");

    lblWidthVal = new QLabel("0.00", this);
    lblWidthVal->setAlignment(Qt::AlignCenter);
    lblWidthVal->setStyleSheet("color: red; font-weight: bold; font-size: 14pt; background-color: #222222; border: 2px solid #FF8000; border-radius: 6px; min-height: 34px;");

    lblRatioTitle = new QLabel("RATIO", this);
    lblRatioTitle->setStyleSheet("color: white; font-weight: bold; font-size: 9pt; background: transparent; border: none;");

    lblRatioVal = new QLabel("0.00", this);
    lblRatioVal->setAlignment(Qt::AlignCenter);
    lblRatioVal->setStyleSheet("color: red; font-weight: bold; font-size: 14pt; background-color: #222222; border: 2px solid #FF8000; border-radius: 6px; min-height: 34px;");

    leftLayout->addWidget(lblLengthTitle);
    leftLayout->addWidget(lblLengthVal);
    leftLayout->addWidget(lblWidthTitle);
    leftLayout->addWidget(lblWidthVal);
    leftLayout->addWidget(lblRatioTitle);
    leftLayout->addWidget(lblRatioVal);
    leftLayout->addStretch();

    // ==========================================
    // 2. CENTER CONTAINER (Top Bar + Camera Feed)
    // ==========================================
    QWidget *centerContainer = new QWidget(this);
    QVBoxLayout *centerLayout = new QVBoxLayout(centerContainer);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    QFrame *topBar = new QFrame(this);
    topBar->setFixedHeight(60);
    topBar->setStyleSheet("background-color: #111111; border-bottom: 2px solid #FF8000;");

    QHBoxLayout *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(2, 2, 2, 2);
    topBarLayout->setSpacing(2);

    btnCalib    = createImageBtn("btnCalib", "CALIBRATION1", false);
    btnRound    = createImageBtn("btnRound", "ROUND", true);
    btnPear     = createImageBtn("btnPear", "PEAR", true);
    btnHeart    = createImageBtn("btnHeart", "HEART", true);
    btnOval     = createImageBtn("btnOval", "OVAL", true);
    btnMarquise = createImageBtn("btnMarquise", "MARQUISE", true);
    btnPolygon  = createImageBtn("btnPolygon", "POLYGON", true);
    btnGeneral  = createImageBtn("btnGeneral", "GENERAL", true);
    btnGeneralC = createImageBtn("btnGeneralC", "GENERALC", true);
    btnSettings = createImageBtn("btnSettings", "SETTING", false);
    btnStop     = createImageBtn("btnStop", "Stop_processed", false);

    topBarLayout->addWidget(btnCalib);
    topBarLayout->addWidget(btnRound);
    topBarLayout->addWidget(btnPear);
    topBarLayout->addWidget(btnHeart);
    topBarLayout->addWidget(btnOval);
    topBarLayout->addWidget(btnMarquise);
    topBarLayout->addWidget(btnPolygon);
    topBarLayout->addWidget(btnGeneral);
    topBarLayout->addWidget(btnGeneralC);
    topBarLayout->addWidget(btnSettings);
    topBarLayout->addWidget(btnStop);
    topBarLayout->addStretch();

    m_cameraFeed = new QLabel(this);
    m_cameraFeed->setStyleSheet("background-color: #1A1A1A; border: none;");
    m_cameraFeed->setAlignment(Qt::AlignCenter);
    m_cameraFeed->setText("INITIALIZING CAMERA...");

    centerLayout->addWidget(topBar);
    centerLayout->addWidget(m_cameraFeed, 1);

    // ==========================================
    // 3. RIGHT PANEL (Actions, Custom Shapes Menu & Shutdown)
    // ==========================================
    QFrame *rightPanel = new QFrame(this);
    rightPanel->setFixedWidth(130);
    rightPanel->setStyleSheet("background-color: #111111; border-left: 2px solid #333333;");

    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    rightLayout->setSpacing(4);

    btnBlankBg         = createActionBtn("Blank BG");
    btnCaptureCustom   = createActionBtn("Capture Custom");

    btnCustomShapes    = createActionBtn("Custom Shapes");
    m_customShapesMenu = new QMenu(this);
    m_customShapesMenu->setStyleSheet("QMenu { background-color: #222222; color: white; border: 1px solid #FF8000; } QMenu::item:selected { background-color: #FF8000; color: black; }");
    btnCustomShapes->setMenu(m_customShapesMenu);

    btnClose           = createActionBtn("Exit");
    btnClose->setStyleSheet(
        "QPushButton {"
        "  background-color: #331111;"
        "  color: #FF4444;"
        "  font-weight: bold;"
        "  font-size: 8pt;"
        "  border: 1px solid #FF4444;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #FF4444;"
        "  color: white;"
        "}"
    );

    m_picPreview = new QLabel(this);
    m_picPreview->setFixedSize(110, 80);
    m_picPreview->setStyleSheet("background-color: #222222; border: 1px solid #555555;");
    m_picPreview->setAlignment(Qt::AlignCenter);

    // --- REPLACED LABEL WITH COMBOBOX ---
    m_fileCombo = new QComboBox(this);
    m_fileCombo->setFixedHeight(26);
    m_fileCombo->setStyleSheet(
        "QComboBox {"
        "  background-color: #222222;"
        "  color: white;"
        "  border: 1px solid #FF8000;"
        "  font-weight: bold;"
        "  font-size: 8pt;"
        "  padding-left: 4px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: #222222;"
        "  color: white;"
        "  selection-background-color: #FF8000;"
        "}"
    );

    rightLayout->addWidget(btnBlankBg);
    rightLayout->addWidget(btnCaptureCustom);
    rightLayout->addWidget(btnCustomShapes);
    rightLayout->addWidget(btnClose);
    rightLayout->addWidget(m_picPreview);
    rightLayout->addStretch();
    rightLayout->addWidget(m_fileCombo); // Added combo box at the bottom

    mainLayout->addWidget(leftPanel);
    mainLayout->addWidget(centerContainer, 1);
    mainLayout->addWidget(rightPanel);

    // Populate and connect the profile combo box
    populateProfileComboBox();
    connect(m_fileCombo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onProfileComboChanged);

    rightLayout->addWidget(btnBlankBg);
    rightLayout->addWidget(btnCaptureCustom);
    rightLayout->addWidget(btnCustomShapes);
    rightLayout->addWidget(btnClose);
    rightLayout->addWidget(m_picPreview);
    rightLayout->addStretch();
    rightLayout->addWidget(m_fileCombo);

    mainLayout->addWidget(leftPanel);
    mainLayout->addWidget(centerContainer, 1);
    mainLayout->addWidget(rightPanel);

    // Connections
    connect(btnRound, &QPushButton::clicked, this, [this](){ onShapeButtonClicked(MeasurementMode::Round, "Round"); });
    connect(btnPear, &QPushButton::clicked, this, [this](){ onShapeButtonClicked(MeasurementMode::Pear, "Pear"); });
    connect(btnHeart, &QPushButton::clicked, this, [this](){ onShapeButtonClicked(MeasurementMode::Heart, "Heart"); });
    connect(btnOval, &QPushButton::clicked, this, [this](){ onShapeButtonClicked(MeasurementMode::Oval, "Oval"); });
    connect(btnMarquise, &QPushButton::clicked, this, [this](){ onShapeButtonClicked(MeasurementMode::Marquise, "Marquise"); });
    connect(btnPolygon, &QPushButton::clicked, this, [this](){ onShapeButtonClicked(MeasurementMode::Poly, "Polygon"); });
    connect(btnGeneral, &QPushButton::clicked, this, [this](){ onShapeButtonClicked(MeasurementMode::General, "General"); });
    connect(btnGeneralC, &QPushButton::clicked, this, [this](){ onShapeButtonClicked(MeasurementMode::GeneralC, "GeneralC"); });

    connect(btnStop, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(btnCalib, &QPushButton::clicked, this, &MainWindow::onCalibClicked);
    connect(btnSettings, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    connect(btnBlankBg, &QPushButton::clicked, this, &MainWindow::onBlankBgClicked);
    connect(btnCaptureCustom, &QPushButton::clicked, this, &MainWindow::onCaptureCustomClicked);
    connect(btnClose, &QPushButton::clicked, this, &MainWindow::onCloseClicked);
}

void MainWindow::populateProfileComboBox() {
    if (!m_fileCombo) return;
    m_fileCombo->clear();

    QSqlDatabase db = QSqlDatabase::contains("qt_sql_default_connection")
                      ? QSqlDatabase::database("qt_sql_default_connection")
                      : QSqlDatabase::addDatabase("QSQLITE");

    if (!db.isOpen()) {
        db.setDatabaseName("History.db");
        db.open();
    }

    bool hasDefault = false;
    if (db.isOpen()) {
        QSqlQuery query("SELECT DISTINCT FileName FROM TrayRules;");
        while (query.next()) {
            QString fName = query.value(0).toString();
            m_fileCombo->addItem(fName);
            if (fName == "Default_Profile") hasDefault = true;
        }
    }

    if (!hasDefault) {
        m_fileCombo->insertItem(0, "Default_Profile");
    }

    // Load saved profile selection from QSettings
    QSettings settings("MetricScope", "Settings");
    QString lastFile = settings.value("cmbFile", "Default_Profile").toString();
    int idx = m_fileCombo->findText(lastFile);
    if (idx != -1) {
        m_fileCombo->setCurrentIndex(idx);
    } else {
        m_fileCombo->setCurrentIndex(0);
    }
}

void MainWindow::onProfileComboChanged(const QString &fileName) {
    if (fileName.isEmpty()) return;

    // Save selection immediately to QSettings so CameraWorker and TraySettingsDialog sync up
    QSettings settings("MetricScope", "Settings");
    settings.setValue("cmbFile", fileName);

    qDebug() << "Active Tray Profile switched to:" << fileName;
}

void MainWindow::populateCustomShapesMenu() {
    if (!m_customShapesMenu) return;
    m_customShapesMenu->clear();

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName("History.db");
    }

    if (db.open()) {
        QSqlQuery query("SELECT Name, ImagePath FROM Shapes");
        bool foundShapes = false;
        while (query.next()) {
            foundShapes = true;
            QString shapeName = query.value(0).toString();
            QString imagePath = query.value(1).toString();

            QAction *action = m_customShapesMenu->addAction(shapeName);
            connect(action, &QAction::triggered, this, [this, shapeName, imagePath]() {
                m_activeMode = MeasurementMode::Custom;
                m_cameraWorker->setMode(MeasurementMode::Custom);
                updateMeasurementUI("Active Custom Shape: " + shapeName);
                if (!imagePath.isEmpty() && QFile::exists(imagePath)) {
                    m_picPreview->setPixmap(QPixmap(imagePath).scaled(m_picPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
            });
        }
        if (!foundShapes) {
            QAction *noAction = m_customShapesMenu->addAction("No Custom Shapes Saved");
            noAction->setEnabled(false);
        }
        db.close();
    } else {
        QAction *noAction = m_customShapesMenu->addAction("No Database Connection");
        noAction->setEnabled(false);
    }
}

QPushButton* MainWindow::createImageBtn(const QString &objectName, const QString &imagePrefix, bool isCheckable) {
    QPushButton *btn = new QPushButton(this);
    btn->setObjectName(objectName);
    btn->setFixedSize(44, 44);

    if (isCheckable) {
        btn->setCheckable(true);
        m_shapeGroup->addButton(btn);
    }

    QString styleSheet;
    if (objectName == "btnSettings" || objectName == "btnStop" || objectName == "btnCalib") {
        styleSheet = QString(
            "QPushButton#%1 {"
            "  border-image: url(:/images/%2.png);"
            "  background-color: transparent;"
            "  border: none;"
            "}"
            "QPushButton#%1:pressed {"
            "  background-color: rgba(255, 128, 0, 0.3);"
            "  border: none;"
            "}"
        ).arg(objectName, imagePrefix);
    }
    else {
        styleSheet = QString(
            "QPushButton#%1 {"
            "  border-image: url(:/images/%2.png);"
            "  background-color: transparent;"
            "  border: none;"
            "}"
            "QPushButton#%1:checked {"
            "  border-image: url(:/images/%2_Select.jpeg);"
            "}"
            "QPushButton#%1:pressed {"
            "  border-image: url(:/images/%2_Select.jpeg);"
            "}"
        ).arg(objectName, imagePrefix);
    }

    btn->setStyleSheet(styleSheet);
    m_shapeButtons.push_back(btn);
    return btn;
}

QPushButton* MainWindow::createActionBtn(const QString &text) {
    QPushButton *btn = new QPushButton(text, this);
    btn->setMinimumHeight(35);
    btn->setStyleSheet(
        "QPushButton {"
        "  background-color: #222222;"
        "  color: #FF8000;"
        "  font-weight: bold;"
        "  font-size: 8pt;"
        "  border: 1px solid #FF8000;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #FF8000;"
        "  color: black;"
        "}"
    );
    return btn;
}

void MainWindow::switchMeasurementMode(MeasurementMode newMode, const QString &modeName) {
    if (m_activeMode == newMode) return;
    m_activeMode = newMode;
    m_cameraWorker->setMode(newMode);

    lightBlinkCounters.clear();
    QFile file("tray_counters.json");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "{}";
        file.close();
    }

    if (m_picPreview) m_picPreview->clear();
    updateMeasurementUI("Mode: Auto " + modeName + " Measurement. Counters Reset.");
}

void MainWindow::onShapeButtonClicked(MeasurementMode mode, const QString &modeName) {
    switchMeasurementMode(mode, modeName);
}

void MainWindow::onStopClicked() {
    m_activeMode = MeasurementMode::None;
    m_cameraWorker->setMode(MeasurementMode::None);
    updateMeasurementUI("Auto Measurement Stopped.");

    if (m_shapeGroup->checkedButton()) {
        m_shapeGroup->setExclusive(false);
        m_shapeGroup->checkedButton()->setChecked(false);
        m_shapeGroup->setExclusive(true);
    }
}

void MainWindow::updateCameraFeed(const QImage &img) {
    m_cameraFeed->setPixmap(QPixmap::fromImage(img).scaled(m_cameraFeed->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::handleMeasurement(const QString &resultText, double length, double width) {
    Q_UNUSED(length);
    Q_UNUSED(width);
    updateMeasurementUI(resultText);
}

void MainWindow::updateMeasurementUI(const QString &resultText) {
    if (resultText.isEmpty()) return;

    QRegularExpression rx("[\\d\\.]+");
    QRegularExpressionMatchIterator i = rx.globalMatch(resultText);
    QStringList numbers;
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        numbers << match.captured(0);
    }

    if (resultText.toLower().contains("diameter") && !numbers.isEmpty()) {
        lblLengthTitle->setText("DIAMETER");
        lblLengthVal->setText(numbers[0]);

        lblWidthTitle->hide();
        lblWidthVal->hide();
        lblRatioTitle->hide();
        lblRatioVal->hide();
    }
    else if (resultText.toLower().contains("length") && resultText.toLower().contains("width") && numbers.size() >= 2) {
        lblLengthTitle->setText("LENGTH");
        lblWidthTitle->show();
        lblWidthVal->show();
        lblRatioTitle->show();
        lblRatioVal->show();

        QString lenStr = numbers[0];
        QString widStr = numbers[1];

        lblLengthVal->setText(lenStr);
        lblWidthVal->setText(widStr);

        bool okLen, okWid;
        double len = lenStr.toDouble(&okLen);
        double wid = widStr.toDouble(&okWid);

        if (okLen && okWid && wid > 0) {
            double ratio = len / wid;
            lblRatioVal->setText(QString::number(ratio, 'f', 2));
        } else {
            lblRatioVal->setText("0.00");
        }
    }
    else {
        lblLengthTitle->setText("STATUS");
        lblLengthVal->setText("--");

        lblWidthTitle->hide();
        lblWidthVal->hide();
        lblRatioTitle->hide();
        lblRatioVal->hide();
    }
}

void MainWindow::onBlankBgClicked() {
    m_cameraWorker->captureBackground();
}

void MainWindow::onCalibClicked() {
    CalibrationDialog dlg(this);
    connect(&dlg, &CalibrationDialog::calibrationRequested, m_cameraWorker, &CameraWorker::requestCalibration);
    dlg.exec();
}

void MainWindow::onSettingsClicked() {
    CameraSettingsDialog dlg(this);
    connect(&dlg, &CameraSettingsDialog::settingsUpdated, this, [this](){
        QSettings settings("MetricScope", "Settings");
        int exp = settings.value("trackBarExposure1", 100).toInt();
        int gain = settings.value("trackBarGainMaster1", 10).toInt();
        int gamma = settings.value("trackBarGamma", 100).toInt();

        m_cameraWorker->updateCameraSettings(exp, gain, gamma);
    });
    dlg.exec();
}

void MainWindow::onCaptureCustomClicked() {
    cv::Mat snapshot = m_cameraWorker->getLatestFrame();
    if (snapshot.empty()) {
        QMessageBox::warning(this, "Camera Error", "No live frame available from camera.");
        return;
    }
    CustomShapeDialog dlg(snapshot, this);
    if (dlg.exec() == QDialog::Accepted) {
        populateCustomShapesMenu();
    }
}

void MainWindow::onCloseClicked() {
    int ret = QMessageBox::question(
        this,
        "Shutdown System",
        "Are you sure you want to shut down the Raspberry Pi?",
        QMessageBox::Yes | QMessageBox::No
    );

    if (ret == QMessageBox::Yes) {
        // Now executes instantly without asking for a password!
        system("sudo shutdown -h now");
    }
}
