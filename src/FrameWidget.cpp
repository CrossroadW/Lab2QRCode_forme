#include "FrameWidget.h"
#include <QPainter>
#include <QStyleOption>

FrameWidget::FrameWidget(QWidget *parent)
    : QWidget(parent)

{
    setStyleSheet("QWidget{border:1px solid black; background-color:black;}");
}

void FrameWidget::setFrame(const QImage &frame)
{
    m_image = frame.copy();  // 深拷贝，保证线程安全
    update();                // 触发重绘
}

void FrameWidget::clear()
{
    m_image = QImage();
    update();
}


QRect FrameWidget::scaleKeepAspect(const QRect &outer, int w, int h) const
{
    if (w <= 0 || h <= 0) return {};

    const float outerW = outer.width();
    const float outerH = outer.height();
    const float imgRatio = float(w) / float(h);
    const float viewRatio = outerW / outerH;

    int newW, newH;
    if (imgRatio > viewRatio) {
        newW = outerW;
        newH = outerW / imgRatio;
    } else {
        newH = outerH;
        newW = outerH * imgRatio;
    }

    return QRect(
        outer.x() + (outerW - newW) / 2,
        outer.y() + (outerH - newH) / 2,
        newW, newH
    );
}


void FrameWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // 绘制背景（保持样式）
    QStyleOption opt;
    opt.init(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);

    if (m_image.isNull())
        return;

    // 等比缩放并居中
    QRect dst = scaleKeepAspect(rect(), m_image.width(), m_image.height());
    painter.drawImage(dst, m_image);

}