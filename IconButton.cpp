#include "IconButton.h"
#include "Theme.h"

#include <QPainter>

namespace {
constexpr int kCornerRadius = 6;
// Gap between the icon and the label on a button carrying both.
constexpr int kIconTextGap = 6;
// The accent at low alpha, so a toggled button (mute, playlist, fullscreen)
// tints to match the sliders instead of Windows blue. A function, not a
// constant: the accent can change while the app runs.
QColor checkedColor()
{
    QColor color = Theme::accentEnd();
    color.setAlpha(70);
    return color;
}
const QColor kHoverColor(255, 255, 255, 28);
const QColor kPressedColor(255, 255, 255, 50);
}

IconButton::IconButton(QWidget *parent)
    : QPushButton(parent)
{
    init();
}

IconButton::IconButton(const QIcon &icon, const QString &text, QWidget *parent)
    : QPushButton(icon, text, parent)
{
    init();
}

void IconButton::init()
{
    setFlat(true);
    setCursor(Qt::PointingHandCursor);
    hoverColor_ = kHoverColor;
}

void IconButton::setHoverColor(const QColor &color)
{
    hoverColor_ = color;
    update();
}

void IconButton::paintEvent(QPaintEvent * /*event*/)
{
    // Fully self-painted (icon, text, and highlight) rather than delegating
    // to QPushButton::paintEvent(): Fusion draws its own sunken/checked
    // bevel for flat buttons in those same states, which would double up
    // with the rounded highlight below instead of replacing it.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (isChecked() || isDown() || underMouse()) {
        painter.setPen(Qt::NoPen);
        if (isChecked()) {
            painter.setBrush(checkedColor());
            painter.drawRoundedRect(rect(), kCornerRadius, kCornerRadius);
        }
        if (isDown()) {
            painter.setBrush(kPressedColor);
            painter.drawRoundedRect(rect(), kCornerRadius, kCornerRadius);
        } else if (underMouse()) {
            painter.setBrush(hoverColor_);
            painter.drawRoundedRect(rect(), kCornerRadius, kCornerRadius);
        }
    }

    QRect contentRect = rect();
    if (isDown()) {
        // Nudge content down 1px on press for a tactile "held" feel.
        contentRect.translate(0, 1);
    }

    if (!icon().isNull()) {
        const QIcon::Mode iconMode = isEnabled() ? QIcon::Normal : QIcon::Disabled;
        const QIcon::State iconState = isChecked() ? QIcon::On : QIcon::Off;
        const QSize iSize = iconSize();
        if (text().isEmpty()) {
            const QRect iconRect(contentRect.center() - QPoint(iSize.width() / 2 - 1, iSize.height() / 2 - 1), iSize);
            icon().paint(&painter, iconRect, Qt::AlignCenter, iconMode, iconState);
        } else {
            // Icon and label side by side, the pair centred as a unit -- the
            // title bar's menu button is the only place both are set.
            const int textWidth = fontMetrics().horizontalAdvance(text());
            const int totalWidth = iSize.width() + kIconTextGap + textWidth;
            const int left = contentRect.center().x() - totalWidth / 2;
            const QRect iconRect(left, contentRect.center().y() - iSize.height() / 2,
                                 iSize.width(), iSize.height());
            icon().paint(&painter, iconRect, Qt::AlignCenter, iconMode, iconState);
            painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled,
                                           QPalette::ButtonText));
            painter.drawText(QRect(iconRect.right() + kIconTextGap, contentRect.top(),
                                   textWidth, contentRect.height()),
                             Qt::AlignVCenter | Qt::AlignLeft, text());
        }
    } else if (!text().isEmpty()) {
        painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText));
        painter.drawText(contentRect, Qt::AlignCenter, text());
    }
}
