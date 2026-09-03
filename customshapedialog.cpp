#include "customshapedialog.h"
#include "databasehelper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QInputDialog>
#include <QMessageBox>
#include <QDir>
#include <QDateTime>

CustomShapeDialog::CustomShapeDialog(const cv::Mat &capturedFrame, QWidget *parent)
    : QDialog(parent), m_currentState(ClickState::None), m_zoomFactor(1.0f), m_isPanning(false)
{
    if (!capturedFrame.empty() && capturedFrame.depth() == CV_8U) {
        if (capturedFrame.channels() == 1) {
            cv::cvtColor(capturedFrame, m_sourceFrame, cv::COLOR_GRAY2BGR);
        } else if (capturedFrame.channels() == 3) {
            m_sourceFrame = capturedFrame.clone();
        } else if (capturedFrame.channels() == 4) {
            cv::cvtColor(capturedFrame, m_sourceFrame, cv::COLOR_BGRA2BGR);
        }
    }
    if (m_sourceFrame.empty()) {
        m_sourceFrame = cv::Mat(480, 640, CV_8UC3, cv::Scalar::all(0));
    }
    m_displayFrame = m_sourceFrame.clone();

    setWindowTitle("Custom Shape Trainer (Affine Mapping)");
    resize(1100, 720);
    setStyleSheet("QDialog { background-color: #111111; color: white; }");

    // ==========================================
    // UI LAYOUT
    // ==========================================
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QHBoxLayout *topBarLayout = new QHBoxLayout();
    btnSetWidth = new QPushButton("1. Set Width (2 Clicks)", this);
    btnSetLength = new QPushButton("2. Set Length (2 Clicks)", this);
    chkSnapToEdge = new QCheckBox("Snap to Edge", this);
    chkSnapToEdge->setChecked(true);
    btnResetZoom = new QPushButton("Reset Zoom", this);
    btnSave = new QPushButton("3. Save Shape", this);
    lblStatus = new QLabel("Status: Select Width or Length mode.", this);

    QString btnStyle = "QPushButton { background-color: #333; color: white; border: 1px solid #555; border-radius: 4px; padding: 6px; font-weight: bold; }"
                       "QPushButton:pressed { background-color: #FF8000; color: black; }";
    btnSetWidth->setStyleSheet(btnStyle);
    btnSetLength->setStyleSheet(btnStyle);
    btnResetZoom->setStyleSheet(btnStyle);
    btnSave->setStyleSheet(btnStyle);
    chkSnapToEdge->setStyleSheet("color: white; font-weight: bold;");
    lblStatus->setStyleSheet("color: #FF8000; font-weight: bold; font-size: 13px;");

    topBarLayout->addWidget(btnSetWidth);
    topBarLayout->addWidget(btnSetLength);
    topBarLayout->addWidget(chkSnapToEdge);
    topBarLayout->addWidget(btnResetZoom);
    topBarLayout->addWidget(btnSave);
    topBarLayout->addWidget(lblStatus);
    topBarLayout->addStretch();

    mainLayout->addLayout(topBarLayout);

    detectBaseOrientation();
    redrawOverlay();

    // Connections
    connect(btnSetWidth, &QPushButton::clicked, this, &CustomShapeDialog::onSetWidthClicked);
    connect(btnSetLength, &QPushButton::clicked, this, &CustomShapeDialog::onSetLengthClicked);
    connect(btnResetZoom, &QPushButton::clicked, this, &CustomShapeDialog::onResetZoomClicked);
    connect(btnSave, &QPushButton::clicked, this, &CustomShapeDialog::onSaveClicked);
}

void CustomShapeDialog::detectBaseOrientation()
{
    cv::Mat gray, edges;
    cv::cvtColor(m_sourceFrame, gray, cv::COLOR_BGR2GRAY);
    cv::medianBlur(gray, gray, 5);
    cv::Canny(gray, edges, 40, 120);

    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(9, 9));
    cv::morphologyEx(edges, edges, cv::MORPH_CLOSE, element);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    auto largest = std::max_element(contours.begin(), contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        });

    if (largest != contours.end() && cv::contourArea(*largest) > 1200) {
        std::vector<cv::Point> hull;
        cv::convexHull(*largest, hull);
        cv::RotatedRect box = cv::minAreaRect(hull);
        m_centroid = box.center;
        m_baseAngle = box.angle;
        m_refBoxSize = std::max(box.size.width, box.size.height);
    } else {
        m_centroid = cv::Point2f(m_sourceFrame.cols / 2.0f, m_sourceFrame.rows / 2.0f);
        m_baseAngle = 0.0f;
        m_refBoxSize = 100.0f;
    }
}

void CustomShapeDialog::onSetWidthClicked() {
    m_hasW1 = false;
    m_hasW2 = false;
    m_currentState = ClickState::Width;
    lblStatus->setText("Click 2 points for WIDTH on the image.");
    setCursor(Qt::CrossCursor);
}

void CustomShapeDialog::onSetLengthClicked() {
    m_hasL1 = false;
    m_hasL2 = false;
    m_currentState = ClickState::Length;
    lblStatus->setText("Click 2 points for LENGTH on the image.");
    setCursor(Qt::CrossCursor);
}

void CustomShapeDialog::onResetZoomClicked() {
    m_zoomFactor = 1.0f;
    m_panOffset = QPoint(0, 0);
    update();
}

cv::Point2f CustomShapeDialog::mapScreenToImageCoordinates(QPoint screenPt)
{
    // Fix: use m_panOffset variable directly (no parentheses)
    float x = screenPt.x() - m_panOffset.x();
    float y = screenPt.y() - m_panOffset.y();
    x /= m_zoomFactor;
    y /= m_zoomFactor;

    // Aspect ratio mapping
    int imgW = m_sourceFrame.cols;
    int imgH = m_sourceFrame.rows;
    int boxW = width();
    int boxH = height() - 60; // account for top panel

    float ratio = std::min((float)boxW / imgW, (float)boxH / imgH);
    int targetW = imgW * ratio;
    int targetH = imgH * ratio;
    int targetX = (boxW - targetW) / 2;
    int targetY = (boxH - targetH) / 2 + 60;

    float imgX = (x - targetX) * ((float)imgW / targetW);
    float imgY = (y - targetY) * ((float)imgH / targetH);
    return cv::Point2f(imgX, imgY);
}
void CustomShapeDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton) {
        m_isPanning = true;
        m_dragStart = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() == Qt::LeftButton && m_currentState != ClickState::None) {
        cv::Point2f imgPt = mapScreenToImageCoordinates(event->pos());
        if (imgPt.x >= 0 && imgPt.x < m_sourceFrame.cols && imgPt.y >= 0 && imgPt.y < m_sourceFrame.rows) {
            if (m_currentState == ClickState::Width) {
                if (!m_hasW1) { m_w1 = imgPt; m_hasW1 = true; }
                else { m_w2 = imgPt; m_hasW2 = true; m_currentState = ClickState::None; lblStatus->setText("Width points recorded."); setCursor(Qt::ArrowCursor); }
            } else if (m_currentState == ClickState::Length) {
                if (!m_hasL1) { m_l1 = imgPt; m_hasL1 = true; }
                else { m_l2 = imgPt; m_hasL2 = true; m_currentState = ClickState::None; lblStatus->setText("Length points recorded."); setCursor(Qt::ArrowCursor); }
            }
            redrawOverlay();
        }
    }
}

void CustomShapeDialog::mouseMoveEvent(QMouseEvent *event) {
    if (m_isPanning) {
        m_panOffset += event->pos() - m_dragStart;
        m_dragStart = event->pos();
        update();
    }
}

void CustomShapeDialog::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::RightButton) {
        m_isPanning = false;
        setCursor(m_currentState != ClickState::None ? Qt::CrossCursor : Qt::ArrowCursor);
    }
}

void CustomShapeDialog::wheelEvent(QWheelEvent *event) {
    //float oldZoom = m_zoomFactor;
    if (event->angleDelta().y() > 0)
        m_zoomFactor = std::min(m_zoomFactor * 1.25f, 10.0f);
    else
        m_zoomFactor = std::max(m_zoomFactor / 1.25f, 1.0f);

    update();
}

void CustomShapeDialog::redrawOverlay() {
    m_displayFrame = m_sourceFrame.clone();

    cv::circle(m_displayFrame, m_centroid, 5, cv::Scalar(0, 255, 255), -1);

    if (m_hasW1 && m_hasW2) cv::line(m_displayFrame, m_w1, m_w2, cv::Scalar(0, 0, 255), 2);
    if (m_hasL1 && m_hasL2) cv::line(m_displayFrame, m_l1, m_l2, cv::Scalar(255, 0, 0), 2);

    update();
}

void CustomShapeDialog::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.translate(m_panOffset);
    painter.scale(m_zoomFactor, m_zoomFactor);

    // Convert cv::Mat to QImage for rendering
    cv::Mat rgb;
    cv::cvtColor(m_displayFrame, rgb, cv::COLOR_BGR2RGB);
    QImage qimg(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);

    int boxW = width();
    int boxH = height() - 60;
    float ratio = std::min((float)boxW / m_sourceFrame.cols, (float)boxH / m_sourceFrame.rows);
    int targetW = m_sourceFrame.cols * ratio;
    int targetH = m_sourceFrame.rows * ratio;
    int targetX = (boxW - targetW) / 2;
    int targetY = (boxH - targetH) / 2 + 60;

    painter.drawImage(QRect(targetX, targetY, targetW, targetH), qimg);
}

cv::Point2f CustomShapeDialog::projectToLocal(cv::Point2f pt, cv::Point2f centroid, double angle, float scale) {
    double dx = pt.x - centroid.x;
    double dy = pt.y - centroid.y;
    double cosA = std::cos(angle);
    double sinA = std::sin(angle);
    return cv::Point2f((dx * cosA + dy * sinA) / scale, (-dx * sinA + dy * cosA) / scale);
}

void CustomShapeDialog::onSaveClicked() {
    if (!m_hasW1 || !m_hasW2 || !m_hasL1 || !m_hasL2) {
        QMessageBox::warning(this, "Warning", "Please click 2 points for Width and 2 points for Length before saving.");
        return;
    }

    const double selectedWidth = cv::norm(m_w2 - m_w1);
    const double selectedLength = cv::norm(m_l2 - m_l1);
    if (selectedWidth < 2.0 || selectedLength < 2.0) {
        QMessageBox::warning(this, "Invalid Measurement",
                             "Width and length lines must each contain two different points.");
        return;
    }

    bool ok;
    QString shapeName = QInputDialog::getText(this, "Save Shape", "Enter Custom Shape Name:", QLineEdit::Normal, "NewShape", &ok);
    if (!ok || shapeName.trimmed().isEmpty()) return;

    cv::Point2f refCenter;
    double refAngle;
    float uniformScale;

    cv::Mat gray, edges;
    cv::cvtColor(m_sourceFrame, gray, cv::COLOR_BGR2GRAY);
    cv::medianBlur(gray, gray, 5);
    cv::Canny(gray, edges, 40, 120);
    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(9, 9));
    cv::morphologyEx(edges, edges, cv::MORPH_CLOSE, element);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    auto largest = contours.end();
    double largestArea = 0.0;
    for (auto it = contours.begin(); it != contours.end(); ++it) {
        const cv::Rect bounds = cv::boundingRect(*it);
        const bool touchesFrame = bounds.x <= 2 || bounds.y <= 2 ||
            bounds.br().x >= m_sourceFrame.cols - 2 || bounds.br().y >= m_sourceFrame.rows - 2;
        const double area = cv::contourArea(*it);
        if (!touchesFrame && area > largestArea) {
            largestArea = area;
            largest = it;
        }
    }

    if (largest == contours.end() || largestArea < 1200.0) {
        QMessageBox::warning(this, "Error", "Error detecting stone outline for training.");
        return;
    }

    std::vector<cv::Point> hull;
    cv::convexHull(*largest, hull);

    CameraWorker worker;
    worker.getInvariantTransform(hull, refCenter, refAngle, uniformScale);
    if (!std::isfinite(uniformScale) || uniformScale <= 0.0f) {
        QMessageBox::warning(this, "Error", "The detected stone outline is invalid. Please capture it again.");
        return;
    }

    QString recordsFolder = QDir::currentPath() + "/CustomShapes";
    QDir().mkpath(recordsFolder);
    QString imagePath = recordsFolder + QString("/CustomShape_%1_%2.png").arg(shapeName).arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    cv::imwrite(imagePath.toStdString(), m_displayFrame);

    ShapeData shape;
    shape.Name = shapeName;
    shape.ImagePath = imagePath;
    shape.WidthPt1 = projectToLocal(m_w1, refCenter, refAngle, uniformScale);
    shape.WidthPt2 = projectToLocal(m_w2, refCenter, refAngle, uniformScale);
    shape.LengthPt1 = projectToLocal(m_l1, refCenter, refAngle, uniformScale);
    shape.LengthPt2 = projectToLocal(m_l2, refCenter, refAngle, uniformScale);
    shape.RefAngle = static_cast<float>(refAngle);
    shape.SnapToEdge = chkSnapToEdge->isChecked();

    // Save to SQLite database using our new helper
    DatabaseHelper::initializeDatabase();
    DatabaseHelper::saveShape(shape);

    QMessageBox::information(this, "Success", QString("Custom Shape '%1' Saved Successfully!").arg(shapeName));
    accept();
}
