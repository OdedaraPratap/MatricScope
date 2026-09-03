#ifndef CALIBRATIONDIALOG_H
#define CALIBRATIONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPoint>
#include <QMouseEvent>

class CalibrationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CalibrationDialog(QWidget *parent = nullptr);

signals:
    // Tells the CameraWorker to run the calibration algorithm on the next frame
    void calibrationRequested();

protected:
    // These replace the user32.dll SendMessage/ReleaseCapture logic for dragging
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void onSaveClicked();

private:
    QLineEdit *txtCalib;
    QPoint m_dragPosition;
};

#endif // CALIBRATIONDIALOG_H
