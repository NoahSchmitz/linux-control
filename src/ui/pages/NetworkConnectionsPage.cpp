#include "NetworkConnectionsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QMenu>
#include <QNetworkInterface>
#include <QMessageBox>
#include <QLabel>
#include <QFormLayout>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>
#include <QFile>
#include <QStyle>
#include <QRadioButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QTabWidget>
#include <QButtonGroup>
#include <QMap>
#include <QSpinBox>
#include <QCheckBox>
#include <QRegularExpression>
#include <QApplication>
#include <QTextEdit>
#include <QDateTime>
#include <QComboBox>
#include <QGridLayout>
#include <QInputDialog>
#include <QTreeWidget>
#include <QHeaderView>
#include <QInputDialog>

#include "../MainWindow.h"
#include "PageRegistry.h"

// ==============================================================================
// IPv4PropertiesDialog
// ==============================================================================

IPv4PropertiesDialog::IPv4PropertiesDialog(const QString &ifaceName, QWidget *parent)
    : QDialog(parent), m_iface(ifaceName) 
{
    setWindowTitle("Internet Protocol Version 4 (TCP/IPv4) Properties");
    resize(420, 480);

    auto *mainLayout = new QVBoxLayout(this);
    auto *tabWidget = new QTabWidget(this);
    auto *generalTab = new QWidget(tabWidget);
    auto *generalLayout = new QVBoxLayout(generalTab);

    auto *descLabel = new QLabel("You can get IP settings assigned automatically if your network supports\n"
                                 "this capability. Otherwise, you need to ask your network administrator\n"
                                 "for the appropriate IP settings.", generalTab);
    generalLayout->addWidget(descLabel);
    generalLayout->addSpacing(10);

    m_dhcpRadio = new QRadioButton("Obtain an IP address automatically", generalTab);
    m_staticRadio = new QRadioButton("Use the following IP address:", generalTab);
    
    // Group IP radios so they don't conflict with DNS radios
    auto *ipGroup = new QButtonGroup(this);
    ipGroup->addButton(m_dhcpRadio);
    ipGroup->addButton(m_staticRadio);

    generalLayout->addWidget(m_dhcpRadio);
    generalLayout->addWidget(m_staticRadio);

    auto *ipLayout = new QFormLayout();
    ipLayout->setContentsMargins(20, 0, 0, 0);
    m_ipEdit = new QLineEdit(generalTab);
    m_maskEdit = new QLineEdit(generalTab);
    m_gatewayEdit = new QLineEdit(generalTab);
    ipLayout->addRow("IP address:", m_ipEdit);
    ipLayout->addRow("Subnet mask:", m_maskEdit);
    ipLayout->addRow("Default gateway:", m_gatewayEdit);
    generalLayout->addLayout(ipLayout);

    generalLayout->addSpacing(15);

    m_dnsDhcpRadio = new QRadioButton("Obtain DNS server address automatically", generalTab);
    m_dnsStaticRadio = new QRadioButton("Use the following DNS server addresses:", generalTab);
    
    // Group DNS radios
    auto *dnsGroup = new QButtonGroup(this);
    dnsGroup->addButton(m_dnsDhcpRadio);
    dnsGroup->addButton(m_dnsStaticRadio);

    generalLayout->addWidget(m_dnsDhcpRadio);
    generalLayout->addWidget(m_dnsStaticRadio);

    auto *dnsLayout = new QFormLayout();
    dnsLayout->setContentsMargins(20, 0, 0, 0);
    m_prefDnsEdit = new QLineEdit(generalTab);
    m_altDnsEdit = new QLineEdit(generalTab);
    dnsLayout->addRow("Preferred DNS server:", m_prefDnsEdit);
    dnsLayout->addRow("Alternate DNS server:", m_altDnsEdit);
    generalLayout->addLayout(dnsLayout);

    generalLayout->addStretch();
    tabWidget->addTab(generalTab, "General");
    tabWidget->addTab(new QWidget(), "Alternate Configuration");
    mainLayout->addWidget(tabWidget);

    auto *buttonBox = new QHBoxLayout();
    buttonBox->addStretch();
    auto *okBtn = new QPushButton("OK", this);
    auto *cancelBtn = new QPushButton("Cancel", this);
    buttonBox->addWidget(okBtn);
    buttonBox->addWidget(cancelBtn);
    mainLayout->addLayout(buttonBox);

    connect(m_dhcpRadio, &QRadioButton::toggled, this, &IPv4PropertiesDialog::toggleIpFields);
    connect(m_dnsDhcpRadio, &QRadioButton::toggled, this, &IPv4PropertiesDialog::toggleDnsFields);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, &IPv4PropertiesDialog::applySettings);

    // Fetch current system settings and populate the dialog
    loadCurrentSettings();
}

void IPv4PropertiesDialog::toggleIpFields() {
    bool isStatic = m_staticRadio->isChecked();
    m_ipEdit->setEnabled(isStatic);
    m_maskEdit->setEnabled(isStatic);
    m_gatewayEdit->setEnabled(isStatic);
}

void IPv4PropertiesDialog::toggleDnsFields() {
    bool isStatic = m_dnsStaticRadio->isChecked();
    m_prefDnsEdit->setEnabled(isStatic);
    m_altDnsEdit->setEnabled(isStatic);
}

QString IPv4PropertiesDialog::prefixToSubnetMask(int prefix) {
    if (prefix < 0 || prefix > 32) return "";
    uint32_t mask = (prefix == 0) ? 0 : (~0U << (32 - prefix));
    return QString("%1.%2.%3.%4").arg((mask >> 24) & 0xFF).arg((mask >> 16) & 0xFF).arg((mask >> 8) & 0xFF).arg(mask & 0xFF);
}

int IPv4PropertiesDialog::subnetMaskToPrefix(const QString &mask) {
    QStringList parts = mask.split('.');
    if (parts.size() != 4) return 24; 
    uint32_t m = (parts[0].toUInt() << 24) | (parts[1].toUInt() << 16) | (parts[2].toUInt() << 8) | parts[3].toUInt();
    int prefix = 0;
    for (int i = 31; i >= 0; --i) {
        if (m & (1U << i)) prefix++;
        else break; 
    }
    return prefix;
}

void IPv4PropertiesDialog::loadCurrentSettings() {
    QProcess proc;
    proc.start("nmcli", {"-t", "-f", "GENERAL.CONNECTION", "device", "show", m_iface});
    proc.waitForFinished();
    
    QString conName = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (conName.startsWith("GENERAL.CONNECTION:")) {
        conName = conName.mid(QString("GENERAL.CONNECTION:").length());
    }
    
    if (conName.isEmpty()) {
        m_dhcpRadio->setChecked(true);
        m_dnsDhcpRadio->setChecked(true);
        return;
    }

    proc.start("nmcli", {"-t", "-f", "ipv4.method,ipv4.addresses,ipv4.gateway,ipv4.dns", "con", "show", conName});
    proc.waitForFinished();
    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    
    QMap<QString, QString> settings;
    for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
        int colonIdx = line.indexOf(':');
        if (colonIdx != -1) {
            settings[line.left(colonIdx)] = line.mid(colonIdx + 1).trimmed();
        }
    }

    QString method = settings["ipv4.method"];
    QString addresses = settings["ipv4.addresses"];
    QString gateway = settings["ipv4.gateway"];
    QString dns = settings["ipv4.dns"];

    // Apply IP Configuration
    if (method == "manual") {
        m_staticRadio->setChecked(true);
        QString firstAddress = addresses.split(',').first().trimmed();
        if (firstAddress.contains('/')) {
            QStringList parts = firstAddress.split('/');
            m_ipEdit->setText(parts[0]);
            m_maskEdit->setText(prefixToSubnetMask(parts[1].toInt()));
        } else {
            m_ipEdit->setText(firstAddress);
            m_maskEdit->setText("255.255.255.0"); // Fallback
        }
        m_gatewayEdit->setText(gateway);
    } else {
        m_dhcpRadio->setChecked(true);
    }

    // Apply DNS Configuration
    if (!dns.isEmpty() && (method == "manual" || method == "auto")) {
        m_dnsStaticRadio->setChecked(true);
        QStringList dnsList = dns.split(',');
        if (dnsList.size() > 0) m_prefDnsEdit->setText(dnsList[0].trimmed());
        if (dnsList.size() > 1) m_altDnsEdit->setText(dnsList[1].trimmed());
    } else {
        m_dnsDhcpRadio->setChecked(true);
    }

    toggleIpFields();
    toggleDnsFields();
}

void IPv4PropertiesDialog::applySettings() {
    QProcess proc;
    proc.start("nmcli", {"-t", "-f", "GENERAL.CONNECTION", "device", "show", m_iface});
    proc.waitForFinished();
    
    // Read the output and strip the key name from the terse output
    QString conName = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (conName.startsWith("GENERAL.CONNECTION:")) {
        conName = conName.mid(QString("GENERAL.CONNECTION:").length());
    }
    
    if (conName.isEmpty()) {
        accept();
        return;
    }

    QStringList args;
    args << "nmcli" << "con" << "modify" << conName;

    if (m_dhcpRadio->isChecked()) {
        args << "ipv4.method" << "auto" << "ipv4.addresses" << "" << "ipv4.gateway" << "";
    } else {
        int prefix = subnetMaskToPrefix(m_maskEdit->text());
        QString ipCidr = QString("%1/%2").arg(m_ipEdit->text()).arg(prefix);
        args << "ipv4.method" << "manual" << "ipv4.addresses" << ipCidr << "ipv4.gateway" << m_gatewayEdit->text();
    }

    if (m_dnsDhcpRadio->isChecked()) {
        args << "ipv4.dns" << "" << "ipv4.ignore-auto-dns" << "no";
    } else {
        QStringList dnsList;
        if (!m_prefDnsEdit->text().isEmpty()) dnsList << m_prefDnsEdit->text();
        if (!m_altDnsEdit->text().isEmpty()) dnsList << m_altDnsEdit->text();
        args << "ipv4.dns" << dnsList.join(",") << "ipv4.ignore-auto-dns" << "yes";
    }

    // Execute changes via PolicyKit 
    QProcess::execute("pkexec", args);
    // Bounce the connection for changes to take effect immediately
    QProcess::execute("pkexec", {"nmcli", "con", "up", conName});

    accept();
}

// ==============================================================================
// IPv6PropertiesDialog
// ==============================================================================

IPv6PropertiesDialog::IPv6PropertiesDialog(const QString &ifaceName, QWidget *parent)
    : QDialog(parent), m_iface(ifaceName) 
{
    setWindowTitle("Internet Protocol Version 6 (TCP/IPv6) Properties");
    resize(420, 480);

    auto *mainLayout = new QVBoxLayout(this);
    auto *generalGroup = new QGroupBox("General", this);
    auto *generalLayout = new QVBoxLayout(generalGroup);

    m_dhcpRadio = new QRadioButton("Obtain an IPv6 address automatically", this);
    m_staticRadio = new QRadioButton("Use the following IPv6 address:", this);
    
    auto *ipGroup = new QButtonGroup(this);
    ipGroup->addButton(m_dhcpRadio);
    ipGroup->addButton(m_staticRadio);

    generalLayout->addWidget(m_dhcpRadio);
    generalLayout->addWidget(m_staticRadio);

    auto *ipLayout = new QFormLayout();
    ipLayout->setContentsMargins(20, 0, 0, 0);
    m_ipEdit = new QLineEdit(this);
    m_prefixEdit = new QLineEdit(this); 
    m_gatewayEdit = new QLineEdit(this);
    ipLayout->addRow("IPv6 address:", m_ipEdit);
    ipLayout->addRow("Subnet prefix length:", m_prefixEdit);
    ipLayout->addRow("Default gateway:", m_gatewayEdit);
    generalLayout->addLayout(ipLayout);
    generalLayout->addSpacing(15);

    m_dnsDhcpRadio = new QRadioButton("Obtain DNS server address automatically", this);
    m_dnsStaticRadio = new QRadioButton("Use the following DNS server addresses:", this);
    
    auto *dnsGroup = new QButtonGroup(this);
    dnsGroup->addButton(m_dnsDhcpRadio);
    dnsGroup->addButton(m_dnsStaticRadio);

    generalLayout->addWidget(m_dnsDhcpRadio);
    generalLayout->addWidget(m_dnsStaticRadio);

    auto *dnsLayout = new QFormLayout();
    dnsLayout->setContentsMargins(20, 0, 0, 0);
    m_prefDnsEdit = new QLineEdit(this);
    m_altDnsEdit = new QLineEdit(this);
    dnsLayout->addRow("Preferred DNS server:", m_prefDnsEdit);
    dnsLayout->addRow("Alternate DNS server:", m_altDnsEdit);
    generalLayout->addLayout(dnsLayout);
    
    mainLayout->addWidget(generalGroup);

    auto *buttonBox = new QHBoxLayout();
    buttonBox->addStretch();
    auto *okBtn = new QPushButton("OK", this);
    auto *cancelBtn = new QPushButton("Cancel", this);
    buttonBox->addWidget(okBtn);
    buttonBox->addWidget(cancelBtn);
    mainLayout->addLayout(buttonBox);

    connect(m_dhcpRadio, &QRadioButton::toggled, this, &IPv6PropertiesDialog::toggleFields);
    connect(m_dnsDhcpRadio, &QRadioButton::toggled, this, &IPv6PropertiesDialog::toggleFields);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, &IPv6PropertiesDialog::applySettings);

    loadCurrentSettings();
}

void IPv6PropertiesDialog::toggleFields() {
    bool ipStatic = m_staticRadio->isChecked();
    m_ipEdit->setEnabled(ipStatic);
    m_prefixEdit->setEnabled(ipStatic);
    m_gatewayEdit->setEnabled(ipStatic);

    bool dnsStatic = m_dnsStaticRadio->isChecked();
    m_prefDnsEdit->setEnabled(dnsStatic);
    m_altDnsEdit->setEnabled(dnsStatic);
}

QString IPv6PropertiesDialog::getConnectionName() {
    QProcess proc;
    proc.start("nmcli", {"-t", "-f", "GENERAL.CONNECTION", "device", "show", m_iface});
    proc.waitForFinished();
    QString conName = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (conName.startsWith("GENERAL.CONNECTION:")) {
        return conName.mid(19); // 19 is length of "GENERAL.CONNECTION:"
    }
    return conName;
}

void IPv6PropertiesDialog::loadCurrentSettings() {
    QString conName = getConnectionName();
    if (conName.isEmpty()) {
        m_dhcpRadio->setChecked(true);
        m_dnsDhcpRadio->setChecked(true);
        return;
    }

    QProcess proc;
    proc.start("nmcli", {"-t", "-f", "ipv6.method,ipv6.addresses,ipv6.gateway,ipv6.dns", "con", "show", conName});
    proc.waitForFinished();
    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    
    QMap<QString, QString> settings;
    for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
        int colonIdx = line.indexOf(':');
        if (colonIdx != -1) settings[line.left(colonIdx)] = line.mid(colonIdx + 1).trimmed();
    }

    QString method = settings["ipv6.method"];
    if (method == "manual") {
        m_staticRadio->setChecked(true);
        QString addr = settings["ipv6.addresses"].split(',').first().trimmed();
        if (addr.contains('/')) {
            QStringList parts = addr.split('/');
            m_ipEdit->setText(parts[0]);
            m_prefixEdit->setText(parts[1]);
        } else {
            m_ipEdit->setText(addr);
            m_prefixEdit->setText("64");
        }
        m_gatewayEdit->setText(settings["ipv6.gateway"]);
    } else {
        m_dhcpRadio->setChecked(true);
    }

    QString dns = settings["ipv6.dns"];
    if (!dns.isEmpty() && (method == "manual" || method == "auto")) {
        m_dnsStaticRadio->setChecked(true);
        QStringList dnsList = dns.split(',');
        if (dnsList.size() > 0) m_prefDnsEdit->setText(dnsList[0].trimmed());
        if (dnsList.size() > 1) m_altDnsEdit->setText(dnsList[1].trimmed());
    } else {
        m_dnsDhcpRadio->setChecked(true);
    }
    toggleFields();
}

void IPv6PropertiesDialog::applySettings() {
    QString conName = getConnectionName();
    if (conName.isEmpty()) { accept(); return; }

    QStringList args;
    args << "nmcli" << "con" << "modify" << conName;

    if (m_dhcpRadio->isChecked()) {
        args << "ipv6.method" << "auto" << "ipv6.addresses" << "" << "ipv6.gateway" << "";
    } else {
        QString prefix = m_prefixEdit->text().isEmpty() ? "64" : m_prefixEdit->text();
        QString ipCidr = QString("%1/%2").arg(m_ipEdit->text(), prefix);
        args << "ipv6.method" << "manual" << "ipv6.addresses" << ipCidr << "ipv6.gateway" << m_gatewayEdit->text();
    }

    if (m_dnsDhcpRadio->isChecked()) {
        args << "ipv6.dns" << "" << "ipv6.ignore-auto-dns" << "no";
    } else {
        QStringList dnsList;
        if (!m_prefDnsEdit->text().isEmpty()) dnsList << m_prefDnsEdit->text();
        if (!m_altDnsEdit->text().isEmpty()) dnsList << m_altDnsEdit->text();
        args << "ipv6.dns" << dnsList.join(",") << "ipv6.ignore-auto-dns" << "yes";
    }

    QProcess::execute("pkexec", args);
    QProcess::execute("pkexec", {"nmcli", "con", "up", conName});
    accept();
}

// ==============================================================================
// QoSPropertiesDialog
// ==============================================================================

QoSPropertiesDialog::QoSPropertiesDialog(const QString &ifaceName, QWidget *parent)
    : QDialog(parent), m_iface(ifaceName) 
{
    setWindowTitle("QoS Packet Scheduler Properties");
    resize(350, 150);

    auto *layout = new QVBoxLayout(this);
    m_enableCb = new QCheckBox("Enable Bandwidth Limiting (TBF)", this);
    layout->addWidget(m_enableCb);

    auto *form = new QFormLayout();
    m_rateSpinBox = new QSpinBox(this);
    m_rateSpinBox->setRange(1, 10000);
    m_rateSpinBox->setSuffix(" Mbit/s");
    form->addRow("Maximum Egress Rate:", m_rateSpinBox);
    layout->addLayout(form);
    layout->addStretch();

    auto *btns = new QHBoxLayout();
    btns->addStretch();
    auto *okBtn = new QPushButton("OK", this);
    auto *cancelBtn = new QPushButton("Cancel", this);
    btns->addWidget(okBtn);
    btns->addWidget(cancelBtn);
    layout->addLayout(btns);

    connect(m_enableCb, &QCheckBox::toggled, m_rateSpinBox, &QWidget::setEnabled);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, &QoSPropertiesDialog::applyQos);

    loadQos();
}

void QoSPropertiesDialog::loadQos() {
    QProcess proc;
    proc.start("tc", {"qdisc", "show", "dev", m_iface});
    proc.waitForFinished();
    QString output = QString::fromUtf8(proc.readAllStandardOutput());

    if (output.contains("qdisc tbf")) {
        m_enableCb->setChecked(true);
        m_rateSpinBox->setEnabled(true);
        
        QRegularExpression rx(R"(rate\s+(\d+)Mbit)");
        QRegularExpressionMatch match = rx.match(output);
        if (match.hasMatch()) {
            m_rateSpinBox->setValue(match.captured(1).toInt());
        }
    } else {
        m_enableCb->setChecked(false);
        m_rateSpinBox->setEnabled(false);
        m_rateSpinBox->setValue(100);
    }
}

void QoSPropertiesDialog::applyQos() {
    QProcess::execute("pkexec", {"tc", "qdisc", "del", "dev", m_iface, "root"});

    if (m_enableCb->isChecked()) {
        QString rate = QString("%1mbit").arg(m_rateSpinBox->value());
        // Apply new TBF rule (burst and latency are standard defaults for general limiting)
        QProcess::execute("pkexec", {"tc", "qdisc", "add", "dev", m_iface, "root", "tbf", 
                                     "rate", rate, "burst", "32kbit", "latency", "400ms"});
    }
    accept();
}

// ==============================================================================
// AdapterPropertiesDialog
// ==============================================================================

AdapterPropertiesDialog::AdapterPropertiesDialog(const QString &ifaceName, const QString &hardwareName, QWidget *parent)
    : QDialog(parent), m_ifaceName(ifaceName) 
{
    setWindowTitle(QString("%1 Properties").arg(ifaceName));
    resize(400, 520);

    auto *mainLayout = new QVBoxLayout(this);
    
    auto *tabWidget = new QTabWidget(this);
    auto *netTab = new QWidget(tabWidget);
    auto *netLayout = new QVBoxLayout(netTab); 

    netLayout->addWidget(new QLabel("Connect using:", netTab));
    
    auto *adapterRow = new QHBoxLayout();
    auto *iconLabel = new QLabel(netTab);
    QIcon myIcon = QIcon::fromTheme(QStringLiteral("audio-card"), style()->standardIcon(QStyle::SP_ComputerIcon));

    // Set to 16 bc anything larger seems to load an icon with the audio icon on it.
    iconLabel->setPixmap(myIcon.pixmap(16, 16));
    adapterRow->addWidget(iconLabel);
    adapterRow->addWidget(new QLabel(hardwareName, netTab), 1);
    auto *configBtn = new QPushButton("Configure...", netTab);
    adapterRow->addWidget(configBtn);
    netLayout->addLayout(adapterRow);

    netLayout->addSpacing(10);
    netLayout->addWidget(new QLabel("This connection uses the following items:", netTab));

    m_itemsList = new QListWidget(netTab);

    QIcon qosIcon = QIcon::fromTheme("system-upgrade-symbolic");
    bool qosEnabled = getQosState();
    addListItem("QoS Packet Scheduler", qosEnabled, qosIcon);
    
    QIcon tcpIpIcon = QIcon::fromTheme("network-workgroup");
    bool ipv4Enabled = getIpv4State();
    addListItem("Internet Protocol Version 4 (TCP/IPv4)", ipv4Enabled, tcpIpIcon);
    
    bool ipv6Enabled = getIpv6State();
    addListItem("Internet Protocol Version 6 (TCP/IPv6)", ipv6Enabled, tcpIpIcon);

    QIcon lldpIcon = QIcon::fromTheme("network-server");
    bool lldpEnabled = getLldpState();
    addListItem("LLDP Protocol Driver", lldpEnabled, lldpIcon);
    
    netLayout->addWidget(m_itemsList);

    auto *listBtns = new QHBoxLayout();
    m_propsBtn = new QPushButton("Properties", netTab);
    m_propsBtn->setEnabled(false); 

    listBtns->addStretch();
    listBtns->addWidget(m_propsBtn);
    netLayout->addLayout(listBtns);

    auto *descGroup = new QGroupBox("Description", netTab);
    auto *descLayout = new QVBoxLayout(descGroup);
    m_descLabel = new QLabel("Select an item to see its description.", descGroup);
    m_descLabel->setWordWrap(true);
    m_descLabel->setMinimumHeight(50);
    m_descLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    descLayout->addWidget(m_descLabel);
    netLayout->addWidget(descGroup);

    tabWidget->addTab(netTab, "General");

// Determine if the interface is Wi-Fi and if it is currently connected
    QProcess typeProc;
    typeProc.start("nmcli", {"-t", "-f", "GENERAL.TYPE,GENERAL.CONNECTION", "dev", "show", m_ifaceName});
    typeProc.waitForFinished();
    QString devOutput = QString::fromUtf8(typeProc.readAllStandardOutput());
    
    bool isWifi = false;
    bool isConnected = false;
    QString activeCon;

    for (const QString &line : devOutput.split('\n', Qt::SkipEmptyParts)) {
        if (line.startsWith("GENERAL.TYPE:")) {
            isWifi = line.mid(13).toLower().contains("wifi");
        } else if (line.startsWith("GENERAL.CONNECTION:")) {
            activeCon = line.mid(19).trimmed();
            isConnected = (!activeCon.isEmpty() && activeCon != "--");
        }
    }

    auto *authTab = new QWidget(tabWidget);

    if (isWifi && isConnected) {
        // ================= SECURITY TAB (Connected Wi-Fi) =================
        auto *wifiSecLayout = new QVBoxLayout(authTab);
        wifiSecLayout->setContentsMargins(12, 16, 12, 12);
        
        auto *gridLayout = new QGridLayout();
        gridLayout->setSpacing(10);
        
        gridLayout->addWidget(new QLabel("Security type:", authTab), 0, 0);
        auto *secTypeCombo = new QComboBox(authTab);
        secTypeCombo->setObjectName("wifiSecType");
        secTypeCombo->addItems({"No authentication (Open)", "Shared", "WPA2-Personal", "WPA-Personal", "WPA2-Enterprise", "WPA-Enterprise", "802.1x"});
        gridLayout->addWidget(secTypeCombo, 0, 1);
        
        gridLayout->addWidget(new QLabel("Encryption type:", authTab), 1, 0);
        auto *encTypeCombo = new QComboBox(authTab);
        encTypeCombo->addItems({"TKIP", "AES"});
        gridLayout->addWidget(encTypeCombo, 1, 1);
        
        gridLayout->addWidget(new QLabel("Network security key:", authTab), 2, 0);
        auto *keyEdit = new QLineEdit(authTab);
        keyEdit->setObjectName("wifiKey");
        keyEdit->setEchoMode(QLineEdit::Password);
        gridLayout->addWidget(keyEdit, 2, 1);
        
        auto *showCharsCb = new QCheckBox("Show characters", authTab);
        gridLayout->addWidget(showCharsCb, 3, 1);
        
        gridLayout->setColumnStretch(1, 1);
        wifiSecLayout->addLayout(gridLayout);
        wifiSecLayout->addStretch();
        
        connect(showCharsCb, &QCheckBox::toggled, keyEdit, [keyEdit](bool checked) {
            keyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        });

        connect(secTypeCombo, &QComboBox::currentTextChanged, this, [keyEdit, encTypeCombo](const QString &text) {
            keyEdit->setEnabled(text.contains("Personal") || text == "Shared");
            if (text.contains("Open")) {
                if (encTypeCombo->findText("None") == -1) encTypeCombo->addItem("None");
                encTypeCombo->setCurrentText("None");
                encTypeCombo->setEnabled(false);
            } else {
                int idx = encTypeCombo->findText("None");
                if (idx != -1) encTypeCombo->removeItem(idx);
                encTypeCombo->setEnabled(true);
                encTypeCombo->setCurrentText("AES"); 
            }
        });

        QProcess wifiProc;
        wifiProc.start("nmcli", {"-t", "-s", "-f", "802-11-wireless-security.key-mgmt,802-11-wireless-security.psk", "con", "show", activeCon});
        wifiProc.waitForFinished();
        QString out = QString::fromUtf8(wifiProc.readAllStandardOutput());
        QMap<QString, QString> secMap;
        for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
            int idx = line.indexOf(':');
            if (idx > 0) secMap[line.left(idx)] = line.mid(idx + 1).trimmed();
        }

        QString keyMgmt = secMap["802-11-wireless-security.key-mgmt"];
        if (keyMgmt == "wpa-psk" || keyMgmt == "sae") {
            secTypeCombo->setCurrentText("WPA2-Personal");
            keyEdit->setText(secMap["802-11-wireless-security.psk"]);
        } else if (keyMgmt == "wpa-eap") { secTypeCombo->setCurrentText("WPA2-Enterprise");
        } else if (keyMgmt == "ieee8021x") { secTypeCombo->setCurrentText("802.1x");
        } else if (keyMgmt == "none") { secTypeCombo->setCurrentText(secMap.contains("wep-key0") ? "Shared" : "No authentication (Open)");
        } else { secTypeCombo->setCurrentText("No authentication (Open)"); }

        tabWidget->addTab(authTab, "Security");

    } else if (isWifi && !isConnected) {
        // ================= WIRELESS NETWORKS TAB (Disconnected Wi-Fi) =================
        auto *wirelessLayout = new QVBoxLayout(authTab);
        wirelessLayout->setContentsMargins(12, 12, 12, 12);

        auto *useLinuxCb = new QCheckBox("Use Linux to configure my wireless network settings", authTab);
        useLinuxCb->setChecked(true);
        wirelessLayout->addWidget(useLinuxCb);

        // Group 1: Available networks
        auto *availGroup = new QGroupBox("Available networks", authTab);
        auto *availLayout = new QVBoxLayout(availGroup);
        availLayout->setSpacing(8);
        
        auto *availDesc = new QLabel("To connect to, disconnect from, or find out more information\nabout wireless networks in range, click the button below.", availGroup);
        availDesc->setWordWrap(true);
        availLayout->addWidget(availDesc);

        auto *availBtnLayout = new QHBoxLayout();
        availBtnLayout->addStretch();
        auto *viewNetsBtn = new QPushButton("View Wireless Networks", availGroup);
        availBtnLayout->addWidget(viewNetsBtn);
        availLayout->addLayout(availBtnLayout);
        wirelessLayout->addWidget(availGroup);

        // Group 2: Preferred networks
        auto *prefGroup = new QGroupBox("Preferred networks", authTab);
        auto *prefLayout = new QVBoxLayout(prefGroup);
        prefLayout->addWidget(new QLabel("Automatically connect to available networks in the order listed\nbelow:", prefGroup));

        auto *listRowLayout = new QHBoxLayout();
        auto *savedNetsList = new QListWidget(prefGroup);
        savedNetsList->setObjectName("prefNetsList");
        listRowLayout->addWidget(savedNetsList);

        auto *moveBtnsLayout = new QVBoxLayout();
        auto *moveUpBtn = new QPushButton("Move up", prefGroup);
        auto *moveDownBtn = new QPushButton("Move down", prefGroup);
        moveUpBtn->setEnabled(false);
        moveDownBtn->setEnabled(false);
        moveBtnsLayout->addWidget(moveUpBtn);
        moveBtnsLayout->addWidget(moveDownBtn);
        moveBtnsLayout->addStretch();
        listRowLayout->addLayout(moveBtnsLayout);
        prefLayout->addLayout(listRowLayout);

        auto *actionBtnsLayout = new QHBoxLayout();
        auto *addBtn = new QPushButton("Add...", prefGroup);
        auto *removeBtn = new QPushButton("Remove", prefGroup);
        auto *propsBtn = new QPushButton("Properties", prefGroup);
        removeBtn->setEnabled(false);
        propsBtn->setEnabled(false);
        actionBtnsLayout->addWidget(addBtn);
        actionBtnsLayout->addWidget(removeBtn);
        actionBtnsLayout->addWidget(propsBtn);
        actionBtnsLayout->addStretch();
        prefLayout->addLayout(actionBtnsLayout);
        wirelessLayout->addWidget(prefGroup);

        // Bottom link / Advanced
        auto *bottomRow = new QHBoxLayout();
        auto *learnLabel = new QLabel("Learn about <a href=\"#\">setting up wireless network<br>configuration.</a>", authTab);
        learnLabel->setOpenExternalLinks(false); // Decorative
        auto *advancedBtn = new QPushButton("Advanced", authTab);
        bottomRow->addWidget(learnLabel);
        bottomRow->addStretch();
        bottomRow->addWidget(advancedBtn, 0, Qt::AlignBottom);
        wirelessLayout->addLayout(bottomRow);

        tabWidget->addTab(authTab, "Wireless Networks");

        // --- Logic & Wiring ---
        auto loadSavedNetworks = [savedNetsList]() {
            savedNetsList->clear();
            QProcess p;
            p.start("nmcli", {"-t", "-f", "NAME,UUID,TYPE", "con", "show"});
            p.waitForFinished();
            QString out = QString::fromUtf8(p.readAllStandardOutput());
            for (const QString &line : out.split('\n', Qt::SkipEmptyParts)) {
                QStringList parts = line.split(':');
                if (parts.size() >= 3 && parts[2] == "802-11-wireless") {
                    auto *item = new QListWidgetItem(parts[0], savedNetsList);
                    item->setData(Qt::UserRole, parts[1]); // Store UUID
                    item->setIcon(QIcon::fromTheme("network-wireless-signal-excellent"));
                }
            }
        };
        loadSavedNetworks();

        // Manage button states
        connect(savedNetsList, &QListWidget::itemSelectionChanged, [=]() {
            bool hasSel = !savedNetsList->selectedItems().isEmpty();
            int row = savedNetsList->currentRow();
            removeBtn->setEnabled(hasSel);
            propsBtn->setEnabled(hasSel);
            moveUpBtn->setEnabled(hasSel && row > 0);
            moveDownBtn->setEnabled(hasSel && row < savedNetsList->count() - 1);
        });

        // Move Up / Down (visually resorts them; saved via pkexec in applySettings)
        connect(moveUpBtn, &QPushButton::clicked, [=]() {
            int row = savedNetsList->currentRow();
            if (row > 0) {
                QListWidgetItem *item = savedNetsList->takeItem(row);
                savedNetsList->insertItem(row - 1, item);
                savedNetsList->setCurrentRow(row - 1);
            }
        });
        connect(moveDownBtn, &QPushButton::clicked, [=]() {
            int row = savedNetsList->currentRow();
            if (row >= 0 && row < savedNetsList->count() - 1) {
                QListWidgetItem *item = savedNetsList->takeItem(row);
                savedNetsList->insertItem(row + 1, item);
                savedNetsList->setCurrentRow(row + 1);
            }
        });

        // Launch nm-connection-editor tools
        connect(viewNetsBtn, &QPushButton::clicked, [this]() {
            WirelessNetworksDialog dlg(m_ifaceName, this);
            dlg.exec();
        });
        connect(advancedBtn, &QPushButton::clicked, []() { QProcess::startDetached("nm-connection-editor", {}); });
        connect(addBtn, &QPushButton::clicked, []() { QProcess::startDetached("nm-connection-editor", {"--type", "wifi", "--create"}); });
        
        connect(propsBtn, &QPushButton::clicked, [savedNetsList]() {
            if (savedNetsList->selectedItems().isEmpty()) return;
            QString uuid = savedNetsList->selectedItems().first()->data(Qt::UserRole).toString();
            QProcess::startDetached("nm-connection-editor", {"--edit", uuid});
        });

        connect(removeBtn, &QPushButton::clicked, [savedNetsList, loadSavedNetworks]() {
            if (savedNetsList->selectedItems().isEmpty()) return;
            QString uuid = savedNetsList->selectedItems().first()->data(Qt::UserRole).toString();
            // Delete inline immediately
            QProcess::execute("pkexec", {"nmcli", "con", "delete", "uuid", uuid});
            loadSavedNetworks();
        });

    } else {
        // ================= AUTHENTICATION TAB (Wired 802.1X) =================
        auto *authLayout = new QVBoxLayout(authTab);
        authLayout->setContentsMargins(12, 16, 12, 12);
        
        auto *authEnableCb = new QCheckBox("Enable IEEE 802.1X authentication for this network", authTab);
        authLayout->addWidget(authEnableCb);
        
        auto *authMethodGroup = new QWidget(authTab);
        auto *authMethodLayout = new QHBoxLayout(authMethodGroup);
        authMethodLayout->setContentsMargins(20, 4, 0, 0); 
        
        auto *authComboLayout = new QVBoxLayout();
        authComboLayout->setSpacing(2);
        authComboLayout->addWidget(new QLabel("Choose a network authentication method:", authMethodGroup));
        auto *authMethodCombo = new QComboBox(authMethodGroup);
        authMethodCombo->addItems({"Protected EAP (PEAP)", "Smart Card or other certificate", "Tunneled TLS (TTLS)", "MD5-Challenge"});
        authComboLayout->addWidget(authMethodCombo);
        authMethodLayout->addLayout(authComboLayout);
        
        auto *authBtnLayout = new QVBoxLayout();
        authBtnLayout->addSpacing(18); 
        auto *authSettingsBtn = new QPushButton("Settings...", authMethodGroup);
        authBtnLayout->addWidget(authSettingsBtn);
        authMethodLayout->addLayout(authBtnLayout);
        
        authLayout->addWidget(authMethodGroup);
        
        auto *authRememberCb = new QCheckBox("Remember my credentials for this connection each time I'm\nlogged on", authTab);
        authLayout->addWidget(authRememberCb);
        authLayout->addStretch();
        
        connect(authEnableCb, &QCheckBox::toggled, authMethodGroup, &QWidget::setEnabled);
        connect(authEnableCb, &QCheckBox::toggled, authRememberCb, &QWidget::setEnabled);
        authEnableCb->setChecked(false); 
        
        tabWidget->addTab(authTab, "Authentication");
    }

    // ================= ADVANCED TAB =================
    auto *advTab = new QWidget(tabWidget);
    auto *advLayout = new QVBoxLayout(advTab);
    advLayout->setContentsMargins(12, 16, 12, 12);
    
    auto *fwGroup = new QGroupBox("Linux Firewall", advTab);
    auto *fwLayout = new QHBoxLayout(fwGroup);
    fwLayout->setContentsMargins(12, 16, 12, 12);
    fwLayout->setSpacing(15);
    
    auto *fwLabel = new QLabel("Protect my computer and network by limiting\nor preventing access to this computer from\nthe Internet", fwGroup);
    fwLabel->setWordWrap(true);
    
    auto *fwBtnLayout = new QVBoxLayout();
    auto *fwBtn = new QPushButton("Settings...", fwGroup);
    fwBtnLayout->addWidget(fwBtn);
    fwBtnLayout->addStretch(); // Push the button to the top to match classic layout
    
    fwLayout->addWidget(fwLabel, 1);
    fwLayout->addLayout(fwBtnLayout);
    
    advLayout->addWidget(fwGroup);
    advLayout->addStretch();
    
    tabWidget->addTab(advTab, "Advanced");

    // Wire up the Firewall button to navigate the underlying window and close the dialog
    connect(fwBtn, &QPushButton::clicked, this, [this]() {
        this->accept();
        for (QWidget *w : QApplication::topLevelWidgets()) {
            if (auto *mainWindow = qobject_cast<MainWindow*>(w)) {
                // Lambda-based invokeMethod avoids string lookups and slot requirements
                QMetaObject::invokeMethod(mainWindow, [mainWindow]() {
                    mainWindow->navigateTo(PageRegistry::pathFor(PageId::Firewall));
                }, Qt::QueuedConnection);
                break;
            }
        }
    });

    mainLayout->addWidget(tabWidget);

    auto *bottomBtns = new QHBoxLayout();
    bottomBtns->addStretch();
    auto *okBtn = new QPushButton("OK", this);
    auto *cancelBtn = new QPushButton("Cancel", this);
    bottomBtns->addWidget(okBtn);
    bottomBtns->addWidget(cancelBtn);
    mainLayout->addLayout(bottomBtns);

    connect(m_itemsList, &QListWidget::itemSelectionChanged, this, &AdapterPropertiesDialog::onSelectionChanged);
    connect(m_propsBtn, &QPushButton::clicked, this, &AdapterPropertiesDialog::openItemProperties);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, &AdapterPropertiesDialog::applySettings);
}

void AdapterPropertiesDialog::addListItem(const QString &text, bool checked, const QIcon &icon)
{
    auto *item = new QListWidgetItem(text, m_itemsList);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    if (!icon.isNull()) {
        item->setIcon(icon);
    }
}

bool AdapterPropertiesDialog::getLldpState() {
    QProcess proc;
    proc.start("nmcli", {"-t", "-f", "GENERAL.CONNECTION", "device", "show", m_ifaceName});
    proc.waitForFinished();
    QString conName = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (conName.startsWith("GENERAL.CONNECTION:")) conName = conName.mid(19);

    if (conName.isEmpty()) return false;

    proc.start("nmcli", {"-t", "-f", "connection.lldp", "con", "show", conName});
    proc.waitForFinished();
    QString val = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();

    // Returns true if value is 1 (rx), 2 (tx), or 3 (rx/tx)
    return val.contains(QRegularExpression("[123]"));
}

bool AdapterPropertiesDialog::getQosState() {
    QProcess proc;
    proc.start("tc", {"qdisc", "show", "dev", m_ifaceName});
    proc.waitForFinished();
    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    
    // If a TBF rule exists on this interface, QoS limiting is enabled
    return output.contains("qdisc tbf");
}

bool AdapterPropertiesDialog::getIpv4State() {
    QProcess proc;
    proc.start("nmcli", {"-t", "-f", "GENERAL.CONNECTION", "device", "show", m_ifaceName});
    proc.waitForFinished();
    QString conName = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (conName.startsWith("GENERAL.CONNECTION:")) conName = conName.mid(19);
    if (conName.isEmpty()) return false;

    proc.start("nmcli", {"-t", "-f", "ipv4.method", "con", "show", conName});
    proc.waitForFinished();
    QString val = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (val.startsWith("ipv4.method:")) val = val.mid(12);
    
    return val != "disabled";
}

bool AdapterPropertiesDialog::getIpv6State() {
    QProcess proc;
    proc.start("nmcli", {"-t", "-f", "GENERAL.CONNECTION", "device", "show", m_ifaceName});
    proc.waitForFinished();
    QString conName = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (conName.startsWith("GENERAL.CONNECTION:")) conName = conName.mid(19);
    if (conName.isEmpty()) return false;

    proc.start("nmcli", {"-t", "-f", "ipv6.method", "con", "show", conName});
    proc.waitForFinished();
    QString val = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (val.startsWith("ipv6.method:")) val = val.mid(12);
    
    // IPv6 can be "ignore" or "disabled" when turned off
    return (val != "ignore" && val != "disabled");
}

void AdapterPropertiesDialog::onSelectionChanged() {
    auto items = m_itemsList->selectedItems();
    if (items.isEmpty()) return;
    
    QString text = items.first()->text();
    m_propsBtn->setEnabled(text != "LLDP Protocol Driver"); // LLDP is just a toggle

    if (text == "Internet Protocol Version 4 (TCP/IPv4)") {
        m_descLabel->setText("Transmission Control Protocol/Internet Protocol. The default wide area network protocol.");
    } else if (text == "Internet Protocol Version 6 (TCP/IPv6)") {
        m_descLabel->setText("TCP/IPv6. The latest version of the internet protocol that provides communication across diverse interconnected networks.");
    } else if (text == "QoS Packet Scheduler") {
        m_descLabel->setText("Quality of Service Packet Scheduler. This component provides network traffic rate limiting (via Linux tc).");
    } else if (text == "LLDP Protocol Driver") {
        m_descLabel->setText("Link Layer Discovery Protocol. Allows the device to advertise its identity and capabilities on the local network.");
    }
}

void AdapterPropertiesDialog::openItemProperties() {
    auto items = m_itemsList->selectedItems();
    if (items.isEmpty()) return;

    QString text = items.first()->text();
    if (text == "Internet Protocol Version 4 (TCP/IPv4)") {
        IPv4PropertiesDialog dlg(m_ifaceName, this);
        dlg.exec();
    } else if (text == "Internet Protocol Version 6 (TCP/IPv6)") {
        IPv6PropertiesDialog dlg(m_ifaceName, this);
        dlg.exec();
    } else if (text == "QoS Packet Scheduler") {
        QoSPropertiesDialog dlg(m_ifaceName, this);
        dlg.exec();
    }
}

void AdapterPropertiesDialog::applySettings() {
    QProcess proc;
    proc.start("nmcli", {"-t", "-f", "GENERAL.CONNECTION", "device", "show", m_ifaceName});
    proc.waitForFinished();
    QString conName = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (conName.startsWith("GENERAL.CONNECTION:")) conName = conName.mid(19);

    QStringList modifyArgs;
    
    // Only attempt to modify the live interface LLDP/Security if there is an active connection
    if (!conName.isEmpty() && conName != "--") {
        modifyArgs << "nmcli" << "con" << "modify" << conName;

        // 1. LLDP Configuration
        QListWidgetItem* lldpItem = m_itemsList->findItems("LLDP Protocol Driver", Qt::MatchExactly).first();
        bool enableLldp = (lldpItem->checkState() == Qt::Checked);
        modifyArgs << "connection.lldp" << (enableLldp ? "1" : "0");

        // 2. Wi-Fi Security Configuration (if currently on the Connected Wi-Fi Tab)
        auto *secTypeCombo = findChild<QComboBox*>("wifiSecType");
        auto *keyEdit = findChild<QLineEdit*>("wifiKey");

        if (secTypeCombo) {
            QString secType = secTypeCombo->currentText();
            QString psk = keyEdit->text();

            if (secType.contains("Personal")) {
                modifyArgs << "wifi-sec.key-mgmt" << "wpa-psk";
                if (!psk.isEmpty()) modifyArgs << "wifi-sec.psk" << psk;
            } else if (secType.contains("Enterprise")) {
                modifyArgs << "wifi-sec.key-mgmt" << "wpa-eap";
            } else if (secType == "Shared") {
                modifyArgs << "wifi-sec.key-mgmt" << "none" << "wifi-sec.wep-key-type" << "1"; 
                if (!psk.isEmpty()) modifyArgs << "wifi-sec.wep-key0" << psk;
            } else {
                modifyArgs << "wifi-sec.key-mgmt" << "none" << "wifi-sec.wep-key-type" << "0";
            }
        }

        QProcess::execute("pkexec", modifyArgs);
        QProcess::execute("pkexec", {"nmcli", "con", "up", conName});
    }

    // 3. Save "Preferred Network" Priorities (if currently on the Disconnected Wi-Fi Tab)
    auto *prefNetsList = findChild<QListWidget*>("prefNetsList");
    if (prefNetsList) {
        QString script;
        int count = prefNetsList->count();
        for (int i = 0; i < count; ++i) {
            QString uuid = prefNetsList->item(i)->data(Qt::UserRole).toString();
            int priority = count - i; // Top of the list gets highest number (highest priority)
            script += QString("nmcli con modify uuid %1 connection.autoconnect-priority %2; ").arg(uuid).arg(priority);
        }
        
        // Execute all priority changes in a single shell command to avoid multiple pkexec password prompts
        if (!script.isEmpty()) {
            QProcess::execute("pkexec", {"/bin/sh", "-c", script});
        }
    }
    
    accept();
}

// ==============================================================================
// ConnectionStatusDialog
// ==============================================================================

ConnectionStatusDialog::ConnectionStatusDialog(const QString &ifaceName, QWidget *parent)
    : QDialog(parent), m_iface(ifaceName), m_secondsConnected(0) 
{
    setWindowTitle(QString("%1 Status").arg(ifaceName));
    resize(380, 460);

    auto *mainLayout = new QVBoxLayout(this);
    auto *tabWidget = new QTabWidget(this);

    // ================= GENERAL TAB =================
    auto *generalTab = new QWidget(tabWidget);
    auto *genLayout = new QVBoxLayout(generalTab);

    auto *connGroup = new QGroupBox("Connection", generalTab);
    auto *connForm = new QFormLayout(connGroup);

    m_statusLabel = new QLabel("Disconnected", connGroup);
    m_durationLabel = new QLabel("00:00:00", connGroup);
    m_speedLabel = new QLabel("Unknown", connGroup);

    connForm->addRow("Status:", m_statusLabel);

    checkIfaceType();

    if (m_isWifi) {
        m_networkLabel = new QLabel("Searching...", connGroup);
        connForm->addRow("Network:", m_networkLabel);
    } else {
        m_networkLabel = nullptr;
        m_signalBar = nullptr;
    }

    connForm->addRow("Duration:", m_durationLabel);
    connForm->addRow("Speed:", m_speedLabel);

    if (m_isWifi) {
        m_signalBar = new QProgressBar(connGroup);
        m_signalBar->setRange(0, 100);
        m_signalBar->setTextVisible(false);
        m_signalBar->setFixedHeight(14);
        connForm->addRow("Signal Strength:", m_signalBar);
    }

    genLayout->addWidget(connGroup);

    // Activity Group
    auto *actGroup = new QGroupBox("Activity", generalTab);
    auto *actLayout = new QVBoxLayout(actGroup);

    auto *headerLayout = new QHBoxLayout();
    auto *iconLabel = new QLabel(actGroup);
    iconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("network-workgroup"), style()->standardIcon(QStyle::SP_ComputerIcon)).pixmap(32, 32));
    
    headerLayout->addStretch();
    headerLayout->addWidget(new QLabel("Sent ―", actGroup));
    headerLayout->addSpacing(15);
    headerLayout->addWidget(iconLabel);
    headerLayout->addSpacing(15);
    headerLayout->addWidget(new QLabel("― Received", actGroup));
    headerLayout->addStretch();

    actLayout->addLayout(headerLayout);

    auto *packetsLayout = new QHBoxLayout();
    m_sentLabel = new QLabel("0", actGroup);
    m_recvLabel = new QLabel("0", actGroup);

    packetsLayout->addWidget(new QLabel("Packets:", actGroup));
    packetsLayout->addStretch();
    packetsLayout->addWidget(m_sentLabel);
    packetsLayout->addSpacing(20);
    packetsLayout->addWidget(new QLabel("|", actGroup));
    packetsLayout->addSpacing(20);
    packetsLayout->addWidget(m_recvLabel);
    packetsLayout->addStretch();

    actLayout->addLayout(packetsLayout);
    genLayout->addWidget(actGroup);

    tabWidget->addTab(generalTab, "General");

    // ================= SUPPORT TAB =================
    auto *supportTab = new QWidget(tabWidget);
    auto *suppLayout = new QVBoxLayout(supportTab);

    auto *suppGroup = new QGroupBox("Connection status", supportTab);
    auto *suppForm = new QFormLayout(suppGroup);

    m_addrTypeLabel = new QLabel("Assigned by DHCP", suppGroup);
    m_ipLabel = new QLabel("0.0.0.0", suppGroup);
    m_maskLabel = new QLabel("0.0.0.0", suppGroup);
    m_gatewayLabel = new QLabel("0.0.0.0", suppGroup);

    suppForm->addRow("Address Type:", m_addrTypeLabel);
    suppForm->addRow("IP Address:", m_ipLabel);
    suppForm->addRow("Subnet Mask:", m_maskLabel);
    suppForm->addRow("Default Gateway:", m_gatewayLabel);

    suppLayout->addWidget(suppGroup);

    auto *detailsBtnLayout = new QHBoxLayout();
    auto *detailsBtn = new QPushButton("Details...", supportTab);
    detailsBtnLayout->addWidget(detailsBtn);
    detailsBtnLayout->addStretch();
    suppLayout->addLayout(detailsBtnLayout);
    suppLayout->addStretch();

    tabWidget->addTab(supportTab, "Support");
    mainLayout->addWidget(tabWidget);

    // ================= BOTTOM BUTTONS =================
    auto *bottomBtns = new QHBoxLayout();
    auto *propBtn = new QPushButton("Properties", this);
    auto *disableBtn = new QPushButton("Disable", this);
    
    bottomBtns->addWidget(propBtn);
    bottomBtns->addWidget(disableBtn);

    if (m_isWifi) {
        auto *wifiBtn = new QPushButton("View Wireless Networks", this);
        bottomBtns->addWidget(wifiBtn);
        connect(wifiBtn, &QPushButton::clicked, this, [this]() {
            WirelessNetworksDialog dlg(m_iface, this);
            dlg.exec();
        });
    }

    bottomBtns->addStretch();
    auto *closeBtn = new QPushButton("Close", this);
    bottomBtns->addWidget(closeBtn);
    mainLayout->addLayout(bottomBtns);

    // Signal Connections
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(propBtn, &QPushButton::clicked, this, [this]() {
        AdapterPropertiesDialog dlg(m_iface, m_iface, this);
        dlg.exec();
    });
    connect(disableBtn, &QPushButton::clicked, this, [this]() {
        QProcess::execute("pkexec", {"ip", "link", "set", m_iface, "down"});
        reject();
    });
    connect(detailsBtn, &QPushButton::clicked, this, &ConnectionStatusDialog::showDetailsDialog);

    // Timer for Live Refresh
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ConnectionStatusDialog::updateMetrics);

    // Fetch the actual connection uptime from NetworkManager
    QProcess proc;
    proc.start("nmcli", {"-t", "-f", "GENERAL.CONNECTION", "device", "show", m_iface});
    proc.waitForFinished();
    QString conName = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    
    if (conName.startsWith("GENERAL.CONNECTION:")) {
        conName = conName.mid(19).trimmed();
    }

    if (!conName.isEmpty() && conName != "--") {
        proc.start("nmcli", {"-t", "-f", "connection.timestamp", "con", "show", conName});
        proc.waitForFinished();
        QString tsOutput = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        
        if (tsOutput.startsWith("connection.timestamp:")) {
            tsOutput = tsOutput.mid(21).trimmed();
        }
        
        qint64 ts = tsOutput.toLongLong();
        if (ts > 0) {
            qint64 now = QDateTime::currentSecsSinceEpoch();
            if (now >= ts) {
                m_secondsConnected = now - ts;
            }
        }
    }

    m_timer->start(1000);

    loadSupportData();
    updateMetrics();
}

void ConnectionStatusDialog::checkIfaceType() {
    QProcess proc;
    proc.start("nmcli", {"-t", "-f", "GENERAL.TYPE", "dev", "show", m_iface}); 
    proc.waitForFinished();
    QString type = QString::fromUtf8(proc.readAllStandardOutput()).toLower();
    m_isWifi = type.contains("wifi");
}

quint64 ConnectionStatusDialog::readSysStat(const QString &stat) {
    QFile file(QString("/sys/class/net/%1/statistics/%2").arg(m_iface, stat));
    if (file.open(QIODevice::ReadOnly)) {
        return file.readAll().trimmed().toULongLong();
    }
    return 0;
}

void ConnectionStatusDialog::updateMetrics() {
    QNetworkInterface iface = QNetworkInterface::interfaceFromName(m_iface);
    bool isUp = iface.isValid() && iface.flags().testFlag(QNetworkInterface::IsUp);

    bool isConnected = false;
    QProcess proc; // Declare it here so the entire function can use it

    if (!isUp) {
        m_statusLabel->setText("Disabled");
    } else {
        proc.start("nmcli", {"-t", "-f", "GENERAL.STATE", "dev", "show", m_iface});
        proc.waitForFinished();
        QString stateStr = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        
        if (stateStr.contains("(")) {
            int start = stateStr.indexOf('(') + 1;
            int end = stateStr.indexOf(')');
            if (start > 0 && end > start) {
                QString rawStatus = stateStr.mid(start, end - start);
                if (!rawStatus.isEmpty()) {
                    isConnected = (rawStatus == "connected");
                    rawStatus[0] = rawStatus[0].toUpper();
                    m_statusLabel->setText(rawStatus);
                }
            }
        }
    }

    if (isConnected) {
        m_secondsConnected++;
        int hrs = m_secondsConnected / 3600;
        int mins = (m_secondsConnected % 3600) / 60;
        int secs = m_secondsConnected % 60;
        m_durationLabel->setText(QString("%1:%2:%3")
            .arg(hrs, 2, 10, QChar('0'))
            .arg(mins, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0')));
    } else {
        m_secondsConnected = 0;
        m_durationLabel->setText("00:00:00");
    }

    // Activity Packet Counters
    quint64 txPackets = readSysStat("tx_packets");
    quint64 rxPackets = readSysStat("rx_packets");
    m_sentLabel->setText(QString::number(txPackets));
    m_recvLabel->setText(QString::number(rxPackets));

    // Activity Packet Counters
    QFile speedFile(QString("/sys/class/net/%1/speed").arg(m_iface));
    if (speedFile.open(QIODevice::ReadOnly)) {
        int spd = speedFile.readAll().trimmed().toInt();
        if (spd > 0) m_speedLabel->setText(QString("%1.0 Mbps").arg(spd));
    }

    // Wi-Fi Specific Updates
    if (m_isWifi) {
        proc.start("nmcli", {"-t", "-f", "ACTIVE,SSID,SIGNAL", "dev", "wifi"});
        proc.waitForFinished();
        QString output = QString::fromUtf8(proc.readAllStandardOutput());
        for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
            if (line.startsWith("yes:")) {
                QStringList parts = line.split(':');
                if (parts.size() >= 3) {
                    if (m_networkLabel) m_networkLabel->setText(parts[1]);
                    if (m_signalBar) m_signalBar->setValue(parts[2].toInt());
                }
                break;
            }
        }
    }
}

void ConnectionStatusDialog::loadSupportData() {
    QNetworkInterface iface = QNetworkInterface::interfaceFromName(m_iface);
    QString ipStr = "0.0.0.0";
    QString maskStr = "0.0.0.0";
    
    for (const auto &entry : iface.addressEntries()) {
        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
            ipStr = entry.ip().toString();
            maskStr = entry.netmask().toString();
            break;
        }
    }
    m_ipLabel->setText(ipStr);
    m_maskLabel->setText(maskStr);

    QProcess proc;
    proc.start("nmcli", {"-t", "-f", "IP4.GATEWAY,GENERAL.CONNECTION", "dev", "show", m_iface});
    proc.waitForFinished();
    QString devOutput = QString::fromUtf8(proc.readAllStandardOutput());

    QString gateway = "None";
    QString conName = "";

    for (const QString &line : devOutput.split('\n', Qt::SkipEmptyParts)) {
        if (line.startsWith("IP4.GATEWAY:")) {
            gateway = line.mid(12).trimmed();
        } else if (line.startsWith("GENERAL.CONNECTION:")) {
            conName = line.mid(19).trimmed();
        }
    }
    m_gatewayLabel->setText(gateway.isEmpty() ? "None" : gateway);

    if (!conName.isEmpty() && conName != "--") {
        proc.start("nmcli", {"-t", "-f", "ipv4.method", "con", "show", conName});
        proc.waitForFinished();
        QString methodOutput = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (methodOutput.startsWith("ipv4.method:")) {
            methodOutput = methodOutput.mid(12).trimmed();
        }
        m_addrTypeLabel->setText(methodOutput == "manual" ? "Manually Configured" : "Assigned by DHCP");
    } else {
        m_addrTypeLabel->setText("Assigned by DHCP");
    }
}

void ConnectionStatusDialog::showDetailsDialog() {
    QNetworkInterface iface = QNetworkInterface::interfaceFromName(m_iface);
    QString hwAddress = iface.hardwareAddress();
    QString dhcpEnabled = (m_addrTypeLabel->text() == "Assigned by DHCP") ? "Yes" : "No";

    // Query extended details via nmcli
    QProcess proc;
    proc.start("nmcli", {"-t", "dev", "show", m_iface});
    proc.waitForFinished();
    QString output = QString::fromUtf8(proc.readAllStandardOutput());

    QString description = iface.humanReadableName(); // Fallback
    QString dnsSuffix = "";
    QStringList ipv4Dns, ipv6Dns;
    QString dhcpServer = "";
    QString ipv6LinkLocal = "";
    QString ipv6Gateway = "";

    // Parse the nmcli output
    for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
        QStringList parts = line.split(':');
        if (parts.size() < 2) continue;
        
        QString key = parts[0].trimmed();
        // Re-join the value in case it contains colons (like MAC addresses or IPv6 addresses)
        QString val = parts.mid(1).join(':').trimmed(); 

        if (key == "GENERAL.PRODUCT") {
            description = val;
        } else if (key.startsWith("IP4.DOMAIN")) {
            dnsSuffix = val;
        } else if (key.startsWith("IP4.DNS")) {
            ipv4Dns << val;
        } else if (key.startsWith("IP6.DNS")) {
            ipv6Dns << val;
        } else if (key.startsWith("IP6.GATEWAY")) {
            ipv6Gateway = val;
        } else if (key.startsWith("IP6.ADDRESS") && val.startsWith("fe80")) {
            ipv6LinkLocal = val.section('/', 0, 0); // Strip the /64 subnet mask
        } else if (key.startsWith("DHCP4.OPTION") && val.contains("dhcp_server_identifier")) {
            dhcpServer = val.section('=', 1, 1).trimmed();
        }
    }

    // Format the details string
    QString details = QString(
        "Property\t\t|\tValue\n"
        "================================================\n"
        "DNS Suffix:\t\t%1\n"
        "Description:\t\t%2\n"
        "Physical Address:\t\t%3\n"
        "DHCP Enabled:\t\t%4\n"
        "IPv4 Address:\t\t%5\n"
        "IPv4 Subnet Mask:\t\t%6\n"
        "IPv4 Default Gateway:\t%7\n"
        "IPv4 DHCP Server:\t\t%8\n"
        "IPv4 DNS Servers:\t\t%9\n"
        "Link-local IPv6 Address:\t%10\n"
        "IPv6 Default Gateway:\t%11\n"
        "IPv6 DNS Servers:\t\t%12"
    ).arg(
        dnsSuffix.isEmpty() ? "N/A" : dnsSuffix, 
        description,
        hwAddress,
        dhcpEnabled,
        m_ipLabel->text(),
        m_maskLabel->text(),
        m_gatewayLabel->text(),
        dhcpServer.isEmpty() ? "N/A" : dhcpServer,
        ipv4Dns.isEmpty() ? "N/A" : ipv4Dns.join(", "),
        ipv6LinkLocal.isEmpty() ? "N/A" : ipv6LinkLocal,
        ipv6Gateway.isEmpty() ? "N/A" : ipv6Gateway,
        ipv6Dns.isEmpty() ? "N/A" : ipv6Dns.join(", ")
    );

    QDialog dlg(this);
    dlg.setWindowTitle("Network Connection Details");
    dlg.resize(475, 500);

    auto *layout = new QVBoxLayout(&dlg);
    
    // Add the top label
    auto *headerLabel = new QLabel("Network connection details:", &dlg);
    layout->addWidget(headerLabel);
    
    // Create a read-only text box for the details
    auto *textBox = new QTextEdit(&dlg);
    textBox->setReadOnly(true);
    textBox->setPlainText(details);
    textBox->setLineWrapMode(QTextEdit::NoWrap); // Completely prevents wrapping
    textBox->setStyleSheet("QTextEdit { background-color: #ffffff; color: #000000; }");
    layout->addWidget(textBox);

    // Add a close button aligned to the right
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto *closeBtn = new QPushButton("Close", &dlg);
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);

    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    dlg.exec();
}

// ==============================================================================
// WirelessNetworksDialog
// ==============================================================================

WirelessNetworksDialog::WirelessNetworksDialog(const QString &ifaceName, QWidget *parent)
    : QDialog(parent), m_iface(ifaceName)
{
    setWindowTitle("Wireless Network Connection");
    resize(480, 300);

    // Classic flat gray background
    // setStyleSheet("QDialog { background-color: #D4D0C8; }");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    auto *lbl = new QLabel("Select a wireless network from the list below to connect or disconnect:", this);
    mainLayout->addWidget(lbl);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({"Network Name (SSID)", "Security", "Signal"});
    m_tree->setRootIsDecorated(false);
    m_tree->setAllColumnsShowFocus(true);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    
    mainLayout->addWidget(m_tree);

    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    auto *refreshBtn = new QPushButton("Refresh", this);
    m_connectBtn = new QPushButton("Connect", this);
    auto *cancelBtn = new QPushButton("Cancel", this);

    m_connectBtn->setEnabled(false);

    btnLayout->addWidget(refreshBtn);
    btnLayout->addWidget(m_connectBtn);
    btnLayout->addWidget(cancelBtn);

    mainLayout->addLayout(btnLayout);

    connect(refreshBtn, &QPushButton::clicked, this, &WirelessNetworksDialog::refreshNetworks);
    connect(m_connectBtn, &QPushButton::clicked, this, &WirelessNetworksDialog::connectToNetwork);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    
    // Toggle between Connect and Disconnect depending on the item's connection state
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        auto items = m_tree->selectedItems();
        if (items.isEmpty()) {
            m_connectBtn->setEnabled(false);
            m_connectBtn->setText("Connect");
        } else {
            m_connectBtn->setEnabled(true);
            bool inUse = items.first()->data(0, Qt::UserRole + 2).toBool();
            m_connectBtn->setText(inUse ? "Disconnect" : "Connect");
        }
    });

    refreshNetworks();
}

void WirelessNetworksDialog::refreshNetworks() {
    m_tree->clear();
    m_connectBtn->setEnabled(false);
    m_connectBtn->setText("Connect");

    QProcess p;
    p.start("nmcli", {"-t", "-f", "IN-USE,SSID,SECURITY,SIGNAL", "dev", "wifi", "list", "ifname", m_iface});
    p.waitForFinished();
    QString out = QString::fromUtf8(p.readAllStandardOutput());
    
    QSet<QString> seenSSIDs;
    for (const QString& line : out.split('\n', Qt::SkipEmptyParts)) {
        QStringList parts = line.split(':');
        if (parts.size() >= 4) {
            bool inUse = (parts[0].trimmed() == "*");
            QString ssid = parts[1].trimmed();
            QString sec = parts[2].trimmed();
            int signal = parts[3].toInt();

            if (ssid.isEmpty() || seenSSIDs.contains(ssid)) continue;
            seenSSIDs.insert(ssid);

            bool isSecure = !(sec.isEmpty() || sec == "--" || sec.contains("Open", Qt::CaseInsensitive));
            QString secText = isSecure ? sec : "None";
            
            // Override security text if this is the active connection
            if (inUse) {
                secText = "Connected";
            }
            
            auto *item = new QTreeWidgetItem(m_tree);
            item->setText(0, ssid);
            item->setText(1, secText);
            item->setText(2, QString("%1%").arg(signal));

            // Set small icons standard for late 90s lists
            QString iconName = isSecure ? "network-wireless-encrypted" : "network-wireless";
            item->setIcon(0, QIcon::fromTheme(iconName));

            item->setData(0, Qt::UserRole, ssid);
            item->setData(0, Qt::UserRole + 1, isSecure);
            item->setData(0, Qt::UserRole + 2, inUse); // Store connection state
            
            // Bold the currently connected network
            if (inUse) {
                QFont f = item->font(0);
                f.setBold(true);
                item->setFont(0, f);
                item->setFont(1, f);
                item->setFont(2, f);
            }
        }
    }
    
    // Auto-size columns to fit content
    m_tree->resizeColumnToContents(0);
    m_tree->resizeColumnToContents(1);
}

void WirelessNetworksDialog::connectToNetwork() {
    auto items = m_tree->selectedItems();
    if (items.isEmpty()) return;

    QString ssid = items.first()->data(0, Qt::UserRole).toString();
    bool isSecure = items.first()->data(0, Qt::UserRole + 1).toBool();
    bool inUse = items.first()->data(0, Qt::UserRole + 2).toBool();

    // 1. Disconnect if already connected
    if (inUse) {
        int res = QProcess::execute("nmcli", {"dev", "disconnect", m_iface});
        if (res != 0) QProcess::execute("pkexec", {"nmcli", "dev", "disconnect", m_iface});
        accept();
        return;
    }

    // 2. Check if a profile already exists for this network
    QProcess p;
    p.start("nmcli", {"-t", "-f", "NAME,TYPE", "con", "show"});
    p.waitForFinished();
    QString conOut = QString::fromUtf8(p.readAllStandardOutput());
    
    bool profileExists = false;
    for(const QString& line : conOut.split('\n', Qt::SkipEmptyParts)) {
        QStringList parts = line.split(':');
        if (parts.size() >= 2 && parts[1] == "802-11-wireless" && parts[0] == ssid) {
            profileExists = true; 
            break;
        }
    }

    if (profileExists) {
        // Try connecting as the user first (allows native access to the GNOME/KDE user keyring)
        int exitCode = QProcess::execute("nmcli", {"con", "up", "id", ssid});
        if (exitCode == 0) {
            accept();
            return;
        }
        
        // If it failed (missing secrets, locked keyring, corrupted profile),
        // we purge the broken profile to start fresh.
        if (QProcess::execute("nmcli", {"con", "delete", "id", ssid}) != 0) {
            QProcess::execute("pkexec", {"nmcli", "con", "delete", "id", ssid});
        }
    }

    // 3. Establish a new connection (Prompt for password if required)
    if (isSecure) {
        bool ok;
        QString pwd = QInputDialog::getText(this, "WEP/WPA Key Required",
                                            QString("Network security key for '%1':").arg(ssid), 
                                            QLineEdit::Password, "", &ok);
        // Ensure user hit OK and didn't leave the password blank
        if (ok && !pwd.isEmpty()) {
            int connRes = QProcess::execute("nmcli", {"dev", "wifi", "connect", ssid, "password", pwd, "ifname", m_iface});
            
            // Fallback to pkexec if their specific Linux PolKit policy strictly mandates root
            if (connRes != 0) {
                QProcess::execute("pkexec", {"nmcli", "dev", "wifi", "connect", ssid, "password", pwd, "ifname", m_iface});
            }
        } else {
            return; // Cancel the connection attempt
        }
    } else {
        // Connect to open network
        int connRes = QProcess::execute("nmcli", {"dev", "wifi", "connect", ssid, "ifname", m_iface});
        if (connRes != 0) {
            QProcess::execute("pkexec", {"nmcli", "dev", "wifi", "connect", ssid, "ifname", m_iface});
        }
    }
    
    accept();
}

// ==============================================================================
// NetworkConnectionsPage
// ==============================================================================

NetworkConnectionsPage::NetworkConnectionsPage(QScrollArea *sidebar, QWidget *parent)
    : QWidget(parent), m_sidebar(sidebar)
{
    auto *contentV = Win7::pageScaffold(this, sidebar, 20, 700);

    contentV->addWidget(Win7::pageTitle("Network Connections"));
    contentV->addSpacing(16);

    m_listWidget = new QListWidget(this);
    m_listWidget->setViewMode(QListWidget::IconMode);
    m_listWidget->setIconSize(QSize(32, 32));
    m_listWidget->setGridSize(QSize(150, 70));
    m_listWidget->setResizeMode(QListWidget::Adjust);
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_listWidget->setMinimumHeight(350);
    m_listWidget->setStyleSheet("QListWidget { background: transparent; border: 1px solid #D9D9D9; }");

    connect(m_listWidget, &QListWidget::customContextMenuRequested, this, &NetworkConnectionsPage::showContextMenu);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item){ showProperties(item); });
    connect(m_listWidget, &QListWidget::itemSelectionChanged, this, &NetworkConnectionsPage::updateSidebar);
    
    contentV->addWidget(m_listWidget);
    contentV->addStretch(1);
    refreshInterfaces();
}

void NetworkConnectionsPage::disableSelected() {
    auto items = m_listWidget->selectedItems();
    if (items.isEmpty()) return;

    QString ifaceName = items.first()->data(Qt::UserRole).toString();
    QNetworkInterface interface = QNetworkInterface::interfaceFromName(ifaceName);
    bool isUp = interface.flags().testFlag(QNetworkInterface::IsUp);

    toggleInterface(ifaceName, !isUp);
}

void NetworkConnectionsPage::showSelectedStatus() {
    auto items = m_listWidget->selectedItems();
    if (items.isEmpty()) return;

    showStatus(items.first()->data(Qt::UserRole).toString());
}

void NetworkConnectionsPage::showSelectedProperties() {
    auto items = m_listWidget->selectedItems();
    if (items.isEmpty()) return;

    showProperties(items.first());
}

// Ensure your sidebarLinks provides the FULL static list. 
// We will hide/show them dynamically in updateSidebar()
QList<SidebarLink> NetworkConnectionsPage::sidebarLinks() {
    auto getPage = []() -> NetworkConnectionsPage* {
        for (QWidget *w : QApplication::topLevelWidgets()) {
            if (w->inherits("QMainWindow")) {
                if (auto *page = w->findChild<NetworkConnectionsPage*>()) {
                    return page;
                }
            }
        }
        return nullptr;
    };
return {
        Nav::plain("Create a new connection"),
        
        // Default text here doesn't matter much, updateSidebar() will correct it.
        Nav::action("Disable this network device", [getPage]() {
            if (auto *page = getPage()) page->disableSelected();
        }),
        
        Nav::plain("Diagnose this connection"),
        Nav::plain("Rename this connection"),
        
        Nav::action("View status of this connection", [getPage]() {
            if (auto *page = getPage()) page->showSelectedStatus();
        }),
        
        Nav::action("Change settings of this connection", [getPage]() {
            if (auto *page = getPage()) page->showSelectedProperties();
        }),
    };
}

QList<SidebarLink> NetworkConnectionsPage::sidebarSeeAlso() {
    return {
        Nav::to("Network and Sharing Center", PageId::NetworkSharing),
        Nav::plain("Internet Options"),
        Nav::to("Linux Firewall", PageId::Firewall),
    };
}

void NetworkConnectionsPage::refreshInterfaces() {
    QString selectedDev;
    if (!m_listWidget->selectedItems().isEmpty()) {
        selectedDev = m_listWidget->selectedItems().first()->data(Qt::UserRole).toString();
    }
    m_listWidget->clear();
    
    QProcess proc;
    proc.start("nmcli", {"-t", "-f", "DEVICE,TYPE,STATE,CONNECTION", "device", "status"});
    proc.waitForFinished();
    
    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    for (const QString &line : lines) {
        QStringList parts = line.split(':');
        if (parts.size() < 3) continue;
        
        QString dev = parts[0];
        QString type = parts[1];
        QString state = parts[2];
        
        if (type == "loopback") continue;

        QNetworkInterface interface = QNetworkInterface::interfaceFromName(dev);
        bool isUp = interface.isValid() && interface.flags().testFlag(QNetworkInterface::IsUp);

        if (!isUp) {
            state = "Disabled";
        }
        
        QString iconName = "nm-device-wired";

        if (dev.startsWith("br-")) {
            iconName = "bluetooth";
        } else if (dev.startsWith("veth") || dev.startsWith("vnet")) {
            if (state == "connected") iconName = "nm-device-wired";
            else iconName = "network-unavailable";
        } else if (type == "ethernet") {
            if (state == "connected") iconName = "nm-device-wired";
            else if (state == "unavailable") iconName = "network-unavailable";
            else iconName = "network-offline";
        } else if ((type == "wifi") && dev.startsWith("w")) {
            if (state == "connected") iconName = "network-wireless-signal-excellent";
            else if (state == "disconnected") iconName = "network-wireless-disconnected";
            else iconName = "network-wireless-offline";
        } else if (type == "bt" || type == "bluetooth") {
            iconName = "bluetooth";
        } else if (type == "tun" || type == "wireguard") {
            iconName = "network-vpn";
        }
        
        if (state == "connected (local only)" || state == "connected (site only)") {
            iconName = "network-wired-activated-limited";
        } else if (state == "failed") {
            iconName = "network-error";
        }

        if (!isUp) {
            if (type == "wifi" || type.contains("wireless")) {
                iconName = "network-wireless-offline";
            } else {
                iconName = "network-offline";
            }
        }

        QString displayText = QString("%1\n%2").arg(dev, state);
        auto *item = new QListWidgetItem(displayText, m_listWidget);
        
        QIcon devIcon = QIcon::fromTheme(iconName);
        if (devIcon.isNull()) {
            devIcon = style()->standardIcon(QStyle::SP_ComputerIcon);
        }
        
        item->setIcon(devIcon);
        item->setData(Qt::UserRole, dev);
    }

    // Restore selection if the device still exists
    if (!selectedDev.isEmpty()) {
        for (int i = 0; i < m_listWidget->count(); ++i) {
            if (m_listWidget->item(i)->data(Qt::UserRole).toString() == selectedDev) {
                m_listWidget->item(i)->setSelected(true);
                break;
            }
        }
    }
    
    // Trigger sidebar state evaluation
    updateSidebar();
}

void NetworkConnectionsPage::showContextMenu(const QPoint &pos) {
    auto *item = m_listWidget->itemAt(pos);
    if (!item) return;

    QString ifaceName = item->data(Qt::UserRole).toString();
    QNetworkInterface interface = QNetworkInterface::interfaceFromName(ifaceName);
    bool isUp = interface.flags().testFlag(QNetworkInterface::IsUp);

    QMenu contextMenu(this);
    QAction *toggleAction = contextMenu.addAction(isUp ? "Disable" : "Enable");
    contextMenu.addSeparator();
    QAction *statusAction = contextMenu.addAction("Status");
    QAction *propAction = contextMenu.addAction("Properties");
    contextMenu.addSeparator();
    QAction *refreshAction = contextMenu.addAction("Refresh");

    QAction *selectedAction = contextMenu.exec(m_listWidget->mapToGlobal(pos));
    if (selectedAction == toggleAction) {
        toggleInterface(ifaceName, !isUp);
    } else if (selectedAction == statusAction) {
        showStatus(ifaceName);
    } else if (selectedAction == propAction) {
        showProperties(item);
    } else if (selectedAction == refreshAction) {
        refreshInterfaces();
    }
}

void NetworkConnectionsPage::toggleInterface(const QString &ifaceName, bool enable) {
    QString state = enable ? "up" : "down";
    QString cmd = QString("pkexec ip link set %1 %2").arg(ifaceName, state);
    QProcess::execute("/bin/sh", QStringList() << "-c" << cmd);
    refreshInterfaces();
}

void NetworkConnectionsPage::showStatus(const QString &ifaceName) {
    ConnectionStatusDialog dlg(ifaceName, this);
    dlg.exec();
    refreshInterfaces();
}

void NetworkConnectionsPage::showProperties(QListWidgetItem *item) {
    QString ifaceName = item->data(Qt::UserRole).toString();
    QNetworkInterface interface = QNetworkInterface::interfaceFromName(ifaceName);

    AdapterPropertiesDialog dlg(ifaceName, interface.humanReadableName(), this);
    dlg.exec();
    refreshInterfaces();
}

void NetworkConnectionsPage::updateSidebar() {
    if (!m_sidebar) return;

    auto items = m_listWidget->selectedItems();
    bool hasSelection = !items.isEmpty();
    bool isUp = false;

    if (hasSelection) {
        QString ifaceName = items.first()->data(Qt::UserRole).toString();
        QNetworkInterface interface = QNetworkInterface::interfaceFromName(ifaceName);
        isUp = interface.isValid() && interface.flags().testFlag(QNetworkInterface::IsUp);
    }

    // Search for the labels within the sidebar layout and hide/show them
    QList<QLabel*> labels = m_sidebar->findChildren<QLabel*>();
    for (QLabel *lbl : labels) {
        QString text = lbl->text();
        
        // Handle Enable/Disable specifically to toggle its text
        if (text == "Disable this network device" || text == "Enable this network device") {
            lbl->setVisible(hasSelection);
            if (hasSelection) {
                lbl->setText(isUp ? "Disable this network device" : "Enable this network device");
            }
        } 
        // Handle all other context-sensitive buttons
        else if (text == "Diagnose this connection" || 
                 text == "Rename this connection" || 
                 text == "View status of this connection" || 
                 text == "Change settings of this connection") {
            lbl->setVisible(hasSelection);
        }
    }
}