#include "network/ConnectionManager.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUrl>
#include <QWebSocket>

ConnectionManager::ConnectionManager(QObject *parent)
    : QObject(parent)
    , m_connectTimer(new QTimer(this))
    , m_aliveTimer(new QTimer(this))
{
    m_connectTimer->setSingleShot(true);
    m_connectTimer->setInterval(kConnectTimeoutMs);
    m_aliveTimer->setSingleShot(true);
    m_aliveTimer->setInterval(kLivenessTimeoutMs);

    connect(m_connectTimer, &QTimer::timeout, this, &ConnectionManager::onConnectTimeout);
    connect(m_aliveTimer, &QTimer::timeout, this, &ConnectionManager::onAliveTimeout);
}

void ConnectionManager::connectToDevice(const QString &deviceId, const QHostAddress &address, quint16 port)
{
    if (m_state == DeviceState::Connecting) {
        qWarning() << "connectToDevice ignored: a connection attempt is in progress";
        return;
    }
    if (m_state == DeviceState::Connected) {
        if (deviceId == m_activeDeviceId) {
            return; // already connected to this device
        }
        dropConnection(false); // synchronous: the old link is gone before the new one starts
    }
    startConnecting(deviceId, address, port);
}

void ConnectionManager::connectForPairing(const QHostAddress &address, quint16 port)
{
    if (m_pairingMode && m_state == DeviceState::Connecting) {
        qWarning() << "connectForPairing ignored: a pairing connection attempt is in progress";
        return;
    }
    if (m_state != DeviceState::Disconnected) {
        dropConnection(false);   // also clears m_pairingMode
    }
    m_pairingMode = true;        // must be set before startConnecting. setState() uses it to pick the signal
    startConnecting(QString(), address, port);
}

void ConnectionManager::disconnectFromDevice()
{
    if (m_state == DeviceState::Disconnected) {
        return;
    }
    dropConnection(false);
}

bool ConnectionManager::sendFrame(const QJsonObject &frame)
{
    if (m_state != DeviceState::Connected || !m_socket) {
        qWarning() << "sendFrame: not connected, frame dropped";
        return false;
    }
    const QJsonDocument doc(frame);
    return m_socket->sendTextMessage(QString::fromUtf8(doc.toJson(QJsonDocument::Compact))) > 0;
}

QString ConnectionManager::currentDeviceId() const
{
    return m_activeDeviceId;
}

DeviceState ConnectionManager::state() const
{
    return m_state;
}

void ConnectionManager::startConnecting(const QString &deviceId, const QHostAddress &address, quint16 port)
{
    createSocket();
    m_targetDeviceId = deviceId;
    setState(DeviceState::Connecting);

    QUrl url;
    url.setScheme(QStringLiteral("ws"));
    url.setHost(address.toString());
    url.setPort(port);

    m_socket->open(url);
    m_connectTimer->start();
}

void ConnectionManager::createSocket()
{
    destroySocket();

    QWebSocket *socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_socket = socket;

    connect(socket, &QWebSocket::connected, this, &ConnectionManager::onSocketConnected);
    connect(socket, &QWebSocket::disconnected, this, &ConnectionManager::onSocketDisconnected);
    connect(socket, &QWebSocket::textMessageReceived, this, &ConnectionManager::onSocketMessageReceived);
    connect(socket, &QWebSocket::errorOccurred, this, [socket](QAbstractSocket::SocketError) {
        qWarning() << "Socket error:" << socket->errorString();
    });
}

void ConnectionManager::destroySocket()
{
    if (!m_socket) {
        return;
    }
    m_socket->disconnect(this);   // from now on the old socket cannot reach us
    m_socket->abort();
    m_socket->deleteLater();
    m_socket = nullptr;
}

void ConnectionManager::dropConnection(bool lost)
{
    m_connectTimer->stop();
    m_aliveTimer->stop();
    destroySocket();

    const bool wasConnected = (m_state == DeviceState::Connected);
    const bool wasPairing = m_pairingMode;
    const QString previousId = m_activeDeviceId;
    m_activeDeviceId.clear();

    setState(DeviceState::Disconnected);   // still routed by m_pairingMode
    m_pairingMode = false;

    // The rest of the app only hears about normal connections.
    if (wasConnected && !wasPairing) {
        emit activeDeviceChanged(QString());
        if (lost) {
            emit connectionLost(previousId);
        }
    }
}

void ConnectionManager::setState(DeviceState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    if (m_pairingMode) {
        emit pairingConnectionChanged(m_state);
    } else {
        emit connectionStatusChanged(m_targetDeviceId, m_state);
    }
}

void ConnectionManager::onSocketConnected()
{
    if (m_state != DeviceState::Connecting) {
        return;
    }
    m_connectTimer->stop();

    if (m_pairingMode) {
        // No device id yet, so no m_activeDeviceId and no activeDeviceChanged.
        setState(DeviceState::Connected);
        return;
    }

    m_activeDeviceId = m_targetDeviceId;
    setState(DeviceState::Connected);
    emit activeDeviceChanged(m_activeDeviceId);
}

void ConnectionManager::onSocketDisconnected()
{
    // Remember everything BEFORE dropConnection(): it resets the state and the pairing flag.
    const bool pairing = m_pairingMode;
    const DeviceState previous = m_state;
    const QString id = m_targetDeviceId;

    if (previous == DeviceState::Connected) {
        dropConnection(!pairing);   // normal mode: nobody asked for this, the link was lost
        if (pairing) {
            emit pairingSocketClosed();   // expected after status 1, an error before it: the controller decides
        }
    } else if (previous == DeviceState::Connecting) {
        dropConnection(false);
        if (pairing) {
            emit pairingConnectionFailed();
        } else {
            emit connectionFailed(id);
        }
    }
}

void ConnectionManager::onConnectTimeout()
{
    if (m_state != DeviceState::Connecting) {
        return;
    }
    const bool pairing = m_pairingMode;
    const QString id = m_targetDeviceId;
    dropConnection(false);
    if (pairing) {
        emit pairingConnectionFailed();
    } else {
        emit connectionFailed(id);
    }
}

void ConnectionManager::onAliveTimeout()
{
    if (m_state != DeviceState::Connected || m_pairingMode) {
        return;
    }
    qWarning() << "No data from device for" << kLivenessTimeoutMs << "ms: connection considered lost";
    dropConnection(true);
}

void ConnectionManager::onSocketMessageReceived(const QString &message)
{
    if (m_state != DeviceState::Connected) {
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    const QJsonObject root = doc.object();
    const QString type = root.value(QStringLiteral("type")).toString();
    const QJsonObject payload = root.value(QStringLiteral("payload")).toObject();

    if (m_pairingMode) {
        // Only pairing_status matters here. The liveness timer is never armed in this mode.
        if (type == QStringLiteral("pairing_status")) {
            emit pairingStatusChanged(payload.value(QStringLiteral("status")).toInt(),
                                      payload.value(QStringLiteral("device_id")).toString());
        }
        return;
    }

    if (type == QStringLiteral("data")) {
        m_aliveTimer->start(); // first data frame arms the watchdog, every next one restarts it
        emit dataReceived(payload, m_activeDeviceId, QDateTime::currentDateTime());
    }
}