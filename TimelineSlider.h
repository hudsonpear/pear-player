#pragma once

#include <QWidget>
#include <QVector>

/// Custom-painted seek bar.
///
/// Unlike a QSlider-based bar, position is tracked as a plain double in
/// seconds (no int-quantized range to round-trip through), which makes it
/// exactly as precise as mpv's own time-pos reporting. Click anywhere jumps
/// straight to that point, dragging scrubs smoothly, and hovering previews
/// the target time. External position updates are ignored while the user
/// is actively dragging so they never fight each other.
class TimelineSlider : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineSlider(QWidget *parent = nullptr);

    /// Total length of the current media, in seconds. 0 disables the bar.
    void setDurationSeconds(double seconds);

    /// Reflects the player's current position. Ignored while the user is
    /// dragging the handle.
    void setPositionSeconds(double seconds);

    /// Chapter start times in seconds, drawn as ticks across the track. An
    /// empty list removes them, which is how the setting turns them off.
    void setChapterMarkers(const QVector<double> &startTimes);

    QSize sizeHint() const override;

signals:
    /// Emitted whenever the user clicks or drags to a new position.
    void seekRequested(double seconds);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void updateFromPixelX(qreal x);
    static QString formatTime(double seconds);

    QVector<double> chapterMarkers_;
    double durationSeconds_ = 0.0;
    double currentValueSeconds_ = 0.0;
    double hoverFraction_ = 0.0;
    bool dragging_ = false;
    bool hovering_ = false;
};
