#pragma once

#include <QWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QNetworkInterface>
#include <QMenu>
#include <QMessageBox>
#include <QDialog>
#include <QLabel>
#include <QFormLayout>
#include <QPushButton>
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
        auto *netLayout = new QVBoxLayout();

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
        netLayout->addWidget(new QLabel("This connection uses the following items:"));

        m_itemsList = new QListWidget(this);

        addListItem("QoS Packet Scheduler");
        addListItem("Internet Protocol Version 4 (TCP/IPv4)");
        addListItem("Internet Protocol Version 6 (TCP/IPv6)");
        
        // Load actual LLDP state from NetworkManager
        bool lldpEnabled = getLldpState();
        addListItem("LLDP Protocol Driver", lldpEnabled);
        
        netLayout->addWidget(m_itemsList);

        auto *listBtns = new QHBoxLayout();
        // auto *installBtn = new QPushButton("Install...", netTab);
        // auto *uninstallBtn = new QPushButton("Uninstall", netTab);
        m_propsBtn = new QPushButton("Properties", this);
        m_propsBtn->setEnabled(false); 

        // listBtns->addWidget(installBtn);
        // listBtns->addWidget(uninstallBtn);
        listBtns->addStretch();
        listBtns->addWidget(m_propsBtn);
        netLayout->addLayout(listBtns);

        auto *descGroup = new QGroupBox("Description", this);
        auto *descLayout = new QVBoxLayout(descGroup);
        m_descLabel = new QLabel("Select an item to see its description.", descGroup);
        m_descLabel->setWordWrap(true);
        m_descLabel->setMinimumHeight(50);
        m_descLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        descLayout->addWidget(m_descLabel);
        netLayout->addWidget(descGroup);

        mainLayout->addLayout(netLayout);

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
// Main Network Connections Window

class NetworkConnectionsWindow : public QWidget {
public:
    NetworkConnectionsWindow(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle("Network Connections");
        resize(520, 360);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);

        m_listWidget = new QListWidget(this);
        m_listWidget->setViewMode(QListWidget::IconMode);
        m_listWidget->setIconSize(QSize(32, 32));
        m_listWidget->setGridSize(QSize(150, 70));
        m_listWidget->setResizeMode(QListWidget::Adjust);
        m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);

        connect(m_listWidget, &QListWidget::customContextMenuRequested, this, &NetworkConnectionsWindow::showContextMenu);
        connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item){ showProperties(item); });

        layout->addWidget(m_listWidget);
        refreshInterfaces();
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
            QString connName = (parts.size() > 3) ? parts[3] : "";
            
            if (type == "loopback") continue;
            
            QString iconName = "network-wired"; // Default
            
            // Determine Icon based on Type and State
            if (type == "ethernet") {
                if (state == "connected") iconName = "network-wired";
                else if (state == "unavailable") iconName = "network-unavailable";
                else iconName = "network-offline";
            } else if (type == "wifi") {
                if (state == "connected") iconName = "network-wireless-signal-excellent";
                else if (state == "disconnected") iconName = "network-wireless-disconnected";
                else iconName = "network-wireless-offline";
            } else if (type == "bt") {
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
            
            // Load icon from system theme, fallback to a standard Qt icon
            item->setIcon(QIcon::fromTheme(iconName, style()->standardIcon(QStyle::SP_ComputerIcon)));
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
        QNetworkInterface interface = QNetworkInterface::interfaceFromName(ifaceName);
        QDialog dlg(this);
        dlg.setWindowTitle(QString("Status: %1").arg(interface.humanReadableName()));
        dlg.resize(300, 220);

        auto *layout = new QFormLayout(&dlg);
        layout->addRow("Interface:", new QLabel(interface.name(), &dlg));
        layout->addRow("MAC Address:", new QLabel(interface.hardwareAddress(), &dlg));
        
        QString ipAddresses;
        for (const auto &entry : interface.addressEntries()) {
            ipAddresses += entry.ip().toString() + "\n";
        }
        layout->addRow("IP Address:", new QLabel(ipAddresses.trimmed(), &dlg));

        dlg.exec();
    }

    void showProperties(QListWidgetItem *item) {
        QString ifaceName = item->data(Qt::UserRole).toString();
        QNetworkInterface interface = QNetworkInterface::interfaceFromName(ifaceName);

        AdapterPropertiesDialog dlg(ifaceName, interface.humanReadableName(), this);
        dlg.exec();
    }
};