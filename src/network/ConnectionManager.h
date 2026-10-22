#pragma once

#include <QDateTime>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

#include "core/DataTypes.h"

class QWebSocket;

class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionManager(QObject *parent = nullptr);

    void connectToDevice(const QString &deviceId, const QHostAddress &address, quint16 port);
    // Pairing connection: the device is not known yet (SoftAP), so there is no deviceId.
    // Drops any current connection first. No watchdog, data frames are ignored.
    void connectForPairing(const QHostAddress &address, quint16 port);
    void disconnectFromDevice();      // closes whatever is open: normal or pairing connection
    bool sendFrame(const QJsonObject &frame);

    QString currentDeviceId() const;   // empty unless Connected to a normal device (always empty while pairing)
    DeviceState state() const;

signals:
    void connectionStatusChanged(const QString &deviceId, DeviceState state);
    void connectionLost(const QString &deviceId);     // only when the link dropped without user's request
    void connectionFailed(const QString &deviceId);   // timeout or immediate refusal
    void activeDeviceChanged(const QString &deviceId); // empty when no device is active
    void dataReceived(const QJsonObject &payload, const QString &deviceId, const QDateTime &timestamp);

    void pairingConnectionChanged(DeviceState state);
    void pairingConnectionFailed();                   // could not open the socket (timeout / refusal)
    void pairingSocketClosed();                       // socket closed after it had been open
    void pairingStatusChanged(int status, const QString &deviceId);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketMessageReceived(const QString &message);
    void onConnectTimeout();
    void onAliveTimeout();

private:
    void startConnecting(const QString &deviceId, const QHostAddress &address, quint16 port);
    void createSocket();
    void destroySocket();
    void dropConnection(bool lost);
    void setState(DeviceState state);

    static constexpr int kConnectTimeoutMs = 5000;
    static constexpr int kLivenessTimeoutMs = 6000; // 3x the 2 s data interval; to raise, if firmware turns out slower

    QWebSocket *m_socket = nullptr;
    QTimer *m_connectTimer = nullptr;
    QTimer *m_aliveTimer = nullptr;
    DeviceState m_state = DeviceState::Disconnected;
    bool m_pairingMode = false;  // true from connectForPairing() until the connection is dropped
    QString m_targetDeviceId;  // device we are connecting to / were connected to
    QString m_activeDeviceId;  // set only while Connected (normal mode)
};