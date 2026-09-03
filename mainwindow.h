#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QMenu>
#include <QVector>
#include <QMap>
#include <QComboBox>
#include "cameraworker.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void populateCustomShapesMenu();

private slots:
    void onShapeButtonClicked(MeasurementMode mode, const QString &modeName);
    void onStopClicked();
    void updateCameraFeed(const QImage &img);
    void handleMeasurement(const QString &resultText, double length, double width); // 3-Argument Slot
    void updateMeasurementUI(const QString &resultText);
    void onBlankBgClicked();
    void onCalibClicked();
    void onSettingsClicked();
    void onCaptureCustomClicked();
    void onCloseClicked();

private:
    void setupUi();
    QPushButton* createImageBtn(const QString &objectName, const QString &imagePrefix, bool isCheckable);
    QPushButton* createActionBtn(const QString &text);
    void switchMeasurementMode(MeasurementMode newMode, const QString &modeName);

    MeasurementMode m_activeMode;
    CameraWorker *m_cameraWorker;
    QButtonGroup *m_shapeGroup;
    QComboBox *m_fileCombo;
    void populateProfileComboBox();
    void onProfileComboChanged(const QString &fileName);

    QLabel *m_cameraFeed;
    QLabel *m_picPreview;

    QLabel *lblLengthTitle;
    QLabel *lblLengthVal;
    QLabel *lblWidthTitle;
    QLabel *lblWidthVal;
    QLabel *lblRatioTitle;
    QLabel *lblRatioVal;

    QPushButton *btnCalib;
    QPushButton *btnRound;
    QPushButton *btnPear;
    QPushButton *btnHeart;
    QPushButton *btnOval;
    QPushButton *btnMarquise;
    QPushButton *btnPolygon;
    QPushButton *btnGeneral;
    QPushButton *btnGeneralC;
    QPushButton *btnSettings;
    QPushButton *btnStop;

    QPushButton *btnBlankBg;
    QPushButton *btnCaptureCustom;
    QPushButton *btnCustomShapes;
    QMenu *m_customShapesMenu;
    QPushButton *btnClose;

    QVector<QPushButton*> m_shapeButtons;
    QMap<int, int> lightBlinkCounters;
};

#endif // MAINWINDOW_H
