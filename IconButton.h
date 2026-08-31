#pragma once

#include <QColor>
#include <QPushButton>

/// Transport-bar button with a hand-painted hover/press highlight instead of
/// Fusion's default bevel, which reads as almost flat against this dark
/// palette. Paints its own state for the same reason TimelineSlider and
/// VolumeSlider do: QApplication::setStyleSheet() is off-limits (see
/// Theme.cpp), and per-widget QSS carries the same risk of knocking the
/// QOpenGLWidget video surface out of stacking order.
class IconButton : public QPushButton
{
    Q_OBJECT

public:
    explicit IconButton(QWidget *parent = nullptr);
    IconButton(const QIcon &icon, const QString &text, QWidget *parent = nullptr);

    /// Replaces the default white wash under the pointer. Used by the title
    /// bar's close button, the one button whose hover should warn.
    void setHoverColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void init();

    QColor hoverColor_;
};
