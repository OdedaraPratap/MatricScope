#pragma once

#include <QThread>
#include <QMutex>
#include <QImage>
#include <QString>
#include <QElapsedTimer>
#include <QFuture>
#include <atomic>
#include <opencv2/opencv.hpp>
#include "MvCameraControl.h"

enum class MeasurementMode {
    None, Round, Pear, Oval, Heart, Marquise, Poly, General, GeneralC, Custom
};

struct ShapeData {
    int Id = 0;
    QString Name;
    QString ImagePath;
    QString TemplateMaskPath;
    cv::Point2f WidthPt1;
    cv::Point2f WidthPt2;
    cv::Point2f LengthPt1;
    cv::Point2f LengthPt2;
    float RefAngle = 0.0f;
    QString ContourData;
    bool SnapToEdge = false;
};


class CameraWorker : public QThread {
    Q_OBJECT
public:
    CameraWorker(QObject *parent = nullptr);
    ~CameraWorker();

    void stop();
    void setMode(MeasurementMode mode);
    void captureBackground();
    void setPpm(double ppmValue);
    cv::Mat getLatestFrame();
    void updateCameraSettings(int exposure, int gain, int gamma);
    void requestCalibration();
    double getPpm();

signals:
    void frameReady(const QImage &image);
    void statusUpdated(const QString &status);
    void measurementResult(const QString &resultText, double length, double width); // 3-Argument Signal

protected:
    void run() override;

public:
    static void __stdcall ImageCallBackEx(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void *pUser);
    void getInvariantTransform(const std::vector<cv::Point>& hull, cv::Point2f& centroid, double& angle, float& scale);

private:
    void handleFrame(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo);
    void resetSnapshotState();

    void doCalibration(cv::Mat &src);
    void processFrame(cv::Mat &frame);
    void triggerAutoMeasurement(cv::Mat &frame);
    double applyVariation(double rawMeasurementMM, bool isLength);
    void triggerPiGpioOutput(int boxNumber);
    QImage matToQImage(const cv::Mat &mat);

    QString measureGeneral(cv::Mat &src);
    QString measureMarquise(cv::Mat &src);
    QString measureHeart(cv::Mat &src);
    QString measurePear(cv::Mat &src);
    QString measureOval(cv::Mat &src);
    QString measurePolygon(cv::Mat &src);
    QString measureCustom(cv::Mat &src);
    QString measureGeneralC(cv::Mat &src);
    QString measureRound(cv::Mat &src);

    double distance(cv::Point p1, cv::Point p2);
    double calculateAngle(cv::Point prev, cv::Point current, cv::Point next);
    void sortCornersClockwise(std::vector<cv::Point>& points);

    cv::Point2f projectToScreen(cv::Point2f localPt, cv::Point2f centroid, double angle, float scale);
    cv::Point2f snapToEdgeStraight(cv::Point2f center, cv::Point2f targetPt, const std::vector<cv::Point>& contour);

    QMutex m_mutex;
    bool m_running;
    std::atomic_bool m_processingFrame{false};
    QFuture<void> m_processingFuture;
    MeasurementMode m_mode;
    double m_ppm;
    int m_thresholdValue;
    cv::Point m_lastCentroid;
    int m_stableFrameCount;
    bool m_hasMeasuredCurrentObject;

    cv::Mat m_backgroundGray;
    cv::Mat m_latestFrame;
    bool m_captureFlag = false;
    bool m_calibrationFlag = false;
    bool m_settingsChanged = false;

    cv::Mat m_lastProcessedFrame;
    bool m_isRenderingSnapshot = false;

    QElapsedTimer m_stopWatch;
    QElapsedTimer m_uiTimer;

    double m_currentExposure = 100.0;
    double m_currentGain = 10.0;
    double m_currentGamma = 1.0;

    ShapeData m_activeCustomShape;
    void *m_devHandle = nullptr;
    const double MOVEMENT_THRESHOLD = 3.0;
    const int FRAMES_TO_STABILIZE = 5;
};
