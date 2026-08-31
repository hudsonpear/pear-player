#include "Translation.h"
#include "SettingsKeys.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace Translation
{
namespace {

/// Serves translations straight out of a hash keyed by the English text.
///
/// Qt calls translate() with a context (the class name) as well as the source
/// text. The context is deliberately ignored: a translator should not have to
/// know that "Open..." belongs to MainWindow, and the same English string
/// wants the same translation wherever it appears in a player this size.
class JsonTranslator : public QTranslator
{
public:
    bool load(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }

        QJsonParseError error{};
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            return false;
        }

        const QJsonObject strings = document.object().value(QStringLiteral("strings")).toObject();
        for (auto it = strings.constBegin(); it != strings.constEnd(); ++it) {
            const QString translated = it.value().toString();
            // An empty value means "not translated yet", which must fall back
            // to English rather than blanking the label.
            if (!translated.isEmpty()) {
                strings_.insert(it.key(), translated);
            }
        }
        return !strings_.isEmpty();
    }

    [[nodiscard]] bool isEmpty() const override { return strings_.isEmpty(); }

    QString translate(const char *context, const char *sourceText,
                      const char *disambiguation, int n) const override
    {
        Q_UNUSED(context);
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        // Returning a null string tells Qt to use the source text, which is
        // the English original -- exactly the fallback wanted for a partial
        // translation.
        return strings_.value(QString::fromUtf8(sourceText));
    }

private:
    QHash<QString, QString> strings_;
};

} // namespace

QString translationsDirectory()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
}

QVector<Language> availableLanguages()
{
    QVector<Language> languages;

    const QDir directory(translationsDirectory());
    const QFileInfoList entries =
        directory.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);

    for (const QFileInfo &entry : entries) {
        // The blank starting point shipped for translators, not a language.
        if (entry.completeBaseName().compare(QLatin1String("template"), Qt::CaseInsensitive) == 0) {
            continue;
        }

        QFile file(entry.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (!document.isObject()) {
            continue; // a malformed file is skipped rather than breaking the list
        }

        Language language;
        language.code = entry.completeBaseName();
        // Falls back to the file name so a file missing its "language" field
        // is still selectable instead of showing a blank row.
        language.name = document.object().value(QStringLiteral("language")).toString(language.code);
        language.path = entry.absoluteFilePath();
        languages.append(language);
    }

    std::sort(languages.begin(), languages.end(),
              [](const Language &a, const Language &b) { return a.name.localeAwareCompare(b.name) < 0; });
    return languages;
}

QString languageCodeForLocale(const QString &localeName)
{
    // "pt_BR", "de_DE", "fr" ... Normalised to the '-' the files use.
    const QString systemName = QString(localeName).replace(QLatin1Char('_'), QLatin1Char('-'));
    const QString systemLanguage = systemName.section(QLatin1Char('-'), 0, 0);

    const QVector<Language> languages = availableLanguages();

    // A full match first, so a Brazilian system gets pt-BR rather than a
    // European pt file that happens to be listed earlier.
    for (const Language &language : languages) {
        if (language.code.compare(systemName, Qt::CaseInsensitive) == 0) {
            return language.code;
        }
    }
    // Then the language alone, either way round: a "pt" system takes pt-BR,
    // and a "pt-PT" system takes it too rather than falling back to English.
    for (const Language &language : languages) {
        if (language.code.section(QLatin1Char('-'), 0, 0)
                .compare(systemLanguage, Qt::CaseInsensitive) == 0) {
            return language.code;
        }
    }
    return {};
}

QString detectSystemLanguageCode()
{
    // Split from languageCodeForLocale() so the matching can be checked
    // against any locale: QLocale::system() ignores QLocale::setDefault(),
    // so with the lookup inlined here it could only ever be tested against
    // whatever locale the machine running the test happened to have.
    return languageCodeForLocale(QLocale::system().name());
}

QString currentLanguageCode()
{
    const QSettings settings;
    // contains(), not an empty check: an empty *stored* value means the user
    // chose English, which must not be re-detected away on the next launch.
    if (!settings.contains(SettingsKeys::kLanguage)) {
        return detectSystemLanguageCode();
    }
    return settings.value(SettingsKeys::kLanguage).toString();
}

void setLanguageCode(const QString &code)
{
    QSettings settings;
    settings.setValue(SettingsKeys::kLanguage, code);
}

bool applyLanguage(QApplication &app, const QString &code)
{
    // The one installed by the previous call, so switching languages replaces
    // it rather than stacking a second translator that would keep answering
    // first for any string both files happen to carry.
    static JsonTranslator *installed = nullptr;
    if (installed) {
        app.removeTranslator(installed);
        delete installed;
        installed = nullptr;
    }

    if (code.isEmpty()) {
        return true; // English: the strings compiled into the app
    }

    const QString path = translationsDirectory() + QStringLiteral("/") + code + QStringLiteral(".json");
    auto *translator = new JsonTranslator;
    if (!translator->load(path)) {
        // A language that has been deleted or corrupted should not leave the
        // app half-translated or complaining on every launch; English is a
        // working fallback.
        delete translator;
        return false;
    }

    app.installTranslator(translator);
    installed = translator;
    return true;
}

void install(QApplication &app)
{
    const QString code = currentLanguageCode();

    // First run: settle on whatever was detected so the choice is visible in
    // Settings and stops depending on the system locale from here on.
    QSettings settings;
    if (!settings.contains(SettingsKeys::kLanguage)) {
        settings.setValue(SettingsKeys::kLanguage, code);
    }

    applyLanguage(app, code);
}

} // namespace Translation
