#include "cameraworker.h"
#include <QDebug>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSettings>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <QtConcurrentRun>


namespace {

bool toGray8(const cv::Mat &source, cv::Mat &gray)
{
    if (source.empty() || source.depth() != CV_8U) return false;

    switch (source.channels()) {
    case 1:
        gray = source.clone();
        return true;
    case 3:
        cv::cvtColor(source, gray, cv::COLOR_BGR2GRAY);
        return true;
    case 4:
        cv::cvtColor(source, gray, cv::COLOR_BGRA2GRAY);
        return true;
    default:
        return false;
    }
}

bool ensureBgr8(cv::Mat &image)
{
    if (image.empty() || image.depth() != CV_8U) return false;
    if (image.channels() == 3) return true;

    cv::Mat converted;
    if (image.channels() == 1) {
        cv::cvtColor(image, converted, cv::COLOR_GRAY2BGR);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, converted, cv::COLOR_BGRA2BGR);
    } else {
        return false;
    }
    image = converted;
    return true;
}

bool sameImageGeometry(const cv::Mat &first, const cv::Mat &second)
{
    return first.rows == second.rows && first.cols == second.cols;
}

} // namespace

CameraWorker::CameraWorker(QObject *parent)
    : QThread(parent), m_running(true), m_mutex(QMutex::Recursive),m_mode(MeasurementMode::None),
      m_ppm(100.0), m_thresholdValue(25), m_lastCentroid(0, 0),
      m_stableFrameCount(0), m_hasMeasuredCurrentObject(false), m_devHandle(nullptr)
{
    qDebug() << "DEBUG: CameraWorker constructor called.";
    m_backgroundGray = cv::imread("Blank_Bg.png", cv::IMREAD_GRAYSCALE);
    if (!m_backgroundGray.empty()) {
        cv::medianBlur(m_backgroundGray, m_backgroundGray, 7);
        qDebug() << "DEBUG: Blank_Bg.png loaded successfully.";
    } else {
        qDebug() << "DEBUG: Warning - Blank_Bg.png could not be loaded or is empty.";
    }
}

CameraWorker::~CameraWorker() {
    qDebug() << "DEBUG: CameraWorker destructor called.";
    stop();
}

void CameraWorker::stop() {
    qDebug() << "DEBUG: stop() called on CameraWorker.";
    {
        QMutexLocker locker(&m_mutex);
        m_running = false;
    }
    wait();
    qDebug() << "DEBUG: CameraWorker thread successfully stopped and joined.";
}

void CameraWorker::setMode(MeasurementMode mode) {
    {
        QMutexLocker locker(&m_mutex);
        m_mode = mode;
    }
    qDebug() << "DEBUG: MeasurementMode changed to:" << static_cast<int>(mode);
    resetSnapshotState();
}

void CameraWorker::setActiveCustomShape(const ShapeData &shape) {
    QMutexLocker locker(&m_mutex);
    m_activeCustomShape = shape;
    m_hasMeasuredCurrentObject = false;
    m_stableFrameCount = 0;
    m_isRenderingSnapshot = false;
}

void CameraWorker::captureBackground() {
    QMutexLocker locker(&m_mutex);
    m_captureFlag = true;
    qDebug() << "DEBUG: Background capture requested.";
}

void CameraWorker::setPpm(double ppmValue) {
    QMutexLocker locker(&m_mutex);
    m_ppm = (ppmValue > 0) ? ppmValue : 1.0;
    qDebug() << "DEBUG: PPM updated to:" << m_ppm;
}



void CameraWorker::resetSnapshotState() {
    QMutexLocker locker(&m_mutex);
    m_stableFrameCount = 0;
    m_hasMeasuredCurrentObject = false;
    m_lastCentroid = cv::Point(0, 0);
    m_isRenderingSnapshot = false;
    if (!m_lastProcessedFrame.empty()) {
        m_lastProcessedFrame.release();
    }
    emit statusUpdated("Waiting for object...");
}


cv::Mat CameraWorker::getLatestFrame() {
    QMutexLocker locker(&m_mutex);
    if (m_latestFrame.empty()) {
        qDebug() << "DEBUG: getLatestFrame() called, but m_latestFrame is empty!";
    }
    return m_latestFrame.clone();
}

void CameraWorker::updateCameraSettings(int exposure, int gain, int gamma) {
    QMutexLocker locker(&m_mutex);
    qDebug() << "DEBUG: updateCameraSettings called -> Exposure:" << exposure << "Gain:" << gain << "Gamma:" << gamma;

    QSettings settings("MetricScope", "Settings");
    settings.setValue("trackBarExposure1", exposure);
    settings.setValue("trackBarGainMaster1", gain);
    settings.setValue("trackBarGamma", gamma);

    m_currentExposure = exposure;
    m_currentGain = gain;
    m_currentGamma = gamma;
    m_settingsChanged = true;

    if (m_devHandle) {
        int nRetExp = MV_CC_SetFloatValue(m_devHandle, "ExposureTime", m_currentExposure);
        int nRetGain = MV_CC_SetFloatValue(m_devHandle, "Gain", m_currentGain);
        qDebug() << "DEBUG: Set camera settings SDK return codes -> Exposure:" << nRetExp << "Gain:" << nRetGain;
    } else {
        qDebug() << "DEBUG: Warning - m_devHandle is NULL while updating settings.";
    }
}

void CameraWorker::run() {
    qDebug() << "DEBUG: CameraWorker thread run() started.";
    int nRet = MV_OK;

    // 1. Enumerate Devices
    MV_CC_DEVICE_INFO_LIST stDeviceList;
    memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
    if (nRet != MV_OK || stDeviceList.nDeviceNum == 0) {
        qDebug() << "DEBUG: Error - No Hikrobot Camera Found. EnumDevices return:" << nRet << "Count:" << stDeviceList.nDeviceNum;
        emit statusUpdated("Error: No Hikrobot Camera Found.");
        return;
    }
    qDebug() << "DEBUG: Hikrobot camera(s) found count:" << stDeviceList.nDeviceNum;

    // 2. Create Handle for the first device
    nRet = MV_CC_CreateHandle(&m_devHandle, stDeviceList.pDeviceInfo[0]);
    if (nRet != MV_OK || !m_devHandle) {
        qDebug() << "DEBUG: Error - Failed to create Hikrobot handle. Return:" << nRet;
        emit statusUpdated("Error: Failed to create Hikrobot handle.");
        return;
    }
    qDebug() << "DEBUG: Hikrobot handle created successfully.";

    // 3. Open Device
    nRet = MV_CC_OpenDevice(m_devHandle);
    if (nRet != MV_OK) {
        qDebug() << "DEBUG: Error - Failed to open Hikrobot camera. Return:" << nRet;
        emit statusUpdated("Error: Failed to open Hikrobot camera.");
        MV_CC_DestroyHandle(m_devHandle);
        m_devHandle = nullptr;
        return;
    }
    qDebug() << "DEBUG: Hikrobot camera opened successfully.";

    // 4. Set Trigger Mode to Off (Continuous Frame Grabbing)
    nRet = MV_CC_SetEnumValue(m_devHandle, "TriggerMode", 0);
    qDebug() << "DEBUG: Set TriggerMode to Off. Return:" << nRet;

    // 5. Apply Initial Settings from QSettings
    QSettings settings("MetricScope", "Settings");
    double initialExposure = settings.value("trackBarExposure1", 10000.0).toDouble();
    double initialGain = settings.value("trackBarGainMaster1", 10.0).toDouble();
    qDebug() << "DEBUG: Applying initial settings -> Exposure:" << initialExposure << "Gain:" << initialGain;

    MV_CC_SetEnumValue(m_devHandle, "ExposureAuto", 0);
    MV_CC_SetEnumValue(m_devHandle, "GainAuto", 0);

    MV_CC_SetFloatValue(m_devHandle, "ExposureTime", initialExposure);
    MV_CC_SetFloatValue(m_devHandle, "Gain", initialGain);

    // 6. Register Continuous Callback Hook
    nRet = MV_CC_RegisterImageCallBackEx(m_devHandle, ImageCallBackEx, this);
    if (nRet != MV_OK) {
        qDebug() << "DEBUG: Error - Failed to register camera callback. Return:" << nRet;
        emit statusUpdated("Error: Failed to register camera callback.");
    } else {
        qDebug() << "DEBUG: Image callback registered successfully.";
    }

    // 7. Start Grabbing Frames
    nRet = MV_CC_StartGrabbing(m_devHandle);
    if (nRet != MV_OK) {
        qDebug() << "DEBUG: Error - Failed to start grabbing. Return:" << nRet;
        emit statusUpdated("Error: Failed to start grabbing.");
    } else {
        qDebug() << "DEBUG: Hikrobot Camera Streaming started successfully.";
        emit statusUpdated("Hikrobot Camera Streaming...");
    }
    m_stopWatch.start();
    m_uiTimer.start();
    // Keep thread alive while running
    while (true) {
        {
            QMutexLocker locker(&m_mutex);
            if (!m_running) {
                qDebug() << "DEBUG: Stop flag caught in run loop. Exiting thread loop.";
                break;
            }

            if (m_settingsChanged) {
                MV_CC_SetFloatValue(m_devHandle, "ExposureTime", m_currentExposure);
                MV_CC_SetFloatValue(m_devHandle, "Gain", m_currentGain);
                m_settingsChanged = false;
                qDebug() << "DEBUG: Applied deferred camera settings update.";
            }
        }
        QThread::msleep(100);
    }



    // Cleanup Device
    if (m_devHandle) {
        qDebug() << "DEBUG: Cleaning up camera device handles...";
        MV_CC_StopGrabbing(m_devHandle);
        MV_CC_CloseDevice(m_devHandle);
        MV_CC_DestroyHandle(m_devHandle);
        m_devHandle = nullptr;
        qDebug() << "DEBUG: Camera cleanup completed.";
    }
    if (m_processingFuture.isRunning()) {
        m_processingFuture.waitForFinished();
    }
}

// Static SDK Callback Trampoline
void __stdcall CameraWorker::ImageCallBackEx(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void *pUser) {
    CameraWorker *pWorker = static_cast<CameraWorker*>(pUser);
    if (pWorker) {
        pWorker->handleFrame(pData, pFrameInfo);
    }
}

// Frame Processing Callback Worker
void CameraWorker::handleFrame(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo) {
    if (!pData || !pFrameInfo) return;

    // 1. Top-Level Throttle to 50ms (~20 FPS) exactly like C#
    if (m_stopWatch.isValid() && m_stopWatch.elapsed() < 50) {
        return;
    }
    m_stopWatch.restart();

    // 2. Convert Pixel Formats
    cv::Mat matImage;
    if (pFrameInfo->enPixelType == PixelType_Gvsp_Mono8) {
        matImage = cv::Mat(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC1, pData).clone();
    }
    else if (pFrameInfo->enPixelType == PixelType_Gvsp_BayerRG8 ||
             pFrameInfo->enPixelType == PixelType_Gvsp_BayerGR8 ||
             pFrameInfo->enPixelType == PixelType_Gvsp_BayerGB8 ||
             pFrameInfo->enPixelType == PixelType_Gvsp_BayerBG8) {
        cv::Mat bayerMat(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC1, pData);
        int cvBayerCode = cv::COLOR_BayerRG2BGR;
        if (pFrameInfo->enPixelType == PixelType_Gvsp_BayerGR8) cvBayerCode = cv::COLOR_BayerGR2BGR;
        else if (pFrameInfo->enPixelType == PixelType_Gvsp_BayerGB8) cvBayerCode = cv::COLOR_BayerGB2BGR;
        else if (pFrameInfo->enPixelType == PixelType_Gvsp_BayerBG8) cvBayerCode = cv::COLOR_BayerBG2BGR;
        cv::cvtColor(bayerMat, matImage, cvBayerCode);
    }
    else if (pFrameInfo->enPixelType == PixelType_Gvsp_RGB8_Packed) {
        cv::Mat rgbMat(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3, pData);
        cv::cvtColor(rgbMat, matImage, cv::COLOR_RGB2BGR);
    }
    else if (pFrameInfo->enPixelType == PixelType_Gvsp_BGR8_Packed) {
        matImage = cv::Mat(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3, pData).clone();
    } else {
        qWarning() << "Unsupported camera pixel type:" << pFrameInfo->enPixelType;
        return;
    }

    if (matImage.empty()) return;

    cv::Mat processingCopy;
    cv::Mat displayMat;
    bool doProcess = false;

    // 3. Safely Lock, Copy, and Handle Background/Calibration (Equivalent to C# lock(frameLock))
    {
        QMutexLocker locker(&m_mutex);
        m_latestFrame = matImage.clone();

        if (m_captureFlag) {
            cv::imwrite("Blank_Bg.png", m_latestFrame);
            cv::Mat gray;
            if (toGray8(m_latestFrame, gray)) {
                cv::medianBlur(gray, m_backgroundGray, 7);
                m_captureFlag = false;
                emit statusUpdated("Background Captured!");
            } else {
                m_captureFlag = false;
                emit statusUpdated("Background capture failed: unsupported image format");
            }
        }

        if (m_calibrationFlag) {
            cv::Mat calibCopy = m_latestFrame.clone();
            doCalibration(calibCopy);
            m_calibrationFlag = false;
        }

        // Prepare the async processing copy
        if (!m_backgroundGray.empty() && m_mode != MeasurementMode::None) {
            processingCopy = m_latestFrame.clone();
            doProcess = true;
        }

        // Determine what to show on the UI (Snapshot vs Live)
        if (m_isRenderingSnapshot && !m_lastProcessedFrame.empty()) {
            displayMat = m_lastProcessedFrame.clone();
        } else {
            displayMat = m_latestFrame.clone();
        }
    }

    // 4. HIGH-SPEED ALGORITHM PROCESSING ENGINE (Equivalent to C# Task.Run)
    if (doProcess && !processingCopy.empty() && !m_processingFrame.exchange(true)) {
        // Keep at most one analysis task in flight. Queuing every camera frame makes
        // measurements stale and allows several tasks to race on tracking state.
        m_processingFuture = QtConcurrent::run([this, processingCopy]() {
            try {
                cv::Mat frameToProcess = processingCopy;
                processFrame(frameToProcess);
            } catch (const cv::Exception &error) {
                qWarning() << "OpenCV frame-processing error:" << error.what();
                emit statusUpdated("Image processing failed");
            }
            m_processingFrame.store(false);
        });
    }

    // 5. UNIFIED DISPLAY RENDERING PIPELINE (Equivalent to C# BeginInvoke)
    QImage qimg = matToQImage(displayMat);
    if (!qimg.isNull()) {
        emit frameReady(qimg);
    }
}
void CameraWorker::doCalibration(cv::Mat &src)
{
    if (src.empty()) return;

    QSettings settings("MetricScope", "Settings");
    double physicalDiameter = settings.value("calibval", 10.0).toDouble();
    if (physicalDiameter <= 0) return;

    cv::Mat gray, blur, thresh;
    if (!toGray8(src, gray) || !ensureBgr8(src)) {
        emit statusUpdated("Calibration failed: unsupported image format");
        return;
    }
    cv::GaussianBlur(gray, blur, cv::Size(5, 5), 0);

    cv::threshold(blur, thresh, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(thresh, thresh, cv::MORPH_CLOSE, kernel);
    cv::morphologyEx(thresh, thresh, cv::MORPH_OPEN, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (!contours.empty()) {
        const auto &largestContour = *std::max_element(contours.begin(), contours.end(),
            [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
                return cv::contourArea(a) < cv::contourArea(b);
            });

        cv::Point2f center;
        float radius = 0;
        cv::minEnclosingCircle(largestContour, center, radius);

        if (radius > 50)
        {
            double calculatedPpm = (radius * 2.0) / physicalDiameter;
            settings.setValue("ppm", calculatedPpm);
            setPpm(calculatedPpm);

            cv::circle(src, cv::Point(std::round(center.x), std::round(center.y)), std::round(radius), cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
            cv::circle(src, cv::Point(std::round(center.x), std::round(center.y)), 3, cv::Scalar(0, 0, 255), -1, cv::LINE_AA);
            cv::imwrite("calib_result.png", src);

            emit statusUpdated("Calibration complete");
            return;
        }
    }
    emit statusUpdated("Calibration Not Done - No Circle Found");
}

QImage CameraWorker::matToQImage(const cv::Mat &mat) {
    if (mat.empty() || mat.depth() != CV_8U) return QImage();

    if (mat.channels() == 3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    }
    if (mat.channels() == 4) {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        return QImage(rgba.data, rgba.cols, rgba.rows, static_cast<int>(rgba.step), QImage::Format_RGBA8888).copy();
    }
    if (mat.channels() == 1) {
        QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_Indexed8);
        QVector<QRgb> colorTable(256);
        for (int value = 0; value < 256; ++value) colorTable[value] = qRgb(value, value, value);
        image.setColorTable(colorTable);
        return image.copy();
    }
    return QImage();
}

void CameraWorker::triggerAutoMeasurement(cv::Mat &frame) {
    qDebug() << "DEBUG: [triggerAutoMeasurement] Started. Mode =" << static_cast<int>(m_mode);
    QString cvDisplayString = "";

    switch (m_mode) {
        case MeasurementMode::Round:    cvDisplayString = measureRound(frame); break;
        case MeasurementMode::Pear:     cvDisplayString = measurePear(frame); break;
        case MeasurementMode::Oval:     cvDisplayString = measureOval(frame); break;
        case MeasurementMode::Heart:    cvDisplayString = measureHeart(frame); break;
        case MeasurementMode::Marquise: cvDisplayString = measureMarquise(frame); break;
        case MeasurementMode::Poly:     cvDisplayString = measurePolygon(frame); break;
        case MeasurementMode::General:  cvDisplayString = measureGeneral(frame); break;
        case MeasurementMode::GeneralC: cvDisplayString = measureGeneralC(frame); break;
        case MeasurementMode::Custom:   cvDisplayString = measureCustom(frame); break;
        default:
            qDebug() << "DEBUG: [triggerAutoMeasurement] Mode is None or Default. Aborting.";
            return;
    }

    qDebug() << "DEBUG: [triggerAutoMeasurement] Result string from measure function:\n" << cvDisplayString;

    if (!cvDisplayString.isEmpty()) emit statusUpdated(cvDisplayString);

    // Check for failure keywords
    if (cvDisplayString.isEmpty() ||
        cvDisplayString.contains("Error") ||
        cvDisplayString.contains("Please Calibrate") ||
        cvDisplayString.toLower().contains("object") ||
        cvDisplayString.contains("No Shape") ||
        cvDisplayString.contains("No Valid"))
    {
        qDebug() << "DEBUG: [triggerAutoMeasurement] Measurement failed or rejected. Aborting snapshot rendering.";
        return;
    }

    qDebug() << "DEBUG: [triggerAutoMeasurement] Measurement valid. Parsing dimensions.";
    double extractedLength = 0.0, extractedWidth = 0.0;

    if (m_mode == MeasurementMode::Round) {
        QStringList parts = cvDisplayString.split(" ", QString::SkipEmptyParts);
        if (parts.size() >= 3) extractedLength = parts[2].toDouble();
    }
    else if (cvDisplayString.contains("Side")) {
        QRegularExpression regex("Side\\s+\\d+:\\s+([0-9]+(?:\\.[0-9]+)?)");
        QRegularExpressionMatchIterator i = regex.globalMatch(cvDisplayString);
        double maxSide = 0.0, minSide = 999999.0;
        while (i.hasNext()) {
            double val = i.next().captured(1).toDouble();
            if (val > maxSide) maxSide = val;
            if (val < minSide) minSide = val;
        }
        extractedLength = maxSide;
        extractedWidth = (minSide == 999999.0) ? 0.0 : minSide;
    }
    else {
        QRegularExpression regex("[0-9]+(?:\\.[0-9]+)?");
        QRegularExpressionMatchIterator i = regex.globalMatch(cvDisplayString);
        if (i.hasNext()) extractedLength = i.next().captured(0).toDouble();
        if (i.hasNext()) extractedWidth = i.next().captured(0).toDouble();
    }

    qDebug() << "DEBUG: [triggerAutoMeasurement] Extracted L:" << extractedLength << " W:" << extractedWidth;
    emit measurementResult(cvDisplayString, extractedLength, extractedWidth);

    // SAVE FROZEN OVERLAY SNAPSHOT
    {
        QMutexLocker locker(&m_mutex);
        if (m_lastProcessedFrame.empty()) {
            m_lastProcessedFrame = cv::Mat();
        }
        frame.clone().copyTo(m_lastProcessedFrame);
        m_isRenderingSnapshot = true;
        qDebug() << "DEBUG: [triggerAutoMeasurement] Saved frozen snapshot and activated m_isRenderingSnapshot = true.";
    }

    // ... (Keep the rest of your SQLite / GPIO logic down here exactly as it is) ...
    qDebug() << "DEBUG: [triggerAutoMeasurement] Finished processing database/GPIO.";
}


void CameraWorker::processFrame(cv::Mat &frame) {
    cv::Mat liveGray, diff, thresh;

    if (!toGray8(frame, liveGray)) return;
    cv::medianBlur(liveGray, liveGray, 7);

    cv::Mat bgCopy;
    {
        QMutexLocker locker(&m_mutex);
        bgCopy = m_backgroundGray.clone();
    }

    if (bgCopy.empty() || !sameImageGeometry(bgCopy, liveGray)) return;

    cv::absdiff(bgCopy, liveGray, diff);
    cv::threshold(diff, thresh, m_thresholdValue, 255, cv::THRESH_BINARY);

    if (cv::countNonZero(thresh) > 1250) {
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        if (!contours.empty()) {
            int bestIdx = -1;
            double maxArea = 0;

            for (size_t i = 0; i < contours.size(); ++i) {
                double area = cv::contourArea(contours[i]);
                cv::Rect rect = cv::boundingRect(contours[i]);

                // Ignore Hikrobot edge artifacts
                bool touchesBorder = (rect.x <= 2 || rect.y <= 2 ||
                                      rect.x + rect.width >= frame.cols - 2 ||
                                      rect.y + rect.height >= frame.rows - 2);

                if (!touchesBorder && area > maxArea) {
                    maxArea = area;
                    bestIdx = static_cast<int>(i);
                }
            }

            if (bestIdx != -1) {
                cv::Rect boundingBox = cv::boundingRect(contours[bestIdx]);
                double aspect = static_cast<double>(boundingBox.width) / boundingBox.height;

                if (aspect > 0.22 && aspect < 4.5 && maxArea > 800) {
                    cv::Moments mu = cv::moments(contours[bestIdx], true);
                    if (mu.m00 > 0) {
                        cv::Point currentCentroid(mu.m10 / mu.m00, mu.m01 / mu.m00);

                        QMutexLocker locker(&m_mutex); // Lock tracking variables
                        double dist = std::hypot(currentCentroid.x - m_lastCentroid.x, currentCentroid.y - m_lastCentroid.y);

                        if (dist > MOVEMENT_THRESHOLD) {
                            m_stableFrameCount = 0;
                            m_hasMeasuredCurrentObject = false;
                            m_isRenderingSnapshot = false; // Reset snapshot instantly
                            emit statusUpdated("Object moving...");
                        } else {
                            if (!m_hasMeasuredCurrentObject) {
                                m_stableFrameCount++;
                                if (m_stableFrameCount >= FRAMES_TO_STABILIZE) {
                                    m_hasMeasuredCurrentObject = true;

                                    cv::Mat isolatedFrame = frame.clone();

                                    // Unlock before jumping into the heavy drawing function
                                    locker.unlock();
                                    triggerAutoMeasurement(isolatedFrame);
                                    locker.relock();
                                }
                            }
                        }
                        m_lastCentroid = currentCentroid;
                    }
                }
            }
        }
    } else {
        resetSnapshotState();
    }
}
// ============================================================================
// HELPER METHODS
// ============================================================================

void CameraWorker::requestCalibration() {
    QMutexLocker locker(&m_mutex);
    m_calibrationFlag = true;
}

double CameraWorker::getPpm() {
    QSettings settings("MetricScope", "Settings");
    double p = settings.value("ppm", 1.0).toDouble();
    return (p <= 0) ? 1.0 : p;
}

double CameraWorker::applyVariation(double rawMeasurementMM, bool isLength) {
    int rangeIndex = static_cast<int>(std::floor(rawMeasurementMM));
    if (rangeIndex > 24) rangeIndex = 24;
    if (rangeIndex < 0) rangeIndex = 0;

    QSettings settings("MetricScope", "Settings");
    QString regKey = isLength ? QString("LenVar_%1").arg(rangeIndex) : QString("WidVar_%1").arg(rangeIndex);

    double variation = settings.value(regKey, 0.0).toDouble();
    return rawMeasurementMM + variation;
}

void CameraWorker::triggerPiGpioOutput(int boxNumber) {
    int gpioPin = -1;
    switch (boxNumber) {
        case 1:  gpioPin = 4;  break;  // BCM 4  (Physical Pin 7)
        case 2:  gpioPin = 5;  break;  // BCM 5  (Physical Pin 29)
        case 3:  gpioPin = 6;  break;  // BCM 6  (Physical Pin 31)
        case 4:  gpioPin = 7;  break;  // BCM 7  (Physical Pin 26)
        case 5:  gpioPin = 8;  break;  // BCM 8  (Physical Pin 24)
        case 6:  gpioPin = 9;  break;  // BCM 9  (Physical Pin 21)
        case 7:  gpioPin = 10; break;  // BCM 10 (Physical Pin 19)
        case 8:  gpioPin = 11; break;  // BCM 11 (Physical Pin 23)
        case 9:  gpioPin = 12; break;  // BCM 12 (Physical Pin 32)
        case 10: gpioPin = 13; break;  // BCM 13 (Physical Pin 33)
        case 11: gpioPin = 16; break;  // BCM 16 (Physical Pin 36)
        case 12: gpioPin = 19; break;  // BCM 19 (Physical Pin 35)
        case 13: gpioPin = 20; break;  // BCM 20 (Physical Pin 38)
        case 14: gpioPin = 21; break;  // BCM 21 (Physical Pin 40)
        case 15: gpioPin = 22; break;  // BCM 22 (Physical Pin 15)
        case 16: gpioPin = 23; break;  // BCM 23 (Physical Pin 16)
        case 17: gpioPin = 24; break;  // BCM 24 (Physical Pin 18)
        case 18: gpioPin = 25; break;  // BCM 25 (Physical Pin 22)
        case 19: gpioPin = 26; break;  // BCM 26 (Physical Pin 37)
        case 20: gpioPin = 27; break;  // BCM 27 (Physical Pin 13)
        default: return;
    }

    if (gpioPin != -1) {
        QString cmdOn = QString("gpioset 0 %1=1").arg(gpioPin);
        QString cmdOff = QString("gpioset 0 %1=0").arg(gpioPin);

        system(cmdOn.toUtf8().constData());
        QThread::msleep(150);
        system(cmdOff.toUtf8().constData());
    }
}

// ============================================================================
// SHAPE LOGIC
// ============================================================================

QString CameraWorker::measureGeneral(cv::Mat &src) {
    if (src.empty()) return "No Image Data";
    double ppm = getPpm();
    if (!ensureBgr8(src)) return "Error: Unsupported image format";
    cv::Mat gray, blur, thresh;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blur, cv::Size(3, 3), 0);
    cv::threshold(blur, thresh, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(thresh, thresh, cv::MORPH_CLOSE, kernel);
    cv::morphologyEx(thresh, thresh, cv::MORPH_OPEN, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return "No Shape Found";

    const auto &largestContour = *std::max_element(contours.begin(), contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) { return cv::contourArea(a) < cv::contourArea(b); });

    if (cv::contourArea(largestContour) < 800) return "No Object Detected (Noise Ignored)";

    std::vector<cv::Point> hull;
    cv::convexHull(largestContour, hull);

    std::vector<cv::Point> smoothContour;
    double epsilon = 0.01 * cv::arcLength(hull, true);
    cv::approxPolyDP(hull, smoothContour, epsilon, true);

    cv::RotatedRect minRect = cv::minAreaRect(smoothContour);

    double boxLengthPx = std::max(minRect.size.width, minRect.size.height);
    double boxWidthPx = std::min(minRect.size.width, minRect.size.height);

    double boxLengthMM = applyVariation(boxLengthPx / ppm, true);
    double boxWidthMM = applyVariation(boxWidthPx / ppm, false);
    double ratio = (boxWidthMM == 0) ? 0 : boxLengthMM / boxWidthMM;

    cv::Point2f rectPoints[4];
    minRect.points(rectPoints);
    std::vector<cv::Point> boxPoints(4);
    for (int i = 0; i < 4; i++) boxPoints[i] = cv::Point(std::round(rectPoints[i].x), std::round(rectPoints[i].y));

    std::vector<std::vector<cv::Point>> boxWrapper = { boxPoints };
    std::vector<std::vector<cv::Point>> smoothWrapper = { smoothContour };

    cv::polylines(src, boxWrapper, true, cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
    cv::polylines(src, smoothWrapper, true, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
    cv::circle(src, minRect.center, 4, cv::Scalar(255, 255, 255), -1, cv::LINE_AA);

    QString text = QString("Box L: %1mm | W: %2mm").arg(boxLengthMM, 0, 'f', 2).arg(boxWidthMM, 0, 'f', 2);
    cv::putText(src, text.toStdString(), cv::Point(15, 35), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);

    //cv::Mat printCanvas(src.size(), CV_8UC3, cv::Scalar(255, 255, 255));
    //cv::polylines(printCanvas, smoothWrapper, true, cv::Scalar(0, 0, 0), 3, cv::LINE_AA);
    //cv::Rect cropRect = cv::boundingRect(smoothContour);
    //cropRect.x = std::max(0, cropRect.x - 15);
    //cropRect.y = std::max(0, cropRect.y - 15);
    //cropRect.width = std::min(printCanvas.cols - cropRect.x, cropRect.width + 30);
    //cropRect.height = std::min(printCanvas.rows - cropRect.y, cropRect.height + 30);
    //cv::imwrite("ShapeForLabel.png", printCanvas(cropRect));

    cv::imwrite("Box_Shape_Result.png", src);
    return QString("Length : %1 mm\nWidth  : %2 mm\nL/W Ratio : %3").arg(boxLengthMM, 0, 'f', 2).arg(boxWidthMM, 0, 'f', 2).arg(ratio, 0, 'f', 2);
}

QString CameraWorker::measureMarquise(cv::Mat &src) {
    if (src.empty()) return "No Image Data";
    double ppm = getPpm();
    if (!ensureBgr8(src)) return "Error: Unsupported image format";
    cv::Mat gray, blur, thresh;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blur, cv::Size(5, 5), 0);
    cv::threshold(blur, thresh, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(thresh, thresh, cv::MORPH_CLOSE, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) return "No Shape Found";

    const auto &largestContour = *std::max_element(contours.begin(), contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) { return cv::contourArea(a) < cv::contourArea(b); });

    std::vector<cv::Point> hull;
    cv::convexHull(largestContour, hull);

    std::vector<cv::Point> smoothContour;
    double epsilon = 0.01 * cv::arcLength(hull, true);
    cv::approxPolyDP(hull, smoothContour, epsilon, true);

    cv::Point tip1(0, 0), tip2(0, 0);
    double maxTipDist = 0;
    for (size_t i = 0; i < smoothContour.size(); i++) {
        for (size_t j = i + 1; j < smoothContour.size(); j++) {
            double d = std::hypot(smoothContour[i].x - smoothContour[j].x, smoothContour[i].y - smoothContour[j].y);
            if (d > maxTipDist) { maxTipDist = d; tip1 = smoothContour[i]; tip2 = smoothContour[j]; }
        }
    }

    double maxWidthDist = 0;
    cv::Point widthPoint1(0, 0), widthPoint2(0, 0);
    double axisX = tip2.x - tip1.x;
    double axisY = tip2.y - tip1.y;
    double axisLength = std::hypot(axisX, axisY);

    for (size_t i = 0; i < smoothContour.size(); i++) {
        for (size_t j = i + 1; j < smoothContour.size(); j++) {
            double sX = smoothContour[j].x - smoothContour[i].x;
            double sY = smoothContour[j].y - smoothContour[i].y;
            double crossProduct = std::abs(sX * axisY - sY * axisX) / (axisLength == 0 ? 1 : axisLength);
            if (crossProduct > maxWidthDist) {
                maxWidthDist = crossProduct;
                widthPoint1 = smoothContour[i];
                widthPoint2 = smoothContour[j];
            }
        }
    }

    double lengthMM = applyVariation(maxTipDist / ppm, true);
    double widthMM = applyVariation(maxWidthDist / ppm, false);
    double ratio = (widthMM == 0) ? 0 : lengthMM / widthMM;

    cv::line(src, tip1, tip2, cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
    cv::line(src, widthPoint1, widthPoint2, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
    std::vector<std::vector<cv::Point>> wrapper = { smoothContour };
    cv::polylines(src, wrapper, true, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);

    cv::Point lenMid((tip1.x + tip2.x) / 2 + 20, (tip1.y + tip2.y) / 2 - 15);
    cv::Point widMid((widthPoint1.x + widthPoint2.x) / 2 - 80, (widthPoint1.y + widthPoint2.y) / 2 + 25);

    cv::putText(src, QString("L: %1mm").arg(lengthMM, 0, 'f', 2).toStdString(), lenMid, cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 0), 2, cv::LINE_AA);
    cv::putText(src, QString("W: %1mm").arg(widthMM, 0, 'f', 2).toStdString(), widMid, cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 255, 255), 2, cv::LINE_AA);

    //cv::Mat printCanvas(src.size(), CV_8UC3, cv::Scalar(255, 255, 255));
    //cv::polylines(printCanvas, wrapper, true, cv::Scalar(0, 0, 0), 3, cv::LINE_AA);
    //cv::Rect cropRect = cv::boundingRect(smoothContour);
    //cropRect.x = std::max(0, cropRect.x - 15);
    //cropRect.y = std::max(0, cropRect.y - 15);
    //cropRect.width = std::min(printCanvas.cols - cropRect.x, cropRect.width + 30);
    //cropRect.height = std::min(printCanvas.rows - cropRect.y, cropRect.height + 30);
    //cv::imwrite("ShapeForLabel.png", printCanvas(cropRect));

    cv::imwrite("Marquise_Result.png", src);
    return QString("Length : %1 mm\nWidth  : %2 mm\nL/W Ratio : %3").arg(lengthMM, 0, 'f', 2).arg(widthMM, 0, 'f', 2).arg(ratio, 0, 'f', 2);
}

QString CameraWorker::measureHeart(cv::Mat &src) {
    if (src.empty()) return "No Shape Found";
    double ppm = getPpm();
    if (!ensureBgr8(src)) return "Error: Unsupported image format";
    cv::Mat gray, blur, thresh;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blur, cv::Size(3, 3), 0);
    cv::threshold(blur, thresh, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
    if (contours.empty()) return "No Shape Found";

    const auto &largestContour = *std::max_element(contours.begin(), contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) { return cv::contourArea(a) < cv::contourArea(b); });

    if (largestContour.size() < 5) return "Shape too simple";

    std::vector<int> hullIndices;
    cv::convexHull(largestContour, hullIndices, false, false);

    std::vector<cv::Vec4i> defects;
    cv::convexityDefects(largestContour, hullIndices, defects);
    if (defects.empty()) return "No Heart Cleft Found";

    auto maxDefect = *std::max_element(defects.begin(), defects.end(), [](const cv::Vec4i& a, const cv::Vec4i& b) { return a[3] < b[3]; });

    cv::Point lobe1 = largestContour[maxDefect[0]];
    cv::Point lobe2 = largestContour[maxDefect[1]];
    cv::Point cleft = largestContour[maxDefect[2]];

    double baseDx = lobe2.x - lobe1.x;
    double baseDy = lobe2.y - lobe1.y;
    double baseLenSq = baseDx * baseDx + baseDy * baseDy;
    if (baseLenSq == 0) baseLenSq = 1;

    double t = ((cleft.x - lobe1.x) * baseDx + (cleft.y - lobe1.y) * baseDy) / baseLenSq;
    cv::Point dipTop(std::round(lobe1.x + t * baseDx), std::round(lobe1.y + t * baseDy));
    double dipPx = std::hypot(cleft.x - dipTop.x, cleft.y - dipTop.y);

    cv::Point apex = dipTop;
    double maxDistToDipTop = 0;
    for (const auto& p : largestContour) {
        double dist = std::hypot(p.x - dipTop.x, p.y - dipTop.y);
        if (dist > maxDistToDipTop) { maxDistToDipTop = dist; apex = p; }
    }

    double lenDx = apex.x - dipTop.x;
    double lenDy = apex.y - dipTop.y;
    double totalLengthPx = std::hypot(lenDx, lenDy);
    double dirX = lenDx / (totalLengthPx == 0 ? 1 : totalLengthPx);
    double dirY = lenDy / (totalLengthPx == 0 ? 1 : totalLengthPx);

    double perpX = -dirY;
    double perpY = dirX;

    double minProj = 1e9, maxProj = -1e9;
    cv::Point leftPoint = dipTop, rightPoint = dipTop;
    for (const auto& p : largestContour) {
        double proj = (p.x - dipTop.x) * perpX + (p.y - dipTop.y) * perpY;
        if (proj < minProj) { minProj = proj; leftPoint = p; }
        if (proj > maxProj) { maxProj = proj; rightPoint = p; }
    }

    double lengthMM = applyVariation(totalLengthPx / ppm, true);
    double widthMM = applyVariation((maxProj - minProj) / ppm, false);
    double dipDepthMM = dipPx / ppm;
    double ratio = (widthMM == 0) ? 0 : lengthMM / widthMM;

    cv::line(src, lobe1, lobe2, cv::Scalar(128, 128, 128), 1, cv::LINE_AA);
    cv::line(src, cleft, dipTop, cv::Scalar(0, 165, 255), 2, cv::LINE_AA);
    cv::line(src, dipTop, apex, cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
    cv::line(src, leftPoint, rightPoint, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    std::vector<std::vector<cv::Point>> contourWrapper = { largestContour };
    cv::polylines(src, contourWrapper, true, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);

    cv::putText(src, QString("Total L: %1mm").arg(lengthMM, 0, 'f', 2).toStdString(), cv::Point(dipTop.x + 15, dipTop.y + static_cast<int>(lenDy * 0.5)), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 0), 2, cv::LINE_AA);
    cv::putText(src, QString("W: %1mm").arg(widthMM, 0, 'f', 2).toStdString(), cv::Point(leftPoint.x + 20, leftPoint.y - 15), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
    cv::putText(src, QString("Dip: %1mm").arg(dipDepthMM, 0, 'f', 2).toStdString(), cv::Point(cleft.x - 35, cleft.y + 25), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 165, 255), 2, cv::LINE_AA);

    //cv::Mat printCanvas(src.size(), CV_8UC3, cv::Scalar(255, 255, 255));
    //cv::polylines(printCanvas, contourWrapper, true, cv::Scalar(0, 0, 0), 3, cv::LINE_AA);
    //cv::Rect cropRect = cv::boundingRect(largestContour);
    //cropRect.x = std::max(0, cropRect.x - 15);
    //cropRect.y = std::max(0, cropRect.y - 15);
    //cropRect.width = std::min(printCanvas.cols - cropRect.x, cropRect.width + 30);
    //cropRect.height = std::min(printCanvas.rows - cropRect.y, cropRect.height + 30);
    //cv::imwrite("ShapeForLabel.png", printCanvas(cropRect));

    cv::imwrite("Heart_Result.png", src);
    return QString("Length : %1 mm\nWidth  : %2 mm\nDip    : %3 mm\nL/W Ratio : %4").arg(lengthMM, 0, 'f', 2).arg(widthMM, 0, 'f', 2).arg(dipDepthMM, 0, 'f', 2).arg(ratio, 0, 'f', 2);
}

QString CameraWorker::measurePear(cv::Mat &src) {
    if (src.empty()) return "No Shape Found";
    double ppm = getPpm();
    if (!ensureBgr8(src)) return "Error: Unsupported image format";
    cv::Mat gray, blur, thresh;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blur, cv::Size(5, 5), 0);
    cv::threshold(blur, thresh, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(thresh, thresh, cv::MORPH_CLOSE, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
    if (contours.empty()) return "No Shape Found";

    const auto &largestContour = *std::max_element(contours.begin(), contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) { return cv::contourArea(a) < cv::contourArea(b); });

    std::vector<cv::Point> hull;
    cv::convexHull(largestContour, hull);

    cv::Moments mu = cv::moments(hull);
    cv::Point centroid(mu.m10 / mu.m00, mu.m01 / mu.m00);

    cv::Point pearTip = hull[0];
    double maxDistToCentroid = 0;
    for (const auto& p : hull) {
        double dist = std::hypot(p.x - centroid.x, p.y - centroid.y);
        if (dist > maxDistToCentroid) { maxDistToCentroid = dist; pearTip = p; }
    }

    double dirX = centroid.x - pearTip.x;
    double dirY = centroid.y - pearTip.y;
    double lenVector = std::hypot(dirX, dirY);
    dirX /= (lenVector == 0 ? 1 : lenVector);
    dirY /= (lenVector == 0 ? 1 : lenVector);

    cv::Point pearBase = centroid;
    double maxBaseProjection = 0;
    for (const auto& p : hull) {
        double projection = (p.x - pearTip.x) * dirX + (p.y - pearTip.y) * dirY;
        if (projection > maxBaseProjection) {
            maxBaseProjection = projection;
            pearBase = cv::Point(pearTip.x + (int)(dirX * projection), pearTip.y + (int)(dirY * projection));
        }
    }

    double maxWidthDist = 0;
    cv::Point widthL(0, 0), widthR(0, 0);
    for (size_t i = 0; i < hull.size(); i++) {
        for (size_t j = i + 1; j < hull.size(); j++) {
            double sX = hull[j].x - hull[i].x;
            double sY = hull[j].y - hull[i].y;
            double perpDistance = std::abs(sX * dirY - sY * dirX);
            if (perpDistance > maxWidthDist) { maxWidthDist = perpDistance; widthL = hull[i]; widthR = hull[j]; }
        }
    }

    double lengthMM = applyVariation(maxBaseProjection / ppm, true);
    double widthMM = applyVariation(maxWidthDist / ppm, false);
    double ratio = (widthMM == 0) ? 0 : lengthMM / widthMM;

    cv::line(src, pearTip, pearBase, cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
    cv::line(src, widthL, widthR, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
    cv::circle(src, centroid, 5, cv::Scalar(0, 165, 255), -1, cv::LINE_AA);
    std::vector<std::vector<cv::Point>> hullWrapper = { hull };
    cv::polylines(src, hullWrapper, true, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);

    cv::putText(src, QString("L: %1mm").arg(lengthMM, 0, 'f', 2).toStdString(), cv::Point(centroid.x + 25, centroid.y - 20), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 0), 2, cv::LINE_AA);
    cv::putText(src, QString("W: %1mm").arg(widthMM, 0, 'f', 2).toStdString(), cv::Point(widthL.x + 15, widthL.y + 25), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 255, 255), 2, cv::LINE_AA);

    //cv::Mat printCanvas(src.size(), CV_8UC3, cv::Scalar(255, 255, 255));
    //cv::polylines(printCanvas, hullWrapper, true, cv::Scalar(0, 0, 0), 3, cv::LINE_AA);
    //cv::Rect cropRect = cv::boundingRect(hull);
    //cropRect.x = std::max(0, cropRect.x - 15);
    //cropRect.y = std::max(0, cropRect.y - 15);
    //cropRect.width = std::min(printCanvas.cols - cropRect.x, cropRect.width + 30);
    //cropRect.height = std::min(printCanvas.rows - cropRect.y, cropRect.height + 30);
    //cv::imwrite("ShapeForLabel.png", printCanvas(cropRect));

    cv::imwrite("Pear_Result.png", src);
    return QString("Length : %1 mm\nWidth  : %2 mm\nL/W Ratio : %3").arg(lengthMM, 0, 'f', 2).arg(widthMM, 0, 'f', 2).arg(ratio, 0, 'f', 2);
}

QString CameraWorker::measureOval(cv::Mat &src) {
    qDebug() << "DEBUG: [measureOval] Started.";
    if (src.empty()) {
        qDebug() << "DEBUG: [measureOval] src is empty!";
        return "No Shape Found";
    }

    double ppm = getPpm();
    if (!ensureBgr8(src)) return "Error: Unsupported image format";

    cv::Mat gray, blur, diff, thresh;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blur, cv::Size(5, 5), 0);

    cv::Mat background;
    {
        QMutexLocker locker(&m_mutex);
        background = m_backgroundGray.clone();
    }
    if (!background.empty() && sameImageGeometry(background, blur)) {
        cv::absdiff(background, blur, diff);
        cv::threshold(diff, thresh, m_thresholdValue, 255, cv::THRESH_BINARY);
        qDebug() << "DEBUG: [measureOval] Applied background subtraction threshold.";
    } else {
        cv::threshold(blur, thresh, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
        qDebug() << "DEBUG: [measureOval] Fallback to Otsu threshold.";
    }

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(thresh, thresh, cv::MORPH_CLOSE, kernel);
    cv::morphologyEx(thresh, thresh, cv::MORPH_OPEN, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
    qDebug() << "DEBUG: [measureOval] findContours found:" << contours.size() << "contours.";

    if (contours.empty()) return "No Shape Found";

    std::vector<cv::Point> bestContour;
    double maxArea = 0;

    for (const auto& c : contours) {
        double area = cv::contourArea(c);
        cv::Rect rect = cv::boundingRect(c);

        bool touchesBorder = (rect.x <= 2 || rect.y <= 2 ||
                              rect.x + rect.width >= src.cols - 2 ||
                              rect.y + rect.height >= src.rows - 2);

        // Print details of larger contours to track if they are being rejected
        if (area > 200) {
             qDebug() << "DEBUG: [measureOval] Inspecting contour -> Area:" << area << "Touches Border:" << touchesBorder;
        }

        if (!touchesBorder && area > maxArea && area > 500) {
            maxArea = area;
            bestContour = c;
        }
    }

    qDebug() << "DEBUG: [measureOval] maxArea after border filtering:" << maxArea;

    if (bestContour.empty()) {
        qDebug() << "DEBUG: [measureOval] bestContour is empty. Returning 'No Valid Object Found'.";
        return "No Valid Object Found";
    }

    std::vector<cv::Point> hull;
    cv::convexHull(bestContour, hull);

    cv::Point lenStart(0, 0), lenEnd(0, 0);
    double maxLenDist = 0;
    for (size_t i = 0; i < hull.size(); i++) {
        for (size_t j = i + 1; j < hull.size(); j++) {
            double d = std::hypot(hull[i].x - hull[j].x, hull[i].y - hull[j].y);
            if (d > maxLenDist) {
                maxLenDist = d;
                lenStart = hull[i];
                lenEnd = hull[j];
            }
        }
    }

    double maxWidthDist = 0;
    cv::Point widStart(0, 0), widEnd(0, 0);
    double axisX = lenEnd.x - lenStart.x;
    double axisY = lenEnd.y - lenStart.y;
    double axisLength = std::hypot(axisX, axisY);

    for (size_t i = 0; i < hull.size(); i++) {
        for (size_t j = i + 1; j < hull.size(); j++) {
            double sX = hull[j].x - hull[i].x;
            double sY = hull[j].y - hull[i].y;
            double crossProduct = std::abs(sX * axisY - sY * axisX) / (axisLength == 0 ? 1 : axisLength);

            if (crossProduct > maxWidthDist) {
                maxWidthDist = crossProduct;
                widStart = hull[i];
                widEnd = hull[j];
            }
        }
    }

    double lengthMM = applyVariation(maxLenDist / ppm, true);
    double widthMM = applyVariation(maxWidthDist / ppm, false);
    double ratio = (widthMM == 0) ? 0 : lengthMM / widthMM;
    cv::Point center((lenStart.x + lenEnd.x) / 2, (lenStart.y + lenEnd.y) / 2);

    qDebug() << "DEBUG: [measureOval] Drawing lines... L:" << lengthMM << "W:" << widthMM;

    cv::line(src, lenStart, lenEnd, cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
    cv::line(src, widStart, widEnd, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
    cv::circle(src, center, 5, cv::Scalar(0, 165, 255), -1, cv::LINE_AA);

    std::vector<std::vector<cv::Point>> hullWrapper = { hull };
    cv::polylines(src, hullWrapper, true, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);

    cv::putText(src, QString("L: %1mm").arg(lengthMM, 0, 'f', 2).toStdString(), cv::Point(center.x + 25, center.y - 20), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 0), 2, cv::LINE_AA);
    cv::putText(src, QString("W: %1mm").arg(widthMM, 0, 'f', 2).toStdString(), cv::Point(widStart.x + 15, widStart.y + 25), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 255, 255), 2, cv::LINE_AA);

    cv::imwrite("ovel_Result.png", src);
    qDebug() << "DEBUG: [measureOval] Finished writing ovel_Result.png and returning string.";

    return QString("Length : %1 mm\nWidth  : %2 mm\nL/W Ratio : %3").arg(lengthMM, 0, 'f', 2).arg(widthMM, 0, 'f', 2).arg(ratio, 0, 'f', 2);
}

// ============================================================================
// POLYGON & CUSTOM SHAPE MATH HELPERS
// ============================================================================

double CameraWorker::distance(cv::Point p1, cv::Point p2) {
    return std::hypot(p2.x - p1.x, p2.y - p1.y);
}

double CameraWorker::calculateAngle(cv::Point prev, cv::Point current, cv::Point next) {
    double ax = prev.x - current.x;
    double ay = prev.y - current.y;
    double bx = next.x - current.x;
    double by = next.y - current.y;

    double dot = (ax * bx) + (ay * by);
    double magA = std::hypot(ax, ay);
    double magB = std::hypot(bx, by);

    if (magA == 0 || magB == 0) return 0;

    double cosTheta = dot / (magA * magB);
    cosTheta = std::max(-1.0, std::min(1.0, cosTheta));
    return std::acos(cosTheta) * 180.0 / CV_PI;
}

void CameraWorker::sortCornersClockwise(std::vector<cv::Point>& points) {
    if (points.empty()) return;
    cv::Point2f center(0, 0);
    for (auto p : points) { center.x += p.x; center.y += p.y; }
    center.x /= points.size();
    center.y /= points.size();

    std::sort(points.begin(), points.end(), [center](cv::Point a, cv::Point b) {
        return std::atan2(a.y - center.y, a.x - center.x) < std::atan2(b.y - center.y, b.x - center.x);
    });
}

void CameraWorker::getInvariantTransform(const std::vector<cv::Point>& hull, cv::Point2f& centroid, double& angle, float& scale) {
    if (hull.empty()) {
        centroid = cv::Point2f();
        angle = 0.0;
        scale = 1.0f;
        return;
    }
    cv::Moments mu = cv::moments(hull);
    if (std::abs(mu.m00) <= std::numeric_limits<double>::epsilon()) {
        centroid = cv::Point2f(hull.front());
        angle = 0.0;
        scale = 1.0f;
        return;
    }
    centroid = cv::Point2f(mu.m10 / mu.m00, mu.m01 / mu.m00);
    scale = std::max(1.0f, static_cast<float>(std::sqrt(std::abs(mu.m00))));

    // The principal (long) axis is substantially more stable than choosing an
    // axis from small contour asymmetries.  The old asymmetry test could jump by
    // 90 degrees between otherwise identical frames and interchange L and W.
    double theta = 0.5 * std::atan2(2 * mu.mu11, mu.mu20 - mu.mu02);

    double dx = std::cos(theta), dy = std::sin(theta);
    double nx = -dy, ny = dx;

    double maxP1 = 0, minP1 = 0, maxP2 = 0, minP2 = 0;
    for (auto pt : hull) {
        double p1 = (pt.x - centroid.x) * dx + (pt.y - centroid.y) * dy;
        double p2 = (pt.x - centroid.x) * nx + (pt.y - centroid.y) * ny;
        if (p1 > maxP1) maxP1 = p1; if (p1 < minP1) minP1 = p1;
        if (p2 > maxP2) maxP2 = p2; if (p2 < minP2) minP2 = p2;
    }

    if ((maxP2 - minP2) > (maxP1 - minP1)) theta += CV_PI / 2.0;

    // Resolve only the harmless 180-degree ambiguity.  Never select the
    // perpendicular axis based on asymmetry: that is what caused axis swaps.
    const double finalDx = std::cos(theta);
    const double finalDy = std::sin(theta);
    if (finalDy > 0.001 || (std::abs(finalDy) <= 0.001 && finalDx < 0)) theta += CV_PI;

    while (theta < 0) theta += 2 * CV_PI;
    while (theta >= 2 * CV_PI) theta -= 2 * CV_PI;
    angle = theta;
}

// ============================================================================
// POLYGON MEASUREMENT
// ============================================================================
QString CameraWorker::measurePolygon(cv::Mat &src) {
    if (src.empty()) return "No Image Data";
    double ppm = getPpm();
    if (!ensureBgr8(src)) return "Error: Unsupported image format";
    cv::Mat gray, blur, thresh;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blur, cv::Size(5, 5), 0);
    cv::threshold(blur, thresh, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(thresh, thresh, cv::MORPH_CLOSE, kernel);
    cv::morphologyEx(thresh, thresh, cv::MORPH_OPEN, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return "No Polygon Found";
    const auto &largestContour = *std::max_element(contours.begin(), contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) { return cv::contourArea(a) < cv::contourArea(b); });

    if (cv::contourArea(largestContour) < 800) return "No Object Detected (Noise Ignored)";

    std::vector<cv::Point> hull;
    cv::convexHull(largestContour, hull);

    double perimeter = cv::arcLength(hull, true);
    std::vector<cv::Point> polygon;
    cv::approxPolyDP(hull, polygon, 0.02 * perimeter, true);

    if (polygon.size() < 3) return "Invalid Polygon Corners";

    sortCornersClockwise(polygon);

    double maxDistSq = 0;
    cv::Point tip1 = polygon[0], tip2 = polygon[0];
    for (size_t i = 0; i < polygon.size(); i++) {
        for (size_t j = i + 1; j < polygon.size(); j++) {
            double dSq = std::pow(polygon[i].x - polygon[j].x, 2) + std::pow(polygon[i].y - polygon[j].y, 2);
            if (dSq > maxDistSq) { maxDistSq = dSq; tip1 = polygon[i]; tip2 = polygon[j]; }
        }
    }

    double lengthPx = std::sqrt(maxDistSq);
    if (lengthPx == 0) return "Invalid Shape Profile";

    double ux = (tip2.x - tip1.x) / lengthPx;
    double uy = (tip2.y - tip1.y) / lengthPx;
    double nx = uy;
    double ny = -ux;

    double maxLeftDist = 0, maxRightDist = 0;
    for (const auto& p : polygon) {
        double px = p.x - tip1.x;
        double py = p.y - tip1.y;
        double projection = px * nx + py * ny;
        if (projection > maxLeftDist) maxLeftDist = projection;
        if (projection < maxRightDist) maxRightDist = projection;
    }

    double widthPx = maxLeftDist - maxRightDist;
    double generalLengthMM = applyVariation(lengthPx / ppm, true);
    double generalWidthMM = applyVariation(widthPx / ppm, false);

    double midX = (tip1.x + tip2.x) / 2.0;
    double midY = (tip1.y + tip2.y) / 2.0;
    double shift = (maxLeftDist + maxRightDist) / 2.0;
    double centerX = midX + shift * nx;
    double centerY = midY + shift * ny;
    double halfL = lengthPx / 2.0, halfW = widthPx / 2.0;

    std::vector<cv::Point> boxPoints(4);
    boxPoints[0] = cv::Point(std::round(centerX + halfL * ux + halfW * nx), std::round(centerY + halfL * uy + halfW * ny));
    boxPoints[1] = cv::Point(std::round(centerX - halfL * ux + halfW * nx), std::round(centerY - halfL * uy + halfW * ny));
    boxPoints[2] = cv::Point(std::round(centerX - halfL * ux - halfW * nx), std::round(centerY - halfL * uy - halfW * ny));
    boxPoints[3] = cv::Point(std::round(centerX + halfL * ux - halfW * nx), std::round(centerY + halfL * uy - halfW * ny));

    std::vector<std::vector<cv::Point>> boxWrapper = { boxPoints };
    cv::polylines(src, boxWrapper, true, cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
    cv::line(src, tip1, tip2, cv::Scalar(0, 165, 255), 1, cv::LINE_AA);

    for (size_t i = 0; i < polygon.size(); i++) {
        cv::Point p1 = polygon[i];
        cv::Point p2 = polygon[(i + 1) % polygon.size()];
        cv::line(src, p1, p2, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);

        double mmDistance = distance(p1, p2) / ppm;
        cv::Point mid((p1.x + p2.x) / 2, (p1.y + p2.y) / 2);
        cv::putText(src, QString("%1mm").arg(mmDistance, 0, 'f', 2).toStdString(), mid, cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
    }

    QString angleString = "";
    for (size_t i = 0; i < polygon.size(); i++) {
        cv::Point prev = polygon[(i - 1 + polygon.size()) % polygon.size()];
        cv::Point current = polygon[i];
        cv::Point next = polygon[(i + 1) % polygon.size()];

        double angle = calculateAngle(prev, current, next);
        angleString += QString("Angle %1: %2° ").arg(i + 1).arg(angle, 0, 'f', 1);

        cv::circle(src, current, 4, cv::Scalar(0, 0, 255), -1, cv::LINE_AA);
        cv::putText(src, QString("%1°").arg(angle, 0, 'f', 1).toStdString(), cv::Point(current.x + 10, current.y), cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 0), 1, cv::LINE_AA);
    }

    cv::imwrite("Polygon_Result.png", src);
    return QString("Length: %1 mm\nWidth : %2 mm\n").arg(generalLengthMM, 0, 'f', 2).arg(generalWidthMM, 0, 'f', 2) + angleString;
}

// ============================================================================
// CUSTOM SHAPE ENGINE
// ============================================================================
QString CameraWorker::measureCustom(cv::Mat &src) {
    ShapeData activeShape;
    {
        QMutexLocker locker(&m_mutex);
        activeShape = m_activeCustomShape;
    }
    if (activeShape.Name.isEmpty()) return "Error: No Active Shape Selected";
    double ppm = getPpm();
    if (!std::isfinite(ppm) || ppm <= 0.0) return "Please Calibrate";
    if (!ensureBgr8(src)) return "Error: Unsupported image format";
    cv::Mat gray, edges;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::medianBlur(gray, gray, 5);
    cv::Canny(gray, edges, 40, 120);

    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(9, 9));
    cv::morphologyEx(edges, edges, cv::MORPH_CLOSE, element);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return "Object not present";
    auto bestContour = contours.end();
    double bestArea = 0.0;
    for (auto it = contours.begin(); it != contours.end(); ++it) {
        const cv::Rect bounds = cv::boundingRect(*it);
        const bool touchesFrame = bounds.x <= 2 || bounds.y <= 2 ||
            bounds.br().x >= src.cols - 2 || bounds.br().y >= src.rows - 2;
        const double area = cv::contourArea(*it);
        if (!touchesFrame && area > bestArea) {
            bestArea = area;
            bestContour = it;
        }
    }
    if (bestContour == contours.end()) return "Object not present";
    const auto &largestContour = *bestContour;

    if (cv::contourArea(largestContour) < 1200) return "Object not present";

    std::vector<cv::Point> liveHull;
    cv::convexHull(largestContour, liveHull);

    cv::Point2f liveCenter;
    double liveAngle;
    float liveScale;
    getInvariantTransform(liveHull, liveCenter, liveAngle, liveScale);

    cv::Point2f calcW1 = projectToScreen(activeShape.WidthPt1, liveCenter, liveAngle, liveScale);
    cv::Point2f calcW2 = projectToScreen(activeShape.WidthPt2, liveCenter, liveAngle, liveScale);
    cv::Point2f calcL1 = projectToScreen(activeShape.LengthPt1, liveCenter, liveAngle, liveScale);
    cv::Point2f calcL2 = projectToScreen(activeShape.LengthPt2, liveCenter, liveAngle, liveScale);

    if (activeShape.SnapToEdge) {
        // Intersect the complete annotated chord with the outline.  Radial rays
        // from the centroid are incorrect for measurements that are off-centre.
        snapLineToContour(calcW1, calcW2, liveHull);
        snapLineToContour(calcL1, calcL2, liveHull);
    }

    double lengthVal = std::hypot(calcL1.x - calcL2.x, calcL1.y - calcL2.y) / ppm;
    double widthVal = std::hypot(calcW1.x - calcW2.x, calcW1.y - calcW2.y) / ppm;

    // Length is the larger named dimension.  This also protects nearly square
    // profiles whose principal axis can legitimately be ambiguous.
    if (widthVal > lengthVal) {
        std::swap(widthVal, lengthVal);
        std::swap(calcW1, calcL1);
        std::swap(calcW2, calcL2);
    }

    cv::line(src, calcW1, calcW2, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
    cv::line(src, calcL1, calcL2, cv::Scalar(255, 0, 0), 2, cv::LINE_AA);
    cv::circle(src, liveCenter, 4, cv::Scalar(0, 255, 0), -1, cv::LINE_AA);

    //cv::Mat printCanvas(src.size(), CV_8UC3, cv::Scalar(255, 255, 255));
    //std::vector<std::vector<cv::Point>> outlineWrapper = { largestContour };
    //cv::polylines(printCanvas, outlineWrapper, true, cv::Scalar(0, 0, 0), 3, cv::LINE_AA);

    //cv::Rect cropRect = cv::boundingRect(largestContour);
    //cropRect.x = std::max(0, cropRect.x - 15); cropRect.y = std::max(0, cropRect.y - 15);
    //cropRect.width = std::min(printCanvas.cols - cropRect.x, cropRect.width + 30);
    //cropRect.height = std::min(printCanvas.rows - cropRect.y, cropRect.height + 30);
    //cv::imwrite("ShapeForLabel.png", printCanvas(cropRect));

    cv::imwrite("CUSTOMS.png", src);
    return QString("Length: %1\nWidth: %2").arg(lengthVal, 0, 'f', 2).arg(widthVal, 0, 'f', 2);
}

bool CameraWorker::snapLineToContour(cv::Point2f &point1, cv::Point2f &point2,
                                     const std::vector<cv::Point>& contour) const {
    const cv::Point2f delta = point2 - point1;
    const float length = cv::norm(delta);
    if (length <= 1e-4f || contour.size() < 2) return false;

    const cv::Point2f direction = delta * (1.0f / length);
    float minimum = std::numeric_limits<float>::max();
    float maximum = std::numeric_limits<float>::lowest();
    for (size_t index = 0; index < contour.size(); ++index) {
        const cv::Point2f start(contour[index]);
        const cv::Point2f segment = cv::Point2f(contour[(index + 1) % contour.size()]) - start;
        const float cross = direction.x * segment.y - direction.y * segment.x;
        if (std::abs(cross) <= 1e-6f) continue;
        const cv::Point2f offset = start - point1;
        const float linePosition = (offset.x * segment.y - offset.y * segment.x) / cross;
        const float segmentPosition = (offset.x * direction.y - offset.y * direction.x) / cross;
        if (segmentPosition >= -1e-4f && segmentPosition <= 1.0001f) {
            minimum = std::min(minimum, linePosition);
            maximum = std::max(maximum, linePosition);
        }
    }
    if (minimum == std::numeric_limits<float>::max() || maximum - minimum <= 1e-4f) return false;
    point2 = point1 + direction * maximum;
    point1 += direction * minimum;
    return true;
}

cv::Point2f CameraWorker::projectToScreen(cv::Point2f localPt, cv::Point2f centroid, double angle, float scale) {
    double cosA = std::cos(angle);
    double sinA = std::sin(angle);
    double uScale = localPt.x * scale;
    double vScale = localPt.y * scale;
    return cv::Point2f(centroid.x + uScale * cosA - vScale * sinA, centroid.y + uScale * sinA + vScale * cosA);
}

cv::Point2f CameraWorker::snapToEdgeStraight(cv::Point2f center, cv::Point2f targetPt, const std::vector<cv::Point>& contour) {
    const cv::Point2f ray = targetPt - center;
    const float rayLength = cv::norm(ray);
    if (rayLength <= std::numeric_limits<float>::epsilon() || contour.size() < 2) return targetPt;

    const cv::Point2f direction = ray * (1.0f / rayLength);
    float nearestDistance = std::numeric_limits<float>::max();
    for (size_t index = 0; index < contour.size(); ++index) {
        const cv::Point2f segmentStart(contour[index]);
        const cv::Point2f segmentEnd(contour[(index + 1) % contour.size()]);
        const cv::Point2f segment = segmentEnd - segmentStart;
        const cv::Point2f offset = segmentStart - center;
        const float cross = direction.x * segment.y - direction.y * segment.x;
        if (std::abs(cross) <= std::numeric_limits<float>::epsilon()) continue;

        const float rayDistance = (offset.x * segment.y - offset.y * segment.x) / cross;
        const float segmentPosition = (offset.x * direction.y - offset.y * direction.x) / cross;
        if (rayDistance >= 0.0f && segmentPosition >= 0.0f && segmentPosition <= 1.0f) {
            nearestDistance = std::min(nearestDistance, rayDistance);
        }
    }

    return nearestDistance == std::numeric_limits<float>::max()
        ? targetPt
        : center + direction * nearestDistance;
}

QString CameraWorker::measureGeneralC(cv::Mat &src) {
    return measureMarquise(src);
}

QString CameraWorker::measureRound(cv::Mat &src) {
    qDebug() << "DEBUG: [measureRound] Started.";

    if (src.empty()) {
        qDebug() << "DEBUG: [measureRound] src is empty! Returning 'No Image Data'.";
        return "No Image Data";
    }

    double ppm = getPpm();
    qDebug() << "DEBUG: [measureRound] PPM =" << ppm << "| Input channels =" << src.channels();

    if (!ensureBgr8(src)) return "Error: Unsupported image format";

    cv::Mat gray, blur, thresh;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blur, cv::Size(5, 5), 0);

    // Note: You are currently using global Otsu thresholding here instead of the
    // background subtraction we used for Oval.
    cv::threshold(blur, thresh, 0, 255, cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    qDebug() << "DEBUG: [measureRound] Applied Otsu Thresholding.";

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(thresh, thresh, cv::MORPH_CLOSE, kernel);
    cv::morphologyEx(thresh, thresh, cv::MORPH_OPEN, kernel);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    qDebug() << "DEBUG: [measureRound] findContours found:" << contours.size() << "contours.";

    if (!contours.empty()) {
        const auto &largestContour = *std::max_element(contours.begin(), contours.end(),
            [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
                return cv::contourArea(a) < cv::contourArea(b);
            });

        double area = cv::contourArea(largestContour);
        qDebug() << "DEBUG: [measureRound] Largest contour area =" << area;

        if (area > 500) {
            cv::Point2f center;
            float radius;
            cv::minEnclosingCircle(largestContour, center, radius);

            double realSize = (radius * 2.0) / ppm;
            qDebug() << "DEBUG: [measureRound] minEnclosingCircle -> Center: (" << center.x << "," << center.y << "), Radius:" << radius << "px, Real Size:" << realSize << "mm";

            cv::circle(src, cv::Point(std::round(center.x), std::round(center.y)), std::round(radius), cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
            cv::circle(src, cv::Point(std::round(center.x), std::round(center.y)), 4, cv::Scalar(0, 0, 255), -1, cv::LINE_AA);

            QString resultStr = QString("Diameter : %1 mm").arg(realSize, 0, 'f', 2);
            cv::Point textPosition(std::round(center.x) + 15, std::round(center.y) + 5);
            cv::putText(src, resultStr.toStdString(), textPosition, cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2, cv::LINE_AA);

            cv::imwrite("Circle.png", src);
            qDebug() << "DEBUG: [measureRound] Overlays drawn and image saved as Circle.png. Returning:" << resultStr;

            return resultStr;
        } else {
            qDebug() << "DEBUG: [measureRound] Largest area (" << area << ") is <= 500. Rejecting.";
        }
    } else {
        qDebug() << "DEBUG: [measureRound] Contours vector is empty!";
    }

    qDebug() << "DEBUG: [measureRound] Returning 'Diameter Not Detected'.";
    return "Diameter Not Detected";
}
