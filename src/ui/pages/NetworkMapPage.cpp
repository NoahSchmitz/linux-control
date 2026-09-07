#include "NetworkMapPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include <QIcon>
#include <QPen>
#include <QPainterPath>
#include <QFile>
#include <QDir>
#include <QHostInfo>
#include <QSysInfo>
#include <QRegularExpression>
#include <QTimer>
#include <QProcess>
#include <QNetworkInterface>

namespace {

static QString getActiveInterface()
{
    QFile f(QStringLiteral("/proc/net/route"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    const QString text = QString::fromUtf8(f.readAll());
    const QList<QStringView> lines = QStringView(text).split(u'\n');

    QString best;
    long bestMetric = -1;
    for (int i = 1; i < lines.size(); ++i) {
        const QList<QStringView> cols = lines[i].split(u'\t', Qt::SkipEmptyParts);
        if (cols.size() < 7 || cols[1].trimmed() != u"00000000")
            continue;
        const long metric = cols[6].trimmed().toString().toLong();
        if (bestMetric < 0 || metric < bestMetric) {
            bestMetric = metric;
            best = cols[0].trimmed().toString();
        }
    }
    return best;
}

static bool isInterfaceWireless(const QString &iface)
{
    if (iface.isEmpty()) return false;
    return QDir(QStringLiteral("/sys/class/net/%1/wireless").arg(iface)).exists();
}

} // namespace

NetworkMapPage::NetworkMapPage(QWidget *parent) 
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    m_activeIface = getActiveInterface();
    m_isWifi = isInterfaceWireless(m_activeIface);
    bool isConnected = !m_activeIface.isEmpty();

    QString localName = QHostInfo::localHostName().toUpper();
    if (localName.isEmpty())
        localName = QSysInfo::machineHostName().toUpper();

    // Top Control Bar
    auto *topLayout = new QHBoxLayout;
    topLayout->addWidget(new QLabel("Network map of"));
    
    auto *adapterCombo = new QComboBox;
    if (isConnected) {
        adapterCombo->addItem(QString("%1 - %2")
            .arg(m_isWifi ? "Wireless Network Connection" : "Local Area Connection")
            .arg(m_activeIface));
    } else {
        adapterCombo->addItem("No Network Connection");
    }
    topLayout->addWidget(adapterCombo);
    topLayout->addStretch(1);
    
    mainLayout->addLayout(topLayout);

    // Network Map Canvas
    m_scene = new QGraphicsScene(this);
    auto *view = new QGraphicsView(m_scene);
    view->setFrameStyle(QFrame::WinPanel | QFrame::Sunken);
    view->setBackgroundBrush(Qt::white);
    view->setRenderHint(QPainter::Antialiasing, false); 
    
    // Local Machine Node
    auto *localPc = createMapNode(m_scene, m_isWifi ? "computer-laptop" : "computer", localName, 50, 50);

    // Gateway / Router Node
    if (isConnected) {
        m_gatewayNode = createMapNode(m_scene, m_isWifi ? "network-wireless" : "network-wired", "Gateway", 300, 100);
        drawOrthogonalLine(m_scene, localPc, m_gatewayNode, m_isWifi);
        
        // Internet Node
        auto *internetNode = createMapNode(m_scene, "applications-internet", "Internet", 550, 100);
        drawOrthogonalLine(m_scene, m_gatewayNode, internetNode, false);
    }

    mainLayout->addWidget(view, 1);

    // Unplaced Devices Section
    m_unplacedLabel = new QLabel("The following discovered device(s) can not be placed in the map. <a href=\"#\">Click here to see all other devices.</a>");
    m_unplacedLabel->setTextFormat(Qt::RichText);
    m_unplacedLabel->setOpenExternalLinks(false);
    m_unplacedLabel->hide(); 
    mainLayout->addWidget(m_unplacedLabel);

    m_unplacedLayout = new QHBoxLayout;
    m_unplacedLayout->setContentsMargins(0, 0, 0, 0);
    m_unplacedLayout->setSpacing(20);
    m_unplacedLayout->setAlignment(Qt::AlignLeft);
    mainLayout->addLayout(m_unplacedLayout);

    // Network Discovery Initialization
    if (isConnected) {
        // Step A: Calculate the local subnet to sweep
        QString subnet;
        for (const auto &iface : QNetworkInterface::allInterfaces()) {
            if (iface.name() == m_activeIface) {
                for (const auto &entry : iface.addressEntries()) {
                    if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                        subnet = entry.ip().toString() + "/" + QString::number(entry.prefixLength());
                        break;
                    }
                }
            }
        }

        // Step B: Spawn a background nmap process to passively light up the kernel's ARP table
        if (!subnet.isEmpty()) {
            m_sweepProcess = new QProcess(this);
            m_sweepProcess->start("nmap", {"-sn", subnet});
        }

        // Step C: Start polling the ARP table
        m_arpTimer = new QTimer(this);
        connect(m_arpTimer, &QTimer::timeout, this, &NetworkMapPage::pollArpTable);
        m_arpTimer->start(2000); // Check every 2 seconds
        
        // Run once immediately
        pollArpTable(); 
    }
}

void NetworkMapPage::pollArpTable()
{
    QFile arpFile("/proc/net/arp");
    if (!arpFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    arpFile.readLine(); // Skip header
    while (!arpFile.atEnd()) {
        QString line = QString::fromUtf8(arpFile.readLine());
        auto cols = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        
        if (cols.size() >= 6 && cols[5] == m_activeIface && cols[3] != "00:00:00:00:00:00") {
            QString ip = cols[0];
            if (!m_knownPeers.contains(ip)) {
                m_knownPeers.insert(ip);
                
                // Asynchronous reverse DNS lookup
                QHostInfo::lookupHost(ip, this, [this, ip](const QHostInfo &info) {
                    QString displayName = ip;
                    // If a hostname is found, strip the local domain for a cleaner label
                    if (info.error() == QHostInfo::NoError && !info.hostName().isEmpty()) {
                        QString hostName = info.hostName().section('.', 0, 0);
                        displayName = QString("%1\n(%2)").arg(hostName, ip);
                    }
                    addPeerToMap(ip, displayName);
                });
            }
        }
    }
}

void NetworkMapPage::addPeerToMap(const QString &ip, const QString &displayName)
{
    // If the gateway is known, we establish parentage and plot it on the map.
    // The canvas will naturally expand downward as m_peerY increases.
    if (m_gatewayNode) {
        auto *peerNode = createMapNode(m_scene, "computer", displayName, 50, m_peerY);
        drawOrthogonalLine(m_scene, peerNode, m_gatewayNode, m_isWifi);
        m_peerY += 100;
        m_placedPeersCount++;
    } else {
        // Unknown parentage (no gateway found for this interface)
        m_unplacedLabel->show();
        m_unplacedLayout->addWidget(createStaticNode("computer", displayName));
    }
}

QWidget* NetworkMapPage::createStaticNode(const QString& iconName, const QString& name) 
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    
    auto *icon = new QLabel;
    icon->setPixmap(QIcon::fromTheme(iconName, QIcon::fromTheme("preferences-system")).pixmap(32, 32));
    icon->setAlignment(Qt::AlignHCenter);
    l->addWidget(icon);
    
    auto *label = new QLabel(name);
    label->setAlignment(Qt::AlignHCenter);
    l->addWidget(label);
    return w;
}

QGraphicsProxyWidget* NetworkMapPage::createMapNode(QGraphicsScene *scene, const QString &iconName, const QString &name, int x, int y)
{
    auto *container = new QWidget;
    container->setStyleSheet("background: transparent;"); 
    
    // Increased from 100 to 140 to comfortably fit IPv4 addresses and hostnames
    container->setFixedWidth(140); 

    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto *iconLabel = new QLabel;
    iconLabel->setPixmap(QIcon::fromTheme(iconName, QIcon::fromTheme("preferences-system")).pixmap(32, 32));
    iconLabel->setAlignment(Qt::AlignHCenter);
    layout->addWidget(iconLabel);

    auto *nameLabel = new QLabel(name);
    nameLabel->setAlignment(Qt::AlignHCenter);
    
    // Enable word wrap to prevent long hostnames from being cropped
    nameLabel->setWordWrap(true);
    
    layout->addWidget(nameLabel);

    auto *proxy = scene->addWidget(container);
    proxy->setPos(x, y);
    
    container->adjustSize();
    return proxy;
}

void NetworkMapPage::drawOrthogonalLine(QGraphicsScene *scene, QGraphicsProxyWidget *n1, QGraphicsProxyWidget *n2, bool dashed)
{
    QRectF r1 = n1->sceneBoundingRect();
    QRectF r2 = n2->sceneBoundingRect();

    QPointF p1(r1.right(), r1.center().y());
    QPointF p2;

    if (r2.left() > r1.right()) {
        p2 = QPointF(r2.left(), r2.center().y());
    } else {
        p1 = QPointF(r1.center().x(), r1.top());
        p2 = QPointF(r2.center().x(), r2.bottom());
        
        if (r1.bottom() < r2.top()) {
            p1 = QPointF(r1.center().x(), r1.bottom());
            p2 = QPointF(r2.center().x(), r2.top());
        }
    }

    QPen pen(QColor(34, 177, 76)); // Standard map green
    pen.setWidth(1);
    if (dashed) {
        pen.setStyle(Qt::DashLine);
    }

    QPainterPath path;
    path.moveTo(p1);

    if (r2.left() > r1.right()) {
        qreal midX = (p1.x() + p2.x()) / 2.0;
        path.lineTo(midX, p1.y());
        path.lineTo(midX, p2.y());
        path.lineTo(p2);
    } else {
        qreal midY = (p1.y() + p2.y()) / 2.0;
        path.lineTo(p1.x(), midY);
        path.lineTo(p2.x(), midY);
        path.lineTo(p2);
    }

    auto *pathItem = scene->addPath(path, pen);
    pathItem->setZValue(-1); 
}