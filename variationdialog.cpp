#include "variationdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QSettings>
#include <QMessageBox>
#include <QMouseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QScroller>

VariationDialog::VariationDialog(QWidget *parent)
    : QDialog(parent), previousGlobalLen(0.0), previousGlobalWid(0.0)
{
    // Form Core Settings - Scaled to 7-inch Pi Display resolution (800x480)
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        setGeometry(screen->geometry());
    }
    setWindowState(windowState() | Qt::WindowFullScreen);
    setStyleSheet("QDialog { background-color: #000000; border: 1px solid #555; color: white; }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ==========================================
    // 1. CUSTOM ORANGE TITLE BAR
    // ==========================================
    QFrame *titleBar = new QFrame(this);
    titleBar->setFixedHeight(40);
    titleBar->setStyleSheet("background-color: #FF8000;");

    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(10, 0, 5, 0);

    QLabel *lblTitle = new QLabel("Size Variations", this);
    lblTitle->setStyleSheet("color: white; font-weight: bold; font-size: 12pt; border: none; background: transparent;");

    QPushButton *btnClose = new QPushButton("X", this);
    btnClose->setFixedSize(36, 30);
    btnClose->setStyleSheet("QPushButton { background-color: #DC143C; color: white; font-weight: bold; font-size: 10pt; border: none; } "
                            "QPushButton:hover { background-color: red; }");

    titleLayout->addWidget(lblTitle);
    titleLayout->addStretch();
    titleLayout->addWidget(btnClose);

    // ==========================================
    // 2. TOOLBAR (Reset All & Two Global Adjusts)
    // ==========================================
    QFrame *toolsPanel = new QFrame(this);
    toolsPanel->setFixedHeight(48);
    toolsPanel->setStyleSheet("background-color: #191919; border: none;");

    QHBoxLayout *toolsLayout = new QHBoxLayout(toolsPanel);
    toolsLayout->setContentsMargins(8, 5, 8, 5);
    toolsLayout->setSpacing(6);

    QLabel *lblGlobalLen = new QLabel("All Len +/-", this);
    lblGlobalLen->setStyleSheet("color: #FF8000; font-weight: bold; font-size: 9pt;");
    numGlobalLength = createCustomSpinBox();
    numGlobalLength->setFixedWidth(82);

    QLabel *lblGlobalWid = new QLabel("All Wid +/-", this);
    lblGlobalWid->setStyleSheet("color: #FF8000; font-weight: bold; font-size: 9pt;");
    numGlobalWidth = createCustomSpinBox();
    numGlobalWidth->setFixedWidth(82);

    QPushButton *btnReset = new QPushButton("RESET ALL", this);
    btnReset->setFixedSize(92, 34);
    btnReset->setStyleSheet("QPushButton { background-color: #DC143C; color: white; font-weight: bold; font-size: 8pt; border: none; border-radius: 3px; } "
                            "QPushButton:pressed { background-color: darkred; }");

    toolsLayout->addWidget(lblGlobalLen);
    toolsLayout->addWidget(numGlobalLength);
    toolsLayout->addWidget(lblGlobalWid);
    toolsLayout->addWidget(numGlobalWidth);
    toolsLayout->addStretch();
    toolsLayout->addWidget(btnReset);

    // ==========================================
    // 3. SCROLLABLE DATA GRID (25 Rows)
    // ==========================================
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background-color: black; } "
                              "QScrollBar:vertical { background: #111; width: 22px; margin: 0px; } "
                              "QScrollBar::handle:vertical { background: #FF8000; min-height: 42px; border-radius: 6px; }");
    QScroller::grabGesture(scrollArea->viewport(), QScroller::TouchGesture);

    QWidget *gridContainer = new QWidget(scrollArea);
    gridContainer->setStyleSheet("background-color: black;");
    QGridLayout *grid = new QGridLayout(gridContainer);
    grid->setContentsMargins(12, 8, 12, 8);
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(4);

    // Headers
    QLabel *h1 = new QLabel("Size Range", gridContainer);
    QLabel *h2 = new QLabel("Length +/-", gridContainer);
    QLabel *h3 = new QLabel("Width +/-", gridContainer);
    QString headerStyle = "color: #FF8000; font-weight: bold; font-size: 10pt;";
    h1->setStyleSheet(headerStyle);
    h2->setStyleSheet(headerStyle);
    h3->setStyleSheet(headerStyle);

    grid->addWidget(h1, 0, 0);
    grid->addWidget(h2, 0, 1);
    grid->addWidget(h3, 0, 2);

    for (int i = 0; i < 25; i++) {
        QLabel *lblRange = new QLabel(QString("%1 to %2 mm").arg(i).arg(i + 1), gridContainer);
        lblRange->setStyleSheet("color: white; font-weight: bold; font-size: 9pt;");

        lengthCorrections[i] = createCustomSpinBox();
        widthCorrections[i] = createCustomSpinBox();
        lblRange->setMinimumHeight(34);

        grid->addWidget(lblRange, i + 1, 0);
        grid->addWidget(lengthCorrections[i], i + 1, 1);
        grid->addWidget(widthCorrections[i], i + 1, 2);
    }
    grid->setColumnStretch(0, 2);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    scrollArea->setWidget(gridContainer);

    // ==========================================
    // 4. SAVE BUTTON
    // ==========================================
    QPushButton *btnSave = new QPushButton("SAVE", this);
    btnSave->setFixedHeight(44);
    btnSave->setStyleSheet("QPushButton { background-color: #FF8000; color: white; font-weight: bold; font-size: 10pt; border: none; } "
                           "QPushButton:pressed { background-color: #cc6600; }");

    mainLayout->addWidget(titleBar);
    mainLayout->addWidget(toolsPanel);
    mainLayout->addWidget(scrollArea, 1);
    mainLayout->addWidget(btnSave);

    // ==========================================
    // 5. SIGNAL CONNECTIONS
    // ==========================================
    connect(btnClose, &QPushButton::clicked, this, &VariationDialog::reject);
    connect(numGlobalLength, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &VariationDialog::onGlobalLengthChanged);
    connect(numGlobalWidth, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), this, &VariationDialog::onGlobalWidthChanged);
    connect(btnReset, &QPushButton::clicked, this, &VariationDialog::onResetClicked);
    connect(btnSave, &QPushButton::clicked, this, &VariationDialog::onSaveClicked);

    // Load saved settings
    loadSavedVariations();
}

QDoubleSpinBox* VariationDialog::createCustomSpinBox()
{
    QDoubleSpinBox *box = new QDoubleSpinBox(this);
    box->setDecimals(3);
    box->setSingleStep(0.005);
    box->setRange(-5.000, 5.000);
    box->setFixedWidth(96);
    box->setMinimumHeight(32);
    box->setStyleSheet("QDoubleSpinBox { background-color: black; color: white; border: 1px solid #555; font-size: 10pt; padding: 2px; }"
                       "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { background-color: #333; width: 24px; }"
                       "QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover { background-color: #FF8000; }");
    return box;
}

void VariationDialog::loadSavedVariations()
{
    QSettings settings("MetricScope", "Settings");
    for (int i = 0; i < 25; i++) {
        double lenVal = settings.value(QString("LenVar_%1").arg(i), 0.0).toDouble();
        double widVal = settings.value(QString("WidVar_%1").arg(i), 0.0).toDouble();

        lengthCorrections[i]->setValue(lenVal);
        widthCorrections[i]->setValue(widVal);
    }
}

void VariationDialog::onGlobalLengthChanged(double value)
{
    double delta = value - previousGlobalLen;
    for (int i = 0; i < 25; i++) {
        double newVal = lengthCorrections[i]->value() + delta;
        if (newVal > 5.0) newVal = 5.0;
        if (newVal < -5.0) newVal = -5.0;
        lengthCorrections[i]->setValue(newVal);
    }
    previousGlobalLen = value;
}

void VariationDialog::onGlobalWidthChanged(double value)
{
    double delta = value - previousGlobalWid;
    for (int i = 0; i < 25; i++) {
        double newVal = widthCorrections[i]->value() + delta;
        if (newVal > 5.0) newVal = 5.0;
        if (newVal < -5.0) newVal = -5.0;
        widthCorrections[i]->setValue(newVal);
    }
    previousGlobalWid = value;
}

void VariationDialog::onResetClicked()
{
    numGlobalLength->setValue(0.0);
    previousGlobalLen = 0.0;
    numGlobalWidth->setValue(0.0);
    previousGlobalWid = 0.0;

    for (int i = 0; i < 25; i++) {
        lengthCorrections[i]->setValue(0.0);
        widthCorrections[i]->setValue(0.0);
    }
}

void VariationDialog::onSaveClicked()
{
    QSettings settings("MetricScope", "Settings");
    for (int i = 0; i < 25; i++) {
        settings.setValue(QString("LenVar_%1").arg(i), lengthCorrections[i]->value());
        settings.setValue(QString("WidVar_%1").arg(i), widthCorrections[i]->value());
    }
    QMessageBox::information(this, "Saved", "Variations saved successfully!");
    accept();
}

void VariationDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void VariationDialog::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    }
}
