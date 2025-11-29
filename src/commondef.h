#pragma once 
#include <QString>
#include <QImage>
#include <ZXing/Barcode.h>
class QStandardItemModel;
class QTableView;
class QLabel;
class QStatusBar;
class QMenuBar;
class QMenu;
class QVBoxLayout;
class FrameWidget;
class CameraVideoSurface;
class QCheckBox;

struct FrameResult {
    QImage frame;
    QString type;
    QString content;
    QRect box; // 新增，保存二维码在图像中的位置
};