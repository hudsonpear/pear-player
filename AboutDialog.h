#pragma once

#include <QDialog>

/// Small "About Pear Player" box: the app icon over the name, version, year
/// and author, plus a link to the project page.
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
};
