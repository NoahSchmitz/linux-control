#include "UserAccountsPage.h"
#include "Commands.h"
#include "LinkLabel.h"
#include "IconHelper.h"
#include "Win7Ui.h"

#include <QScrollArea>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QFont>
#include <QFile>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QRadioButton>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QProcess>
#include <QFileDialog>
#include <QDir>

#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <vector>

// ----------------------------------------------------------------------------
// Custom Dialog mimicking the classic Windows User Properties
// ----------------------------------------------------------------------------
class UserPropertiesDialog : public QDialog {
public:
    UserPropertiesDialog(const QString& username, const QString& fullname, bool isAdmin, QWidget* parent = nullptr)
        : QDialog(parent), m_username(username), m_initialIsAdmin(isAdmin)
    {
        setWindowTitle(username + " Properties"); // Matches legacy title format
        
        // Remove the 380px hardcode. Set a minimum width, then let Qt calculate the rest.
        setMinimumWidth(460);

        auto *layout = new QVBoxLayout(this);
        // This constraint snaps the window strictly to the exact size of its contents, 
        // mimicking an unresizable Windows dialog without risking font clipping.
        layout->setSizeConstraint(QLayout::SetFixedSize);

        m_tabs = new QTabWidget(this);

        // --- General Tab ---
        auto *generalTab = new QWidget;
        auto *generalLayout = new QGridLayout(generalTab);
        generalLayout->setContentsMargins(15, 15, 15, 15);
        generalLayout->setVerticalSpacing(10);
        
        generalLayout->addWidget(new QLabel("User name:"), 0, 0);
        auto *userEdit = new QLineEdit(username);
        userEdit->setReadOnly(true); 
        userEdit->setStyleSheet("background-color: #EBEBE4; color: #333;");
        generalLayout->addWidget(userEdit, 0, 1);

        generalLayout->addWidget(new QLabel("Full name:"), 1, 0);
        m_fullNameEdit = new QLineEdit(fullname);
        generalLayout->addWidget(m_fullNameEdit, 1, 1);

        generalLayout->addWidget(new QLabel("Description:"), 2, 0);
        generalLayout->addWidget(new QLineEdit(), 2, 1); // Left inert as Linux doesn't natively use this
        generalLayout->setRowStretch(3, 1);
        m_tabs->addTab(generalTab, "General");

        // --- Group Membership Tab ---
        auto *groupTab = new QWidget;
        auto *groupLayout = new QVBoxLayout(groupTab);
        groupLayout->setContentsMargins(15, 15, 15, 15);
        
        groupLayout->addWidget(new QLabel("What level of access do you want to grant this user?"));
        groupLayout->addSpacing(10);

        m_stdRadio = new QRadioButton("Standard user");
        groupLayout->addWidget(m_stdRadio);
        
        // Enable word wrap and remove hardcoded mid-sentence newlines so text flows fluidly
        auto *stdDesc = new QLabel("(Power Users Group)\nUsers can modify the computer and install programs, but cannot read files that belong to other users.");
        stdDesc->setWordWrap(true);
        stdDesc->setContentsMargins(20, 0, 0, 10);
        groupLayout->addWidget(stdDesc);

        auto *restrictedRadio = new QRadioButton("Restricted user");
        groupLayout->addWidget(restrictedRadio);
        
        auto *restrictedDesc = new QLabel("(Users Group)\nUsers can operate the computer and save documents, but cannot install programs or make potentially damaging changes to the system files and settings.");
        restrictedDesc->setWordWrap(true);
        restrictedDesc->setContentsMargins(20, 0, 0, 10);
        groupLayout->addWidget(restrictedDesc);

        m_adminRadio = new QRadioButton("Other:");
        auto *otherH = new QHBoxLayout;
        otherH->addWidget(m_adminRadio);
        m_otherCombo = new QComboBox;
        m_otherCombo->addItem("Administrators");
        otherH->addWidget(m_otherCombo);
        otherH->addStretch();
        groupLayout->addLayout(otherH);

        auto *adminDesc = new QLabel("Administrators have complete and unrestricted access to the computer/domain.");
        adminDesc->setWordWrap(true);
        adminDesc->setContentsMargins(20, 0, 0, 10);
        groupLayout->addWidget(adminDesc);
        
        groupLayout->addStretch();
        m_tabs->addTab(groupTab, "Group Membership");

        // Set initial state based on wheel group
        if (isAdmin) {
            m_adminRadio->setChecked(true);
        } else {
            m_stdRadio->setChecked(true);
        }

        layout->addWidget(m_tabs);

        auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
        layout->addWidget(btnBox);

        connect(btnBox, &QDialogButtonBox::accepted, this, [this]() { applyChanges(); accept(); });
        connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(btnBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &UserPropertiesDialog::applyChanges);
    }

    void showTab(int index) { m_tabs->setCurrentIndex(index); }

private:
    QString m_username;
    bool m_initialIsAdmin;
    QTabWidget *m_tabs;
    QLineEdit *m_fullNameEdit;
    QRadioButton *m_stdRadio;
    QRadioButton *m_adminRadio;
    QComboBox *m_otherCombo;

    void applyChanges() {
        // Apply Full Name change via polkit + usermod
        if (m_fullNameEdit->text() != m_username) {
            QProcess::execute("pkexec", {"usermod", "-c", m_fullNameEdit->text(), m_username});
        }
        
        // Apply Group Membership (Wheel) via polkit
        bool wantAdmin = m_adminRadio->isChecked();
        if (wantAdmin != m_initialIsAdmin) {
            if (wantAdmin) {
                QProcess::execute("pkexec", {"usermod", "-aG", "wheel", m_username});
            } else {
                QProcess::execute("pkexec", {"gpasswd", "-d", m_username, "wheel"});
            }
            m_initialIsAdmin = wantAdmin;
        }
    }
};

// ----------------------------------------------------------------------------
// Data gathering
// ----------------------------------------------------------------------------
UserAccountsPage::Account UserAccountsPage::gatherAccount()
{
    Account a;

    const uid_t uid = getuid();
    const passwd *pw = getpwuid(uid);
    if (pw) {
        a.userName = QString::fromLocal8Bit(pw->pw_name);
        // GECOS is a comma-separated list; the first field is the full name.
        a.fullName = QString::fromLocal8Bit(pw->pw_gecos).section(QLatin1Char(','), 0, 0);

        // The account is an "Administrator" if it belongs to the wheel or sudo
        // group, the standard admin groups a polkit/sudo policy grants rights to.
        int ngroups = 0;
        getgrouplist(pw->pw_name, pw->pw_gid, nullptr, &ngroups);
        if (ngroups > 0) {
            std::vector<gid_t> gids(ngroups);
            if (getgrouplist(pw->pw_name, pw->pw_gid, gids.data(), &ngroups) != -1) {
                for (gid_t g : gids) {
                    if (const group *gr = getgrgid(g)) {
                        const QString name = QString::fromLocal8Bit(gr->gr_name);
                        if (name == QLatin1String("wheel")
                            || name == QLatin1String("sudo")) {
                            a.accountType = QStringLiteral("Administrator");
                            break;
                        }
                    }
                }
            }
        }
    }
    if (a.userName.isEmpty())
        a.userName = QString::fromLocal8Bit(qgetenv("USER"));
    if (a.fullName.isEmpty())
        a.fullName = a.userName;
    if (a.accountType.isEmpty())
        a.accountType = QStringLiteral("Standard user");

    // The account picture: user dotfiles first, then the AccountsService icon
    // that KDE/GDM login screens use.
    const QString home = QString::fromLocal8Bit(qgetenv("HOME"));
    const QStringList candidates = {
        home + QStringLiteral("/.face.icon"),
        home + QStringLiteral("/.face"),
        QStringLiteral("/var/lib/AccountsService/icons/") + a.userName,
    };
    for (const QString &path : candidates) {
        if (QFile::exists(path)) {
            a.picturePath = path;
            break;
        }
    }

    return a;
}

// ----------------------------------------------------------------------------
// Sidebar
// ----------------------------------------------------------------------------
QList<SidebarLink> UserAccountsPage::sidebarLinks()
{
    return {
        Nav::command("Manage another account", userAccounts()),
        Nav::command("Change User Account Control settings", userAccounts()),
    };
}

QList<SidebarLink> UserAccountsPage::sidebarSeeAlso()
{
    return {
        Nav::plain("Parental Controls"),
        Nav::command("Credential Manager", credentialManager()),
    };
}

// ----------------------------------------------------------------------------
// Render the avatar
// ----------------------------------------------------------------------------
static QPixmap avatarPixmap(const QString &path, int size)
{
    QPixmap src;
    if (!path.isEmpty())
        src.load(path);
    if (src.isNull())
        src = themeIcon({"user-identity", "avatar-default",
                         "system-users"}).pixmap(size, size);

    QPixmap out(size, size);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath clip;
    clip.addRoundedRect(QRectF(0, 0, size, size), 6, 6);
    p.setClipPath(clip);
    const QPixmap scaled = src.scaled(size, size, Qt::KeepAspectRatioByExpanding,
                                      Qt::SmoothTransformation);
    p.drawPixmap((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled);

    p.setClipping(false);
    p.setPen(QPen(QColor("#9DA7B5"), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(0.5, 0.5, size - 1, size - 1), 6, 6);
    return out;
}

// ----------------------------------------------------------------------------
// Page Construction
// ----------------------------------------------------------------------------
UserAccountsPage::UserAccountsPage(QScrollArea *sidebar, QWidget *parent)
    : QWidget(parent)
{
    const Account acct = gatherAccount();

    // Windows 7 lays the content out at a fixed width and leaves the rest of
    // the window blank on the right rather than stretching to fill it.
    auto *contentV = Win7::pageScaffold(this, sidebar, /*bottomMargin=*/20,
                                        /*fixedWidth=*/700);

    // Page title.
    contentV->addWidget(Win7::pageTitle("Make changes to your user account"));
    contentV->addSpacing(18);

    // Body: task links on the left, the account summary card on the right.
    auto *body = new QHBoxLayout;
    body->setContentsMargins(6, 0, 0, 0);
    body->setSpacing(24);

    auto *tasks = new QVBoxLayout;
    tasks->setContentsMargins(0, 0, 0, 0);
    tasks->setSpacing(12);

    // --- Task 1: Change Password ---
    auto *pwdLink = new LinkLabel("Change your password");
    QObject::connect(pwdLink, &LinkLabel::clicked, this, []() {
        // Spawns user's Wayland terminal to run standard Linux password change
        QProcess::startDetached("foot", {"-e", "passwd"});
    });
    tasks->addWidget(pwdLink, 0, Qt::AlignLeft);

    // --- Task 2: Change Picture ---
    auto *picLink = new LinkLabel("Change your picture");
    QObject::connect(picLink, &LinkLabel::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Choose a picture", QDir::homePath(), "Images (*.png *.jpg *.jpeg)");
        if (!file.isEmpty()) {
            // Write to legacy .face file standard used by display managers
            QFile::remove(QDir::homePath() + "/.face");
            QFile::copy(file, QDir::homePath() + "/.face");
            QFile::remove(QDir::homePath() + "/.face.icon");
            QFile::copy(file, QDir::homePath() + "/.face.icon");
        }
    });
    tasks->addWidget(picLink, 0, Qt::AlignLeft);

    // --- Task 3: Change Account Name ---
    auto *nameLink = new LinkLabel("Change your account name");
    QObject::connect(nameLink, &LinkLabel::clicked, this, [this, acct]() {
        UserPropertiesDialog dlg(acct.userName, acct.fullName, acct.accountType == "Administrator", this);
        dlg.showTab(0); // Show General Tab
        dlg.exec();
    });
    tasks->addWidget(nameLink, 0, Qt::AlignLeft);

    // --- Task 4: Change Account Type ---
    auto *typeLink = new LinkLabel("Change your account type");
    QObject::connect(typeLink, &LinkLabel::clicked, this, [this, acct]() {
        UserPropertiesDialog dlg(acct.userName, acct.fullName, acct.accountType == "Administrator", this);
        dlg.showTab(1); // Show Group Membership Tab
        dlg.exec();
    });
    tasks->addWidget(typeLink, 0, Qt::AlignLeft);

    // --- Task 5: Manage another account ---
    auto *manageLink = new LinkLabel("Manage another account");
    QObject::connect(manageLink, &LinkLabel::clicked, this, [this]() {
        launchDetached(this, userAccounts());
    });
    tasks->addWidget(manageLink, 0, Qt::AlignLeft);

    // --- Task 6: Change UAC ---
    auto *uacLink = new LinkLabel("Change User Account Control settings");
    QObject::connect(uacLink, &LinkLabel::clicked, this, [this]() {
        launchDetached(this, userAccounts());
    });
    tasks->addWidget(uacLink, 0, Qt::AlignLeft);

    tasks->addStretch(1);
    body->addLayout(tasks, 0);

    // Account summary card: avatar, then name / type / password state.
    auto *card = new QHBoxLayout;
    card->setContentsMargins(0, 0, 0, 0);
    card->setSpacing(14);

    auto *avatar = new QLabel;
    avatar->setFixedSize(96, 96);
    avatar->setPixmap(avatarPixmap(acct.picturePath, 96));
    avatar->setStyleSheet("background: transparent;");
    card->addWidget(avatar, 0, Qt::AlignTop);

    auto *summary = new QVBoxLayout;
    summary->setContentsMargins(0, 2, 0, 0);
    summary->setSpacing(2);
    summary->addWidget(Win7::label(acct.fullName, 11, "#1A3C7A"));
    summary->addWidget(Win7::label(acct.accountType));
    summary->addWidget(Win7::label("Password protected")); // Matches classic UI hardcode[cite: 1]
    summary->addStretch(1);
    card->addLayout(summary, 0);

    body->addLayout(card, 0);
    body->addStretch(1);

    contentV->addLayout(body);
    contentV->addStretch(1);
}