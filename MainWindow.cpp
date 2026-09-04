#include "MainWindow.h"
#include "MpvPlayer.h"
#include "VideoWidget.h"
#include "TimelineSlider.h"
#include "VolumeSlider.h"
#include "IconButton.h"
#include "SettingsDialog.h"
#include "SettingsKeys.h"
#include "PlaylistWidget.h"
#include "TaskbarProgress.h"
#include "Downloader.h"
#include "SubtitleStyle.h"
#include "Translation.h"
#include "AboutDialog.h"
#include "MediaInfoDialog.h"
#include "EqualizerDialog.h"
#include "Equalizer.h"
#include "VideoSettings.h"
#include "FileMemory.h"
#include "TitleBar.h"

#include <QMenuBar>
#include <QMenu>
#include <QActionGroup>
#include <QToolBar>
#include <QAbstractButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QInputDialog>
#include <QDialog>
#include <QLineEdit>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QProcess>
#include <QLocale>
#include <QClipboard>
#include <QUrl>
#include <QMessageBox>
#include <QDockWidget>
#include <QListWidget>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFile>
#include <QTextStream>
#include <QRandomGenerator>
#include <QGridLayout>
#include <QAbstractItemView>
#include <QStyle>
#include <QShortcut>
#include <QApplication>
#include <QKeySequence>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPolygonF>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <functional>

#ifdef Q_OS_WIN
// Last, and narrowed: windows.h defines min/max macros that break std:: and Qt
// headers included after it.
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
constexpr int kDefaultVolume = 100;
constexpr double kDefaultSpeed = 1.0;
// The Playback > Speed entries, in menu order.
constexpr double kSpeedChoices[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
// Range the X and C keys can reach. Below 0.1 mpv's audio filter gives up, and
// above 4x speech is unintelligible anyway.
constexpr double kMinSpeed = 0.1;
constexpr double kMaxSpeed = 4.0;

// Buttons keep the English text of their tooltip in a property, so switching
// language can find and re-translate every one of them without the window
// having to hold a pointer to each. Most of these buttons are created inside
// a lambda and never stored anywhere.
constexpr char kSourceToolTip[] = "sourceToolTip";

void setTranslatableToolTip(QWidget *widget, const char *source)
{
    widget->setProperty(kSourceToolTip, QString::fromUtf8(source));
    widget->setToolTip(QCoreApplication::translate("MainWindow", source));
}

// Fullscreen control bar: how close to the bottom edge the pointer has to come
// before the bar appears, and how long it stays after the pointer leaves.
constexpr int kFullscreenHoverZone = 90;
constexpr int kFullscreenControlsHideMs = 1600;

// Which window edges a point in the central widget is close enough to grab.
// Only three of them: the top edge belongs to the title bar, which handles
// its own.
Qt::Edges resizeEdgesAt(const QPoint &pos, const QSize &size)
{
    Qt::Edges edges;
    if (pos.x() <= TitleBar::kResizeBorder) {
        edges |= Qt::LeftEdge;
    } else if (pos.x() >= size.width() - TitleBar::kResizeBorder) {
        edges |= Qt::RightEdge;
    }
    if (pos.y() >= size.height() - TitleBar::kResizeBorder) {
        edges |= Qt::BottomEdge;
    }
    return edges;
}

Qt::CursorShape cursorForEdges(Qt::Edges edges)
{
    if (edges == (Qt::LeftEdge | Qt::BottomEdge) || edges == (Qt::RightEdge | Qt::TopEdge)) {
        return Qt::SizeBDiagCursor;
    }
    if (edges == (Qt::RightEdge | Qt::BottomEdge) || edges == (Qt::LeftEdge | Qt::TopEdge)) {
        return Qt::SizeFDiagCursor;
    }
    if (edges & (Qt::LeftEdge | Qt::RightEdge)) {
        return Qt::SizeHorCursor;
    }
    if (edges & (Qt::TopEdge | Qt::BottomEdge)) {
        return Qt::SizeVerCursor;
    }
    return Qt::ArrowCursor;
}
constexpr int kIconCanvas = 24;

// Stills mpv can display like any other file. Kept separate from the audio
// and video list so a slideshow can tell whether what is open is an image.
// Playlist button metrics. The dock's opening width is computed from these so
// it always lands on exactly the row of buttons, whatever their size.
constexpr int kPlaylistButtonWidth = 30;
constexpr int kPlaylistButtonSpacing = 3;
constexpr int kPlaylistMargin = 2;
constexpr int kPlaylistButtonColumns = 5;

const QStringList kImageSuffixes = {
    QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
    QStringLiteral("webp"), QStringLiteral("gif"),
};

// Containers mpv's ffmpeg build reads. The list is the single source for the
// Open dialog's filters, the folder scan and the playlist, so a format added
// here shows up everywhere at once -- .3gp and .ts were missing from the file
// associations precisely because those lists were kept separately.
const QStringList kVideoSuffixes = {
    QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
    QStringLiteral("mov"), QStringLiteral("webm"), QStringLiteral("flv"),
    QStringLiteral("wmv"), QStringLiteral("m4v"), QStringLiteral("mpg"),
    QStringLiteral("mpeg"), QStringLiteral("m2v"), QStringLiteral("ts"),
    QStringLiteral("m2ts"), QStringLiteral("mts"), QStringLiteral("vob"),
    QStringLiteral("3gp"), QStringLiteral("3gpp"), QStringLiteral("3g2"),
    QStringLiteral("ogv"),
};

const QStringList kAudioSuffixes = {
    QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"),
    QStringLiteral("aac"), QStringLiteral("ogg"), QStringLiteral("m4a"),
    QStringLiteral("opus"), QStringLiteral("wma"), QStringLiteral("mka"),
    QStringLiteral("aiff"), QStringLiteral("aif"), QStringLiteral("ac3"), QStringLiteral("dts"),
    QStringLiteral("thd"), QStringLiteral("amr"), QStringLiteral("mid"),
    QStringLiteral("fla"), QStringLiteral("oga"), QStringLiteral("au"),
};

// Deliberately not part of kPlayableSuffixes below: a playlist is not media,
// it names media. Folder scans and playlist rows have to stay clear of these,
// or a queue ends up holding an entry that cannot be played.
const QStringList kPlaylistSuffixes = {
    QStringLiteral("m3u"), QStringLiteral("m3u8"),
};

const QStringList kPlayableSuffixes = kVideoSuffixes + kAudioSuffixes + kImageSuffixes;

/// "*.mp4 *.mkv ..." for one group of suffixes.
QString wildcardsFor(const QStringList &suffixes)
{
    QStringList wildcards;
    wildcards.reserve(suffixes.size());
    for (const QString &suffix : suffixes) {
        wildcards << QStringLiteral("*.") + suffix;
    }
    return wildcards.join(QLatin1Char(' '));
}

QStringList playableNameFilters()
{
    QStringList filters;
    for (const QString &suffix : kPlayableSuffixes) {
        filters << QStringLiteral("*.") + suffix;
    }
    return filters;
}

bool isPlayableFile(const QFileInfo &info)
{
    return kPlayableSuffixes.contains(info.suffix(), Qt::CaseInsensitive);
}

bool isPlaylistFile(const QFileInfo &info)
{
    return kPlaylistSuffixes.contains(info.suffix(), Qt::CaseInsensitive);
}

/// The files an .m3u/.m3u8 names, as absolute paths. Sets ok to false when the
/// playlist itself could not be read, which is different from a playlist that
/// was read and turned out to be empty.
///
/// Deliberately not strict about the format: an .m3u8 is a plain .m3u that
/// happens to be UTF-8, and QTextStream reads UTF-8 either way.
QStringList readPlaylistEntries(const QString &path, bool *ok = nullptr)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (ok) {
            *ok = false;
        }
        return {};
    }
    if (ok) {
        *ok = true;
    }

    const QDir base = QFileInfo(path).absoluteDir();
    QStringList entries;
    QTextStream in(&file);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue; // blank lines and #EXTM3U / #EXTINF directives
        }
        // A remote entry is kept as it stands -- mpv opens a URL as readily as
        // a file. Everything else may be relative to the playlist's own folder.
        if (!QUrl(line).scheme().isEmpty() && !QDir::isAbsolutePath(line)) {
            entries << line;
        } else {
            entries << (QDir::isAbsolutePath(line) ? line : base.filePath(line));
        }
    }
    return entries;
}

/// paths with every playlist file replaced by the entries it names, so the
/// rest of the app only ever deals in things that can actually be played.
QStringList expandPlaylists(const QStringList &paths)
{
    QStringList expanded;
    for (const QString &path : paths) {
        if (isPlaylistFile(QFileInfo(path))) {
            expanded += readPlaylistEntries(path);
        } else {
            expanded << path;
        }
    }
    return expanded;
}

// Hand-drawn vector icons instead of QStyle::standardIcon() (dark/black
// regardless of palette) or Unicode glyphs (rendered as full-color emoji by
// Windows font fallback, not plain text). Solid white shapes on a
// transparent canvas: always crisp, always the right color.
QIcon makeIcon(const std::function<void(QPainter &)> &draw)
{
    QPixmap pixmap(kIconCanvas, kIconCanvas);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    draw(painter);
    return QIcon(pixmap);
}

QIcon playIcon()
{
    return makeIcon([](QPainter &p) {
        p.drawPolygon(QPolygonF({QPointF(7, 5), QPointF(7, 19), QPointF(19, 12)}));
    });
}

QIcon pauseIcon()
{
    return makeIcon([](QPainter &p) {
        p.drawRect(QRectF(6, 5, 4, 14));
        p.drawRect(QRectF(14, 5, 4, 14));
    });
}

QIcon stopIcon()
{
    return makeIcon([](QPainter &p) {
        p.drawRect(QRectF(6, 6, 12, 12));
    });
}

QIcon previousIcon()
{
    return makeIcon([](QPainter &p) {
        p.drawRect(QRectF(5, 5, 3, 14));
        p.drawPolygon(QPolygonF({QPointF(19, 5), QPointF(19, 19), QPointF(9, 12)}));
    });
}

QIcon nextIcon()
{
    return makeIcon([](QPainter &p) {
        p.drawPolygon(QPolygonF({QPointF(5, 5), QPointF(5, 19), QPointF(15, 12)}));
        p.drawRect(QRectF(16, 5, 3, 14));
    });
}

// --- Playlist button icons ---------------------------------------------
// Same solid-white vector style as the transport icons above, so the dock
// matches the control bar.

QIcon addIcon()
{
    return makeIcon([](QPainter &p) {
        p.drawRect(QRectF(10.5, 5, 3, 14));
        p.drawRect(QRectF(5, 10.5, 14, 3));
    });
}

QIcon addFolderIcon()
{
    return makeIcon([](QPainter &p) {
        // Folder: a small tab over a body, with a plus cut into the corner so
        // it reads differently from the plain Add button at a glance.
        p.drawRect(QRectF(3, 6, 8, 2.5));
        p.drawRect(QRectF(3, 8, 14, 11));
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.drawRect(QRectF(14.5, 10.5, 8, 3));
        p.drawRect(QRectF(17, 8, 3, 8));
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.drawRect(QRectF(15.5, 11.25, 6.5, 1.5));
        p.drawRect(QRectF(18, 8.75, 1.5, 6.5));
    });
}

QIcon removeIcon()
{
    return makeIcon([](QPainter &p) {
        p.drawRect(QRectF(5, 10.5, 14, 3));
    });
}

QIcon clearIcon()
{
    return makeIcon([](QPainter &p) {
        // Waste basket: lid, handle, tapered body with two slots.
        p.drawRect(QRectF(5, 7, 14, 2));
        p.drawRect(QRectF(10, 4.5, 4, 2));
        p.drawPolygon(QPolygonF({QPointF(6.5, 10), QPointF(17.5, 10),
                                  QPointF(16, 20), QPointF(8, 20)}));
        p.setBrush(Qt::transparent);
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.drawRect(QRectF(10, 12, 1.5, 6));
        p.drawRect(QRectF(13, 12, 1.5, 6));
    });
}

QIcon shuffleIcon()
{
    return makeIcon([](QPainter &p) {
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(Qt::NoBrush);
        // Two paths crossing over, each ending in an arrowhead.
        p.drawLine(QPointF(4, 7), QPointF(9, 7));
        p.drawLine(QPointF(9, 7), QPointF(16, 17));
        p.drawLine(QPointF(4, 17), QPointF(9, 17));
        p.drawLine(QPointF(9, 17), QPointF(16, 7));
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawPolygon(QPolygonF({QPointF(15, 4), QPointF(20, 7), QPointF(15, 10)}));
        p.drawPolygon(QPolygonF({QPointF(15, 14), QPointF(20, 17), QPointF(15, 20)}));
    });
}

QIcon repeatIcon()
{
    return makeIcon([](QPainter &p) {
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(Qt::NoBrush);
        // Open rounded loop, with the gap closed by an arrowhead.
        p.drawLine(QPointF(7, 6), QPointF(17, 6));
        p.drawLine(QPointF(18, 7), QPointF(18, 15));
        p.drawLine(QPointF(17, 18), QPointF(8, 18));
        p.drawLine(QPointF(6, 17), QPointF(6, 9));
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawPolygon(QPolygonF({QPointF(15, 3), QPointF(20, 6), QPointF(15, 9)}));
    });
}

QIcon saveIcon()
{
    return makeIcon([](QPainter &p) {
        // Arrow pointing down into a tray.
        p.drawRect(QRectF(10.5, 4, 3, 8));
        p.drawPolygon(QPolygonF({QPointF(7.5, 11), QPointF(16.5, 11), QPointF(12, 16)}));
        p.drawRect(QRectF(5, 18, 14, 2));
    });
}

QIcon loadIcon()
{
    return makeIcon([](QPainter &p) {
        // The same tray, with the arrow coming back out of it.
        p.drawPolygon(QPolygonF({QPointF(7.5, 9), QPointF(16.5, 9), QPointF(12, 4)}));
        p.drawRect(QRectF(10.5, 8, 3, 8));
        p.drawRect(QRectF(5, 18, 14, 2));
    });
}

QIcon volumeIcon(bool muted)
{
    return makeIcon([muted](QPainter &p) {
        p.drawPolygon(QPolygonF({QPointF(4, 9), QPointF(8, 9), QPointF(13, 5),
                                  QPointF(13, 19), QPointF(8, 15), QPointF(4, 15)}));
        p.setPen(QPen(Qt::white, 1.6));
        p.setBrush(Qt::NoBrush);
        if (muted) {
            p.drawLine(QPointF(16, 8), QPointF(21, 16));
            p.drawLine(QPointF(21, 8), QPointF(16, 16));
        } else {
            p.drawArc(QRectF(14, 7, 8, 10), -50 * 16, 100 * 16);
            p.drawArc(QRectF(16, 4, 12, 16), -50 * 16, 100 * 16);
        }
    });
}

QIcon fullscreenIcon()
{
    return makeIcon([](QPainter &p) {
        p.setPen(QPen(Qt::white, 2));
        p.setBrush(Qt::NoBrush);
        // Four corner brackets, drawn as pairs of short strokes pointing
        // outward from each corner.
        p.drawLine(QPointF(5, 9), QPointF(5, 5));
        p.drawLine(QPointF(5, 5), QPointF(9, 5));

        p.drawLine(QPointF(15, 5), QPointF(19, 5));
        p.drawLine(QPointF(19, 5), QPointF(19, 9));

        p.drawLine(QPointF(5, 15), QPointF(5, 19));
        p.drawLine(QPointF(5, 19), QPointF(9, 19));

        p.drawLine(QPointF(19, 15), QPointF(19, 19));
        p.drawLine(QPointF(19, 19), QPointF(15, 19));
    });
}

QIcon playlistIcon()
{
    return makeIcon([](QPainter &p) {
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        // Three list rows: a bullet square followed by a bar.
        for (int i = 0; i < 3; ++i) {
            const qreal y = 6 + i * 5;
            p.drawRect(QRectF(4, y, 3, 3));
            p.drawRect(QRectF(9, y + 0.75, 11, 1.5));
        }
    });
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , player_(std::make_unique<MpvPlayer>())
{
    setWindowTitle(tr("Pear Player"));
    setAcceptDrops(true);

    // No native caption: TitleBar takes its place further down, in the menu
    // bar's slot. Set before the window is shown, while changing flags is
    // still free of a hide/show cycle.
    setWindowFlag(Qt::FramelessWindowHint);

    // Delete offers to bin whatever is playing. Application context so it
    // still fires in fullscreen, where the menu bar is hidden.
    auto *deleteAction = new QAction(tr("Delete File..."), this);
    deleteAction->setShortcut(QKeySequence(Qt::Key_Delete));
    deleteAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteCurrentFile);
    addAction(deleteAction);

    // Size relative to the actual screen instead of a fixed constant, so
    // the window never opens larger than the display it's on.
    const QRect available = QGuiApplication::primaryScreen()->availableGeometry();
    const QSize initialSize(std::min(1024, static_cast<int>(available.width() * 0.85)),
                             std::min(720, static_cast<int>(available.height() * 0.85)));
    resize(initialSize);

    const QSettings startupSettings;
    // A remembered geometry carries both size and position, so it stands in
    // for the size above as well as the choice below.
    const bool rememberGeometry = startupSettings.value(SettingsKeys::kRememberGeometry,
                                                         SettingsKeys::kDefaultRememberGeometry).toBool();
    const QByteArray storedGeometry = rememberGeometry
        ? startupSettings.value(SettingsKeys::kWindowGeometry).toByteArray()
        : QByteArray();

    // restoreGeometry() refuses a blob it cannot read, and answers false --
    // which covers a first run, a corrupted value, and a screen that has since
    // been unplugged. Falling through to the startup position keeps the window
    // somewhere visible in all three cases.
    if (storedGeometry.isEmpty() || !restoreGeometry(storedGeometry)) {
        const QString startupPosition = startupSettings.value(SettingsKeys::kStartupPosition, QStringLiteral("center")).toString();
        if (startupPosition == QStringLiteral("topleft")) {
            move(available.topLeft());
        } else {
            move(available.center() - QPoint(initialSize.width() / 2, initialSize.height() / 2));
        }
    }

    videoWidget_ = new VideoWidget(this);
    timelineSlider_ = new TimelineSlider(this);
    volumeSlider_ = new VolumeSlider(this);

    videoWidget_->setPlayer(player_.get());
    taskbarProgress_ = std::make_unique<TaskbarProgress>(this);

    // Built here rather than left to QMainWindow::menuBar(), which would
    // install it in the slot the title bar is about to take.
    menuBar_ = new QMenuBar(this);

    createMenus();
    createControlsBar();
    createPlaylistPanel();

    titleBar_ = new TitleBar(this, menuBar_);
    setMenuWidget(titleBar_);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(videoWidget_, /*stretch=*/1);

    // Timeline and buttons share one container so fullscreen can float the
    // pair over the video as a single overlay. Docked below it otherwise.
    controlsContainer_ = new QWidget(central);
    // Painted rather than left transparent: docked it makes no difference, but
    // floating over the picture the buttons and timeline would otherwise sit
    // on the video itself, unreadable against a bright frame.
    controlsContainer_->setAutoFillBackground(true);
    auto *containerLayout = new QVBoxLayout(controlsContainer_);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    // The timeline gets its own row so it can be inset without insetting the
    // video, which stays edge to edge. 6px matches the control bar's side
    // margins, so the track lines up with the buttons underneath it.
    auto *timelineRow = new QHBoxLayout();
    timelineRow->setContentsMargins(6, 0, 6, 0);
    timelineRow->addWidget(timelineSlider_);
    containerLayout->addLayout(timelineRow);
    containerLayout->addWidget(controlsBar_);

    centralLayout_ = layout;
    layout->addWidget(controlsContainer_);
    setCentralWidget(central);

    // Frameless windows have no border to grab, so one is left around the
    // central widget's contents. The band is the central widget's own surface:
    // no child covers it, so the presses land here and can be handed to the
    // window manager.
    central_ = central;
    central_->setMouseTracking(true);
    central_->installEventFilter(this);
    updateResizeBorder();

    // Hides the overlay again once the pointer has been still, away from the
    // bar, for a moment.
    fullscreenControlsTimer_ = new QTimer(this);
    fullscreenControlsTimer_->setSingleShot(true);
    fullscreenControlsTimer_->setInterval(kFullscreenControlsHideMs);
    connect(fullscreenControlsTimer_, &QTimer::timeout, this, [this] {
        // Not while the pointer is on the bar itself -- reaching for a button
        // is exactly when it must not disappear.
        if (isFullScreen() && !controlsContainer_->underMouse()) {
            controlsContainer_->hide();
        }
    });
    // Moving onto the bar keeps it up; leaving it starts the countdown again.
    controlsContainer_->installEventFilter(this);

    const QSettings hwDecSettings;
    const bool hwDecEnabled = hwDecSettings.value(SettingsKeys::kHwDecEnabled, SettingsKeys::kDefaultHwDecEnabled).toBool();
    player_->initialize(hwDecEnabled);
    createConnections();
    loadSettings();

    refreshTimeLabel();
    videoWidget_->setFocus();
}

MainWindow::~MainWindow()
{
    // The mpv render context must be freed while videoWidget_'s GL context
    // is still current, and before player_'s own destructor runs (member
    // destruction below would otherwise free it with no GL context current
    // at all, since Qt only destroys videoWidget_ afterwards as part of the
    // base QWidget teardown).
    if (videoWidget_) {
        videoWidget_->releasePlayerRender();
    }
}

void MainWindow::createMenus()
{
    fileMenu_ = menuBar_->addMenu(tr("&File"));
    openAction_ = fileMenu_->addAction(tr("&Open..."), this, &MainWindow::openFile);
    openAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    openFolderAction_ = fileMenu_->addAction(tr("Open &Folder..."), this, &MainWindow::openFolder);
    openUrlAction_ = fileMenu_->addAction(tr("Open &URL..."), this, &MainWindow::openUrl);
    openUrlAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_U));

    recentFilesMenu_ = fileMenu_->addMenu(tr("&Recent Files"));
    // Off by default in QMenu, and the full path only lives in the tooltip.
    recentFilesMenu_->setToolTipsVisible(true);
    // Rebuilt on each popup: files come and go, and entries whose file no
    // longer exists are worth showing as unavailable rather than silently
    // failing when picked.
    connect(recentFilesMenu_, &QMenu::aboutToShow, this, &MainWindow::refreshRecentFilesMenu);

    fileMenu_->addSeparator();
    copyFileAction_ = fileMenu_->addAction(tr("&Copy File to..."), this, &MainWindow::copyFileTo);
    openFolderOfFileAction_ = fileMenu_->addAction(tr("Open Containing Folde&r"), this,
                                                    &MainWindow::openContainingFolder);
    // Only meaningful for something on disk, so their state is settled as the
    // menu opens rather than left stale.
    connect(fileMenu_, &QMenu::aboutToShow, this, [this] {
        const bool onDisk = !currentMediaPath_.isEmpty() && QFileInfo(currentMediaPath_).isFile();
        copyFileAction_->setEnabled(onDisk);
        openFolderOfFileAction_->setEnabled(onDisk);
    });

    fileMenu_->addSeparator();
    downloadUrlAction_ = fileMenu_->addAction(tr("&Download from URL..."), this, &MainWindow::downloadFromUrl);
    downloadUrlAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    downloadCurrentAction_ = fileMenu_->addAction(tr("Download &Current Media..."), this,
                                                  &MainWindow::downloadCurrentMedia);
    // Only meaningful while something from the network is loaded, and that
    // changes as files are opened, so it is settled each time the menu opens.
    connect(fileMenu_, &QMenu::aboutToShow, this, [this] {
        downloadCurrentAction_->setEnabled(currentMediaIsNetworkUrl());
    });
    fileMenu_->addSeparator();
    exitAction_ = fileMenu_->addAction(tr("E&xit"), this, &QWidget::close);
    exitAction_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Q));

    playbackMenu_ = menuBar_->addMenu(tr("&Playback"));
    playbackMenu_->addAction(tr("&Play"), this, [this] { player_->play(); });
    playbackMenu_->addAction(tr("Pa&use"), this, [this] { player_->pause(); });
    playbackMenu_->addAction(tr("&Stop"), this, [this] { stopPlayback(); });
    playbackMenu_->addSeparator();
    // Frame stepping: mpv pauses and advances exactly one frame.
    playbackMenu_->addAction(tr("Previous Frame"), this, &MainWindow::frameStepBackward)
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Left));
    playbackMenu_->addAction(tr("Next Frame"), this, &MainWindow::frameStepForward)
        ->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right));
    playbackMenu_->addSeparator();

    // Emptied first: createMenus() runs again on a language change, and these
    // lists would otherwise keep the actions from the menus just deleted --
    // dangling pointers that applyVideoState() and onSpeedChanged() walk.
    speedActions_.clear();

    QMenu *speedMenu = playbackMenu_->addMenu(tr("Spee&d"));
    // The "\tZ" and friends only draw the key beside the entry -- the keys
    // themselves are handled in VideoWidget::keyPressEvent(). Binding them
    // here as well would leave two claimants on one key.
    speedMenu->addAction(tr("Normal Speed\tZ"), this, [this] { player_->setSpeed(kDefaultSpeed); });
    speedMenu->addAction(tr("Up Speed\tC"), this, [this] { onSpeedStepRequested(0.1); });
    speedMenu->addAction(tr("Down Speed\tX"), this, [this] { onSpeedStepRequested(-0.1); });
    speedMenu->addSeparator();

    auto *speedGroup = new QActionGroup(speedMenu);
    speedGroup->setExclusive(true);
    // The speed rides along as action data rather than being parsed back out
    // of the label: formatting does not round-trip ("1.0x" formats as "1x"),
    // so matching on the number is what keeps the ticks in step with mpv.
    for (double speed : kSpeedChoices) {
        QAction *action = speedMenu->addAction(QStringLiteral("%1x").arg(speed, 0, 'g', 3));
        action->setCheckable(true);
        action->setData(speed);
        speedGroup->addAction(action);
        speedActions_.append(action);
        connect(action, &QAction::triggered, this, [this, speed] { player_->setSpeed(speed); });
    }
    playbackMenu_->addSeparator();

    QMenu *loopMenu = playbackMenu_->addMenu(tr("&Loop"));
    auto *loopGroup = new QActionGroup(loopMenu);
    loopGroup->setExclusive(true);
    loopOffAction_ = loopMenu->addAction(tr("Loop Off"), this, [this] { setLoopMode(LoopMode::Off); });
    loopFileAction_ = loopMenu->addAction(tr("Loop File"), this, [this] { setLoopMode(LoopMode::File); });
    loopPlaylistAction_ = loopMenu->addAction(tr("Loop Playlist"), this, [this] { setLoopMode(LoopMode::Playlist); });
    for (QAction *action : {loopOffAction_, loopFileAction_, loopPlaylistAction_}) {
        action->setCheckable(true);
        loopGroup->addAction(action);
    }
    loopOffAction_->setChecked(true);

    chaptersMenu_ = playbackMenu_->addMenu(tr("&Chapters"));
    connect(chaptersMenu_, &QMenu::aboutToShow, this, &MainWindow::refreshChaptersMenu);
    playbackMenu_->addSeparator();
    audioMenu_ = playbackMenu_->addMenu(tr("&Audio Track"));
    playbackMenu_->addSeparator();
    equalizerAction_ = playbackMenu_->addAction(tr("&Equalizer..."), this,
                                                &MainWindow::openEqualizerDialog);
    connect(audioMenu_, &QMenu::aboutToShow, this, [this] { populateAudioMenu(audioMenu_); });

    createSubtitlesMenu();
    createVideoMenu();
    createAudioMenu();

    settingsAction_ = menuBar_->addAction(tr("&Settings"), this, &MainWindow::openSettingsDialog);
    aboutAction_ = menuBar_->addAction(tr("&About"), this, &MainWindow::openAboutDialog);
}

void MainWindow::createControlsBar()
{
    controlsBar_ = new QWidget(this);
    auto *layout = new QHBoxLayout(controlsBar_);
    // 3px shifted off the top margin onto the bottom one: the row sits higher
    // while the bar keeps the same overall height.
    layout->setContentsMargins(6, 1, 6, 7);

    // The tip arrives as English source text, not a tr() result, so it can be
    // kept for retranslation later.
    const auto makeButton = [this](const QIcon &icon, const char *tip) {
        auto *button = new IconButton(icon, QString(), controlsBar_);
        setTranslatableToolTip(button, tip);
        button->setFocusPolicy(Qt::NoFocus);
        button->setFixedWidth(36);
        button->setIconSize(QSize(18, 18));
        return button;
    };

    playPauseButton_ = makeButton(playIcon(), QT_TR_NOOP("Play"));
    stopButton_ = makeButton(stopIcon(), QT_TR_NOOP("Stop"));
    prevButton_ = makeButton(previousIcon(), QT_TR_NOOP("Previous"));
    nextButton_ = makeButton(nextIcon(), QT_TR_NOOP("Next"));

    timeLabel_ = new QLabel(controlsBar_);
    // The fixed minimum stops the row shifting as the time text changes
    // length; left-aligned inside it so the text sits against the window edge
    // rather than floating in the middle of a 160px block.
    timeLabel_->setMinimumWidth(160);
    timeLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    // Nudged clear of the window edge without moving anything else in the row.
    timeLabel_->setContentsMargins(3, 0, 0, 0);

    muteButton_ = new IconButton(volumeIcon(/*muted=*/false), QString(), controlsBar_);
    muteButton_->setIconSize(QSize(18, 18));
    muteButton_->setCheckable(true);
    setTranslatableToolTip(muteButton_, QT_TR_NOOP("Mute"));
    muteButton_->setFocusPolicy(Qt::NoFocus);

    fullscreenButton_ = new IconButton(fullscreenIcon(), QString(), controlsBar_);
    fullscreenButton_->setIconSize(QSize(18, 18));
    fullscreenButton_->setCheckable(true);
    setTranslatableToolTip(fullscreenButton_, QT_TR_NOOP("Fullscreen"));
    fullscreenButton_->setFocusPolicy(Qt::NoFocus);

    playlistButton_ = new IconButton(playlistIcon(), QString(), controlsBar_);
    playlistButton_->setIconSize(QSize(18, 18));
    playlistButton_->setCheckable(true);
    setTranslatableToolTip(playlistButton_, QT_TR_NOOP("Playlist"));
    playlistButton_->setFocusPolicy(Qt::NoFocus);

    volumeSlider_->setFixedWidth(120);
    volumeSlider_->setFocusPolicy(Qt::NoFocus);

    layout->addWidget(timeLabel_);
    layout->addStretch(1);
    layout->addWidget(prevButton_);
    layout->addWidget(stopButton_);
    layout->addWidget(playPauseButton_);
    layout->addWidget(nextButton_);
    layout->addStretch(1);
    layout->addWidget(volumeSlider_);
    layout->addWidget(muteButton_);
    layout->addWidget(fullscreenButton_);
    layout->addWidget(playlistButton_);
}

void MainWindow::createPlaylistPanel()
{
    playlistDock_ = new QDockWidget(tr("Playlist"), this);
    playlistDock_->setObjectName(QStringLiteral("PlaylistDock"));
    playlistDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    // The dock holds a container now: the list on top, its buttons underneath.
    auto *container = new QWidget(playlistDock_);
    auto *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(kPlaylistMargin, kPlaylistMargin,
                                         kPlaylistMargin, kPlaylistMargin);
    containerLayout->setSpacing(4);

    playlistWidget_ = new PlaylistWidget(container);
    // Removing several at once is the normal case when tidying a list.
    playlistWidget_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    containerLayout->addWidget(playlistWidget_, 1);

    // Two rows of short buttons: ten controls in one row would force the dock
    // wider than the video.
    auto *buttonGrid = new QGridLayout();
    buttonGrid->setSpacing(kPlaylistButtonSpacing);

    // Icons rather than labels: nine words would not fit a narrow dock, and
    // the tooltip carries the name. IconButton is what the control bar uses,
    // so hover and checked states look the same throughout.
    const auto addButton = [this, container, buttonGrid](int row, int column, const QIcon &icon,
                                                          const QString &tip, void (MainWindow::*slot)()) {
        auto *button = new IconButton(icon, QString(), container);
        button->setToolTip(tip);
        button->setFocusPolicy(Qt::NoFocus);
        button->setIconSize(QSize(16, 16));
        button->setFixedSize(kPlaylistButtonWidth, 26);
        connect(button, &QPushButton::clicked, this, slot);
        buttonGrid->addWidget(button, row, column);
        return button;
    };

    addButton(0, 0, addIcon(), QT_TR_NOOP("Add files to the playlist"), &MainWindow::playlistAddFiles);
    addButton(0, 1, addFolderIcon(), QT_TR_NOOP("Add a folder to the playlist"), &MainWindow::playlistAddFolder);
    addButton(0, 2, removeIcon(), QT_TR_NOOP("Remove the selected entries"), &MainWindow::playlistRemoveSelected);
    addButton(0, 3, clearIcon(), QT_TR_NOOP("Empty the playlist"), &MainWindow::playlistClear);
    addButton(0, 4, shuffleIcon(), QT_TR_NOOP("Shuffle the order"), &MainWindow::playlistShuffle);

    playlistRepeatButton_ = addButton(1, 0, repeatIcon(), QT_TR_NOOP("Repeat the playlist"),
                                       &MainWindow::playlistToggleRepeat);
    playlistRepeatButton_->setCheckable(true);
    addButton(1, 1, previousIcon(), QT_TR_NOOP("Play the previous entry"), &MainWindow::playlistPlayPrevious);
    addButton(1, 2, nextIcon(), QT_TR_NOOP("Play the next entry"), &MainWindow::playlistPlayNext);
    addButton(1, 3, saveIcon(), QT_TR_NOOP("Save the playlist to a file"), &MainWindow::playlistSave);
    addButton(1, 4, loadIcon(), QT_TR_NOOP("Load a playlist from a file"), &MainWindow::playlistLoad);

    // Keeps the buttons left-aligned as the dock widens.
    buttonGrid->setColumnStretch(5, 1);

    containerLayout->addLayout(buttonGrid);
    playlistDock_->setWidget(container);

    addDockWidget(Qt::RightDockWidgetArea, playlistDock_);
    playlistDock_->setVisible(false);

    // A QListWidget asks for enough width to show long filenames, which made
    // the dock open far wider than it needs to be and squeezed the video. It
    // opens on exactly the button row instead -- the narrowest width at which
    // nothing is clipped -- and can still be dragged wider.
    const int buttonRowWidth = kPlaylistButtonColumns * kPlaylistButtonWidth
        + (kPlaylistButtonColumns - 1) * kPlaylistButtonSpacing
        + 2 * kPlaylistMargin;

    playlistWidget_->setMinimumWidth(buttonRowWidth);
    // Just enough over the button row for the dock's own frame. The list
    // scrolls horizontally for long names rather than the dock being widened
    // to accommodate them.
    resizeDocks({playlistDock_}, {buttonRowWidth + 8}, Qt::Horizontal);
}

void MainWindow::createConnections()
{
    // Control bar -> player
    connect(playPauseButton_, &QPushButton::clicked, this, [this] { player_->togglePause(); });
    connect(stopButton_, &QPushButton::clicked, this, [this] { stopPlayback(); });
    connect(player_.get(), &MpvPlayer::playbackFinished, this, [this] {
        taskbarActive_ = false;
        taskbarProgress_->clear();

        // Move to the next entry when there is one. This is what turns a
        // playlist into continuous playback, and is also what advances a
        // slideshow once each image's display time runs out.
        const bool autoAdvance = QSettings()
                                     .value(SettingsKeys::kAutoAdvance,
                                            SettingsKeys::kDefaultAutoAdvance)
                                     .toBool();
        if (autoAdvance && playlistWidget_->count() > 1 && !player_->isLoopingFile()) {
            const int current = playlistWidget_->currentRow();
            const bool atEnd = current >= playlistWidget_->count() - 1;
            if (!atEnd || player_->isLoopingPlaylist()) {
                playlistPlayNext();
            }
        }
    });
    // The window's playlist, not mpv's: mpv is only ever told to load the one
    // file playing now, so its own playlist has a single entry and asking it
    // to step through that does nothing at all. Same slots the playlist
    // panel's buttons, the Playback menu and auto-advance use.
    connect(prevButton_, &QPushButton::clicked, this, &MainWindow::playlistPlayPrevious);
    connect(nextButton_, &QPushButton::clicked, this, &MainWindow::playlistPlayNext);
    connect(muteButton_, &QAbstractButton::toggled, this, [this](bool checked) { player_->setMute(checked); });
    connect(fullscreenButton_, &QAbstractButton::toggled, this, &MainWindow::toggleFullscreen);

    connect(playlistButton_, &QAbstractButton::toggled, playlistDock_, &QWidget::setVisible);
    connect(playlistDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        const QSignalBlocker blocker(playlistButton_);
        playlistButton_->setChecked(visible);
    });
    connect(playlistWidget_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        loadFile(item->data(Qt::UserRole).toString());
    });
    connect(playlistWidget_, &PlaylistWidget::filesDropped, this, &MainWindow::onPlaylistFilesDropped);

    connect(timelineSlider_, &TimelineSlider::seekRequested, this, [this](double seconds) { player_->seekAbsolute(seconds); });
    connect(volumeSlider_, &VolumeSlider::volumeChangeRequested, this, &MainWindow::onVolumeSliderMoved);

    // VideoWidget -> player / window
    connect(videoWidget_, &VideoWidget::playPauseToggleRequested, this, [this] { player_->togglePause(); });
    connect(videoWidget_, &VideoWidget::fullscreenToggleRequested, this, &MainWindow::toggleFullscreen);
    connect(videoWidget_, &VideoWidget::exitFullscreenRequested, this, &MainWindow::exitFullscreenIfActive);
    connect(videoWidget_, &VideoWidget::muteToggleRequested, this, [this] { player_->setMute(!player_->isMuted()); });
    connect(videoWidget_, &VideoWidget::seekStepRequested, this, [this](int direction) {
        const double jump = direction * computeJumpSeconds();
        player_->seekRelative(jump);
        showSeekOsd(jump);
    });
    connect(videoWidget_, &VideoWidget::seekToStartRequested, this, [this] {
        player_->seekAbsolute(0.0);
        showSeekOsd(0.0);
    });
    connect(videoWidget_, &VideoWidget::seekToEndRequested, this, [this] {
        player_->seekAbsolute(std::max(0.0, currentDuration_ - 0.1));
        showSeekOsd(0.0);
    });
    connect(videoWidget_, &VideoWidget::volumeStepRequested, this, &MainWindow::onVolumeStepRequested);
    connect(videoWidget_, &VideoWidget::speedStepRequested, this, &MainWindow::onSpeedStepRequested);
    connect(videoWidget_, &VideoWidget::screenshotRequested, this, &MainWindow::takeScreenshot);
    connect(videoWidget_, &VideoWidget::mouseMoved, this, &MainWindow::onVideoMouseMoved);
    connect(player_.get(), &MpvPlayer::chapterChanged, this, [this](int index) {
        // Quiet for a file with no chapters, and for the -1 mpv reports while
        // one is being loaded.
        if (!mediaLoaded_ || index < 0) {
            return;
        }
        const QVector<MpvPlayer::Chapter> chapters = player_->chapters();
        if (index >= chapters.size()) {
            return;
        }
        const QString title = chapters.at(index).title;
        player_->showOsdMessage(title.isEmpty()
                                     ? tr("Chapter %1").arg(index + 1)
                                     : tr("Chapter %1: %2").arg(index + 1).arg(title), 2000);
    });
    connect(videoWidget_, &VideoWidget::speedResetRequested, this,
             [this] { player_->setSpeed(kDefaultSpeed); });
    connect(videoWidget_, &VideoWidget::contextMenuRequested, this, &MainWindow::showVideoContextMenu);

    // Player -> UI
    connect(player_.get(), &MpvPlayer::positionChanged, this, &MainWindow::onPositionChanged);
    connect(player_.get(), &MpvPlayer::durationChanged, this, &MainWindow::onDurationChanged);
    connect(player_.get(), &MpvPlayer::pauseChanged, this, &MainWindow::onPauseChanged);
    connect(player_.get(), &MpvPlayer::volumeChanged, this, &MainWindow::onVolumeChanged);
    connect(player_.get(), &MpvPlayer::muteChanged, this, &MainWindow::onMuteChanged);
    connect(player_.get(), &MpvPlayer::speedChanged, this, &MainWindow::onSpeedChanged);
    connect(player_.get(), &MpvPlayer::mediaTitleChanged, this, &MainWindow::onMediaTitleChanged);
    connect(player_.get(), &MpvPlayer::fileLoaded, this, &MainWindow::onFileLoaded);
    connect(player_.get(), &MpvPlayer::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(player_.get(), &MpvPlayer::videoDisplaySizeChanged,
             this, &MainWindow::onVideoDisplaySizeChanged);
    // Whether the display has to be kept awake depends on there being a
    // picture to watch, so the inhibit is revisited when that changes.
    connect(player_.get(), &MpvPlayer::hasVideoChanged, this, [this](bool hasVideo) {
        hasVideo_ = hasVideo;
        updateSleepInhibit();
        // Whether this is a music file is exactly what just changed.
        updateAudioTrackInfo();
    });
}

void MainWindow::loadSettings()
{
    applyPreferences();

    QSettings settings;
    const int volume = settings.value(QStringLiteral("playback/volume"), kDefaultVolume).toInt();

    player_->setVolume(volume);
    // Speed is not restored: it lasts only as long as the session, so every
    // launch starts at normal speed. The tick follows from mpv's own
    // speedChanged, which onSpeedChanged() handles.
    player_->setSpeed(kDefaultSpeed);
    volumeSlider_->setVolumeSilently(volume);
}

void MainWindow::applyPreferences()
{
    QSettings settings;

    seekMode_ = settings.value(SettingsKeys::kSeekMode, QStringLiteral("seconds")).toString();
    seekSeconds_ = settings.value(SettingsKeys::kSeekSeconds, SettingsKeys::kDefaultSeekSeconds).toDouble();
    seekPercentage_ = settings.value(SettingsKeys::kSeekPercentage, SettingsKeys::kDefaultSeekPercentage).toDouble();
    seekMinSeconds_ = settings.value(SettingsKeys::kSeekMinSeconds, SettingsKeys::kDefaultSeekMinSeconds).toDouble();
    maxVolume_ = settings.value(SettingsKeys::kMaxVolume, SettingsKeys::kDefaultMaxVolume).toInt();
    fitWindowToVideo_ = settings.value(SettingsKeys::kFitWindowToVideo,
                                        SettingsKeys::kDefaultFitWindowToVideo).toBool();
    openMaximized_ = settings.value(SettingsKeys::kOpenMaximized,
                                     SettingsKeys::kDefaultOpenMaximized).toBool();

    // Only touched when it actually changes: setWindowFlag() on a visible
    // window destroys and recreates the native window, which costs the
    // QOpenGLWidget its GL context and makes the window flicker. This runs
    // every time the Settings dialog is accepted, so doing it unconditionally
    // would pay that price for unrelated edits.
    const bool alwaysOnTop = settings.value(SettingsKeys::kAlwaysOnTop,
                                             SettingsKeys::kDefaultAlwaysOnTop).toBool();
    if (alwaysOnTop != bool(windowFlags() & Qt::WindowStaysOnTopHint)) {
        setWindowFlag(Qt::WindowStaysOnTopHint, alwaysOnTop);
        // Changing a flag hides the window; nothing brings it back on its own.
        show();
        player_->showOsdMessage(alwaysOnTop ? tr("Always on top: on")
                                             : tr("Always on top: off"), 1800);
    }
    showAudioTrackInfo_ = settings.value(SettingsKeys::kShowAudioTrackInfo,
                                          SettingsKeys::kDefaultShowAudioTrackInfo).toBool();
    const bool clickToPause = settings.value(SettingsKeys::kClickToPause, SettingsKeys::kDefaultClickToPause).toBool();
    const QString doubleClickAction = settings.value(SettingsKeys::kDoubleClickAction, QStringLiteral("fullscreen")).toString();

    player_->setMaxVolume(maxVolume_);
    volumeSlider_->setMaxVolume(maxVolume_);
    videoWidget_->setClickToPauseEnabled(clickToPause);
    videoWidget_->setDoubleClickAction(doubleClickAction == QStringLiteral("playpause")
                                            ? VideoWidget::DoubleClickAction::PlayPause
                                            : VideoWidget::DoubleClickAction::Fullscreen);

    player_->applySubtitleStyle(SubtitleStyle::load());
    // mpv keeps the filter chain across files, so this only has to be set
    // here at startup and whenever the Equalizer dialog changes it.
    player_->setEqualizerGains(Equalizer::loadGains());
    // Picks up the setting being toggled while a file is already playing.
    refreshChapterMarkers();
    updateAudioTrackInfo();

    // Video state belongs to whatever is playing, so it is only seeded here
    // when nothing is: this runs again every time the Settings dialog is
    // accepted, and resetting it then would throw away the flip, mirror,
    // rotation and zoom chosen for the current video -- and then persist the
    // loss, since the dialog saves the file's state afterwards.
    // A loaded file gets its values from restoreFileMemory() instead.
    if (currentMediaPath_.isEmpty()) {
        videoAdjust_ = VideoSettings::loadAdjust();
        videoFlip_ = false;
        videoMirror_ = false;
        videoRotation_ = 0;
        videoFrameModeIndex_ = VideoSettings::defaultFrameModeIndex();
        videoAspectRatio_ = -1.0;
        videoZoomPercent_ = 100;
        videoPanX_ = 0.0;
        videoPanY_ = 0.0;
        if (flipAction_) {
            flipAction_->setChecked(false);
            mirrorAction_->setChecked(false);
        }
    }
    applyVideoState();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("playback/volume"), player_->volume());
    // Speed deliberately not stored -- see loadSettings().

    if (settings.value(SettingsKeys::kRememberGeometry,
                        SettingsKeys::kDefaultRememberGeometry).toBool()) {
        // Not while fullscreen: that geometry is the whole screen, and
        // restoring it next launch would give a window with no frame the user
        // never asked for. saveGeometry() keeps the pre-fullscreen rectangle
        // for a maximized window, so that case needs no special handling.
        if (!isFullScreen()) {
            settings.setValue(SettingsKeys::kWindowGeometry, saveGeometry());
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    savePlaybackPosition();
    saveSettings();
    // Hand the machine back its normal sleep behaviour on the way out.
    SetThreadExecutionState(ES_CONTINUOUS);
    QMainWindow::closeEvent(event);
}

void MainWindow::openFile()
{
    // Built from the suffix lists rather than spelled out again, so the groups
    // cannot fall behind what the player actually accepts.
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Open Media"), QString(),
        tr("Media Files (%1);;Video (%2);;Audio (%3);;Images (%4);;Playlists (%5);;All Files (*.*)")
            .arg((playableNameFilters() + wildcardsFor(kPlaylistSuffixes).split(QLatin1Char(' ')))
                      .join(QLatin1Char(' ')),
                  wildcardsFor(kVideoSuffixes),
                  wildcardsFor(kAudioSuffixes),
                  wildcardsFor(kImageSuffixes),
                  wildcardsFor(kPlaylistSuffixes)));
    openFiles(paths);
}

void MainWindow::openFiles(const QStringList &rawPaths)
{
    // An .m3u/.m3u8 stands for the files it names: the first plays and the rest
    // queue behind it, which is the only useful reading of "open this playlist".
    const QStringList paths = expandPlaylists(rawPaths);
    if (paths.isEmpty()) {
        if (!rawPaths.isEmpty()) {
            // Only reachable for a playlist that named nothing this player
            // could use; a plain file never vanishes here.
            player_->showOsdMessage(tr("Playlist is empty"), 2000);
        }
        return;
    }

    // Every real file joins the playlist, the first of them becomes the
    // current row, and that row is what plays. Queuing the played file too
    // (rather than only the ones behind it) is what lets Next, Previous and
    // auto-advance treat the selection as one list.
    //
    // Appended, not replacing: a playlist already built up is the user's, and
    // opening a couple of files is no reason to throw it away.
    const int firstNewRow = playlistWidget_->count();
    for (const QString &path : paths) {
        const QFileInfo info(path);
        // A dragged web link plays but is not queued -- playlist entries are
        // file paths, and there is nothing on disk to point an entry at.
        if (info.isFile()) {
            addPlaylistItem(info);
        }
    }

    if (playlistWidget_->count() > firstNewRow) {
        playlistWidget_->setCurrentRow(firstNewRow);
    }
    if (paths.size() > 1) {
        playlistDock_->setVisible(true);
    }

    loadFile(paths.first());
    // Only worth saying for a selection: opening a single file is its own
    // feedback, since the file starts playing.
    if (paths.size() > 1) {
        showFilesAddedOsd(paths.size());
    }

    // Opening anything at all starts the burst window, this route included.
    // Explorer hands the first file of a selection to the process it starts
    // first, which opens it here rather than through openFromAnotherInstance()
    // -- so without this the very next handover would look like a fresh,
    // unrelated open and replace what had only just started playing.
    externalOpenTimer_.restart();
}

void MainWindow::queueFiles(const QStringList &rawPaths)
{
    const QStringList paths = expandPlaylists(rawPaths);
    int added = 0;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (info.isFile()) {
            addPlaylistItem(info);
            ++added;
        }
    }
    if (added > 0) {
        playlistDock_->setVisible(true);
        showFilesAddedOsd(added);
    }
    externalOpenTimer_.restart();
}

void MainWindow::openFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Open Folder"), QString());
    if (dir.isEmpty()) {
        return;
    }

    const QDir directory(dir);
    const QFileInfoList entries = directory.entryInfoList(playableNameFilters(), QDir::Files, QDir::Name | QDir::IgnoreCase);

    playlistWidget_->clear();
    for (const QFileInfo &info : entries) {
        addPlaylistItem(info);
    }

    playlistDock_->setVisible(true);
}

QUrl MainWindow::promptForUrl(const QString &title, const QString &label)
{
    // A link almost always arrives here straight from a browser's address
    // bar, so offer the clipboard contents when they already look like one.
    QString initial;
    if (const QClipboard *clipboard = QGuiApplication::clipboard()) {
        const QString clipped = clipboard->text().trimmed();
        if (!clipped.isEmpty() && !clipped.contains(QLatin1Char('\n'))
            && !QUrl(clipped).scheme().isEmpty()) {
            initial = clipped;
        }
    }

    // Built explicitly rather than via QInputDialog::getText(): the static
    // helper sizes itself to the label, which leaves a text field far too
    // narrow to read a URL in. Only a constructed dialog can be widened.
    QInputDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setLabelText(label);
    dialog.setInputMode(QInputDialog::TextInput);
    dialog.setTextValue(initial);
    // resize(), not setMinimumWidth(): QInputDialog's layout overrides a
    // minimum width and stays at its ~262px size hint, but honours an
    // explicit resize. Wide enough that a typical media URL is readable in
    // full; longer ones still scroll inside the field.
    dialog.resize(780, dialog.height());

    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }

    const QString entered = dialog.textValue().trimmed();
    if (entered.isEmpty()) {
        return {};
    }

    // fromUserInput() fills in a missing scheme, so a pasted "example.com/a.mp4"
    // resolves the same way it would in a browser.
    const QUrl url = QUrl::fromUserInput(entered);
    if (!url.isValid() || url.scheme().isEmpty()) {
        QMessageBox::warning(this, title, tr("\"%1\" is not a valid URL.").arg(entered));
        return {};
    }

    return url;
}

void MainWindow::openUrl()
{
    const QUrl url = promptForUrl(tr("Open URL"), tr("Video or audio link:"));
    if (!url.isValid()) {
        return;
    }

    // mpv resolves the link itself: direct media URLs stream through FFmpeg,
    // while links to streaming sites go through mpv's ytdl hook, which needs
    // yt-dlp (or youtube-dl) on PATH. If that is missing, mpv reports the
    // failure through errorOccurred() like any other load error.
    loadFile(url.toString());
}

bool MainWindow::currentMediaIsNetworkUrl() const
{
    if (currentMediaPath_.isEmpty()) {
        return false;
    }
    const QString scheme = QUrl(currentMediaPath_).scheme();
    return !scheme.isEmpty() && scheme != QLatin1StringView("file");
}

void MainWindow::downloadFromUrl()
{
    const QUrl url = promptForUrl(tr("Download from URL"), tr("Video or audio link to download:"));
    if (url.isValid()) {
        startDownload(url);
    }
}

void MainWindow::downloadCurrentMedia()
{
    if (currentMediaIsNetworkUrl()) {
        startDownload(QUrl(currentMediaPath_));
    }
}

void MainWindow::startDownload(const QUrl &url)
{
    // Name the file after the URL's last path segment, which is the filename
    // for a direct media link; fall back to something generic otherwise.
    QString suggestedName = QFileInfo(url.path()).fileName();
    if (suggestedName.isEmpty()) {
        suggestedName = tr("download");
    }

    const QString moviesDir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    const QString destination = QFileDialog::getSaveFileName(
        this, tr("Save Media As"),
        moviesDir.isEmpty() ? suggestedName : QDir(moviesDir).filePath(suggestedName),
        tr("All Files (*.*)"));
    if (destination.isEmpty()) {
        return;
    }

    auto *downloader = new Downloader(this);
    auto *progress = new QProgressDialog(tr("Downloading %1...").arg(QFileInfo(destination).fileName()),
                                          tr("Cancel"), 0, 100, this);
    progress->setWindowTitle(tr("Download"));
    progress->setWindowModality(Qt::WindowModal);
    // Left to the explicit close() calls below so the dialog does not vanish
    // the moment the bar reaches 100% but before the file is committed.
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->setMinimumDuration(0);
    progress->setValue(0);

    const auto dismiss = [downloader, progress] {
        downloader->cancel(); // no-op once the transfer has already ended
        progress->close();
        progress->deleteLater();
        downloader->deleteLater();
    };

    connect(downloader, &Downloader::progress, progress, [progress](qint64 received, qint64 total) {
        const QLocale locale;
        if (total > 0) {
            progress->setMaximum(100);
            progress->setValue(static_cast<int>(received * 100 / total));
            progress->setLabelText(tr("Downloading... %1 of %2")
                                        .arg(locale.formattedDataSize(received),
                                             locale.formattedDataSize(total)));
        } else {
            // No content length (a chunked response): show a busy indicator
            // rather than a bar that would sit at zero the whole time.
            progress->setMaximum(0);
            progress->setLabelText(tr("Downloading... %1").arg(locale.formattedDataSize(received)));
        }
    });

    connect(downloader, &Downloader::finished, this, [this, dismiss](const QString &savedPath) {
        dismiss();
        player_->showOsdMessage(tr("Download finished: %1").arg(QFileInfo(savedPath).fileName()), 3000);
        QMessageBox::information(this, tr("Download Complete"),
                                 tr("Saved to %1").arg(QDir::toNativeSeparators(savedPath)));
    });

    connect(downloader, &Downloader::failed, this, [this, dismiss](const QString &message) {
        dismiss();
        player_->showOsdMessage(tr("Download failed"), 3000);
        QMessageBox::warning(this, tr("Download Failed"), message);
    });

    player_->showOsdMessage(tr("Download started: %1").arg(QFileInfo(destination).fileName()), 2500);

    connect(progress, &QProgressDialog::canceled, this, [dismiss] { dismiss(); });

    downloader->start(url, destination);
}

void MainWindow::onPlaylistFilesDropped(const QStringList &paths)
{
    // Counted rather than taken from paths.size(): a dropped folder can add
    // many entries, and a dropped file that is not playable adds none.
    int added = 0;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (info.isDir()) {
            const QDir directory(info.absoluteFilePath());
            const QFileInfoList entries = directory.entryInfoList(playableNameFilters(), QDir::Files, QDir::Name | QDir::IgnoreCase);
            for (const QFileInfo &entry : entries) {
                addPlaylistItem(entry);
                ++added;
            }
        } else if (info.isFile() && isPlaylistFile(info)) {
            // A dropped playlist contributes its entries, not itself.
            for (const QString &entry : readPlaylistEntries(path)) {
                addPlaylistItem(QFileInfo(entry));
                ++added;
            }
        } else if (info.isFile() && isPlayableFile(info)) {
            addPlaylistItem(info);
            ++added;
        }
    }

    playlistDock_->setVisible(true);
    showFilesAddedOsd(added);
}

void MainWindow::addPlaylistItem(const QFileInfo &info)
{
    auto *item = new QListWidgetItem(info.fileName(), playlistWidget_);
    item->setData(Qt::UserRole, info.absoluteFilePath());
}

void MainWindow::openSubtitleFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Load Subtitle"), QString(),
        tr("Subtitle Files (*.srt *.ass *.ssa *.sub *.vtt);;All Files (*.*)"));
    if (!path.isEmpty()) {
        player_->loadExternalSubtitle(path);
        // Remembered so the same subtitle comes back with this file next time.
        updateFileMemory([&path](FileMemory &memory) { memory.subtitlePath = path; });
        player_->showOsdMessage(tr("Subtitles loaded: %1").arg(QFileInfo(path).fileName()), 2500);
    }
}

void MainWindow::registerMenuShortcut(QAction *action)
{
    // Menu actions whose shortcut has to keep working in fullscreen are put on
    // the window itself. Recorded here as well, so rebuilding the menus can
    // take the old ones off -- otherwise each rebuild would leave another
    // action claiming the same key, and Qt fires none of an ambiguous set.
    addAction(action);
    menuShortcutActions_.append(action);
}

void MainWindow::retranslateUi()
{
    // The menus are rebuilt rather than relabelled one action at a time.
    // Several are created inline with the return value discarded, so there is
    // no pointer to relabel, and a string missed that way fails silently: it
    // just stays in the old language until the next restart.
    for (QAction *action : menuShortcutActions_) {
        if (action) {
            removeAction(action);
        }
    }
    menuShortcutActions_.clear();
    delete fullscreenShortcut_; // QPointer, so it reads null from here

    // Deleting each top-level menu takes its actions and submenus with it.
    // Clearing the bar alone would leave the menus alive and their shortcuts
    // still registered.
    const QList<QAction *> barActions = menuBar_->actions();
    menuBar_->clear();
    for (QAction *action : barActions) {
        delete action->menu();
    }

    createMenus();

    // The rebuilt menus start with nothing ticked, so the state has to be put
    // back. Bold, audio channels, chapters, tracks and recent files all
    // refresh themselves on aboutToShow and need nothing here.
    fullscreenAction_->setChecked(isFullScreen());
    onSpeedChanged(player_->speed());

    const LoopMode loop = player_->isLoopingFile()      ? LoopMode::File
                          : player_->isLoopingPlaylist() ? LoopMode::Playlist
                                                          : LoopMode::Off;
    loopOffAction_->setChecked(loop == LoopMode::Off);
    loopFileAction_->setChecked(loop == LoopMode::File);
    loopPlaylistAction_->setChecked(loop == LoopMode::Playlist);

    // Ticks the flip, mirror, rotation, frame mode, zoom and aspect entries.
    applyVideoState();

    retranslateControls();
}

void MainWindow::retranslateControls()
{
    // Every button that carries its English tooltip as a property, wherever it
    // was created -- the control bar and both rows of playlist buttons.
    const QList<QWidget *> widgets = findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        const QVariant source = widget->property(kSourceToolTip);
        if (source.isValid()) {
            widget->setToolTip(QCoreApplication::translate(
                "MainWindow", source.toString().toUtf8().constData()));
        }
    }

    playlistDock_->setWindowTitle(tr("Playlist"));

    // Only the "- Pear Player" half of the window title is translated, but it
    // goes back through the same setter so the title bar and the window title
    // cannot drift apart.
    setMediaTitle(mediaTitle_);
}

void MainWindow::showVideoContextMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    // The real File, Playback, Subtitles and Video menus are reused rather
    // than rebuilt, so the context menu shows the same entries and shortcuts
    // as the menu bar -- including the ticks on Speed and Loop, and the
    // aboutToShow handlers that grey out entries with nothing to act on.
    menu.addMenu(fileMenu_);
    menu.addMenu(playbackMenu_);
    menu.addMenu(subtitlesMenu_);
    menu.addMenu(videoMenu_);
    populateAudioMenu(menu.addMenu(tr("Audio Track")));
    menu.addSeparator();
    menu.addAction(tr("&Settings"), this, &MainWindow::openSettingsDialog);
    menu.addSeparator();
    menu.addAction(tr("&About"), this, &MainWindow::openAboutDialog);
    menu.exec(globalPos);
}

namespace {

// Fills menu with an "Off" entry plus one checkable, mutually-exclusive
// entry per track, wired to onSelect(trackId, label) (0 for "Off"). Shared
// between the Subtitles and Audio Track menus, which differ only in their
// track list, selection callback, and (for subtitles) an extra footer action.
//
// The label is handed back as well as the id so the caller can name the track
// on the OSD without rebuilding the "Title [lang] (file)" string itself.
void populateTrackSelectionMenu(QMenu *menu, QObject *context, const QVector<MpvPlayer::TrackInfo> &tracks,
                                 const std::function<void(int, const QString &)> &onSelect)
{
    // Reparented onto the freshly-cleared menu each time so exclusivity
    // (radio-button look) stays scoped to whichever population is current;
    // the previous group (and its actions) is gone via menu->clear() first.
    auto *trackGroup = new QActionGroup(menu);
    trackGroup->setExclusive(true);

    QAction *offAction = menu->addAction(QObject::tr("Off"));
    offAction->setCheckable(true);
    trackGroup->addAction(offAction);
    QObject::connect(offAction, &QAction::triggered, context,
                      [onSelect] { onSelect(0, QObject::tr("Off")); });

    menu->addSeparator();

    bool anySelected = false;
    for (const auto &track : tracks) {
        QString label = track.title.isEmpty() ? QObject::tr("Track %1").arg(track.id) : track.title;
        if (!track.language.isEmpty()) {
            label += QStringLiteral(" [%1]").arg(track.language);
        }
        if (track.external) {
            label += QObject::tr(" (file)");
        }

        QAction *action = menu->addAction(label);
        action->setCheckable(true);
        action->setChecked(track.selected);
        trackGroup->addAction(action);
        anySelected = anySelected || track.selected;

        const int trackId = track.id;
        QObject::connect(action, &QAction::triggered, context,
                          [onSelect, trackId, label] { onSelect(trackId, label); });
    }
    offAction->setChecked(!anySelected);
}

} // namespace

QAction *MainWindow::addSubtitleAction(QMenu *menu, const QString &text, const QKeySequence &shortcut,
                                        void (MainWindow::*slot)())
{
    auto *action = new QAction(text, this);
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
        // Fullscreen hides the menu bar, and a menu-owned shortcut stops
        // firing with it. Registering the action on the window itself with
        // application context keeps these keys alive in both states -- the
        // same problem the F11 QShortcut above works around.
        action->setShortcutContext(Qt::ApplicationShortcut);
        registerMenuShortcut(action);
    }
    connect(action, &QAction::triggered, this, slot);
    menu->addAction(action);
    return action;
}

void MainWindow::createSubtitlesMenu()
{
    subtitlesMenu_ = menuBar_->addMenu(tr("Su&btitles"));

    addSubtitleAction(subtitlesMenu_, tr("Subtitles Settings"), QKeySequence(),
                       &MainWindow::openSubtitleSettings);
    subtitlesMenu_->addSeparator();

    showSubtitleMenu_ = subtitlesMenu_->addMenu(tr("&Show Subtitle"));
    connect(showSubtitleMenu_, &QMenu::aboutToShow, this, &MainWindow::refreshShowSubtitleMenu);

    // Parented to the window, not the submenu: refreshShowSubtitleMenu()
    // clears the submenu on every popup, which would otherwise delete these.
    subEnabledAction_ = new QAction(tr("Enabled"), this);
    subEnabledAction_->setCheckable(true);
    subEnabledAction_->setShortcut(QKeySequence(Qt::ALT | Qt::Key_H));
    subEnabledAction_->setShortcutContext(Qt::ApplicationShortcut);
        registerMenuShortcut(subEnabledAction_);
    connect(subEnabledAction_, &QAction::triggered, this, &MainWindow::toggleSubtitleVisibility);

    subAddFileAction_ = new QAction(tr("Add File..."), this);
    connect(subAddFileAction_, &QAction::triggered, this, &MainWindow::openSubtitleFile);

    subtitlesMenu_->addSeparator();
    addSubtitleAction(subtitlesMenu_, tr("Default Position"),
                       QKeySequence(Qt::ALT | Qt::Key_Home), &MainWindow::subtitlePositionReset);
    addSubtitleAction(subtitlesMenu_, tr("Up by 2%"),
                       QKeySequence(Qt::ALT | Qt::Key_Up), &MainWindow::subtitleMoveUp);
    addSubtitleAction(subtitlesMenu_, tr("Down by 2%"),
                       QKeySequence(Qt::ALT | Qt::Key_Down), &MainWindow::subtitleMoveDown);
    // No Left/Right entries: mpv writes its single sub-margin-x value into
    // both margins, so centred subtitles cannot be shifted sideways at all.
    // Verified against rendered frames -- a centred line stays on the same
    // pixel at every margin, and with forced asymmetric ASS margins too.

    subtitlesMenu_->addSeparator();
    subBoldAction_ = addSubtitleAction(subtitlesMenu_, tr("Bold"),
                                        QKeySequence(Qt::ALT | Qt::Key_B), &MainWindow::toggleSubtitleBold);
    subBoldAction_->setCheckable(true);
    addSubtitleAction(subtitlesMenu_, tr("Increase Font Size"),
                       QKeySequence(Qt::ALT | Qt::Key_PageUp), &MainWindow::subtitleFontSizeUp);
    addSubtitleAction(subtitlesMenu_, tr("Decrease Font Size"),
                       QKeySequence(Qt::ALT | Qt::Key_PageDown), &MainWindow::subtitleFontSizeDown);

    subtitlesMenu_->addSeparator();
    addSubtitleAction(subtitlesMenu_, tr("0.5 seconds faster"),
                       QKeySequence(Qt::Key_Period), &MainWindow::subtitleSyncEarlier);
    addSubtitleAction(subtitlesMenu_, tr("0.5 seconds slower"),
                       QKeySequence(Qt::Key_Comma), &MainWindow::subtitleSyncLater);
    addSubtitleAction(subtitlesMenu_, tr("Default Sync"),
                       QKeySequence(Qt::Key_Slash), &MainWindow::subtitleSyncReset);
    addSubtitleAction(subtitlesMenu_, tr("Save the Current Sync"),
                       QKeySequence(Qt::ALT | Qt::Key_S), &MainWindow::subtitleSyncSave);

    // Keep the Bold tick in step with whatever the player actually has set,
    // which the Settings dialog can change behind this menu's back.
    connect(subtitlesMenu_, &QMenu::aboutToShow, this, [this] {
        subBoldAction_->setChecked(player_->isSubtitleBold());
    });
}

namespace {
// Index-aligned with MainWindow::rotateActions_.
constexpr int kRotations[] = {0, 90, 180, 270};
} // namespace

void MainWindow::addToRecentFiles(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    // Only real files: a stream URL cannot be reopened reliably later, and
    // would push actual files out of a short list.
    const QFileInfo info(path);
    if (!info.isFile()) {
        return;
    }

    QSettings settings;
    QStringList recent = settings.value(SettingsKeys::kRecentFiles).toStringList();

    const QString absolute = info.absoluteFilePath();
    // Reopening a file moves it back to the top rather than listing it twice.
    recent.removeAll(absolute);
    recent.prepend(absolute);
    while (recent.size() > SettingsKeys::kMaxRecentFiles) {
        recent.removeLast();
    }

    settings.setValue(SettingsKeys::kRecentFiles, recent);
}

void MainWindow::refreshRecentFilesMenu()
{
    recentFilesMenu_->clear();

    const QSettings settings;
    const QStringList recent = settings.value(SettingsKeys::kRecentFiles).toStringList();

    QAction *clearAction = recentFilesMenu_->addAction(tr("Clear Recent Files"), this,
                                                        &MainWindow::clearRecentFiles);
    clearAction->setEnabled(!recent.isEmpty());
    recentFilesMenu_->addSeparator();

    if (recent.isEmpty()) {
        QAction *none = recentFilesMenu_->addAction(tr("(no recent files)"));
        none->setEnabled(false);
        return;
    }

    for (int i = 0; i < recent.size(); ++i) {
        const QString path = recent.at(i);
        const QFileInfo info(path);

        // Numbered so the newest is obvious, with the folder as a tooltip
        // since several files can share a name.
        QAction *action = recentFilesMenu_->addAction(
            QStringLiteral("&%1  %2").arg(i + 1).arg(info.fileName()));
        action->setToolTip(QDir::toNativeSeparators(path));

        if (info.exists()) {
            connect(action, &QAction::triggered, this, [this, path] { loadFile(path); });
        } else {
            // Kept but greyed: a missing file is usually on a drive that is
            // not plugged in, and silently dropping it would be worse.
            action->setEnabled(false);
            action->setText(action->text() + tr("   (missing)"));
        }
    }
}

void MainWindow::clearRecentFiles()
{
    QSettings settings;
    settings.remove(SettingsKeys::kRecentFiles);
}

void MainWindow::openContainingFolder()
{
    const QFileInfo info(currentMediaPath_);
    if (currentMediaPath_.isEmpty() || !info.isFile()) {
        return;
    }

    // "/select," and the path as separate arguments: passed as one string,
    // Explorer is quoted the whole way round by QProcess and answers by
    // opening Documents instead of the folder asked for.
    //
    // The canonical path, so a file reached through a mapped drive or a
    // junction is selected where Explorer can actually find it.
    if (!QProcess::startDetached(QStringLiteral("explorer.exe"),
                                  {QStringLiteral("/select,"),
                                   QDir::toNativeSeparators(info.canonicalFilePath())})) {
        // Falls back to the folder itself, without the file selected in it.
        QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
    }
}

void MainWindow::copyFileTo()
{
    if (currentMediaPath_.isEmpty()) {
        return;
    }
    const QFileInfo source(currentMediaPath_);
    if (!source.isFile()) {
        QMessageBox::information(this, tr("Copy File"),
                                  tr("Only files on disk can be copied; this is a stream."));
        return;
    }

    const QString destination = QFileDialog::getSaveFileName(
        this, tr("Copy File To"), source.fileName(), tr("All Files (*.*)"));
    if (destination.isEmpty()) {
        return;
    }

    if (QFileInfo(destination) == source) {
        QMessageBox::warning(this, tr("Copy File"), tr("That is the file being copied."));
        return;
    }

    // QFile::copy refuses to overwrite, so an existing target is removed first
    // -- the save dialog has already asked about replacing it.
    if (QFile::exists(destination) && !QFile::remove(destination)) {
        QMessageBox::warning(this, tr("Copy File"),
                              tr("Could not replace %1").arg(QDir::toNativeSeparators(destination)));
        return;
    }

    if (QFile::copy(source.absoluteFilePath(), destination)) {
        player_->showOsdMessage(tr("Copied to %1").arg(QFileInfo(destination).fileName()), 2500);
    } else {
        QMessageBox::warning(this, tr("Copy File"),
                              tr("Could not copy to %1").arg(QDir::toNativeSeparators(destination)));
    }
}

void MainWindow::playlistAddFiles()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Add to Playlist"), QString(),
        // Same shared suffix lists as Open Media: this filter was spelled out
        // by hand and had already fallen behind it by half a dozen formats.
        tr("Media Files (%1);;All Files (*.*)")
            .arg(playableNameFilters().join(QLatin1Char(' '))));
    for (const QString &path : paths) {
        addPlaylistItem(QFileInfo(path));
    }
    showFilesAddedOsd(paths.size());
}

void MainWindow::showFilesAddedOsd(int count)
{
    if (count > 0) {
        player_->showOsdMessage(count == 1 ? tr("1 file added")
                                            : tr("%1 files added").arg(count), 1800);
    }
}

void MainWindow::playlistAddFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Add Folder to Playlist"));
    if (dir.isEmpty()) {
        return;
    }

    // Appends rather than replacing, unlike File > Open Folder: these buttons
    // build a playlist up, so clearing it here would be a nasty surprise.
    const QDir directory(dir);
    const QFileInfoList entries = directory.entryInfoList(
        playableNameFilters(), QDir::Files, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &info : entries) {
        addPlaylistItem(info);
    }

    if (entries.isEmpty()) {
        player_->showOsdMessage(tr("No playable files in that folder"), 2000);
    } else {
        showFilesAddedOsd(entries.size());
    }
}

void MainWindow::playlistRemoveSelected()
{
    // Deleting the items directly is safe here: takeItem detaches each one
    // from the list before it is destroyed.
    const QList<QListWidgetItem *> selected = playlistWidget_->selectedItems();
    for (QListWidgetItem *item : selected) {
        delete playlistWidget_->takeItem(playlistWidget_->row(item));
    }
}

void MainWindow::playlistClear()
{
    const int removed = playlistWidget_->count();
    playlistWidget_->clear();
    if (removed > 0) {
        player_->showOsdMessage(tr("Playlist cleared"), 1500);
    }
}

void MainWindow::playlistShuffle()
{
    const int count = playlistWidget_->count();
    if (count < 2) {
        return;
    }

    // Items are detached first, shuffled, then put back: shuffling in place
    // would fight with the widget's own row bookkeeping.
    QList<QListWidgetItem *> items;
    items.reserve(count);
    while (playlistWidget_->count() > 0) {
        items.append(playlistWidget_->takeItem(0));
    }

    std::shuffle(items.begin(), items.end(), *QRandomGenerator::global());
    for (QListWidgetItem *item : items) {
        playlistWidget_->addItem(item);
    }
    player_->showOsdMessage(tr("Playlist shuffled (%1 files)").arg(count), 1800);
}

void MainWindow::playlistPlayRelative(int delta)
{
    const int count = playlistWidget_->count();
    if (count == 0) {
        return;
    }

    const int current = playlistWidget_->currentRow();
    // Wraps at both ends, so Next on the last entry returns to the first.
    const int next = current < 0 ? 0 : ((current + delta) % count + count) % count;

    playlistWidget_->setCurrentRow(next);
    loadFile(playlistWidget_->item(next)->data(Qt::UserRole).toString());
    player_->showOsdMessage(tr("%1 of %2   %3")
                                 .arg(next + 1)
                                 .arg(count)
                                 .arg(playlistWidget_->item(next)->text()), 2000);
}

void MainWindow::playlistPlayNext()
{
    playlistPlayRelative(1);
}

void MainWindow::playlistPlayPrevious()
{
    playlistPlayRelative(-1);
}

void MainWindow::playlistToggleRepeat()
{
    const bool repeat = playlistRepeatButton_->isChecked();
    setLoopMode(repeat ? LoopMode::Playlist : LoopMode::Off);
}

void MainWindow::playlistSave()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Playlist"), QString(), tr("Playlist (*.m3u);;All Files (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save Playlist"),
                              tr("Could not write %1").arg(QDir::toNativeSeparators(path)));
        return;
    }

    // Plain .m3u: one path per line, which any other player can also read.
    QTextStream out(&file);
    out << "#EXTM3U\n";
    for (int row = 0; row < playlistWidget_->count(); ++row) {
        out << playlistWidget_->item(row)->data(Qt::UserRole).toString() << '\n';
    }
    player_->showOsdMessage(tr("Playlist saved: %1").arg(QFileInfo(path).fileName()), 2000);
}

void MainWindow::playlistLoad()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Load Playlist"), QString(), tr("Playlist (*.m3u *.m3u8);;All Files (*.*)"));
    if (path.isEmpty()) {
        return;
    }

    // The same parser that expands a playlist opened as a file, so the two
    // routes cannot drift apart.
    bool readable = false;
    const QStringList entries = readPlaylistEntries(path, &readable);
    if (!readable) {
        QMessageBox::warning(this, tr("Load Playlist"),
                              tr("Could not read %1").arg(QDir::toNativeSeparators(path)));
        return;
    }

    playlistWidget_->clear();
    for (const QString &entry : entries) {
        addPlaylistItem(QFileInfo(entry));
    }

    playlistDock_->setVisible(true);
    player_->showOsdMessage(tr("Playlist loaded: %1 (%2 files)")
                                 .arg(QFileInfo(path).fileName())
                                 .arg(playlistWidget_->count()), 2200);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    QStringList paths;
    for (const QUrl &url : urls) {
        // Local files come through as file:// URLs; a dragged web link is
        // handed to mpv as-is, which can stream it.
        paths.append(url.isLocalFile() ? url.toLocalFile() : url.toString());
    }
    if (paths.isEmpty()) {
        return;
    }

    // First one plays immediately, the rest queue up -- dropping a folder's
    // worth of files should not need a second step to start watching.
    openFiles(paths);
    event->acceptProposedAction();
}

void MainWindow::refreshChapterMarkers()
{
    const QSettings settings;
    const bool show = settings.value(SettingsKeys::kShowChapterMarkers,
                                      SettingsKeys::kDefaultShowChapterMarkers).toBool();

    QVector<double> starts;
    if (show) {
        const QVector<MpvPlayer::Chapter> chapters = player_->chapters();
        starts.reserve(chapters.size());
        for (const MpvPlayer::Chapter &chapter : chapters) {
            starts.append(chapter.time);
        }
    }
    timelineSlider_->setChapterMarkers(starts);
}

void MainWindow::savePlaybackPosition()
{
    if (currentMediaPath_.isEmpty() || !mediaLoaded_) {
        return;
    }
    const QSettings settings;
    if (!settings.value(SettingsKeys::kRememberPosition, SettingsKeys::kDefaultRememberPosition).toBool()) {
        return;
    }

    // Near the start there is nothing to resume; near the end the file has
    // effectively been watched, and resuming into the credits is worse than
    // starting again. Either way the stored position is cleared.
    const double position = currentPosition_;
    const double duration = currentDuration_;
    const bool worthKeeping = duration > 0.0
        && position > SettingsKeys::kResumeMinSeconds
        && position < duration - SettingsKeys::kResumeMinSeconds;

    const double stored = worthKeeping ? position : -1.0;
    updateFileMemory([stored](FileMemory &memory) { memory.playbackPosition = stored; });
}

void MainWindow::deleteCurrentFile()
{
    if (currentMediaPath_.isEmpty()) {
        return;
    }
    const QFileInfo info(currentMediaPath_);
    if (!info.isFile()) {
        // Streams and URLs have nothing on disk to delete.
        return;
    }

    const auto answer = QMessageBox::question(
        this, tr("Delete File"),
        tr("Send this file to the Recycle Bin?\n\n%1").arg(QDir::toNativeSeparators(info.absoluteFilePath())),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    // The file has to be closed first: mpv keeps it open while loaded, and
    // Windows will not move a file that is still in use. unload(), not
    // stopPlayback(), which only pauses and rewinds.
    const QString path = info.absoluteFilePath();
    taskbarActive_ = false;
    taskbarProgress_->clear();
    player_->unload();
    currentMediaPath_.clear();
    mediaLoaded_ = false;

    if (QFile::moveToTrash(path)) {
        // Its remembered settings would otherwise linger for a file that no
        // longer exists.
        FileMemoryStore::remove(path);
        setMediaTitle(QString());
        player_->showOsdMessage(tr("Sent to Recycle Bin: %1").arg(info.fileName()), 2500);
    } else {
        QMessageBox::warning(this, tr("Delete File"),
                              tr("Could not send the file to the Recycle Bin. "
                                  "It may be open in another program."));
    }
}

void MainWindow::frameStepForward()
{
    player_->frameStep();
    showFrameStepOsd(tr("Next frame"));
}

void MainWindow::frameStepBackward()
{
    player_->frameBackStep();
    showFrameStepOsd(tr("Previous frame"));
}

void MainWindow::showFrameStepOsd(const QString &label)
{
    if (!mediaLoaded_) {
        return;
    }
    // Queued rather than shown now: a frame step is asynchronous, so time-pos
    // still holds the previous frame's timestamp at this point and the message
    // would name the frame just left rather than the one arrived at.
    QTimer::singleShot(0, this, [this, label] {
        player_->showOsdMessage(tr("%1   %2").arg(label, formatTime(currentPosition_)), 1200);
    });
}

void MainWindow::setLoopMode(LoopMode mode)
{
    // Only ever one of the two mpv properties is on, so switching modes has to
    // clear the other rather than just set the new one.
    player_->setLoopFile(mode == LoopMode::File);
    player_->setLoopPlaylist(mode == LoopMode::Playlist);

    loopOffAction_->setChecked(mode == LoopMode::Off);
    loopFileAction_->setChecked(mode == LoopMode::File);
    loopPlaylistAction_->setChecked(mode == LoopMode::Playlist);

    switch (mode) {
    case LoopMode::Off:
        player_->showOsdMessage(tr("Loop: off"), 1500);
        break;
    case LoopMode::File:
        player_->showOsdMessage(tr("Loop: file"), 1500);
        break;
    case LoopMode::Playlist:
        player_->showOsdMessage(tr("Loop: playlist"), 1500);
        break;
    }
}

void MainWindow::refreshChaptersMenu()
{
    chaptersMenu_->clear();

    const QVector<MpvPlayer::Chapter> chapters = player_->chapters();
    if (chapters.isEmpty()) {
        QAction *none = chaptersMenu_->addAction(tr("(no chapters)"));
        none->setEnabled(false);
        return;
    }

    const int current = player_->currentChapter();
    auto *group = new QActionGroup(chaptersMenu_);
    group->setExclusive(true);

    for (int i = 0; i < chapters.size(); ++i) {
        const MpvPlayer::Chapter &chapter = chapters.at(i);
        const QString title = chapter.title.isEmpty() ? tr("Chapter %1").arg(i + 1) : chapter.title;
        QAction *action = chaptersMenu_->addAction(
            QStringLiteral("%1  —  %2").arg(formatTime(chapter.time), title));
        action->setCheckable(true);
        action->setChecked(i == current);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, i] { player_->setChapter(i); });
    }
}

void MainWindow::createAudioMenu()
{
    audioMenuTop_ = menuBar_->addMenu(tr("&Audio"));

    audioMenuTop_->addAction(tr("Delay +0.1 s"), this, &MainWindow::audioDelayLater);
    audioMenuTop_->addAction(tr("Delay -0.1 s"), this, &MainWindow::audioDelayEarlier);
    audioMenuTop_->addAction(tr("Reset Delay"), this, &MainWindow::audioDelayReset);
    audioMenuTop_->addSeparator();

    audioDeviceMenu_ = audioMenuTop_->addMenu(tr("Audio &Device"));
    // Devices can be plugged in while the player runs, so the list is rebuilt
    // each time rather than captured once at startup.
    connect(audioDeviceMenu_, &QMenu::aboutToShow, this, &MainWindow::refreshAudioDeviceMenu);

    QMenu *channelsMenu = audioMenuTop_->addMenu(tr("Audio &Channels"));
    auto *channelsGroup = new QActionGroup(channelsMenu);
    channelsGroup->setExclusive(true);
    const struct { QString label; const char *value; QAction **slot; } layouts[] = {
        {tr("Auto"),   "auto-safe", &audioAutoChannelsAction_},
        {tr("Stereo"), "stereo",    &audioStereoAction_},
        {tr("Mono"),   "mono",      &audioMonoAction_},
    };
    for (const auto &layout : layouts) {
        QAction *action = channelsMenu->addAction(layout.label);
        action->setCheckable(true);
        channelsGroup->addAction(action);
        *layout.slot = action;
        const QString value = QString::fromLatin1(layout.value);
        connect(action, &QAction::triggered, this, [this, value] {
            player_->setAudioChannels(value);
            player_->showOsdMessage(tr("Audio channels: %1").arg(value));
        });
    }
    connect(channelsMenu, &QMenu::aboutToShow, this, [this] {
        const QString current = player_->audioChannels();
        audioStereoAction_->setChecked(current == QLatin1StringView("stereo"));
        audioMonoAction_->setChecked(current == QLatin1StringView("mono"));
        audioAutoChannelsAction_->setChecked(current != QLatin1StringView("stereo")
                                              && current != QLatin1StringView("mono"));
    });
}

void MainWindow::refreshAudioDeviceMenu()
{
    audioDeviceMenu_->clear();

    const QVector<MpvPlayer::AudioDevice> devices = player_->audioDevices();
    const QString current = player_->audioDevice();
    auto *group = new QActionGroup(audioDeviceMenu_);
    group->setExclusive(true);

    for (const MpvPlayer::AudioDevice &device : devices) {
        // mpv's first entry is "auto"; give it a clearer name than its own
        // description, which reads oddly in a menu.
        const QString label = device.name == QLatin1StringView("auto")
            ? tr("Default")
            : device.description;
        QAction *action = audioDeviceMenu_->addAction(label.isEmpty() ? device.name : label);
        action->setCheckable(true);
        action->setChecked(device.name == current);
        group->addAction(action);

        const QString name = device.name;
        const QString description = device.description.isEmpty() ? device.name : device.description;
        connect(action, &QAction::triggered, this, [this, name, description] {
            player_->setAudioDevice(name);
            player_->showOsdMessage(tr("Audio device: %1").arg(description), 2000);
        });
    }
}

void MainWindow::audioDelayLater()
{
    const double delay = player_->audioDelay() + 0.1;
    player_->setAudioDelay(delay);
    player_->showOsdMessage(tr("Audio delay: %1 s").arg(delay, 0, 'f', 1));
    updateFileMemory([delay](FileMemory &memory) { memory.audioDelay = delay; });
}

void MainWindow::audioDelayEarlier()
{
    const double delay = player_->audioDelay() - 0.1;
    player_->setAudioDelay(delay);
    player_->showOsdMessage(tr("Audio delay: %1 s").arg(delay, 0, 'f', 1));
    updateFileMemory([delay](FileMemory &memory) { memory.audioDelay = delay; });
}

void MainWindow::audioDelayReset()
{
    player_->setAudioDelay(0.0);
    player_->showOsdMessage(tr("Audio delay: 0.0 s"));
    updateFileMemory([](FileMemory &memory) { memory.audioDelay = 0.0; });
}

void MainWindow::createVideoMenu()
{
    // See createMenus(): these lists are rebuilt with the menu, so anything
    // left from a previous build points at deleted actions.
    rotateActions_.clear();
    frameModeActions_.clear();
    zoomActions_.clear();
    aspectActions_.clear();

    videoMenu_ = menuBar_->addMenu(tr("&Video"));

    // The "\tF11" only draws the key alongside the entry -- it is not bound
    // here on purpose. A QAction's own shortcut stops firing once its menu bar
    // is hidden, which is exactly what happens in fullscreen, so the binding
    // lives on the standalone QShortcut below. Setting both would make F11
    // ambiguous and Qt would then fire neither.
    fullscreenAction_ = videoMenu_->addAction(tr("&Fullscreen\tF11"), this,
                                               &MainWindow::toggleFullscreen);
    fullscreenAction_->setCheckable(true);

    // Kept so a menu rebuild can delete it: a second one bound to the same key
    // would make F11 ambiguous, and Qt then fires neither.
    auto *fullscreenShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    fullscreenShortcut_ = fullscreenShortcut;
    fullscreenShortcut->setContext(Qt::ApplicationShortcut);
    connect(fullscreenShortcut, &QShortcut::activated, this, &MainWindow::toggleFullscreen);

    videoMenu_->addSeparator();

    flipAction_ = videoMenu_->addAction(tr("Flip Video"), this, &MainWindow::toggleVideoFlip);
    flipAction_->setCheckable(true);
    mirrorAction_ = videoMenu_->addAction(tr("Mirror Video"), this, &MainWindow::toggleVideoMirror);
    mirrorAction_->setCheckable(true);

    QMenu *rotateMenu = videoMenu_->addMenu(tr("&Rotate"));
    auto *rotateGroup = new QActionGroup(rotateMenu);
    rotateGroup->setExclusive(true);
    const QStringList rotateLabels = {tr("None"), tr("90°"), tr("180°"), tr("270°")};
    for (int i = 0; i < rotateLabels.size(); ++i) {
        QAction *action = rotateMenu->addAction(rotateLabels.at(i));
        action->setCheckable(true);
        rotateGroup->addAction(action);
        const int degrees = kRotations[i];
        connect(action, &QAction::triggered, this, [this, degrees] { setVideoRotation(degrees); });
        rotateActions_.append(action);
    }

    videoMenu_->addSeparator();

    // Numpad arrows, laid out the way they sit on the keypad.
    QMenu *moveMenu = videoMenu_->addMenu(tr("&Move Picture"));
    const struct { const char *text; int key; void (MainWindow::*slot)(); } moves[] = {
        {QT_TR_NOOP("Up"),     Qt::Key_8, &MainWindow::panVideoUp},
        {QT_TR_NOOP("Down"),   Qt::Key_2, &MainWindow::panVideoDown},
        {QT_TR_NOOP("Left"),   Qt::Key_4, &MainWindow::panVideoLeft},
        {QT_TR_NOOP("Right"),  Qt::Key_6, &MainWindow::panVideoRight},
        {QT_TR_NOOP("Center"), Qt::Key_5, &MainWindow::panVideoCenter},
    };
    for (const auto &move : moves) {
        auto *action = new QAction(tr(move.text), this);
        action->setShortcut(QKeySequence(Qt::KeypadModifier | move.key));
        // Application context, so panning still works in fullscreen where the
        // menu bar is hidden.
        action->setShortcutContext(Qt::ApplicationShortcut);
        registerMenuShortcut(action);
        connect(action, &QAction::triggered, this, move.slot);
        moveMenu->addAction(action);
    }

    QMenu *frameMenu = videoMenu_->addMenu(tr("Video &Frame"));
    auto *frameGroup = new QActionGroup(frameMenu);
    frameGroup->setExclusive(true);

    const QVector<VideoSettings::FrameModeInfo> &modes = VideoSettings::frameModes();
    for (int i = 0; i < modes.size(); ++i) {
        QAction *action = frameMenu->addAction(modes.at(i).name);
        action->setCheckable(true);
        frameGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, i] { setVideoFrameMode(i); });
        frameModeActions_.append(action);

        // Three groups: the untouched default, the fill modes, then the
        // stretch and zoom steps.
        const VideoSettings::FrameMode mode = modes.at(i).mode;
        if (mode == VideoSettings::FrameMode::Default
            || mode == VideoSettings::FrameMode::TouchWindowFromOutside) {
            frameMenu->addSeparator();
        }
    }

    // Picture zoom, separate from the Video Frame entries: those decide how
    // the picture is fitted to the window, this scales it afterwards.
    QMenu *zoomMenu = videoMenu_->addMenu(tr("&Zoom"));
    auto *zoomGroup = new QActionGroup(zoomMenu);
    zoomGroup->setExclusive(true);
    for (int percent : {25, 50, 100, 200}) {
        QAction *action = zoomMenu->addAction(tr("%1%").arg(percent));
        action->setCheckable(true);
        action->setChecked(percent == 100);
        zoomGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, percent] { setVideoZoomPercent(percent); });
        zoomActions_.append(action);
    }

    frameMenu->addSeparator();
    QMenu *aspectMenu = frameMenu->addMenu(tr("&Aspect Ratio"));
    auto *aspectGroup = new QActionGroup(aspectMenu);
    aspectGroup->setExclusive(true);

    const QVector<VideoSettings::AspectRatio> &ratios = VideoSettings::aspectRatios();
    for (const VideoSettings::AspectRatio &ratio : ratios) {
        QAction *action = aspectMenu->addAction(ratio.name);
        action->setCheckable(true);
        aspectGroup->addAction(action);
        const double value = ratio.value;
        connect(action, &QAction::triggered, this, [this, value] { setVideoAspectRatio(value); });
        aspectActions_.append(action);
    }

    // Numpad 0 steps through the frame modes. Application context so it still
    // works with the menu bar hidden.
    auto *cycleAction = new QAction(tr("Cycle Video Frame"), this);
    cycleAction->setShortcut(QKeySequence(Qt::KeypadModifier | Qt::Key_0));
    cycleAction->setShortcutContext(Qt::ApplicationShortcut);
        registerMenuShortcut(cycleAction);
    connect(cycleAction, &QAction::triggered, this, &MainWindow::cycleVideoFrameMode);
    videoMenu_->addAction(cycleAction);

    videoMenu_->addSeparator();
    // The key hint is in the label rather than a real shortcut, the same way
    // Fullscreen advertises F11: S is handled in VideoWidget::keyPressEvent, and
    // a QAction shortcut here would take the key away from it -- or go ambiguous
    // and fire neither. The context menu shows videoMenu_ itself, so this entry
    // appears there too.
    videoMenu_->addAction(tr("Take &Screenshot\tS"), this, &MainWindow::takeScreenshot);
    videoMenu_->addAction(tr("Media Information"), this, &MainWindow::openMediaInfoDialog);
}

void MainWindow::applyVideoState()
{
    player_->setFlipAndMirror(videoFlip_, videoMirror_);
    player_->setVideoRotate(videoRotation_);
    player_->setVideoAdjust(videoAdjust_.brightness, videoAdjust_.contrast,
                             videoAdjust_.saturation, videoAdjust_.hue);
    player_->setGamma(videoAdjust_.gamma);

    const QVector<VideoSettings::FrameModeInfo> &modes = VideoSettings::frameModes();
    const int index = std::clamp(videoFrameModeIndex_, 0, static_cast<int>(modes.size()) - 1);
    player_->setVideoFrame(modes.at(index).keepAspect, modes.at(index).panscan);
    // mpv's video-zoom is a log2 factor, so the frame mode's own zoom and the
    // Zoom submenu's percentage add rather than multiply. 100% contributes 0.
    const double zoomOffset = std::log2(videoZoomPercent_ / 100.0);
    player_->setVideoGeometry(modes.at(index).scaleY, modes.at(index).zoom + zoomOffset);
    if (index < frameModeActions_.size()) {
        frameModeActions_.at(index)->setChecked(true);
    }

    for (int i = 0; i < zoomActions_.size(); ++i) {
        static const int kZoomPercents[] = {25, 50, 100, 200};
        zoomActions_.at(i)->setChecked(kZoomPercents[i] == videoZoomPercent_);
    }

    player_->setAspectOverride(videoAspectRatio_);
    for (int i = 0; i < aspectActions_.size() && i < VideoSettings::aspectRatios().size(); ++i) {
        aspectActions_.at(i)->setChecked(
            qFuzzyCompare(VideoSettings::aspectRatios().at(i).value + 2.0, videoAspectRatio_ + 2.0));
    }

    player_->setVideoPan(videoPanX_, videoPanY_);
}

void MainWindow::saveVideoStateToFile()
{
    const bool flip = videoFlip_;
    const bool mirror = videoMirror_;
    const int rotation = videoRotation_;
    const int frameMode = videoFrameModeIndex_;
    const double aspect = videoAspectRatio_;
    const int zoomPercent = videoZoomPercent_;
    const VideoSettings::Adjust adjust = videoAdjust_;
    const double panX = videoPanX_;
    const double panY = videoPanY_;

    updateFileMemory([=](FileMemory &memory) {
        memory.hasVideoState = true;
        memory.flip = flip;
        memory.mirror = mirror;
        memory.rotation = rotation;
        memory.frameModeIndex = frameMode;
        memory.aspectRatio = aspect;
        memory.zoomPercent = zoomPercent;
        memory.panX = panX;
        memory.panY = panY;
        memory.brightness = adjust.brightness;
        memory.contrast = adjust.contrast;
        memory.saturation = adjust.saturation;
        memory.hue = adjust.hue;
        memory.gamma = adjust.gamma;
    });
}

void MainWindow::toggleVideoFlip()
{
    videoFlip_ = !videoFlip_;
    flipAction_->setChecked(videoFlip_);
    // Deliberately not written app-wide: flipping one video must not flip
    // every video opened afterwards.
    applyVideoState();
    saveVideoStateToFile();
    player_->showOsdMessage(videoFlip_ ? tr("Flip: on") : tr("Flip: off"), 1500);
}

void MainWindow::toggleVideoMirror()
{
    videoMirror_ = !videoMirror_;
    mirrorAction_->setChecked(videoMirror_);
    // Per-video only, same reasoning as the flip above.
    applyVideoState();
    saveVideoStateToFile();
    player_->showOsdMessage(videoMirror_ ? tr("Mirror: on") : tr("Mirror: off"), 1500);
}

void MainWindow::setVideoRotation(int degrees)
{
    videoRotation_ = degrees;
    player_->setVideoRotate(degrees);

    for (int i = 0; i < rotateActions_.size(); ++i) {
        rotateActions_.at(i)->setChecked(kRotations[i] == degrees);
    }

    // Rotation is per-file: a phone clip shot sideways should come back
    // upright next time, without rotating everything else too.
    saveVideoStateToFile();
    player_->showOsdMessage(tr("Rotation: %1°").arg(degrees), 1500);
}

void MainWindow::setVideoFrameMode(int index)
{
    const QVector<VideoSettings::FrameModeInfo> &modes = VideoSettings::frameModes();
    if (index < 0 || index >= modes.size()) {
        return;
    }

    videoFrameModeIndex_ = index;
    const VideoSettings::FrameModeInfo &mode = modes.at(index);

    player_->setVideoFrame(mode.keepAspect, mode.panscan);
    player_->setVideoGeometry(mode.scaleY, mode.zoom + std::log2(videoZoomPercent_ / 100.0));

    if (index < frameModeActions_.size()) {
        frameModeActions_.at(index)->setChecked(true);
    }
    player_->showOsdMessage(mode.name, 1500);

    // Per-video, like the rest of the video state.
    saveVideoStateToFile();
}

void MainWindow::setVideoAspectRatio(double ratio)
{
    videoAspectRatio_ = ratio;
    player_->setAspectOverride(ratio);

    const QVector<VideoSettings::AspectRatio> &ratios = VideoSettings::aspectRatios();
    for (int i = 0; i < ratios.size() && i < aspectActions_.size(); ++i) {
        // Offset before comparing: qFuzzyCompare cannot handle 0, which is a
        // real value here (assume square pixels).
        const bool selected = qFuzzyCompare(ratios.at(i).value + 2.0, ratio + 2.0);
        aspectActions_.at(i)->setChecked(selected);
        if (selected) {
            player_->showOsdMessage(ratios.at(i).name, 1500);
        }
    }

    saveVideoStateToFile();
}

namespace {
// One nudge, as a fraction of the video size -- the same 2% step the subtitle
// position entries use.
constexpr double kPanStep = 0.02;
} // namespace

void MainWindow::panVideoUp()
{
    // mpv's video-pan-y grows downwards, so moving the picture up subtracts.
    videoPanY_ -= kPanStep;
    applyVideoState();
    saveVideoStateToFile();
}

void MainWindow::panVideoDown()
{
    videoPanY_ += kPanStep;
    applyVideoState();
    saveVideoStateToFile();
}

void MainWindow::panVideoLeft()
{
    videoPanX_ -= kPanStep;
    applyVideoState();
    saveVideoStateToFile();
}

void MainWindow::panVideoRight()
{
    videoPanX_ += kPanStep;
    applyVideoState();
    saveVideoStateToFile();
}

void MainWindow::panVideoCenter()
{
    videoPanX_ = 0.0;
    videoPanY_ = 0.0;
    applyVideoState();
    saveVideoStateToFile();
}

void MainWindow::setVideoZoomPercent(int percent)
{
    videoZoomPercent_ = percent;

    for (int i = 0; i < zoomActions_.size(); ++i) {
        static const int kZoomPercents[] = {25, 50, 100, 200};
        zoomActions_.at(i)->setChecked(kZoomPercents[i] == percent);
    }

    applyVideoState();
    player_->showOsdMessage(tr("Zoom: %1%").arg(percent), 1500);
    saveVideoStateToFile();
}

void MainWindow::cycleVideoFrameMode()
{
    const QVector<VideoSettings::FrameModeInfo> &modes = VideoSettings::frameModes();

    // Default and the stretch/zoom series take part, so one key runs from
    // untouched through the steps and back. Window sizes and fill modes are
    // deliberately left out: cycling those would resize the window out from
    // under the viewer.
    QVector<int> cycle;
    for (int i = 0; i < modes.size(); ++i) {
        if (modes.at(i).inNumpadCycle) {
            cycle.append(i);
        }
    }
    if (cycle.isEmpty()) {
        return;
    }

    // Starting from a mode outside the series enters it at the first entry
    // rather than jumping to an arbitrary point.
    const int position = cycle.indexOf(videoFrameModeIndex_);
    const int next = position < 0 ? 0 : (position + 1) % cycle.size();
    setVideoFrameMode(cycle.at(next));
}

void MainWindow::updateFileMemory(const std::function<void(FileMemory &)> &mutate)
{
    if (currentMediaPath_.isEmpty()) {
        return;
    }
    FileMemory memory = FileMemoryStore::load(currentMediaPath_);
    mutate(memory);
    FileMemoryStore::save(currentMediaPath_, memory);
}

void MainWindow::restoreFileMemory()
{
    if (currentMediaPath_.isEmpty()) {
        return;
    }

    const FileMemory memory = FileMemoryStore::load(currentMediaPath_);

    if (!memory.subtitlePath.isEmpty()) {
        player_->loadExternalSubtitle(memory.subtitlePath);
    }
    if (!qFuzzyIsNull(memory.subtitleDelay)) {
        player_->setSubtitleDelay(memory.subtitleDelay);
    }
    // After the external file above, so a remembered track still wins if both
    // were set. Left alone when nothing was chosen, letting mpv pick.
    if (memory.subtitleTrackId >= 0) {
        player_->setSubtitleTrack(memory.subtitleTrackId);
    }
    if (memory.subtitleVisible >= 0) {
        player_->setSubtitleVisible(memory.subtitleVisible == 1);
    }
    if (!qFuzzyIsNull(memory.audioDelay)) {
        player_->setAudioDelay(memory.audioDelay);
    }

    const QSettings settings;
    if (memory.playbackPosition > 0.0
        && settings.value(SettingsKeys::kRememberPosition, SettingsKeys::kDefaultRememberPosition).toBool()) {
        player_->seekAbsolute(memory.playbackPosition);
        player_->showOsdMessage(tr("Resumed at %1").arg(formatTime(memory.playbackPosition)), 2000);
    }

    if (memory.hasVideoState) {
        // This file has been adjusted before, so its own values win over the
        // app-wide defaults that applyPreferences() put in place.
        videoFlip_ = memory.flip;
        videoMirror_ = memory.mirror;
        videoRotation_ = memory.rotation;
        // Entries written before the frame modes existed have no value here,
        // so they fall back to the default rather than to mode 0 (Half Size).
        videoFrameModeIndex_ = memory.frameModeIndex >= 0 ? memory.frameModeIndex
                                                           : VideoSettings::defaultFrameModeIndex();
        videoAspectRatio_ = memory.aspectRatio;
        videoZoomPercent_ = memory.zoomPercent;
        videoPanX_ = memory.panX;
        videoPanY_ = memory.panY;
        videoAdjust_ = {memory.brightness, memory.contrast, memory.saturation,
                         memory.hue, memory.gamma};
    } else {
        // A video with no memory of its own starts clean. Flip, mirror,
        // rotation and zoom are never inherited from another video -- only
        // the picture default from Settings is.
        videoFlip_ = false;
        videoMirror_ = false;
        videoRotation_ = 0;
        videoFrameModeIndex_ = VideoSettings::defaultFrameModeIndex();
        videoAspectRatio_ = -1.0;
        videoZoomPercent_ = 100;
        videoPanX_ = 0.0;
        videoPanY_ = 0.0;
        videoAdjust_ = VideoSettings::loadAdjust();
    }

    flipAction_->setChecked(videoFlip_);
    mirrorAction_->setChecked(videoMirror_);
    for (int i = 0; i < rotateActions_.size(); ++i) {
        rotateActions_.at(i)->setChecked(kRotations[i] == videoRotation_);
    }

    applyVideoState();
}

void MainWindow::refreshShowSubtitleMenu()
{
    showSubtitleMenu_->clear();
    qDeleteAll(subTrackActions_);
    subTrackActions_.clear();

    subEnabledAction_->setChecked(player_->isSubtitleVisible());
    showSubtitleMenu_->addAction(subEnabledAction_);
    showSubtitleMenu_->addSeparator();

    const QVector<MpvPlayer::TrackInfo> tracks = player_->subtitleTracks();
    auto *group = new QActionGroup(showSubtitleMenu_);
    group->setExclusive(true);

    for (const auto &track : tracks) {
        QString label = track.title.isEmpty() ? tr("Track %1").arg(track.id) : track.title;
        if (!track.language.isEmpty()) {
            label += QStringLiteral(" [%1]").arg(track.language);
        }
        if (track.external) {
            label += tr(" (file)");
        }

        auto *action = new QAction(label, this);
        action->setCheckable(true);
        action->setChecked(track.selected && player_->isSubtitleVisible());
        group->addAction(action);

        const int trackId = track.id;
        connect(action, &QAction::triggered, this, [this, trackId, label] {
            player_->setSubtitleTrack(trackId);
            player_->setSubtitleVisible(true);
            player_->showOsdMessage(tr("Subtitle track: %1").arg(label), 2000);
            // Which track -- and so which language -- is remembered for this
            // file, alongside the fact that subtitles were left showing.
            updateFileMemory([trackId](FileMemory &memory) {
                memory.subtitleTrackId = trackId;
                memory.subtitleVisible = 1;
            });
        });

        subTrackActions_.append(action);
        showSubtitleMenu_->addAction(action);
    }

    if (tracks.isEmpty()) {
        auto *none = new QAction(tr("(no subtitle tracks)"), this);
        none->setEnabled(false);
        subTrackActions_.append(none);
        showSubtitleMenu_->addAction(none);
    }

    showSubtitleMenu_->addSeparator();
    showSubtitleMenu_->addAction(subAddFileAction_);
}

void MainWindow::toggleSubtitleVisibility()
{
    const bool visible = !player_->isSubtitleVisible();
    player_->setSubtitleVisible(visible);
    player_->showOsdMessage(visible ? tr("Subtitles on") : tr("Subtitles off"), 1200);
    updateFileMemory([visible](FileMemory &memory) { memory.subtitleVisible = visible ? 1 : 0; });
}

void MainWindow::subtitlePositionReset()
{
    const SubtitleStyle style = SubtitleStyle::load();
    const SubtitleStyle effective = style.useDefaults ? SubtitleStyle::mpvDefaults() : style;
    player_->setSubtitleVerticalPosition(effective.verticalPosition);
    player_->setSubtitleAlignX(effective.align);
    player_->setSubtitleJustify(QStringLiteral("auto"));
}

void MainWindow::subtitleMoveUp()
{
    // sub-pos is already a percentage of frame height, so 2% is a plain -2.
    const double position = player_->subtitleVerticalPosition() - 2.0;
    player_->setSubtitleVerticalPosition(position);
    updateStoredSubtitleStyle([position](SubtitleStyle &style) { style.verticalPosition = position; });
}

void MainWindow::subtitleMoveDown()
{
    const double position = player_->subtitleVerticalPosition() + 2.0;
    player_->setSubtitleVerticalPosition(position);
    updateStoredSubtitleStyle([position](SubtitleStyle &style) { style.verticalPosition = position; });
}

void MainWindow::updateStoredSubtitleStyle(const std::function<void(SubtitleStyle &)> &mutate)
{
    SubtitleStyle style = SubtitleStyle::load();

    // Changing a value from the menu is an explicit choice, so the stored
    // style stops being "use the defaults". Without this the change could not
    // be represented at all, and the Settings dialog would keep showing the
    // default values with every control greyed out.
    if (style.useDefaults) {
        style = SubtitleStyle::mpvDefaults();
        style.useDefaults = false;
    }

    mutate(style);
    style.save();
}

void MainWindow::toggleSubtitleBold()
{
    const bool bold = !player_->isSubtitleBold();
    player_->setSubtitleBold(bold);
    subBoldAction_->setChecked(bold);
    player_->showOsdMessage(bold ? tr("Subtitles: bold") : tr("Subtitles: normal"), 1200);
    updateStoredSubtitleStyle([bold](SubtitleStyle &style) { style.bold = bold; });
}

void MainWindow::subtitleFontSizeUp()
{
    const double size = player_->subtitleFontSize() + 2.0;
    player_->setSubtitleFontSize(size);
    player_->showOsdMessage(tr("Subtitle size: %1").arg(size, 0, 'f', 0), 1200);
    updateStoredSubtitleStyle([size](SubtitleStyle &style) { style.fontSize = size; });
}

void MainWindow::subtitleFontSizeDown()
{
    const double size = player_->subtitleFontSize() - 2.0;
    player_->setSubtitleFontSize(size);
    player_->showOsdMessage(tr("Subtitle size: %1").arg(size, 0, 'f', 0), 1200);
    updateStoredSubtitleStyle([size](SubtitleStyle &style) { style.fontSize = size; });
}

void MainWindow::subtitleSyncEarlier()
{
    // "Faster" means the text should appear sooner, which is a smaller delay.
    const double delay = player_->subtitleDelay() - 0.5;
    player_->setSubtitleDelay(delay);
    player_->showOsdMessage(tr("Subtitle delay: %1 s").arg(delay, 0, 'f', 1), 1200);
    updateFileMemory([delay](FileMemory &memory) { memory.subtitleDelay = delay; });
}

void MainWindow::subtitleSyncLater()
{
    const double delay = player_->subtitleDelay() + 0.5;
    player_->setSubtitleDelay(delay);
    player_->showOsdMessage(tr("Subtitle delay: %1 s").arg(delay, 0, 'f', 1), 1200);
    updateFileMemory([delay](FileMemory &memory) { memory.subtitleDelay = delay; });
}

void MainWindow::subtitleSyncReset()
{
    player_->setSubtitleDelay(0.0);
    player_->showOsdMessage(tr("Subtitle delay: 0.0 s"), 1200);
    updateFileMemory([](FileMemory &memory) { memory.subtitleDelay = 0.0; });
}

void MainWindow::subtitleSyncSave()
{
    const double delay = player_->subtitleDelay();
    QSettings settings;
    settings.setValue(SettingsKeys::kSubSavedDelay, delay);
    player_->showOsdMessage(tr("Subtitle sync saved: %1 s").arg(delay, 0, 'f', 1), 2500);
}

void MainWindow::openSubtitleSettings()
{
    SettingsDialog dialog(this);
    dialog.showSubtitlesTab();
    runSettingsDialog(dialog);
}

void MainWindow::populateAudioMenu(QMenu *menu)
{
    menu->clear();
    populateTrackSelectionMenu(menu, this, player_->audioTracks(),
                                [this](int id, const QString &label) {
                                    player_->setAudioTrack(id);
                                    player_->showOsdMessage(tr("Audio track: %1").arg(label), 2000);
                                });
}

void MainWindow::openFromAnotherInstance(const QString &message)
{
    // Restore first: a minimised window cannot be raised, and the file would
    // otherwise start playing somewhere the user cannot see. Clearing just the
    // minimised bit rather than calling showNormal() keeps a window that was
    // maximised before it was minimised maximised afterwards.
    if (isMinimized()) {
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    }
    show();
    raise();
    activateWindow();

#ifdef Q_OS_WIN
    // Windows only lets the process that owns the foreground put a window
    // there, so activateWindow() and a bare SetForegroundWindow() are both
    // ignored here -- the window would un-minimise but stay behind whatever
    // the user was looking at.
    //
    // Briefly attaching this thread's input queue to the foreground window's
    // makes Windows treat the two as one input context, which is what allows
    // the call to succeed. The attachment is undone immediately.
    const HWND self = reinterpret_cast<HWND>(winId());
    const HWND foreground = GetForegroundWindow();
    const DWORD foregroundThread = GetWindowThreadProcessId(foreground, nullptr);
    const DWORD thisThread = GetCurrentThreadId();

    if (foreground && foregroundThread != thisThread) {
        AttachThreadInput(foregroundThread, thisThread, TRUE);
        SetForegroundWindow(self);
        BringWindowToTop(self);
        AttachThreadInput(foregroundThread, thisThread, FALSE);
    } else {
        SetForegroundWindow(self);
    }
#endif

    // An empty message means the second launch had no file of its own -- there
    // the only thing asked for is to come to the front.
    const QStringList paths = message.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (paths.isEmpty()) {
        return;
    }

    // Selecting several files in Explorer and pressing Enter starts one copy
    // of the player per file, each handing its own path over here. Taken at
    // face value every arrival would replace the one before it, so only the
    // last file of a selection would end up playing and the rest would be
    // lost. Arrivals that come in close together are therefore treated as one
    // selection: the first plays, the rest queue behind it.
    // Read before either call below, since both restart the timer.
    const bool partOfBurst = externalOpenTimer_.isValid()
        && externalOpenTimer_.elapsed() < kExternalOpenBurstMs;

    if (partOfBurst) {
        queueFiles(paths);
    } else {
        openFiles(paths);
    }
}

void MainWindow::loadFile(const QString &path)
{
    // Where the outgoing file was left, before the new one replaces it.
    savePlaybackPosition();

    currentMediaPath_ = path;
    player_->loadFile(path);
    player_->play();
    addToRecentFiles(path);
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        // Just clear the FullScreen bit rather than calling showNormal()/
        // showMaximized(): Qt::WindowMaximized can be combined with
        // Qt::WindowFullScreen in the same state mask, so if the window was
        // maximized before going fullscreen that bit is still set here and
        // gets restored automatically, atomically, in a single call --
        // no intermediate "restored size" frame flashing on screen.
        setWindowState(windowState() & ~Qt::WindowFullScreen);
        // The whole caption, not just the menus inside it: in fullscreen the
        // window buttons and the title have nothing to say either.
        titleBar_->show();

        // Back into the layout, under the video, where it takes its own space.
        fullscreenControlsTimer_->stop();
        controlsContainer_->setParent(centralWidget());
        centralLayout_->addWidget(controlsContainer_);
        controlsContainer_->show();
    } else {
        titleBar_->hide();

        // Reparented onto the video so it floats over the picture instead of
        // taking a strip off the bottom of it -- in fullscreen, shrinking the
        // video every time the bar appears is worse than covering a little of
        // it. Child widgets over a QOpenGLWidget are supported, and the
        // audio-only note and the OSD already rely on that.
        centralLayout_->removeWidget(controlsContainer_);
        controlsContainer_->setParent(videoWidget_);
        controlsContainer_->hide();
        setWindowState(windowState() | Qt::WindowFullScreen);
        // After the state change, so the video widget already has its
        // fullscreen size to be positioned against.
        QTimer::singleShot(0, this, &MainWindow::layoutFullscreenControls);
    }

    const QSignalBlocker fsActionBlocker(fullscreenAction_);
    const QSignalBlocker fsButtonBlocker(fullscreenButton_);
    fullscreenAction_->setChecked(isFullScreen());
    fullscreenButton_->setChecked(isFullScreen());

    // Reported on the next turn: the window has not been resized yet at this
    // point, so asking now would give the size it is leaving, not arriving at.
    const bool entering = isFullScreen();
    QTimer::singleShot(0, this, [this, entering] {
        player_->showOsdMessage(tr("%1   %2x%3")
                                     .arg(entering ? tr("Fullscreen") : tr("Windowed"))
                                     .arg(width())
                                     .arg(height()), 1800);
    });

    videoWidget_->setFocus();
}

void MainWindow::exitFullscreenIfActive()
{
    if (isFullScreen()) {
        toggleFullscreen();
    }
}

void MainWindow::onPositionChanged(double seconds)
{
    currentPosition_ = seconds;
    timelineSlider_->setPositionSeconds(seconds);
    refreshTimeLabel();
    updateTaskbarProgress();
}

void MainWindow::onDurationChanged(double seconds)
{
    currentDuration_ = seconds;
    timelineSlider_->setDurationSeconds(seconds);
    refreshTimeLabel();
    updateTaskbarProgress();
}

void MainWindow::onPauseChanged(bool paused)
{
    // The icon is the action the next click performs, not the current state:
    // paused shows Play, playing shows Pause. The button stays enabled either
    // way, since a single toggle always has something to do.
    //
    // An idle player counts as paused whatever mpv reports: mpv's "pause"
    // property is false before any file is loaded, which would otherwise show
    // the Pause icon on a player that is not playing anything.
    const bool showPlay = paused || !mediaLoaded_;
    playPauseButton_->setIcon(showPlay ? playIcon() : pauseIcon());
    playPauseButton_->setToolTip(showPlay ? tr("Play") : tr("Pause"));

    updateTaskbarProgress();
    updateSleepInhibit();
}

void MainWindow::updateSleepInhibit()
{
    // Windows has no idea a video is playing: it counts keyboard and mouse
    // input, and a film needs neither. mpv cannot speak up on our behalf
    // either -- its own stop-screensaver belongs to a video output that owns a
    // window, and this app renders through vo=libmpv instead. So the request
    // has to come from here, or the display blanks partway through a film.
    const bool playing = mediaLoaded_ && !player_->isPaused();

    EXECUTION_STATE state = ES_CONTINUOUS;
    if (playing) {
        // Music should not hold the display on -- the point of listening with
        // the screen off is that it goes off. It must still keep the machine
        // awake, or playback stops when the system sleeps.
        state |= ES_SYSTEM_REQUIRED;
        if (hasVideo_) {
            state |= ES_DISPLAY_REQUIRED;
        }
    }

    // ES_CONTINUOUS makes the state stick until it is replaced, so a bare
    // ES_CONTINUOUS is what releases it. Reasserted only on change: this runs
    // on every pause toggle.
    if (state != sleepInhibitState_) {
        SetThreadExecutionState(state);
        sleepInhibitState_ = state;
    }
}

void MainWindow::onVolumeChanged(int volume0to200)
{
    volumeSlider_->setVolumeSilently(volume0to200);
    // Only once something is playing: mpv reports its startup volume before
    // any file is open, which would flash an OSD message on an idle player.
    if (mediaLoaded_) {
        player_->showOsdMessage(player_->isMuted() ? tr("Muted")
                                                    : tr("Volume: %1%").arg(volume0to200),
                                 1200);
    }
}

void MainWindow::onMuteChanged(bool muted)
{
    const QSignalBlocker blocker(muteButton_);
    muteButton_->setChecked(muted);
    muteButton_->setIcon(volumeIcon(muted));
    if (mediaLoaded_) {
        player_->showOsdMessage(muted ? tr("Muted") : tr("Volume: %1%").arg(player_->volume()), 1200);
    }
}

void MainWindow::onSpeedChanged(double speed)
{
    // Matched on the value, not the label: a speed set from elsewhere used to
    // leave the menu ticking the old figure whenever the two spellings of the
    // same number disagreed. A speed mpv reached by some other route (a
    // config, a future hotkey) simply leaves nothing ticked.
    for (QAction *action : speedActions_) {
        const QSignalBlocker blocker(action);
        action->setChecked(qFuzzyCompare(action->data().toDouble(), speed));
    }
    if (mediaLoaded_) {
        player_->showOsdMessage(tr("Speed: %1x").arg(speed, 0, 'g', 3), 1200);
    }
}

void MainWindow::onMediaTitleChanged(const QString &title)
{
    if (!title.isEmpty()) {
        setMediaTitle(title);
    }
}

void MainWindow::onFileLoaded(const QString &filename)
{
    // A new file always has something to show, whatever the last one left
    // behind. Belt and braces alongside mpv's own eof-reached going false.
    videoWidget_->setBlankScreen(false);

    // Before the fit is armed below: a maximized window has no size of its own
    // to set, and fitWindowToVideoSize() bows out when it sees one. Fullscreen
    // is left alone -- it already fills the screen, and dropping out of it to
    // maximize would be the opposite of what was asked.
    if (openMaximized_ && !isFullScreen() && !isMaximized()) {
        showMaximized();
    }

    // Armed here and spent on the first size mpv reports for this file, so a
    // later rotation or aspect change does not move the window again.
    pendingFitToVideo_ = fitWindowToVideo_;

    // From here on mpv's pause state is meaningful for the Play/Pause icon.
    mediaLoaded_ = true;
    // Arms the taskbar for this file. Stop, the end of playback and deleting
    // the file each disarm it again.
    taskbarActive_ = true;
    onPauseChanged(player_->isPaused());

    // The file name stands in until mpv reports a media title of its own,
    // which for most files never happens.
    if (mediaTitle_.isEmpty() && !filename.isEmpty()) {
        setMediaTitle(filename);
    }

    // Subtitle properties are reset by mpv on every new file, so the stored
    // style and any saved sync have to be re-applied once it is loaded.
    player_->applySubtitleStyle(SubtitleStyle::load());

    // Chapters only exist once the file is open, so the ticks are filled in
    // here rather than when loading was requested.
    refreshChapterMarkers();

    // Same for the video filter chain, which mpv clears per file.
    applyVideoState();
    // Then this file's own remembered subtitle, sync and rotation, which take
    // precedence over the app-wide defaults just applied.
    restoreFileMemory();

    const QSettings settings;
    const QVariant savedDelay = settings.value(SettingsKeys::kSubSavedDelay);
    if (savedDelay.isValid()) {
        player_->setSubtitleDelay(savedDelay.toDouble());
    }

    // Tags are only readable once the file is open, and mpv clears the margin
    // with every new file, so both ends are settled here.
    updateAudioTrackInfo();
}

void MainWindow::updateAudioTrackInfo()
{
    // Music, as opposed to video: either no video track at all, or the only one
    // is the still mpv shows for cover art.
    const bool audioFile = mediaLoaded_ && (!hasVideo_ || player_->isAlbumArt());
    if (!audioFile || !showAudioTrackInfo_) {
        videoWidget_->setAudioInfo(QString(), {});
        player_->setVideoRightMargin(0.0);
        return;
    }

    // Tag names vary with the container -- ID3 writes "title", Matroska
    // "TITLE", Vorbis comments either -- so they are matched without case.
    const QMap<QString, QString> tags = player_->metadata();
    const auto tag = [&tags](std::initializer_list<const char *> names) {
        for (const char *name : names) {
            for (auto it = tags.constBegin(); it != tags.constEnd(); ++it) {
                if (it.key().compare(QLatin1String(name), Qt::CaseInsensitive) == 0
                    && !it.value().trimmed().isEmpty()) {
                    return it.value().trimmed();
                }
            }
        }
        return QString();
    };

    // The file name is the fallback title, which is what the title bar is
    // already showing for an untagged file.
    QString title = tag({"title"});
    if (title.isEmpty()) {
        title = mediaTitle_;
    }

    QStringList details;
    const QString artist = tag({"artist", "album_artist", "performer"});
    if (!artist.isEmpty()) {
        details << artist;
    }
    QString album = tag({"album"});
    if (!album.isEmpty()) {
        // The year belongs to the album line rather than one of its own; mpv
        // hands back either a bare year or a full date, and only the year is
        // worth the room.
        const QString year = tag({"date", "year"}).left(4);
        if (year.size() == 4) {
            album += QStringLiteral(" (%1)").arg(year);
        }
        details << album;
    }
    const QString track = tag({"track"});
    if (!track.isEmpty()) {
        details << tr("Track %1").arg(track);
    }
    const QString genre = tag({"genre"});
    if (!genre.isEmpty()) {
        details << genre;
    }

    videoWidget_->setAudioInfo(title, details);
    // Half the window kept clear for the panel; the art fits itself into the
    // rest. Harmless for a file with no art at all, where mpv draws nothing.
    player_->setVideoRightMargin(0.5);
}

void MainWindow::onVideoDisplaySizeChanged(const QSize &size)
{
    if (!pendingFitToVideo_ || size.isEmpty()) {
        return;
    }
    pendingFitToVideo_ = false;
    fitWindowToVideoSize(size);
}

void MainWindow::fitWindowToVideoSize(const QSize &videoSize)
{
    // Fullscreen has no size of its own to set, and resizing out of a
    // maximized window would be a surprise -- the user asked for it to fill
    // the screen.
    if (isFullScreen() || isMaximized() || videoSize.isEmpty()) {
        return;
    }

    // Everything that is not the video surface: menu bar, control bar,
    // timeline, the playlist panel when it is open. Measured rather than
    // assumed, so it stays right as the layout changes.
    const QSize chrome = size() - videoWidget_->size();
    // Title bar and borders, which sit outside size() but still take room on
    // the screen. Zero until the window has actually been framed.
    const QSize frame = (frameGeometry().size() - size()).expandedTo(QSize(0, 0));

    // availableGeometry(), not geometry(): the taskbar's strip is not room the
    // window may occupy. Taken from the screen this window is on, so it is the
    // right monitor's size in a multi-monitor setup.
    const QRect available = screen()->availableGeometry();
    const QSize maxWindow = (available.size() - frame).expandedTo(QSize(1, 1));
    const QSize room = maxWindow - chrome;

    // Scale the picture down if it would not fit, keeping its shape. Never
    // scaled up: a 320x240 clip should not stretch the window across the
    // desktop.
    QSize picture = videoSize;
    if (room.width() > 0 && room.height() > 0
        && (picture.width() > room.width() || picture.height() > room.height())) {
        picture.scale(room, Qt::KeepAspectRatio);
    }

    // Hard cap, applied whatever the scaling above decided. It is what
    // guarantees the window fits: the branch is skipped when the chrome alone
    // is taller or wider than the screen, and the frame is unknown until the
    // window has been shown once, so neither can be relied on by itself.
    // Squaring off the aspect ratio is the right trade when the alternative is
    // a window running off the screen.
    resize((picture + chrome).boundedTo(maxWindow));

    // Settled on the next turn of the event loop rather than here: resize()
    // only requests a size, and on Windows frameGeometry() still reports the
    // old rectangle until the window manager has answered. Measuring it now
    // would clamp against stale numbers.
    QTimer::singleShot(0, this, &MainWindow::clampWindowToScreen);
}

void MainWindow::clampWindowToScreen()
{
    if (isFullScreen() || isMaximized()) {
        return;
    }

    const QRect available = screen()->availableGeometry();

    // Whatever the frame really turned out to be, the window must not be
    // bigger than the screen it is on. A layout minimum can still stop the
    // shrink; nothing more can be done about that here.
    const QSize overflow = (frameGeometry().size() - available.size())
                                .expandedTo(QSize(0, 0));
    if (!overflow.isNull()) {
        resize(size() - overflow);
    }

    // A window grown near an edge can end up partly off-screen; pull it back.
    // Left/top last, so a window still too large is clipped at the bottom
    // right rather than having its title bar pushed out of reach.
    QRect placed = frameGeometry();
    placed.moveRight(std::min(placed.right(), available.right()));
    placed.moveBottom(std::min(placed.bottom(), available.bottom()));
    placed.moveLeft(std::max(placed.left(), available.left()));
    placed.moveTop(std::max(placed.top(), available.top()));
    if (placed.topLeft() != frameGeometry().topLeft()) {
        move(placed.topLeft());
    }
}

void MainWindow::onErrorOccurred(const QString &message)
{
    // One box at a time. Errors can arrive in bursts, and because the box is
    // modal each one would queue behind the last, so closing one would just
    // reveal another with no way out.
    if (errorDialogOpen_) {
        return;
    }

    errorDialogOpen_ = true;
    QMessageBox::warning(this, tr("Playback Error"), message);
    errorDialogOpen_ = false;
}

void MainWindow::onVolumeSliderMoved(int value)
{
    player_->setVolume(value);
}

void MainWindow::layoutFullscreenControls()
{
    if (!isFullScreen() || controlsContainer_->parentWidget() != videoWidget_) {
        return;
    }
    const int height = controlsContainer_->sizeHint().height();
    controlsContainer_->setGeometry(0, videoWidget_->height() - height,
                                     videoWidget_->width(), height);
    controlsContainer_->raise();
}

void MainWindow::onVideoMouseMoved(int y, int height)
{
    if (!isFullScreen()) {
        return;
    }

    // The band along the bottom that summons the bar. Measured from the
    // bottom of the video, and at least as tall as the bar itself, so the
    // pointer never has to cross a dead gap to reach it.
    const int zone = std::max(kFullscreenHoverZone, controlsContainer_->sizeHint().height());
    if (y >= height - zone) {
        layoutFullscreenControls();
        controlsContainer_->show();
        fullscreenControlsTimer_->start();
    } else if (controlsContainer_->isVisible()) {
        // Moving away hides it at once rather than waiting out the timer:
        // having deliberately left, waiting two seconds looks like a stall.
        fullscreenControlsTimer_->stop();
        controlsContainer_->hide();
    }
}

void MainWindow::setMediaTitle(const QString &title)
{
    mediaTitle_ = title;
    // The bar shows the file alone; the window title keeps the application
    // name, which is what the task bar and Alt-Tab have to identify.
    setWindowTitle(title.isEmpty() ? tr("Pear Player")
                                   : tr("%1 - Pear Player").arg(title));
    if (titleBar_) {
        titleBar_->setMediaTitle(title);
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && titleBar_) {
        titleBar_->syncWindowState();
        updateResizeBorder();
    }
}

void MainWindow::updateResizeBorder()
{
    // Maximised and fullscreen windows are not resized by dragging, so the
    // band is closed up and the video goes back to filling the width.
    const int border = (isMaximized() || isFullScreen()) ? 0 : TitleBar::kResizeBorder;
    // No top margin: that edge is the title bar's.
    centralLayout_->setContentsMargins(border, 0, border, border);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == central_ && !isMaximized() && !isFullScreen()
        && (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress)) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        const Qt::Edges edges = resizeEdgesAt(mouseEvent->position().toPoint(), central_->size());
        if (event->type() == QEvent::MouseMove) {
            // Only the band's own moves arrive here -- every child widget
            // keeps its own events, and its own cursor with them.
            central_->setCursor(cursorForEdges(edges));
        } else if (edges && mouseEvent->button() == Qt::LeftButton && windowHandle()) {
            windowHandle()->startSystemResize(edges);
            return true;
        }
    }

    if (watched == controlsContainer_ && isFullScreen()) {
        switch (event->type()) {
        case QEvent::Enter:
            // Pointer is on the bar; nothing should take it away.
            fullscreenControlsTimer_->stop();
            break;
        case QEvent::Leave:
            fullscreenControlsTimer_->start();
            break;
        default:
            break;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::showSeekOsd(double deltaSeconds)
{
    if (!mediaLoaded_) {
        return;
    }

    // Where the seek is heading, worked out here rather than read back from
    // mpv: the seek is asynchronous, so time-pos still holds the old position
    // for a frame or two and the message would lag a step behind.
    const double target = std::clamp(currentPosition_ + deltaSeconds, 0.0,
                                      currentDuration_ > 0.0 ? currentDuration_ : currentPosition_);

    const QString position = currentDuration_ > 0.0
        ? tr("%1 / %2").arg(formatTime(target), formatTime(currentDuration_))
        : formatTime(target);

    if (qFuzzyIsNull(deltaSeconds)) {
        player_->showOsdMessage(position, 1200);
        return;
    }

    // Named as well as signed: "Forward 10s" says which way at a glance, where
    // a bare "+10s" has to be read. The amount is shown because the jump is a
    // setting -- this also answers "how far does an arrow key actually move".
    const QString step = deltaSeconds > 0
        ? tr("Forward +%1s").arg(std::abs(deltaSeconds), 0, 'g', 3)
        : tr("Backward -%1s").arg(std::abs(deltaSeconds), 0, 'g', 3);
    player_->showOsdMessage(tr("%1   %2").arg(step, position), 1200);
}

void MainWindow::takeScreenshot()
{
    if (!mediaLoaded_) {
        return;
    }

    const QString directory = SettingsKeys::screenshotDirectory();
    // Created on demand rather than at startup: the folder is a setting, so it
    // can be pointed somewhere that does not exist yet.
    if (!QDir().mkpath(directory)) {
        player_->showOsdMessage(tr("Cannot create %1").arg(directory), 3000);
        return;
    }

    // Named after the file and the position within it, so shots of the same
    // film sort together and two frames never collide -- unlike a plain
    // counter, which restarts every session and overwrites yesterday's.
    const QString stem = currentMediaPath_.isEmpty()
        ? tr("screenshot")
        : QFileInfo(currentMediaPath_).completeBaseName();
    const QString stamp = QStringLiteral("%1h%2m%3s")
                               .arg(int(currentPosition_) / 3600, 2, 10, QLatin1Char('0'))
                               .arg((int(currentPosition_) / 60) % 60, 2, 10, QLatin1Char('0'))
                               .arg(int(currentPosition_) % 60, 2, 10, QLatin1Char('0'));

    QString path = QStringLiteral("%1/%2 %3.png").arg(directory, stem, stamp);
    // Paused on the same frame, S can be pressed twice; the second shot gets a
    // suffix instead of replacing the first.
    for (int attempt = 2; QFileInfo::exists(path) && attempt < 100; ++attempt) {
        path = QStringLiteral("%1/%2 %3 (%4).png").arg(directory, stem, stamp).arg(attempt);
    }

    if (player_->takeScreenshot(path)) {
        player_->showOsdMessage(tr("Saved %1").arg(QFileInfo(path).fileName()), 2500);
    } else {
        player_->showOsdMessage(tr("Screenshot failed"), 2500);
    }
}

void MainWindow::onSpeedStepRequested(double delta)
{
    // Rounded to one decimal at every step: adding 0.1 repeatedly in binary
    // floating point drifts (0.7999999...), which would show as "0.8x" while
    // never matching the 0.75x/1x menu entries it passes through.
    const double stepped = std::round((player_->speed() + delta) * 10.0) / 10.0;
    player_->setSpeed(std::clamp(stepped, kMinSpeed, kMaxSpeed));
}

void MainWindow::onVolumeStepRequested(int delta)
{
    player_->setVolume(std::clamp(player_->volume() + delta, 0, maxVolume_));
}

void MainWindow::runSettingsDialog(SettingsDialog &dialog)
{
    // What is on screen right now, which may be this file's own remembered
    // values rather than the app-wide defaults. Cancel has to come back here,
    // not to whatever is in QSettings.
    const VideoSettings::Adjust adjustBefore = videoAdjust_;
    // The app-wide picture default, kept so it can be put back: with a video
    // open the dialog edits that video, not the default for every other one.
    const VideoSettings::Adjust globalAdjustBefore = VideoSettings::loadAdjust();

    // Only what was actually edited in this dialog session is applied on OK or
    // undone on Cancel. Without these, pressing OK after changing nothing but
    // a hotkey would still pin the picture to the current video, and Cancel
    // would re-apply settings the user never touched.
    bool pictureTouched = false;
    bool subtitlesTouched = false;

    connect(&dialog, &SettingsDialog::videoAdjustPreviewed, this,
            [&](int brightness, int contrast, int saturation, int hue, int gamma) {
                pictureTouched = true;
                videoAdjust_ = {brightness, contrast, saturation, hue, gamma};
                player_->setVideoAdjust(brightness, contrast, saturation, hue);
                player_->setGamma(gamma);
            });
    connect(&dialog, &SettingsDialog::subtitleStylePreviewed, this,
            [&](const SubtitleStyle &style) {
                subtitlesTouched = true;
                player_->applySubtitleStyle(style);
            });

    // Read before the dialog stores its own, so the two can be compared.
    const QString languageBefore = Translation::currentLanguageCode();

    if (dialog.exec() == QDialog::Accepted) {
        const bool pinToFile = pictureTouched && !currentMediaPath_.isEmpty();
        const VideoSettings::Adjust dialledIn = VideoSettings::loadAdjust();

        const QString languageAfter = Translation::currentLanguageCode();
        if (languageAfter != languageBefore) {
            if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance())) {
                Translation::applyLanguage(*app, languageAfter);
                retranslateUi();
            }
        }

        if (pinToFile) {
            // Put the app-wide default back: with a video open these sliders
            // belong to that video, so leaving them stored globally would
            // apply them to every video opened afterwards -- which is exactly
            // what "remember per video" must not do. Set the default instead
            // by opening Settings with nothing playing.
            VideoSettings::saveAdjust(globalAdjustBefore);
        }

        applyPreferences();

        if (pinToFile) {
            // applyPreferences() just reloaded the (restored) global default,
            // so put the dialled-in picture back before pinning it to this file.
            videoAdjust_ = dialledIn;
            applyVideoState();
            saveVideoStateToFile();
        }
    } else {
        // Undo only the live previews this session produced.
        if (pictureTouched) {
            videoAdjust_ = adjustBefore;
            applyVideoState();
        }
        if (subtitlesTouched) {
            player_->applySubtitleStyle(SubtitleStyle::load());
        }
    }
}

void MainWindow::openSettingsDialog()
{
    SettingsDialog dialog(this);
    runSettingsDialog(dialog);
}

void MainWindow::openEqualizerDialog()
{
    EqualizerDialog dialog(this);
    // Applied while dragging rather than on close: an equaliser is set by ear,
    // so the change has to be audible as the slider moves.
    connect(&dialog, &EqualizerDialog::gainsChanged, this, [this](const QVector<int> &gains) {
        player_->setEqualizerGains(gains);
    });
    connect(&dialog, &EqualizerDialog::presetApplied, this, [this](const QString &name) {
        player_->showOsdMessage(tr("Equalizer: %1").arg(name), 2000);
    });
    dialog.exec();
}

void MainWindow::openMediaInfoDialog()
{
    MediaInfoDialog dialog(*player_, currentMediaPath_, this);
    dialog.exec();
}

void MainWindow::openAboutDialog()
{
    AboutDialog dialog(this);
    dialog.exec();
}


double MainWindow::computeJumpSeconds() const
{
    if (seekMode_ == QStringLiteral("percentage")) {
        return std::max(currentDuration_ * (seekPercentage_ / 100.0), seekMinSeconds_);
    }
    return seekSeconds_;
}

void MainWindow::stopPlayback()
{
    player_->stop();
    taskbarActive_ = false;
    taskbarProgress_->clear();
}

void MainWindow::updateTaskbarProgress()
{
    // Recomputed from scratch on every input change rather than latched when
    // one particular event happens to arrive. mpv reports "not paused" before
    // it reports the duration, so deciding this only inside onPauseChanged()
    // meant the duration was still zero at the moment it was asked, and the
    // taskbar stayed blank for the whole file.
    if (!taskbarActive_ || currentDuration_ <= 0.0) {
        taskbarProgress_->clear();
        return;
    }

    taskbarProgress_->setPlaying(!player_->isPaused());

    // A file that has just started sits at 0%, and Windows draws a 0% bar as
    // nothing whatsoever -- so the button looked unchanged for the first
    // minutes of a long file even though its state was already "playing".
    // A small floor makes starting playback visible straight away; it only
    // affects the opening moments, since real progress passes it almost at
    // once on anything short and it is a sliver either way.
    constexpr double kMinVisibleProgress = 0.02;
    taskbarProgress_->setProgress(
        std::max(currentPosition_ / currentDuration_, kMinVisibleProgress));
}

void MainWindow::refreshTimeLabel()
{
    const QString elapsed = formatTime(currentPosition_);
    const QString total = formatTime(currentDuration_);
    const QString remaining = formatTime(std::max(0.0, currentDuration_ - currentPosition_));
    timeLabel_->setText(QStringLiteral("%1 / %2 (-%3)").arg(elapsed, total, remaining));
}

QString MainWindow::formatTime(double seconds)
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



