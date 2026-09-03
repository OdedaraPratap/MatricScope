#ifndef VARIATIONDIALOG_H
#define VARIATIONDIALOG_H

#include <QDialog>
#include <QDoubleSpinBox>
#include <QPoint>

class VariationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VariationDialog(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private slots:
    void onGlobalLengthChanged(double value);
    void onGlobalWidthChanged(double value);
    void onResetClicked();
    void onSaveClicked();

private:
    QDoubleSpinBox *lengthCorrections[25];
    QDoubleSpinBox *widthCorrections[25];

    QDoubleSpinBox *numGlobalLength;
    QDoubleSpinBox *numGlobalWidth;
    double previousGlobalLen;
    double previousGlobalWid;

    QPoint m_dragPosition;

    QDoubleSpinBox* createCustomSpinBox();
    void loadSavedVariations();
};

#endif // VARIATIONDIALOG_H
