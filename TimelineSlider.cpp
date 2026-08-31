#include "TimelineSlider.h"
#include "Theme.h"

#include <QPainter>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QToolTip>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kTrackHeight = 6;
constexpr int kWidgetHeight = 24;

const QColor kTrackColor(58, 58, 62);
const QColor kHoverMarkerColor(255, 255, 255, 110);
// Bright enough to read against both the empty track and the green fill.
// Kept inside the track rather than standing proud of it: contrast alone is
// enough to pick a marker out, without ticks poking above and below the bar.
const QColor kChapterMarkerColor(235, 235, 235, 200);
constexpr qreal kChapterMarkerWidth = 2.0;
constexpr qreal kChapterMarkerInset = 1.0;
}

TimelineSlider::TimelineSlider(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(kWidgetHeight);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setEnabled(false);
    setCursor(Qt::PointingHandCursor);
}

QSize TimelineSlider::sizeHint() const
{
    return QSize(300, kWidgetHeight);
}

void TimelineSlider::setDurationSeconds(double seconds)
{
    durationSeconds_ = std::max(seconds, 0.0);
    setEnabled(durationSeconds_ > 0.0);
    update();
}

void TimelineSlider::setChapterMarkers(const QVector<double> &startTimes)
{
    chapterMarkers_ = startTimes;
    update();
}

void TimelineSlider::setPositionSeconds(double seconds)
{
    if (dragging_ || durationSeconds_ <= 0.0) {
        return;
    }
    currentValueSeconds_ = std::clamp(seconds, 0.0, durationSeconds_);
    update();
}

void TimelineSlider::updateFromPixelX(qreal x)
{
    const double fraction = std::clamp(x / std::max(1, width()), 0.0, 1.0);
    currentValueSeconds_ = fraction * durationSeconds_;
    update();
    emit seekRequested(currentValueSeconds_);
}

void TimelineSlider::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal y = (height() - kTrackHeight) / 2.0;
    const QRectF trackRect(0, y, width(), kTrackHeight);

    painter.setPen(Qt::NoPen);
    painter.setBrush(kTrackColor);
    painter.drawRect(trackRect);

    const double fraction = durationSeconds_ > 0.0
        ? std::clamp(currentValueSeconds_ / durationSeconds_, 0.0, 1.0)
        : 0.0;
    const qreal progressWidth = fraction * width();

    if (progressWidth > 0.0) {
        const QRectF progressRect(0, y, progressWidth, kTrackHeight);
        QLinearGradient gradient(progressRect.topLeft(), progressRect.topRight());
        // Asked for on each repaint rather than cached in a constant: the
        // accent can be changed from Settings while the app is running.
        gradient.setColorAt(0.0, Theme::accentStart());
        gradient.setColorAt(1.0, Theme::accentEnd());
        painter.setBrush(gradient);
        painter.drawRect(progressRect);
    }

    // Drawn after the progress fill so a marker stays visible on the part of
    // the track that has already played.
    if (durationSeconds_ > 0.0 && !chapterMarkers_.isEmpty()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(kChapterMarkerColor);
        for (double start : chapterMarkers_) {
            if (start <= 0.0 || start >= durationSeconds_) {
                continue; // a marker on either end would just thicken the edge
            }
            const qreal markerX = (start / durationSeconds_) * width();
            painter.drawRect(QRectF(markerX - kChapterMarkerWidth / 2.0,
                                     y + kChapterMarkerInset,
                                     kChapterMarkerWidth,
                                     kTrackHeight - kChapterMarkerInset * 2.0));
        }
    }

    if (hovering_ && isEnabled()) {
        const qreal hoverX = hoverFraction_ * width();
        painter.setPen(QPen(kHoverMarkerColor, 1));
        painter.drawLine(QPointF(hoverX, 2), QPointF(hoverX, height() - 2));
    }
}

void TimelineSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || durationSeconds_ <= 0.0) {
        return;
    }
    dragging_ = true;
    updateFromPixelX(event->position().x());
    event->accept();
}

void TimelineSlider::mouseMoveEvent(QMouseEvent *event)
{
    hovering_ = true;
    hoverFraction_ = std::clamp(event->position().x() / std::max(1, width()), 0.0, 1.0);

    if (dragging_) {
        updateFromPixelX(event->position().x());
    } else {
        update();
    }

    if (durationSeconds_ > 0.0) {
        QToolTip::showText(event->globalPosition().toPoint(),
                            formatTime(hoverFraction_ * durationSeconds_), this);
    }
}

void TimelineSlider::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        dragging_ = false;
        update();
    }
}

void TimelineSlider::leaveEvent(QEvent * /*event*/)
{
    hovering_ = false;
    update();
}

QString TimelineSlider::formatTime(double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0.0) {
        seconds = 0.0;
    }
    const int total = static_cast<int>(seconds);
    const int hours = total / 3600;
    const int minutes = (total % 3600) / 60;
    const int secs = total % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0'));
}
