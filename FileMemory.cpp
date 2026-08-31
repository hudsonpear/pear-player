#include "FileMemory.h"
#include "SettingsKeys.h"

#include <QSettings>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QUrl>

namespace FileMemoryStore {

namespace {

/// Local files are keyed by their absolute path so the same file opened by a
/// relative path or through the playlist resolves to one entry; URLs are used
/// as given, since there is no filesystem to canonicalise them against.
QString canonicalKeySource(const QString &mediaPath)
{
    if (!QUrl(mediaPath).scheme().isEmpty() && !QFileInfo(mediaPath).exists()) {
        return mediaPath;
    }
    const QFileInfo info(mediaPath);
    const QString absolute = info.absoluteFilePath();
    return absolute.isEmpty() ? mediaPath : absolute;
}

QString entryGroup(const QString &mediaPath)
{
    const QByteArray digest = QCryptographicHash::hash(
        canonicalKeySource(mediaPath).toUtf8(), QCryptographicHash::Sha1);
    return SettingsKeys::kFileMemoryGroup + QLatin1Char('/') + QString::fromLatin1(digest.toHex());
}

} // namespace

FileMemory load(const QString &mediaPath)
{
    if (mediaPath.isEmpty()) {
        return {};
    }

    QSettings settings;
    settings.beginGroup(entryGroup(mediaPath));
    FileMemory memory;
    memory.subtitlePath = settings.value(QStringLiteral("subtitlePath")).toString();
    memory.subtitleDelay = settings.value(QStringLiteral("subtitleDelay"), 0.0).toDouble();
    memory.subtitleTrackId = SettingsKeys::readSignedInt(
        settings.value(QStringLiteral("subtitleTrackId")), -1);
    memory.subtitleVisible = SettingsKeys::readSignedInt(
        settings.value(QStringLiteral("subtitleVisible")), -1);
    memory.hasVideoState = settings.value(QStringLiteral("hasVideoState"), false).toBool();
    memory.rotation = settings.value(QStringLiteral("rotation"), 0).toInt();
    memory.flip = settings.value(QStringLiteral("flip"), false).toBool();
    memory.mirror = settings.value(QStringLiteral("mirror"), false).toBool();
    // Signed: these four can be negative, which the registry stores in a way
    // plain toInt() cannot read back (see SettingsKeys::readSignedInt).
    memory.brightness = SettingsKeys::readSignedInt(settings.value(QStringLiteral("brightness")));
    memory.contrast = SettingsKeys::readSignedInt(settings.value(QStringLiteral("contrast")));
    memory.saturation = SettingsKeys::readSignedInt(settings.value(QStringLiteral("saturation")));
    memory.hue = SettingsKeys::readSignedInt(settings.value(QStringLiteral("hue")));
    memory.frameModeIndex = SettingsKeys::readSignedInt(
        settings.value(QStringLiteral("frameModeIndex")), -1);
    memory.aspectRatio = settings.value(QStringLiteral("aspectRatio"), -1.0).toDouble();
    memory.zoomPercent = settings.value(QStringLiteral("zoomPercent"), 100).toInt();
    memory.panX = settings.value(QStringLiteral("panX"), 0.0).toDouble();
    memory.panY = settings.value(QStringLiteral("panY"), 0.0).toDouble();
    memory.gamma = SettingsKeys::readSignedInt(settings.value(QStringLiteral("gamma")));
    memory.playbackPosition = settings.value(QStringLiteral("playbackPosition"), -1.0).toDouble();
    memory.audioDelay = settings.value(QStringLiteral("audioDelay"), 0.0).toDouble();
    settings.endGroup();

    // Entries written before rotation joined the video block still carry a
    // rotation but no flag; treat those as video state so they keep working.
    if (memory.rotation != 0) {
        memory.hasVideoState = true;
    }

    // A subtitle file that has since been moved or deleted would otherwise
    // make mpv report a load error on every play.
    if (!memory.subtitlePath.isEmpty() && !QFileInfo::exists(memory.subtitlePath)) {
        memory.subtitlePath.clear();
    }

    return memory;
}

void save(const QString &mediaPath, const FileMemory &memory)
{
    if (mediaPath.isEmpty()) {
        return;
    }

    if (memory.isEmpty()) {
        remove(mediaPath); // back to defaults: drop the entry entirely
        return;
    }

    QSettings settings;
    settings.beginGroup(entryGroup(mediaPath));
    // Kept so the stored data can be read by a human, and so a future
    // "manage remembered files" list has something to show.
    settings.setValue(QStringLiteral("mediaPath"), canonicalKeySource(mediaPath));
    settings.setValue(QStringLiteral("subtitlePath"), memory.subtitlePath);
    settings.setValue(QStringLiteral("subtitleDelay"), memory.subtitleDelay);
    settings.setValue(QStringLiteral("subtitleTrackId"), memory.subtitleTrackId);
    settings.setValue(QStringLiteral("subtitleVisible"), memory.subtitleVisible);
    settings.setValue(QStringLiteral("hasVideoState"), memory.hasVideoState);
    settings.setValue(QStringLiteral("rotation"), memory.rotation);
    settings.setValue(QStringLiteral("flip"), memory.flip);
    settings.setValue(QStringLiteral("mirror"), memory.mirror);
    settings.setValue(QStringLiteral("brightness"), memory.brightness);
    settings.setValue(QStringLiteral("contrast"), memory.contrast);
    settings.setValue(QStringLiteral("saturation"), memory.saturation);
    settings.setValue(QStringLiteral("hue"), memory.hue);
    settings.setValue(QStringLiteral("frameModeIndex"), memory.frameModeIndex);
    settings.setValue(QStringLiteral("aspectRatio"), memory.aspectRatio);
    settings.setValue(QStringLiteral("zoomPercent"), memory.zoomPercent);
    settings.setValue(QStringLiteral("panX"), memory.panX);
    settings.setValue(QStringLiteral("panY"), memory.panY);
    settings.setValue(QStringLiteral("gamma"), memory.gamma);
    settings.setValue(QStringLiteral("playbackPosition"), memory.playbackPosition);
    settings.setValue(QStringLiteral("audioDelay"), memory.audioDelay);
    settings.endGroup();
}

void remove(const QString &mediaPath)
{
    if (mediaPath.isEmpty()) {
        return;
    }
    QSettings settings;
    settings.remove(entryGroup(mediaPath));
}

void clearAll()
{
    QSettings settings;
    settings.remove(SettingsKeys::kFileMemoryGroup);
}

int count()
{
    QSettings settings;
    settings.beginGroup(SettingsKeys::kFileMemoryGroup);
    const int total = settings.childGroups().size();
    settings.endGroup();
    return total;
}

} // namespace FileMemoryStore
