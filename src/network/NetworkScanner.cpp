#include "network/NetworkScanner.h"
#include "database/DatabaseManager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkDatagram>

NetworkScanner::NetworkScanner(DatabaseManager *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
    , m_socket(new QUdpSocket(this))
    , m_scanTimer(new QTimer(this))
    , m_repeatTimer(new QTimer(this))
{
    if(!m_database){
        qWarning() << "NetworkSacnner: no database attached, unable to scan for devices";
    }

    m_socket->bind(QHostAddress::AnyIPv4, 0);

    m_scanTimer->setSingleShot(true);
    m_scanTimer->setInterval(kScanDurationMs);
    m_repeatTimer->setInterval(kScanIntervalMs);

    connect(m_socket, &QUdpSocket::readyRead, this, &NetworkScanner::onReadyRead);
    connect(m_scanTimer, &QTimer::timeout, this, &NetworkScanner::onScanTimeout);
    connect(m_repeatTimer, &QTimer::timeout, this, &NetworkScanner::scanNetwork);
}

QStringList NetworkScanner::pairedDevices() const
{
    return m_pairedDevices;
}

QList<DiscoveredDevice> NetworkScanner::activeDevices() const
{
    return m_knownDevices;
}

void NetworkScanner::startScanning()
{
    scanNetwork();
    m_repeatTimer->start();
}

void NetworkScanner::stopScanning()
{
    m_repeatTimer->stop();
}

void NetworkScanner::scanNetwork()
{
    if (m_scanTimer->isActive()) {
        return; // a scan is already running
    }
    if(m_database){
        m_pairedDevices = m_database->pairedDevices(); //we like fresh new lists ;)
    }m_seenThisScan.clear();

    QJsonObject request;
    request.insert(QStringLiteral("type"), QStringLiteral("discover"));
    const QByteArray data = QJsonDocument(request).toJson(QJsonDocument::Compact);

    m_socket->writeDatagram(data, QHostAddress::Broadcast, kDiscoveryPort);
    m_scanTimer->start();
}

void NetworkScanner::onReadyRead()
{
    while (m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket->receiveDatagram();

        if (!m_scanTimer->isActive()) {
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(datagram.data(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            continue;
        }

        const QJsonObject root = doc.object();
        if (root.value(QStringLiteral("type")).toString() != QStringLiteral("discover_reply")) {
            continue;
        }

        const QJsonObject payload = root.value(QStringLiteral("payload")).toObject();
        const QString deviceId = payload.value(QStringLiteral("device_id")).toString();
        const int wsPort = payload.value(QStringLiteral("ws_port")).toInt();

        if (deviceId.isEmpty() || wsPort <= 0 || wsPort > 65535) {
            continue;
        }
        if (!m_pairedDevices.contains(deviceId) || isSeenThisScan(deviceId)) {
            continue;
        }

        DiscoveredDevice device;
        device.deviceId = deviceId;
        device.address = datagram.senderAddress();
        device.port = static_cast<quint16>(wsPort);
        m_seenThisScan.append(device);
    }
}

void NetworkScanner::onScanTimeout()
{
    bool changed = false;

    // Seen in this scan? Add new ones, refresh address/port of known ones
    for (const DiscoveredDevice &seen : m_seenThisScan) {
        m_missedScans[seen.deviceId] = 0;

        bool found = false;
        for (DiscoveredDevice &known : m_knownDevices) {
            if (known.deviceId == seen.deviceId) {
                found = true;
                if (known.address != seen.address || known.port != seen.port) {
                    known = seen;
                    changed = true;
                }
                break;
            }
        }
        if (!found) {
            m_knownDevices.append(seen);
            changed = true;
        }
    }

    //  Known but not seen? Remove only after kMaxMissedScans misses in a row
    for (int i = m_knownDevices.size() - 1; i >= 0; --i) {
        const QString id = m_knownDevices.at(i).deviceId;
        if (isSeenThisScan(id)) {
            continue;
        }
        if (++m_missedScans[id] >= kMaxMissedScans) {
            m_knownDevices.removeAt(i);
            m_missedScans.remove(id);
            changed = true;
        }
    }

    emit scanFinished();
    if (changed) {
        emit devicesChanged(m_knownDevices);
    }
}

bool NetworkScanner::isSeenThisScan(const QString &deviceId) const
{
    for (const DiscoveredDevice &device : m_seenThisScan) {
        if (device.deviceId == deviceId) {
            return true;
        }
    }
    return false;
}