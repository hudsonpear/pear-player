#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QMainWindow;
class QMenuBar;
class IconButton;

/// Caption row for the frameless main window: the "Pear Player" button, the
/// window title and the minimise/maximise/close buttons, all on the one line
/// the native title bar used to occupy.
///
/// There is no menu bar on screen any more. The one the window still owns is
/// kept alive but hidden, purely as the place the menus and their shortcuts
/// live, and its entries are poured into a popup under the button on each
/// click -- so a rebuild after a language change needs no help from here.
///
/// Dragging and resizing are handed to the window manager through
/// QWindow::startSystemMove()/startSystemResize() rather than moved by hand:
/// that keeps Aero snap, drag-to-restore from maximised and the shake gesture
/// working, none of which a manual "remember the press point and move()" loop
/// can offer.
class TitleBar : public QWidget
{
    Q_OBJECT

public:
    /// Takes menuBar as the source of the popup's entries; it is reparented
    /// here and hidden, never shown as a bar. The caller still owns the window
    /// and is responsible for handing this widget to
    /// QMainWindow::setMenuWidget().
    TitleBar(QMainWindow *window, QMenuBar *menuBar);

    /// Width of the grab band along the window edges. Lives here because the
    /// top edge is this widget's own and the other three are the central
    /// widget's -- both need the same number.
    static constexpr int kResizeBorder = 5;

    /// Swaps the maximise glyph for the restore one and back. Called from
    /// MainWindow::changeEvent, the only place the state actually changes.
    void syncWindowState();

    /// Names the file playing now, centred in the bar and elided when there is
    /// not room for it. Empty leaves the middle of the bar blank.
    ///
    /// Driven by MainWindow rather than read off the window title, which
    /// carries the application name too -- wanted in the task bar and the
    /// Alt-Tab switcher, redundant beside the button that already says it.
    void setMediaTitle(const QString &title);

protected:
    /// Starts a system move, or a top-edge resize when the press lands in the
    /// grab band.
    void mousePressEvent(QMouseEvent *event) override;

    /// Maximises and restores, matching the native title bar it replaces.
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    /// Re-elides the title when the label is resized -- it is centred, so it
    /// cannot simply be clipped at the edge.
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateElidedTitle();

    /// Drops the hidden menu bar's own entries into a popup under the button.
    /// Built per click rather than kept: retranslateUi() deletes and recreates
    /// every menu, so anything held here would dangle after a language change.
    void showMainMenu();

    QMainWindow *window_ = nullptr;
    QMenuBar *menuBar_ = nullptr;
    IconButton *menuButton_ = nullptr;
    QLabel *titleLabel_ = nullptr;
    IconButton *maximizeButton_ = nullptr;
    QString title_;
};
