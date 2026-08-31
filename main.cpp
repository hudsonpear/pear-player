#include "MainWindow.h"
#include "Theme.h"
#include "Translation.h"
#include "SingleInstance.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>

namespace {

// Collects every pre-scaled size into one QIcon so the title bar, task bar and
// Alt-Tab switcher each pick the artwork drawn for their exact pixel size
// rather than a runtime downscale of the largest one.
QIcon applicationIcon()
{
    QIcon icon;
    for (int size : {16, 20, 24, 32, 40, 48, 64, 128, 256}) {
        icon.addFile(QStringLiteral(":/app/resources/icons/pearicon-%1.png").arg(size),
                     QSize(size, size));
    }
    return icon;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName(QStringLiteral("PearPlayer"));
    QCoreApplication::setApplicationName(QStringLiteral("PearPlayer"));
    // Single source for the version the About box shows; keep in step with
    // the FILEVERSION/PRODUCTVERSION block in app.rc.
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QApplication app(argc, argv);

    // Every argument, not just the first: Explorer can hand a whole selection
    // to one process, and the player takes them as "play this, queue those".
    const QStringList startupFiles = QCoreApplication::arguments().mid(1);
    // Newline-separated for the handover, since the socket carries plain text
    // and no path may contain a newline on Windows.
    const QString startupFile = startupFiles.join(QLatin1Char('\n'));

    // Only one window: if a copy is already running, hand it the file and stop
    // here. Done before any UI is built so a second launch costs nothing
    // visible -- no window flashes up on its way to closing again.
    SingleInstance instance(QStringLiteral("PearPlayer.SingleInstance"));
    if (instance.sendToPrimary(startupFile)) {
        return 0;
    }
    instance.listen();

    // Before any widget is built: strings are pulled through tr() as the
    // window is constructed, so a translator installed afterwards would leave
    // everything already created in English.
    Translation::install(app);

    QApplication::setWindowIcon(applicationIcon());
    Theme::applyDark(app);

    MainWindow window;
    QObject::connect(&instance, &SingleInstance::messageReceived,
                     &window, &MainWindow::openFromAnotherInstance);

    window.show();

    if (!startupFiles.isEmpty()) {
        window.openFiles(startupFiles);
    }

    return app.exec();
}
