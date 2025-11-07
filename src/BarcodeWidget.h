#pragma once

#include <QWidget>
#include <QImage>
#include <opencv2/opencv.hpp>


class QLineEdit;
class QPushButton;
class QLabel;

class BarcodeWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BarcodeWidget(QWidget* parent = nullptr);

private slots:
    
    void onBrowseFile();
    void onGenerateClicked();
    void onSaveClicked();

private:
    QLineEdit* filePathEdit;
    QPushButton* generateButton;
    QPushButton* saveButton;
    QLabel* barcodeLabel;
    QImage lastImage;

    QImage MatToQImage(const cv::Mat& mat);
};
