#ifndef CAMERASETTINGSDIALOG_H
#define CAMERASETTINGSDIALOG_H

#include <QDialog>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>

class CameraSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraSettingsDialog(QWidget *parent = nullptr);

signals:
    void settingsUpdated();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void onSaveClicked();
    void onAutoPrintToggled(bool checked);
    void onAutoSaveToggled(bool checked);
    void onTraySettingsClicked();
    void onHistoryClicked();
    void onVariationClicked();

private:
    QSlider *expSlider;
    QSlider *gainSlider;
    QSlider *gammaSlider;

    QSpinBox *expSpin;
    QDoubleSpinBox *gainSpin;   // Changed to Double for precise Hikrobot Gain (0.0 to 20.0)
    QDoubleSpinBox *gammaSpin;

    QCheckBox *chkPrint;
    QCheckBox *chkSave;

    QPushButton *btnMesure;
    QPushButton *btnHistory;
    QPushButton *btnVariation;

    QPoint m_dragPosition;
};

#endif // CAMERASETTINGSDIALOG_H
