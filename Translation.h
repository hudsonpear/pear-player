#pragma once

#include <QString>
#include <QVector>

class QApplication;

/// Interface translations, loaded from plain JSON files rather than Qt's
/// compiled .qm catalogues.
///
/// The point is that adding a language needs nothing but a text editor: drop a
/// file into the translations folder beside the executable and it appears in
/// Settings. A .qm would have to be produced with lrelease, which a translator
/// is unlikely to have.
///
/// File format (UTF-8):
///
///     {
///       "language": "Portugues (Brasil)",
///       "strings": {
///         "&File": "&Arquivo",
///         "Open...": "Abrir..."
///       }
///     }
///
/// Keys are the English text exactly as it appears in the app. Anything absent
/// or left empty falls back to English, so a partial file is perfectly usable.
namespace Translation
{
struct Language {
    QString code; ///< File base name ("pt-BR"), and what is stored in settings.
    QString name; ///< Shown in Settings, from the file's "language" field.
    QString path;
};

/// Folder the app reads languages from: "translations" beside the executable.
[[nodiscard]] QString translationsDirectory();

/// Every readable language file found there, sorted by name. English is not
/// included -- it is the built-in text, not a file.
[[nodiscard]] QVector<Language> availableLanguages();

/// The language file matching a locale name ("pt_BR", "de-DE", "fr"), empty
/// when none does. An exact match wins; failing that, the language alone.
[[nodiscard]] QString languageCodeForLocale(const QString &localeName);

/// The language file matching the system locale, empty when none does.
[[nodiscard]] QString detectSystemLanguageCode();

/// The stored choice, empty for English. Before anything has been chosen this
/// is the system's own language, if a file for it exists.
[[nodiscard]] QString currentLanguageCode();
void setLanguageCode(const QString &code);

/// Switches to a language, replacing whatever was installed before. False if
/// the file could not be read, in which case English is left in force.
///
/// Only changes what tr() returns from here on; text already on screen was
/// resolved when it was created, so the caller has to refresh its own UI.
bool applyLanguage(QApplication &app, const QString &code);

/// Installs the stored language, if any, and settles the first-run detection.
void install(QApplication &app);
}
