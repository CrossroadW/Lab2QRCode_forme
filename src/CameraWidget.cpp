#include <QToolButton>
#include <QToolBar>
#include <QPushButton>
#include <QCheckBox>
#include "CameraWidget.h"
#include <QHeaderView>
#include <QVideoWidget>
// ----------------------
// Implementation
// ----------------------
CameraWidget::CameraWidget(QWidget *parent) : QWidget(parent) {
    setWindowTitle("摄像头预览");
    setMinimumSize(800, 600);

    mainLayout = new QVBoxLayout(this);
    menuBar = new QMenuBar(this);
    mainLayout->setMenuBar(menuBar);

    videoSurface = new CameraVideoSurface(this);

    cameraMenu = new QMenu("摄像头", this);
    QMenu* scanMenu = new QMenu("条码格式", this);
    menuBar->addMenu(scanMenu);
    // 添加条码格式动作
    QAction* actSelectAll = new QAction("全选", this);
    QAction* actClearAll = new QAction("清空", this);
    scanMenu->addAction(actSelectAll);
    scanMenu->addAction(actClearAll);
    scanMenu->addSeparator();

    QVector<QAction*> formatActions;
    static const std::vector<std::pair<ZXing::BarcodeFormat, QString>> kBarcodeFormatList = {
        { ZXing::BarcodeFormat::Aztec,           "Aztec" },
        { ZXing::BarcodeFormat::Codabar,         "Codabar" },
        { ZXing::BarcodeFormat::Code39,          "Code39" },
        { ZXing::BarcodeFormat::Code93,          "Code93" },
        { ZXing::BarcodeFormat::Code128,         "Code128" },
        { ZXing::BarcodeFormat::DataBar,         "DataBar" },
        { ZXing::BarcodeFormat::DataBarExpanded, "DataBarExpanded" },
        { ZXing::BarcodeFormat::DataMatrix,      "DataMatrix" },
        { ZXing::BarcodeFormat::EAN8,            "EAN8" },
        { ZXing::BarcodeFormat::EAN13,           "EAN13" },
        { ZXing::BarcodeFormat::ITF,             "ITF" },
        { ZXing::BarcodeFormat::MaxiCode,        "MaxiCode" },
        { ZXing::BarcodeFormat::PDF417,          "PDF417" },
        { ZXing::BarcodeFormat::QRCode,          "QRCode" },
        { ZXing::BarcodeFormat::UPCA,            "UPCA" },
        { ZXing::BarcodeFormat::UPCE,            "UPCE" },
        { ZXing::BarcodeFormat::MicroQRCode,     "MicroQRCode" },
        { ZXing::BarcodeFormat::RMQRCode,        "RMQRCode" },
        { ZXing::BarcodeFormat::DXFilmEdge,      "DXFilmEdge" },
        { ZXing::BarcodeFormat::DataBarLimited,  "DataBarLimited" },
    };
    for (const auto& item : kBarcodeFormatList) {
        QAction* act = new QAction(item.second, this);
        act->setCheckable(true);
        act->setChecked(true);
        act->setData(static_cast<int>(item.first));
        scanMenu->addAction(act);
        formatActions.push_back(act);
    }
    connect(actSelectAll, &QAction::triggered, this, [formatActions]() {
        for (auto* a : formatActions) a->setChecked(true);
    });
    connect(actClearAll, &QAction::triggered, this, [formatActions]() {
        for (auto* a : formatActions) a->setChecked(false);
    });

    // 更新 currentBarcodeFormat 聚合所有勾选
    connect(scanMenu, &QMenu::triggered, this, [this, formatActions]() {
        int mask = 0;
        for (auto* a : formatActions) {
            if (a->isChecked()) mask |= a->data().toInt();
        }
        currentBarcodeFormat = static_cast<ZXing::BarcodeFormat>(mask == 0 ? 0 : mask);
    });

    cameraMenu = new QMenu("摄像头", this);
    menuBar->addMenu(cameraMenu);

    // 枚举摄像头并预创建 QCamera 实例（但不 start）
    const auto cameras = QCameraInfo::availableCameras();
        // 创建互斥的 Action Group
    QActionGroup *cameraActionGroup = new QActionGroup(this);
    cameraActionGroup->setExclusive(true);  // 设置为互斥
    for (int i = 0; i < cameras.size(); ++i) {
        const auto &camInfo = cameras[i];
        QAction *action = new QAction(camInfo.description(), this);
        action->setData(i);
        action->setCheckable(true);  // 设置为可选中

        cameraMenu->addAction(action);
        cameraActionGroup->addAction(action);

        QCamera *cam = new QCamera(camInfo, this);
        cam->setCaptureMode(QCamera::CaptureVideo);
        // 不在此创建 QCameraImageCapture，使用 video surface
        camerasList.push_back(cam);

        connect(action, &QAction::triggered, this, [this, i]() { onCameraIndexChanged(i); });
    }
      // 如果没有摄像头，设置默认选中状态
    if (cameras.empty()) {
        // 可以添加一个禁用状态的"无摄像头"选项
        QAction *noCameraAction = new QAction("无可用摄像头", this);
        noCameraAction->setEnabled(false);
        cameraMenu->addAction(noCameraAction);
    }

        // === 添加图像控制工具栏 ===
    QToolBar *imageToolBar = new QToolBar(this);
    mainLayout->addWidget(imageToolBar);

    // 翻转X轴按钮
    QToolButton *flipXButton = new QToolButton(this);
    flipXButton->setText("水平翻转");
    flipXButton->setCheckable(true);
    flipXButton->setChecked(flipX);
    flipXButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    imageToolBar->addWidget(flipXButton);

    // 翻转Y轴按钮
    QToolButton *flipYButton = new QToolButton(this);
    flipYButton->setText("垂直翻转");
    flipYButton->setCheckable(true);
    flipYButton->setChecked(flipY);
    flipYButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    imageToolBar->addWidget(flipYButton);

   

    // 连接信号（保持你原来的）
    connect(flipXButton, &QToolButton::toggled, this, [this](bool checked) {
        flipX = checked;
    });
    connect(flipYButton, &QToolButton::toggled, this, [this](bool checked) {
        flipY = checked;
    });


    // frameWidget 用 QWidget 绘制 QImage
    frameWidget = new FrameWidget(this);
    frameWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(frameWidget, 1);

    // 条码结果表格
    resultModel = new QStandardItemModel(0, 3, this);
    resultModel->setHorizontalHeaderLabels({"时间", "类型", "内容"});

        // 创建表格和自动滚动控制的水平布局
    QWidget *resultWidget = new QWidget(this);
    QHBoxLayout *resultLayout = new QHBoxLayout(resultWidget);
    resultLayout->setContentsMargins(0, 0, 0, 0);

    resultDisplay = new QTableView(this);
    resultDisplay->setModel(resultModel);
    resultDisplay->setMaximumHeight(150);
    resultDisplay->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultDisplay->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultDisplay->verticalHeader()->setVisible(false);

     // 创建右侧控制面板
    QWidget *controlPanel = new QWidget(this);
    controlPanel->setMaximumWidth(120);
    QVBoxLayout *controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setAlignment(Qt::AlignTop);
    
    // 自动滚动复选框
    autoScrollCheckBox = new QCheckBox("自动滚动", this);
    autoScrollCheckBox->setChecked(true);
    connect(autoScrollCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        autoScrollToBottom = checked;
    });
    
    // 清空表格按钮
    QPushButton *clearButton = new QPushButton("清空结果", this);
    connect(clearButton, &QPushButton::clicked, this, [this]() {
        resultModel->removeRows(0, resultModel->rowCount());
    });
    
    // 滚动到底部按钮
    QPushButton *scrollToBottomButton = new QPushButton("滚动到底部", this);
    connect(scrollToBottomButton, &QPushButton::clicked, this, [this]() {
        resultDisplay->scrollToBottom();
    });
    
    // 滚动到顶部按钮
    QPushButton *scrollToTopButton = new QPushButton("滚动到顶部", this);
    connect(scrollToTopButton, &QPushButton::clicked, this, [this]() {
        resultDisplay->scrollToTop();
    });
    
    controlLayout->addWidget(autoScrollCheckBox);
    controlLayout->addWidget(clearButton);
    controlLayout->addWidget(scrollToBottomButton);
    controlLayout->addWidget(scrollToTopButton);
    controlLayout->addStretch();
    
    // 将表格和控制面板添加到水平布局
    resultLayout->addWidget(resultDisplay, 1);
    resultLayout->addWidget(controlPanel);

    mainLayout->addWidget(resultWidget);

    // 状态栏
    statusBar = new QStatusBar(this);
    cameraStatusLabel = new QLabel("摄像头就绪...", this);
    statusBar->addWidget(cameraStatusLabel);
    barcodeStatusLabel = new QLabel(this);
    barcodeStatusLabel->setAlignment(Qt::AlignRight);
    statusBar->addPermanentWidget(barcodeStatusLabel);
    mainLayout->addWidget(statusBar);

    barcodeClearTimer = new QTimer(this);
    barcodeClearTimer->setSingleShot(true);
    connect(barcodeClearTimer, &QTimer::timeout,barcodeStatusLabel,&QLabel::clear);

    // watcher 用于拿回异步处理结果
    watcher = new QFutureWatcher<FrameResult>(this);
    connect(watcher, &QFutureWatcher<FrameResult>::finished, this, &CameraWidget::onScanFinished);

    // 连接 video surface 的帧信号
    connect(videoSurface, &CameraVideoSurface::frameReady, this, &CameraWidget::onFrameReady);
}

CameraWidget::~CameraWidget(){
    stopCamera();
}

void CameraWidget::onCameraIndexChanged(int index) {
    stopCamera();
    currentCameraIndex = index;
    startCamera(index);
}

void CameraWidget::startCamera(int index) {
    if (cameraStarted) stopCamera();
    if (index < 0 || index >= camerasList.size()) return;

    currentCamera = camerasList[index];

    // set viewfinder to our surface and start
    currentCamera->setViewfinder(static_cast<QAbstractVideoSurface*>(videoSurface));
    currentCamera->start();

    cameraStarted = true;
    cameraStatusLabel->setText("摄像头已启动");
}

void CameraWidget::stopCamera() {
    if (!cameraStarted) return;
    if (currentCamera) {
        currentCamera->stop();
        currentCamera->setViewfinder(static_cast<QVideoWidget*>(nullptr));
        currentCamera = nullptr;
    }
    cameraStarted = false;
    cameraStatusLabel->setText("摄像头已停止");
}

void CameraWidget::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    if (!cameraStarted && !camerasList.isEmpty())
        startCamera(currentCameraIndex);
}

void CameraWidget::hideEvent(QHideEvent *event) {
    stopCamera();
    QWidget::hideEvent(event);
}

// 将 QImage 转成 ZXing::ImageView（只支持常用格式）
ZXing::ImageView CameraWidget::ImageViewFromQImage(const QImage &img) {
    using ZXing::ImageFormat;

    ImageFormat fmt = ImageFormat::None;

    switch (img.format()) {
    case QImage::Format_Grayscale8:
        fmt = ImageFormat::Lum;
        break;
    case QImage::Format_RGB32: // 0xFFRRGGBB
    case QImage::Format_ARGB32:
        fmt = ImageFormat::BGRA;
        break;
    case QImage::Format_RGB888:
        fmt = ImageFormat::BGR;
        break;
    default:
        // convert to an accepted format to be safe
        {
            QImage conv = img.convertToFormat(QImage::Format_RGB32);
            return ImageViewFromQImage(conv);
        }
    }

    return {img.constBits(), img.width(), img.height(), fmt, static_cast<int>(img.bytesPerLine())};
}

// 同步执行 ZXing 解析（在工作线程中运行）
FrameResult CameraWidget::processFrameSync(const QImage& img) {
    FrameResult out;
    if (!isEnabledScan) return out;

    auto barcodes = ZXing::ReadBarcodes(ImageViewFromQImage(img));

    for (auto &bc : barcodes) {
        if (!bc.isValid())
            continue;
        if (currentBarcodeFormat != ZXing::BarcodeFormat::None &&
            !(static_cast<int>(bc.format()) & static_cast<int>(currentBarcodeFormat)))
            continue;

        out.type = QString::fromStdString(ZXing::ToString(bc.format()));
        out.content = QString::fromStdString(bc.text());


        // 为了不修改传入的 QImage（它可能在 UI 线程被共享），我们将结果中的 box 信息返回，
        // UI 线程负责在显示时绘制标注。
        // 这里我们也可以把位置信息放到 out（示例略）。
        break; // 目前只取第一个匹配
    }

    return out;
}

// 当 video surface 有新帧时：
void CameraWidget::onFrameReady(QSharedPointer<QImage> img) {
    // 快速显示最新帧（UI 线程）
    if (!img) return;
    currentDisplayedImage = img;
        // Y-axis flip
    QImage flipped = img->mirrored(flipX, flipY);
    frameWidget->setFrame(flipped);

    // 异步扫码：如果上一个任务还没完成，可以选择取消或丢弃，这里我们丢弃新任务直到上一个完成
    if (watcher->isRunning()) {
        // 丢弃扫描任务，保持显示流畅
        return;
    }

    // 提交给线程池去做耗时的 ZXing 解析
    QSharedPointer<QImage> imgForScan = img; // share pointer
    auto future = QtConcurrent::run([this, imgForScan]() -> FrameResult {
        return processFrameSync(*imgForScan);
    });
    watcher->setFuture(future);
}

// 当异步解析完成，回到主线程更新 UI
void CameraWidget::onScanFinished() {
    FrameResult r = watcher->result();

    if (r.type.isEmpty() && r.content.isEmpty()) {
        // nothing
        return;
    }

    // 检查内容是否与最近一次插入的内容相同
    bool shouldInsert = true;
    if (resultModel->rowCount() > 0) {
        // 获取最近一次插入的内容（第0行是最新插入的）
        QString lastType = resultModel->item(0, 1)->text();
        QString lastContent = resultModel->item(0, 2)->text();
        
        // 如果类型和内容都相同，则不插入
        if (lastType == r.type && lastContent == r.content) {
            shouldInsert = false;
            
            // 可以更新最近一条记录的时间
            resultModel->item(0, 0)->setText(QDateTime::currentDateTime().toString("hh:mm:ss"));
        }
    }

    if (shouldInsert) {
        // 在 UI 线程更新状态与结果
        barcodeStatusLabel->setText("检测到 " + r.type);
        barcodeStatusLabel->setStyleSheet("color: green; font-weight: bold;");

        QList<QStandardItem *> rowItems;
        rowItems << new QStandardItem(QDateTime::currentDateTime().toString("hh:mm:ss"));
        rowItems << new QStandardItem(r.type);
        rowItems << new QStandardItem(r.content);
        resultModel->insertRow(0, rowItems);
        
        // 限制总行数
        if (resultModel->rowCount() > 50) {
            resultModel->removeRow(50);
        }

        // 自动滚动到底部（如果启用）
        if (autoScrollToBottom) {
            QTimer::singleShot(0, [this]() {
                resultDisplay->scrollToBottom();
            });
        }
    }

    barcodeClearTimer->start(3000);
}


// Note: FrameWidget::setFrame(const QImage&) should be lightweight and copy or share image as needed.

// End of file
