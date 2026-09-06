#pragma once

#include <QWidget>
#include <QListWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProcess>
#include <QMenu>
#include <QNetworkInterface>
#include <QMenu>
#include <QMessageBox>
#include <QDialog>
#include <QLabel>
#include <QFormLayout>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>
#include <QFile>
#include <QStyle>
#include <QProcess>
#include <QRadioButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QTabWidget>
#include <QButtonGroup>
#include <QMap>
#include <QSpinBox>
#include <QCheckBox>
#include <QRegularExpression>
#include "Win7Ui.h"
#include "Commands.h"

// Dialog replicating the "Internet Protocol Version 4 (TCP/IPv4) Properties" window
class IPv4PropertiesDialog : public QDialog {
public:
    IPv4PropertiesDialog(const QString &ifaceName, QWidget *parent = nullptr) 
        : QDialog(parent), m_iface(ifaceName) {
        setWindowTitle("Internet Protocol Version 4 (TCP/IPv4) Properties");
        resize(420, 480);

        auto *mainLayout = new QVBoxLayout(this);
        auto *tabWidget = new QTabWidget(this);
        auto *generalTab = new QWidget(tabWidget);
        auto *generalLayout = new QVBoxLayout(generalTab);

        auto *descLabel = new QLabel("You can get IP settings assigned automatically if your network supports\nthis capability. Otherwise, you need to ask your network administrator\nfor the appropriate IP settings.", generalTab);
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

private:
    QString m_iface;
    QRadioButton *m_dhcpRadio;
    QRadioButton *m_staticRadio;
    QLineEdit *m_ipEdit;
    QLineEdit *m_maskEdit;
    QLineEdit *m_gatewayEdit;

    QRadioButton *m_dnsDhcpRadio;
    QRadioButton *m_dnsStaticRadio;
    QLineEdit *m_prefDnsEdit;
    QLineEdit *m_altDnsEdit;

    void toggleIpFields() {
        bool isStatic = m_staticRadio->isChecked();
        m_ipEdit->setEnabled(isStatic);
        m_maskEdit->setEnabled(isStatic);
        m_gatewayEdit->setEnabled(isStatic);
    }

    void toggleDnsFields() {
        bool isStatic = m_dnsStaticRadio->isChecked();
        m_prefDnsEdit->setEnabled(isStatic);
        m_altDnsEdit->setEnabled(isStatic);
    }

    QString prefixToSubnetMask(int prefix) {
        if (prefix < 0 || prefix > 32) return "";
        uint32_t mask = (prefix == 0) ? 0 : (~0U << (32 - prefix));
        return QString("%1.%2.%3.%4").arg((mask >> 24) & 0xFF).arg((mask >> 16) & 0xFF).arg((mask >> 8) & 0xFF).arg(mask & 0xFF);
    }

    int subnetMaskToPrefix(const QString &mask) {
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

    void loadCurrentSettings() {
        QProcess proc;
        proc.start("nmcli", {"-t", "-f", "GENERAL.CONNECTION", "device", "show", m_iface});
        proc.waitForFinished();
        
        // Read the output and strip the key name from the terse output
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

    void applySettings() {
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
};

class IPv6PropertiesDialog : public QDialog {
public:
    IPv6PropertiesDialog(const QString &ifaceName, QWidget *parent = nullptr) 
        : QDialog(parent), m_iface(ifaceName) {
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
        m_prefixEdit = new QLineEdit(this); // IPv6 uses prefix length (e.g., 64)
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

private:
    QString m_iface;
    QRadioButton *m_dhcpRadio, *m_staticRadio, *m_dnsDhcpRadio, *m_dnsStaticRadio;
    QLineEdit *m_ipEdit, *m_prefixEdit, *m_gatewayEdit, *m_prefDnsEdit, *m_altDnsEdit;

    void toggleFields() {
        bool ipStatic = m_staticRadio->isChecked();
        m_ipEdit->setEnabled(ipStatic);
        m_prefixEdit->setEnabled(ipStatic);
        m_gatewayEdit->setEnabled(ipStatic);

        bool dnsStatic = m_dnsStaticRadio->isChecked();
        m_prefDnsEdit->setEnabled(dnsStatic);
        m_altDnsEdit->setEnabled(dnsStatic);
    }

    QString getConnectionName() {
        QProcess proc;
        proc.start("nmcli", {"-t", "-f", "GENERAL.CONNECTION", "device", "show", m_iface});
        proc.waitForFinished();
        QString conName = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (conName.startsWith("GENERAL.CONNECTION:")) {
            return conName.mid(19); // 19 is length of "GENERAL.CONNECTION:"
        }
        return conName;
    }

    void loadCurrentSettings() {
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

    void applySettings() {
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
};

class QoSPropertiesDialog : public QDialog {
public:
    QoSPropertiesDialog(const QString &ifaceName, QWidget *parent = nullptr) 
        : QDialog(parent), m_iface(ifaceName) {
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

private:
    QString m_iface;
    QCheckBox *m_enableCb;
    QSpinBox *m_rateSpinBox;

    void loadQos() {
        QProcess proc;
        proc.start("tc", {"qdisc", "show", "dev", m_iface});
        proc.waitForFinished();
        QString output = QString::fromUtf8(proc.readAllStandardOutput());

        if (output.contains("qdisc tbf")) {
            m_enableCb->setChecked(true);
            m_rateSpinBox->setEnabled(true);
            
            // Qt 6 regular expression matching
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

    void applyQos() {
        // Clear existing root qdisc
        QProcess::execute("pkexec", {"tc", "qdisc", "del", "dev", m_iface, "root"});

        if (m_enableCb->isChecked()) {
            QString rate = QString("%1mbit").arg(m_rateSpinBox->value());
            // Apply new TBF rule (burst and latency are standard defaults for general limiting)
            QProcess::execute("pkexec", {"tc", "qdisc", "add", "dev", m_iface, "root", "tbf", 
                                         "rate", rate, "burst", "32kbit", "latency", "400ms"});
        }
        accept();
    }
};

// Dialog replicating the "Ethernet Properties" / "Networking" tab window
class AdapterPropertiesDialog : public QDialog {
public:
    AdapterPropertiesDialog(const QString &ifaceName, const QString &hardwareName, QWidget *parent = nullptr) 
        : QDialog(parent), m_ifaceName(ifaceName) {
        setWindowTitle(QString("%1 Properties").arg(ifaceName));
        resize(400, 520);

        auto *mainLayout = new QVBoxLayout(this);
        
        auto *tabWidget = new QTabWidget(this);
        auto *netTab = new QWidget(tabWidget);
        auto *netLayout = new QVBoxLayout(netTab); // Assign layout directly to netTab

        netLayout->addWidget(new QLabel("Connect using:", netTab));
        
        auto *adapterRow = new QHBoxLayout();
        auto *iconLabel = new QLabel(netTab);
        iconLabel->setPixmap(style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(24, 24));
        adapterRow->addWidget(iconLabel);
        adapterRow->addWidget(new QLabel(hardwareName, netTab), 1);
        auto *configBtn = new QPushButton("Configure...", netTab);
        adapterRow->addWidget(configBtn);
        netLayout->addLayout(adapterRow);

        netLayout->addSpacing(10);
        netLayout->addWidget(new QLabel("This connection uses the following items:", netTab));

        m_itemsList = new QListWidget(netTab);

        addListItem("QoS Packet Scheduler");
        addListItem("Internet Protocol Version 4 (TCP/IPv4)");
        addListItem("Internet Protocol Version 6 (TCP/IPv6)");
        
        // Load actual LLDP state from NetworkManager
        bool lldpEnabled = getLldpState();
        addListItem("LLDP Protocol Driver", lldpEnabled);
        
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

        // Populate Tab Widget
        tabWidget->addTab(netTab, "Networking");
        tabWidget->addTab(new QWidget(tabWidget), "Sharing");
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

private:
    QString m_ifaceName;
    QListWidget *m_itemsList;
    QPushButton *m_propsBtn;
    QLabel *m_descLabel;

    void addListItem(const QString &text, bool checked = true) {
        auto *item = new QListWidgetItem(text, m_itemsList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }

    bool getLldpState() {
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

    void onSelectionChanged() {
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

    void openItemProperties() {
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

    void applySettings() {
        // Save LLDP setting based on checkbox state
        QListWidgetItem* lldpItem = m_itemsList->findItems("LLDP Protocol Driver", Qt::MatchExactly).first();
        bool enableLldp = (lldpItem->checkState() == Qt::Checked);

        QProcess proc;
        proc.start("nmcli", {"-t", "-f", "GENERAL.CONNECTION", "device", "show", m_ifaceName});
        proc.waitForFinished();
        QString conName = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (conName.startsWith("GENERAL.CONNECTION:")) conName = conName.mid(19);

        if (!conName.isEmpty()) {
            // Set LLDP to 1 (RX only, standard for endpoints) or 0 (disabled)
            QString lldpVal = enableLldp ? "1" : "0";
            QProcess::execute("pkexec", {"nmcli", "con", "modify", conName, "connection.lldp", lldpVal});
            QProcess::execute("pkexec", {"nmcli", "con", "up", conName});
        }
        accept();
    }
};

class ConnectionStatusDialog : public QDialog {
public:
    ConnectionStatusDialog(const QString &ifaceName, QWidget *parent = nullptr) 
        : QDialog(parent), m_iface(ifaceName), m_secondsConnected(0) {
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
        iconLabel->setPixmap(style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(32, 32));
        
        headerLayout->addStretch();
        headerLayout->addWidget(new QLabel("Sent", actGroup));
        headerLayout->addSpacing(15);
        headerLayout->addWidget(iconLabel);
        headerLayout->addSpacing(15);
        headerLayout->addWidget(new QLabel("Received", actGroup));
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

        auto *suppGroup = new QGroupBox("Internet Protocol (TCP/IP)", supportTab);
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
            connect(wifiBtn, &QPushButton::clicked, this, []() {
                QProcess::startDetached("nm-connection-editor", {});
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
        m_timer->start(1000);

        loadSupportData();
        updateMetrics();
    }

private:
    QString m_iface;
    bool m_isWifi = false;
    int m_secondsConnected;
    QTimer *m_timer;

    QLabel *m_statusLabel;
    QLabel *m_durationLabel;
    QLabel *m_speedLabel;
    QLabel *m_networkLabel = nullptr;
    QProgressBar *m_signalBar = nullptr;
    QLabel *m_sentLabel;
    QLabel *m_recvLabel;

    QLabel *m_addrTypeLabel;
    QLabel *m_ipLabel;
    QLabel *m_maskLabel;
    QLabel *m_gatewayLabel;

    void checkIfaceType() {
        QProcess proc;
        proc.start("nmcli", {"-t", "-f", "TYPE", "dev", "show", m_iface});
        proc.waitForFinished();
        QString type = QString::fromUtf8(proc.readAllStandardOutput()).toLower();
        m_isWifi = type.contains("wifi");
    }

    quint64 readSysStat(const QString &stat) {
        QFile file(QString("/sys/class/net/%1/statistics/%2").arg(m_iface, stat));
        if (file.open(QIODevice::ReadOnly)) {
            return file.readAll().trimmed().toULongLong();
        }
        return 0;
    }

    void updateMetrics() {
        // Fetch Live Connection State from NetworkManager
        QProcess proc;
        proc.start("nmcli", {"-t", "-f", "GENERAL.STATE", "dev", "show", m_iface});
        proc.waitForFinished();
        QString stateStr = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        
        bool isConnected = false;
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

        // Increment Duration Counter if Connected
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
            m_durationLabel->setText("00:00:00");
        }

        // Activity Packet Counters
        quint64 txPackets = readSysStat("tx_packets");
        quint64 rxPackets = readSysStat("rx_packets");
        m_sentLabel->setText(QString::number(txPackets));
        m_recvLabel->setText(QString::number(rxPackets));

        // Speed Reading
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

    void loadSupportData() {
        // 1. IP & Subnet via Qt native QNetworkInterface
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

        // 2. Gateway & Active Connection Name via nmcli dev show
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

        // 3. Method (DHCP vs Manual) via nmcli con show
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

    void showDetailsDialog() {
        QNetworkInterface iface = QNetworkInterface::interfaceFromName(m_iface);
        QString details = QString("Physical Address (MAC):\t%1\n"
                                  "IP Address:\t\t%2\n"
                                  "Subnet Mask:\t\t%3\n"
                                  "Default Gateway:\t%4")
                                  .arg(iface.hardwareAddress(), m_ipLabel->text(), m_maskLabel->text(), m_gatewayLabel->text());

        QMessageBox::information(this, "Network Connection Details", details);
    }
};

// Main Network Connections Window
class NetworkConnectionsPage : public QWidget {
    Q_OBJECT
public:
    NetworkConnectionsPage(QScrollArea *sidebar, QWidget *parent = nullptr) : QWidget(parent) {
        // setWindowTitle("Network Connections");
        // resize(520, 360);

        // auto *layout = new QVBoxLayout(this);
        // layout->setContentsMargins(8, 8, 8, 8);
        // Use standard Control Panel page scaffold
        auto *contentV = Win7::pageScaffold(this, sidebar, /*bottomMargin=*/20, /*fixedWidth=*/700);

        // Page title
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

        // layout->addWidget(m_listWidget);
        contentV->addWidget(m_listWidget);
        contentV->addStretch(1);
        refreshInterfaces();
    }

        static QList<SidebarLink> sidebarLinks() {
        return {
            Nav::plain("Disable this network device"),
            Nav::plain("Diagnose this connection"),
            Nav::plain("Rename this connection"),
            Nav::plain("View status of this connection"),
            Nav::plain("Change settings of this connection"),
        };
    }

    static QList<SidebarLink> sidebarSeeAlso() {
        return {
            Nav::to("Network and Sharing Center", PageId::NetworkSharing),
            Nav::plain("Internet Options"),
            Nav::to("Linux Firewall", PageId::Firewall),
        };
    }
private:
    QListWidget *m_listWidget;

    void refreshInterfaces() {
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
            
            QString iconName = "nm-device-wired";

            // Device Name Overrides
            if (dev.startsWith("br-")) {
                iconName = "bluetooth";
            } else if (dev.startsWith("veth") || dev.startsWith("vnet")) {
                // Virtual ethernet interfaces map to wired instead of wifi
                if (state == "connected") iconName = "nm-device-wired";
                // else if (state == "unavailable") iconName = "network-unavailable";
                // else iconName = "gnome-netstatus-disconn";
                
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
            
            // Handle limited/error states
            if (state == "connected (local only)" || state == "connected (site only)") {
                iconName = "network-wired-activated-limited";
            } else if (state == "failed") {
                iconName = "network-error";
            }

            QString displayText = QString("%1\n%2").arg(dev, state);
            auto *item = new QListWidgetItem(displayText, m_listWidget);
            
            // Load standard icon directly without symbolic override
            QIcon devIcon = QIcon::fromTheme(iconName);
            if (devIcon.isNull()) {
                devIcon = style()->standardIcon(QStyle::SP_ComputerIcon);
            }
            
            item->setIcon(devIcon);
            item->setData(Qt::UserRole, dev);
        }
    }
    void showContextMenu(const QPoint &pos) {
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

    void toggleInterface(const QString &ifaceName, bool enable) {
        QString state = enable ? "up" : "down";
        QString cmd = QString("pkexec ip link set %1 %2").arg(ifaceName, state);
        QProcess::execute("/bin/sh", QStringList() << "-c" << cmd);
        refreshInterfaces();
    }

    void showStatus(const QString &ifaceName) {
        ConnectionStatusDialog dlg(ifaceName, this);
        dlg.exec();
    }

    void showProperties(QListWidgetItem *item) {
        QString ifaceName = item->data(Qt::UserRole).toString();
        QNetworkInterface interface = QNetworkInterface::interfaceFromName(ifaceName);

        AdapterPropertiesDialog dlg(ifaceName, interface.humanReadableName(), this);
        dlg.exec();
    }
};