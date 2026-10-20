#include "network/ConnectionManager.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUrl>
#include <QWebSocket>

ConnectionManager::ConnectionManager(QObject *parent)
    : QObject(parent)
    , m_connectTimer(new QTimer(this))
    , m_livenessTimer(new QTimer(this))
{
    m_connectTimer->setSingleShot(true);
    m_connectTimer->setInterval(kConnectTimeoutMs);
    m_livenessTimer->setSingleShot(true);
    m_livenessTimer->setInterval(kLivenessTimeoutMs);

    connect(m_connectTimer, &QTimer::timeout, this, &ConnectionManager::onConnectTimeout);
    connect(m_livenessTimer, &QTimer::timeout, this, &ConnectionManager::onLivenessTimeout);
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
    m_livenessTimer->stop();
    destroySocket();

    const bool wasConnected = (m_state == DeviceState::Connected);
    const QString previousId = m_activeDeviceId;
    m_activeDeviceId.clear();

    setState(DeviceState::Disconnected);
    if (wasConnected) {
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
    emit connectionStatusChanged(m_targetDeviceId, m_state);
}

void ConnectionManager::onSocketConnected()
{
    if (m_state != DeviceState::Connecting) {
        return;
    }
    m_connectTimer->stop();
    m_activeDeviceId = m_targetDeviceId;
    setState(DeviceState::Connected);
    emit activeDeviceChanged(m_activeDeviceId);
}

void ConnectionManager::onSocketDisconnected()
{
    if (m_state == DeviceState::Connected) {
        dropConnection(true);   // nobody asked for this: the link was lost
    } else if (m_state == DeviceState::Connecting) {
        const QString id = m_targetDeviceId;
        dropConnection(false);
        emit connectionFailed(id);
    }
}

void ConnectionManager::onConnectTimeout()
{
    if (m_state != DeviceState::Connecting) {
        return;
    }
    const QString id = m_targetDeviceId;
    dropConnection(false);
    emit connectionFailed(id);
}

void ConnectionManager::onLivenessTimeout()
{
    if (m_state != DeviceState::Connected) {
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

    if (type == QStringLiteral("data")) {
        m_livenessTimer->start(); // first data frame arms the watchdog, every next one restarts it
        emit dataReceived(payload, m_activeDeviceId, QDateTime::currentDateTime());
    } else if (type == QStringLiteral("pairing_status")) {
        emit pairingStatusChanged(payload.value(QStringLiteral("status")).toInt(),
                                  payload.value(QStringLiteral("device_id")).toString());
    }
}