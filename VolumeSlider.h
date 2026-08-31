#pragma once

#include <QWidget>

/// Custom-painted 0-200 volume bar (100 = unity gain, up to 200 = mpv
/// software-amplified boost), styled to match TimelineSlider (same
/// track/fill look) rather than the platform's native QSlider groove.
class VolumeSlider : public QWidget
{
    Q_OBJECT

public:
    explicit VolumeSlider(QWidget *parent = nullptr);

    /// Sets the displayed value without emitting volumeChangeRequested, so
    /// it can be called from a player-state update without feedback.
    void setVolumeSilently(int volume0to200);

    /// Changes the top of the scale (100-1000). Clamps the current value
    /// down if it now exceeds the new maximum.
    void setMaxVolume(int maxVolume);

    QSize sizeHint() const override;

signals:
    /// Emitted whenever the user clicks or drags to a new volume.
    void volumeChangeRequested(int volume0to200);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void updateFromPixelX(qreal x);
    void setValueInternal(int volume0to200, bool emitSignal);

    int value_ = 100;
    int maxVolume_ = 200;
    bool dragging_ = false;
    bool hovering_ = false;
};
