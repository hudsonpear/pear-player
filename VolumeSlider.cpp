#include "VolumeSlider.h"
#include "Theme.h"

#include <QPainter>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>

#include <algorithm>

namespace {
constexpr int kTrackHeight = 6;
constexpr int kWidgetHeight = 24;
constexpr int kUnityVolume = 100; // 100% = unity gain; above it mpv amplifies in software.

const QColor kTrackColor(58, 58, 62);
const QColor kUnityTickColor(255, 255, 255, 90);
}

VolumeSlider::VolumeSlider(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(kWidgetHeight);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    // No static setToolTip() here: mouseMoveEvent() already shows a live
    // "N%" tooltip, and having both fight over the same tooltip popup made
    // it flicker between "Volume" and the percentage.
}

QSize VolumeSlider::sizeHint() const
{
    return QSize(120, kWidgetHeight);
}

void VolumeSlider::setVolumeSilently(int volume0to200)
{
    setValueInternal(volume0to200, /*emitSignal=*/false);
}

void VolumeSlider::setValueInternal(int volume0to200, bool emitSignal)
{
    const int clamped = std::clamp(volume0to200, 0, maxVolume_);
    if (clamped == value_) {
        return;
    }
    value_ = clamped;
    update();
    if (emitSignal) {
        emit volumeChangeRequested(value_);
    }
}

void VolumeSlider::setMaxVolume(int maxVolume)
{
    maxVolume_ = std::clamp(maxVolume, 100, 1000);
    if (value_ > maxVolume_) {
        value_ = maxVolume_;
    }
    update();
}

void VolumeSlider::updateFromPixelX(qreal x)
{
    const double fraction = std::clamp(x / std::max(1, width()), 0.0, 1.0);
    setValueInternal(static_cast<int>(std::lround(fraction * maxVolume_)), /*emitSignal=*/true);
}

void VolumeSlider::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal y = (height() - kTrackHeight) / 2.0;
    const QRectF trackRect(0, y, width(), kTrackHeight);

    painter.setPen(Qt::NoPen);
    painter.setBrush(kTrackColor);
    painter.drawRect(trackRect);

    const double fraction = value_ / static_cast<double>(maxVolume_);
    const qreal fillWidth = fraction * width();

    if (fillWidth > 0.0) {
        const QRectF fillRect(0, y, fillWidth, kTrackHeight);
        QLinearGradient gradient(fillRect.topLeft(), fillRect.topRight());
        // See TimelineSlider: read live so a theme change takes effect.
        gradient.setColorAt(0.0, Theme::accentStart());
        gradient.setColorAt(1.0, Theme::accentEnd());
        painter.setBrush(gradient);
        painter.drawRect(fillRect);
    }

    // Tick marking 100% (unity gain) so boosting past it is a deliberate choice.
    const qreal unityX = (kUnityVolume / static_cast<double>(maxVolume_)) * width();
    painter.setPen(QPen(kUnityTickColor, 1));
    painter.drawLine(QPointF(unityX, y - 1), QPointF(unityX, y + kTrackHeight + 1));
}

void VolumeSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    dragging_ = true;
    updateFromPixelX(event->position().x());
    event->accept();
}

void VolumeSlider::mouseMoveEvent(QMouseEvent *event)
{
    hovering_ = true;
    if (dragging_) {
        updateFromPixelX(event->position().x());
    } else {
        update();
    }
    QToolTip::showText(event->globalPosition().toPoint(), tr("%1%").arg(value_), this);
}

void VolumeSlider::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragging_ = false;
        update();
    }
}

void VolumeSlider::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps != 0) {
        setValueInternal(value_ + steps * 5, /*emitSignal=*/true);
    }
    event->accept();
}

void VolumeSlider::leaveEvent(QEvent * /*event*/)
{
    hovering_ = false;
    update();
}
