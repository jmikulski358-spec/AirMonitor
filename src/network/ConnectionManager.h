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
    void disconnectFromDevice();
    bool sendFrame(const QJsonObject &frame);

    QString currentDeviceId() const;   // empty unless Connected
    DeviceState state() const;

signals:
    void connectionStatusChanged(const QString &deviceId, DeviceState state);
    void connectionLost(const QString &deviceId);     // only when the link dropped without user's request
    void connectionFailed(const QString &deviceId);   // timeout or immediate refusal
    void activeDeviceChanged(const QString &deviceId); // empty when no device is active
    void dataReceived(const QJsonObject &payload, const QString &deviceId, const QDateTime &timestamp);
    void pairingStatusChanged(int status, const QString &deviceId);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketMessageReceived(const QString &message);
    void onConnectTimeout();
    void onLivenessTimeout();

private:
    void startConnecting(const QString &deviceId, const QHostAddress &address, quint16 port);
    void createSocket();
    void destroySocket();
    void dropConnection(bool lost);
    void setState(DeviceState state);

    static constexpr int kConnectTimeoutMs = 5000;
    static constexpr int kLivenessTimeoutMs = 6000; // 3x the 2 s data interval; raise if firmware is slower

    QWebSocket *m_socket = nullptr;
    QTimer *m_connectTimer = nullptr;
    QTimer *m_livenessTimer = nullptr;
    DeviceState m_state = DeviceState::Disconnected;
    QString m_targetDeviceId;  // device we are connecting to / were connected to
    QString m_activeDeviceId;  // set only while Connected
};