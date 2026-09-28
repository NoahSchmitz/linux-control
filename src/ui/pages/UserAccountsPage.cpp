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
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QCheckBox>
#include <QPushButton>
#include <QMessageBox>
#include <QRegularExpression>

#include <pwd.h>
#include <grp.h>
#include <shadow.h>
#include <unistd.h>
#include <vector>

// ----------------------------------------------------------------------------
// Shared Helper: Get all valid system users
// ----------------------------------------------------------------------------
struct UserInfo {
    QString username;
    QString fullname;
    bool isAdmin;
    QString groupString;
};

static QList<UserInfo> getAllUsers() {
    QList<UserInfo> users;
    setpwent();
    while (passwd *pw = getpwent()) {
        // Filter for standard users (UID >= 1000) and root (UID == 0)
        if ((pw->pw_uid >= 1000 && pw->pw_uid < 65534) || pw->pw_uid == 0) {
            UserInfo u;
            u.username = QString::fromLocal8Bit(pw->pw_name);
            u.fullname = QString::fromLocal8Bit(pw->pw_gecos).section(QLatin1Char(','), 0, 0);
            u.isAdmin = (pw->pw_uid == 0);

            int ngroups = 0;
            getgrouplist(pw->pw_name, pw->pw_gid, nullptr, &ngroups);
            if (ngroups > 0) {
                std::vector<gid_t> gids(ngroups);
                if (getgrouplist(pw->pw_name, pw->pw_gid, gids.data(), &ngroups) != -1) {
                    for (gid_t g : gids) {
                        if (const group *gr = getgrgid(g)) {
                            QString gname = QString::fromLocal8Bit(gr->gr_name);
                            if (gname == "wheel" || gname == "sudo") {
                                u.isAdmin = true;
                                break;
                            }
                        }
                    }
                }
            }
            u.groupString = u.isAdmin ? "Administrators" : "Users";
            users.append(u);
        }
    }
    endpwent();
    return users;
}

// ----------------------------------------------------------------------------
// Custom Dialog mimicking the "Set Password" window
// ----------------------------------------------------------------------------
class SetPasswordDialog : public QDialog {
public:
    SetPasswordDialog(const QString& username, QWidget* parent = nullptr)
        : QDialog(parent), m_username(username) {
        setWindowTitle("Set Password");
        setMinimumWidth(320);

        auto *layout = new QVBoxLayout(this);
        layout->setSizeConstraint(QLayout::SetFixedSize);
        layout->setContentsMargins(15, 15, 15, 15);
        layout->setSpacing(15);

        auto *formLayout = new QGridLayout;
        formLayout->setContentsMargins(0, 0, 0, 0);
        formLayout->setHorizontalSpacing(10);
        formLayout->setVerticalSpacing(10);

        formLayout->addWidget(new QLabel("New password:"), 0, 0);
        m_newPw = new QLineEdit;
        m_newPw->setEchoMode(QLineEdit::Password);
        formLayout->addWidget(m_newPw, 0, 1);

        formLayout->addWidget(new QLabel("Confirm new password:"), 1, 0);
        m_confirmPw = new QLineEdit;
        m_confirmPw->setEchoMode(QLineEdit::Password);
        formLayout->addWidget(m_confirmPw, 1, 1);

        layout->addLayout(formLayout);

        auto *btnLayout = new QHBoxLayout;
        btnLayout->addStretch();
        auto *okBtn = new QPushButton("OK");
        auto *cancelBtn = new QPushButton("Cancel");
        btnLayout->addWidget(okBtn);
        btnLayout->addWidget(cancelBtn);
        layout->addLayout(btnLayout);

        connect(okBtn, &QPushButton::clicked, this, &SetPasswordDialog::onAccept);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    }

private:
    QString m_username;
    QLineEdit *m_newPw;
    QLineEdit *m_confirmPw;

    void onAccept() {
        if (m_newPw->text() != m_confirmPw->text()) {
            QMessageBox::warning(this, "Set Password", "The passwords do not match. Please re-type the new password in both boxes.");
            return;
        }

        QProcess proc;
        proc.start("pkexec", {"chpasswd"});
        if (proc.waitForStarted(-1)) {
            // chpasswd takes piped input formatted as "username:password"
            QByteArray input = QString("%1:%2\n").arg(m_username, m_newPw->text()).toUtf8();
            proc.write(input);
            proc.closeWriteChannel(); // Sends EOF so chpasswd begins execution
            proc.waitForFinished(-1);

            if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
                QMessageBox::information(this, "Set Password", "The password has been successfully changed.");
                accept();
            } else {
                QString err = proc.readAllStandardError().trimmed();
                if (err.isEmpty()) err = "Authentication failed or the action was canceled.";
                QMessageBox::critical(this, "Set Password", "Failed to change password.\n\n" + err);
            }
        } else {
            QMessageBox::critical(this, "Set Password", "Could not execute the password change utility.");
        }
    }
};

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

        // generalLayout->addWidget(new QLabel("Description:"), 2, 0);
        // generalLayout->addWidget(new QLineEdit(), 2, 1); // Left inert as Linux doesn't natively use this

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

        // auto *restrictedRadio = new QRadioButton("Restricted user");
        // groupLayout->addWidget(restrictedRadio);
        
        // auto *restrictedDesc = new QLabel("(Users Group)\nUsers can operate the computer and save documents, but cannot install programs or make potentially damaging changes to the system files and settings.");
        // restrictedDesc->setWordWrap(true);
        // restrictedDesc->setContentsMargins(20, 0, 0, 10);
        // groupLayout->addWidget(restrictedDesc);

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
// Custom Dialog mimicking the "Add New User" Wizard
// ----------------------------------------------------------------------------
class AddNewUserDialog : public QDialog {
public:
    AddNewUserDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Add New User");

        auto *mainLayout = new QHBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        mainLayout->setSizeConstraint(QLayout::SetFixedSize);

        // Left Wizard Banner
        auto *leftBanner = new QFrame(this);
        leftBanner->setFixedWidth(150);
        leftBanner->setStyleSheet("background-color: #000080;"); 
        
        auto *bannerLayout = new QVBoxLayout(leftBanner);
        auto *iconLabel = new QLabel;
        // Using an asterisk or keys as a stand-in for the classic win2k box icon
        iconLabel->setPixmap(themeIcon({"preferences-desktop-user-password", "dialog-password"}).pixmap(64, 64));
        iconLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        bannerLayout->addWidget(iconLabel);
        bannerLayout->addStretch();
        mainLayout->addWidget(leftBanner);

        // Right Content Area
        auto *rightPanel = new QWidget(this);
        auto *rightLayout = new QVBoxLayout(rightPanel);
        rightLayout->setContentsMargins(20, 20, 20, 15);

        auto *instruction = new QLabel("Enter the basic information for the new user.");
        rightLayout->addWidget(instruction);
        rightLayout->addSpacing(15);

        auto *formLayout = new QGridLayout;
        formLayout->addWidget(new QLabel("User name:"), 0, 0);
        m_userEdit = new QLineEdit;
        formLayout->addWidget(m_userEdit, 0, 1);

        formLayout->addWidget(new QLabel("Full name:"), 1, 0);
        m_fullEdit = new QLineEdit;
        formLayout->addWidget(m_fullEdit, 1, 1);

        // formLayout->addWidget(new QLabel("Description:"), 2, 0);
        // m_descEdit = new QLineEdit;
        // formLayout->addWidget(m_descEdit, 2, 1);

        rightLayout->addLayout(formLayout);
        rightLayout->addSpacing(20);
        rightLayout->addWidget(new QLabel("To continue, click Next."));
        rightLayout->addStretch();

        // Standard bottom buttons mimicking Back/Next/Cancel
        auto *btnLayout = new QHBoxLayout;
        btnLayout->addStretch();
        
        auto *backBtn = new QPushButton("< Back");
        backBtn->setEnabled(false);
        btnLayout->addWidget(backBtn);
        
        auto *nextBtn = new QPushButton("Next >");
        connect(nextBtn, &QPushButton::clicked, this, &AddNewUserDialog::createUser);
        btnLayout->addWidget(nextBtn);
        
        auto *cancelBtn = new QPushButton("Cancel");
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        btnLayout->addWidget(cancelBtn);
        
        rightLayout->addLayout(btnLayout);
        mainLayout->addWidget(rightPanel);
    }

private:
    QLineEdit *m_userEdit;
    QLineEdit *m_fullEdit;
    QLineEdit *m_descEdit;

    void createUser() {
        QString user = m_userEdit->text();
        QString full = m_fullEdit->text();
        if (user.isEmpty()) return;

        // Create the user
        QProcess proc;
        proc.start("pkexec", {"useradd", "-m", "-c", full, user});
        proc.waitForFinished(-1);

        if (proc.exitCode() == 0) {
            // Prompt to set the initial password immediately
            SetPasswordDialog pwDlg(user, this);
            pwDlg.exec();
            accept();
        } else {
            QString err = proc.readAllStandardError().trimmed();
            QMessageBox::critical(this, "Add New User", "Failed to create user.\n\n" + err);
        }
    }
};

// ----------------------------------------------------------------------------
// Custom Dialog mimicking the "Users and Passwords" Control Applet
// ----------------------------------------------------------------------------
class UsersAndPasswordsDialog : public QDialog {
public:
    UsersAndPasswordsDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Users and Passwords");
        setMinimumSize(420, 450);

        auto *layout = new QVBoxLayout(this);
        
        auto *tabs = new QTabWidget(this);
        auto *usersTab = new QWidget;
        auto *tabLayout = new QVBoxLayout(usersTab);
        tabLayout->setContentsMargins(15, 15, 15, 15);

        // Header instruction
        auto *headerLayout = new QHBoxLayout;
        auto *headerIcon = new QLabel;
        headerIcon->setPixmap(themeIcon({"system-users"}).pixmap(32, 32));
        headerLayout->addWidget(headerIcon, 0, Qt::AlignTop);
        
        auto *headerText = new QLabel("Use the list below to grant or deny users access to your computer, and to change passwords and other settings.");
        headerText->setWordWrap(true);
        headerLayout->addWidget(headerText, 1);
        tabLayout->addLayout(headerLayout);
        tabLayout->addSpacing(10);

        // auto *reqCheck = new QCheckBox("Users must enter a user name and password to use this computer.");
        // reqCheck->setChecked(true); // Visual stub matching classic UI
        // tabLayout->addWidget(reqCheck);
        // tabLayout->addSpacing(10);

        tabLayout->addWidget(new QLabel("Users for this computer:"));

        // Table
        m_table = new QTableWidget(0, 2);
        m_table->setHorizontalHeaderLabels({"User Name", "Group"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setSelectionMode(QAbstractItemView::SingleSelection);
        m_table->setShowGrid(false);
        m_table->verticalHeader()->setVisible(false);
        m_table->setStyleSheet("QTableWidget { background-color: #FFFFFF; border: 1px solid #808080; }");
        tabLayout->addWidget(m_table);

        // Buttons
        auto *btnRow = new QHBoxLayout;
        btnRow->addStretch();
        auto *addBtn = new QPushButton("Add...");
        m_removeBtn = new QPushButton("Remove");
        m_propBtn = new QPushButton("Properties");
        btnRow->addWidget(addBtn);
        btnRow->addWidget(m_removeBtn);
        btnRow->addWidget(m_propBtn);
        tabLayout->addLayout(btnRow);
        tabLayout->addSpacing(10);

        // Password Group Box
        m_pwGroup = new QGroupBox("Password for [User]");
        auto *pwLayout = new QHBoxLayout(m_pwGroup);
        pwLayout->setContentsMargins(10, 15, 10, 10);
        
        auto *pwIcon = new QLabel;
        pwIcon->setPixmap(themeIcon({"preferences-desktop-user-password"}).pixmap(32, 32));
        pwLayout->addWidget(pwIcon, 0, Qt::AlignTop);

        auto *pwRight = new QVBoxLayout;
        m_pwLabel = new QLabel("To change the password for [User], click Set Password.");
        m_pwLabel->setWordWrap(true);
        pwRight->addWidget(m_pwLabel);
        
        auto *setPwBtnRow = new QHBoxLayout;
        setPwBtnRow->addStretch();
        m_setPwBtn = new QPushButton("Set Password...");
        setPwBtnRow->addWidget(m_setPwBtn);
        pwRight->addLayout(setPwBtnRow);
        pwLayout->addLayout(pwRight, 1);
        tabLayout->addWidget(m_pwGroup);

        tabs->addTab(usersTab, "Users");
        // tabs->addTab(new QWidget(), "Advanced"); // Stub tab
        layout->addWidget(tabs);

        auto *bottomBtns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
        layout->addWidget(bottomBtns);

        // Connections
        connect(m_table, &QTableWidget::itemSelectionChanged, this, &UsersAndPasswordsDialog::onSelectionChanged);
        connect(addBtn, &QPushButton::clicked, this, &UsersAndPasswordsDialog::onAddUser);
        connect(m_removeBtn, &QPushButton::clicked, this, &UsersAndPasswordsDialog::onRemoveUser);
        connect(m_propBtn, &QPushButton::clicked, this, &UsersAndPasswordsDialog::onProperties);
        connect(m_setPwBtn, &QPushButton::clicked, this, &UsersAndPasswordsDialog::onSetPassword);
        
        connect(bottomBtns, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(bottomBtns, &QDialogButtonBox::rejected, this, &QDialog::reject);

        refreshTable();
    }

private:
    QTableWidget *m_table;
    QGroupBox *m_pwGroup;
    QLabel *m_pwLabel;
    QPushButton *m_removeBtn;
    QPushButton *m_propBtn;
    QPushButton *m_setPwBtn;

    void refreshTable() {
        m_table->setRowCount(0);
        QList<UserInfo> users = getAllUsers();
        for (const auto& u : users) {
            int row = m_table->rowCount();
            m_table->insertRow(row);
            
            auto *userItem = new QTableWidgetItem(themeIcon({"user-identity"}), u.username);
            userItem->setData(Qt::UserRole, u.fullname);
            userItem->setData(Qt::UserRole + 1, u.isAdmin);
            userItem->setFlags(userItem->flags() & ~Qt::ItemIsEditable);
            
            auto *groupItem = new QTableWidgetItem(u.groupString);
            groupItem->setFlags(groupItem->flags() & ~Qt::ItemIsEditable);
            
            m_table->setItem(row, 0, userItem);
            m_table->setItem(row, 1, groupItem);
        }
        
        if (m_table->rowCount() > 0) {
            m_table->selectRow(0);
        }
    }

    void onSelectionChanged() {
        int row = m_table->currentRow();
        if (row >= 0) {
            QString user = m_table->item(row, 0)->text();
            m_pwGroup->setTitle("Password for " + user);
            m_pwLabel->setText("To change the password for " + user + ", click Set Password.");
            m_propBtn->setEnabled(true);
            m_setPwBtn->setEnabled(true);
            m_removeBtn->setEnabled(user != qgetenv("USER") && user != "root");
        } else {
            m_pwGroup->setTitle("Password");
            m_pwLabel->setText("");
            m_propBtn->setEnabled(false);
            m_setPwBtn->setEnabled(false);
            m_removeBtn->setEnabled(false);
        }
    }

    void onAddUser() {
        AddNewUserDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            refreshTable();
        }
    }

    void onRemoveUser() {
        int row = m_table->currentRow();
        if (row < 0) return;
        QString user = m_table->item(row, 0)->text();
        
        QProcess::execute("pkexec", {"userdel", "-r", user});
        refreshTable();
    }

    void onProperties() {
        int row = m_table->currentRow();
        if (row < 0) return;
        
        QString user = m_table->item(row, 0)->text();
        QString fullname = m_table->item(row, 0)->data(Qt::UserRole).toString();
        bool isAdmin = m_table->item(row, 0)->data(Qt::UserRole + 1).toBool();

        UserPropertiesDialog dlg(user, fullname, isAdmin, this);
        if (dlg.exec() == QDialog::Accepted) {
            refreshTable();
        }
    }

    void onSetPassword() {
        int row = m_table->currentRow();
        if (row < 0) return;
        QString user = m_table->item(row, 0)->text();
        
        SetPasswordDialog pwDlg(user, this);
        pwDlg.exec();
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
        Nav::action("Manage another account", []() {
            UsersAndPasswordsDialog dlg(nullptr);
            dlg.exec();
        }),
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
    QObject::connect(pwdLink, &LinkLabel::clicked, this, [this, acct]() {
        SetPasswordDialog pwDlg(acct.userName, this);
        pwDlg.exec();
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
        UsersAndPasswordsDialog dlg(this);
        dlg.exec();
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
    
    // --- Dynamic Password Hash Check ---
    QString pwdStatusStr = "Password protected";
    struct spwd *sp = getspnam(acct.userName.toLocal8Bit().constData());
    
    if (sp) {
        QString hash = QString::fromLocal8Bit(sp->sp_pwdp);
        if (hash.isEmpty()) {
            pwdStatusStr = "No password";
        } else if (hash.startsWith("!") || hash.startsWith("*")) {
            pwdStatusStr = "Account locked";
        }
    } else {
        // Fallback for standard users who lack permission to read /etc/shadow directly
        QProcess proc;
        proc.start("passwd", {"-S", acct.userName});
        if (proc.waitForFinished(1000) && proc.exitStatus() == QProcess::NormalExit) {
            QStringList parts = QString::fromLocal8Bit(proc.readAllStandardOutput()).split(QRegularExpression("\\s+"));
            if (parts.size() >= 2) {
                QString status = parts[1];
                if (status == "NP") {
                    pwdStatusStr = "No password";
                } else if (status == "L" || status == "LK") {
                    pwdStatusStr = "Account locked";
                }
            }
        }
    }

    summary->addWidget(Win7::label(pwdStatusStr));
    summary->addStretch(1);
    card->addLayout(summary, 0);

    body->addLayout(card, 0);
    body->addStretch(1);

    contentV->addLayout(body);
    contentV->addStretch(1);
}