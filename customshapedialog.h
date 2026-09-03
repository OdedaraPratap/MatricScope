#ifndef CUSTOMSHAPEDIALOG_H
#define CUSTOMSHAPEDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <opencv2/opencv.hpp>
#include "cameraworker.h" // For ShapeData

class CustomShapeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomShapeDialog(const cv::Mat &capturedFrame, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onSetWidthClicked();
    void onSetLengthClicked();
    void onSaveClicked();
    void onResetZoomClicked();

private:
    cv::Mat m_sourceFrame;
    cv::Mat m_displayFrame;

    cv::Point2f m_centroid;
    double m_baseAngle;
    float m_refBoxSize;

    cv::Point2f *wPt1 = nullptr;
    cv::Point2f *wPt2 = nullptr;
    cv::Point2f *lPt1 = nullptr;
    cv::Point2f *lPt2 = nullptr;

    // Temporary storage for points
    cv::Point2f m_w1, m_w2, m_l1, m_l2;
    bool m_hasW1 = false, m_hasW2 = false;
    bool m_hasL1 = false, m_hasL2 = false;

    enum class ClickState { None, Width, Length };
    ClickState m_currentState;

    float m_zoomFactor;
    QPoint m_panOffset;
    QPoint m_dragStart;
    bool m_isPanning;

    // UI Controls
    QPushButton *btnSetWidth;
    QPushButton *btnSetLength;
    QPushButton *btnResetZoom;
    QPushButton *btnSave;
    QCheckBox *chkSnapToEdge;
    QLabel *lblStatus;

    void detectBaseOrientation();
    void redrawOverlay();
    cv::Point2f mapScreenToImageCoordinates(QPoint screenPt);
    cv::Point2f projectToLocal(cv::Point2f pt, cv::Point2f centroid, double angle, float scale);
};

#endif // CUSTOMSHAPEDIALOG_H
