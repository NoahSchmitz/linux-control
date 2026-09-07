#pragma once

#include <QWidget>
#include <QSet>

class QGraphicsScene;
class QGraphicsProxyWidget;
class QHBoxLayout;
class QLabel;
class QTimer;
class QProcess;

class NetworkMapPage : public QWidget
{
    Q_OBJECT
public:
    explicit NetworkMapPage(QWidget *parent = nullptr);

private slots:
    void pollArpTable();

private:
    // Helpers to populate the map scene
    QGraphicsProxyWidget* createMapNode(QGraphicsScene *scene, const QString &iconName, const QString &name, int x, int y);
    void drawOrthogonalLine(QGraphicsScene *scene, QGraphicsProxyWidget *n1, QGraphicsProxyWidget *n2, bool dashed);
    QWidget* createStaticNode(const QString& iconName, const QString& name);
    void addPeerToMap(const QString &ip, const QString &displayName);

    // State Variables
    QString m_activeIface;
    bool m_isWifi;
    
    // UI Pointers
    QGraphicsScene *m_scene = nullptr;
    QGraphicsProxyWidget *m_gatewayNode = nullptr;
    QHBoxLayout *m_unplacedLayout = nullptr;
    QLabel *m_unplacedLabel = nullptr;
    
    // Tracking
    QSet<QString> m_knownPeers;
    int m_placedPeersCount = 0;
    int m_peerY = 150;

    // Background Tasks
    QTimer *m_arpTimer = nullptr;
    QProcess *m_sweepProcess = nullptr;
};