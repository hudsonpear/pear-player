#include "AboutDialog.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPixmap>
#include <QCoreApplication>
#include <QFont>

namespace {

// Emptying this turns the row back into plain text rather than a link that
// would lead nowhere.
const QString kGithubUrl = QStringLiteral("https://github.com/hudsonpear/pear-player");

constexpr int kReleaseYear = 2026;

QLabel *centeredLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignHCenter);
    return label;
}

} // namespace

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Pear Player"));

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    // --- Icon --------------------------------------------------------------
    auto *iconLabel = new QLabel(this);
    iconLabel->setAlignment(Qt::AlignHCenter);
    // The 128px entry from app.qrc rather than the window icon, so it stays
    // crisp at this size instead of being scaled up from a small one.
    const QPixmap icon(QStringLiteral(":/app/resources/icons/pearicon-128.png"));
    if (!icon.isNull()) {
        iconLabel->setPixmap(icon);
    }
    layout->addWidget(iconLabel);

    // --- Name and version --------------------------------------------------
    auto *nameLabel = centeredLabel(
        tr("Pear Player %1").arg(QCoreApplication::applicationVersion()), this);
    QFont nameFont = nameLabel->font();
    nameFont.setPointSizeF(nameFont.pointSizeF() * 1.4);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    layout->addWidget(nameLabel);

    layout->addWidget(centeredLabel(QString::number(kReleaseYear), this));
    layout->addWidget(centeredLabel(tr("Hudson Pear"), this));

    // --- Project link ------------------------------------------------------
    auto *linkLabel = centeredLabel(QString(), this);
    if (kGithubUrl.isEmpty()) {
        linkLabel->setText(tr("GitHub"));
        linkLabel->setEnabled(false);
    } else {
        linkLabel->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(kGithubUrl, tr("GitHub")));
        linkLabel->setTextFormat(Qt::RichText);
        linkLabel->setOpenExternalLinks(true);
    }
    layout->addWidget(linkLabel);

    layout->addSpacing(4);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttonBox);

    // Wider than the content strictly needs: the labels are short, so a
    // hint-sized box comes out cramped. Height still follows the layout so
    // nothing is clipped.
    setFixedSize(qMax(440, sizeHint().width()), sizeHint().height());
}
