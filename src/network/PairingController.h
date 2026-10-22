#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

#include "core/DataTypes.h"

class ConnectionManager;
class DatabaseManager;

class PairingController : public QObject
{
    Q_OBJECT

public:
    explicit PairingController(ConnectionManager *connectionManager,
                               DatabaseManager *databaseManager,
                               QObject *parent = nullptr);

    PairingState state() const;
    bool isActive() const;   // true from startPairing() until a final state

    // Called by the UI
    void startPairing();
    void connectToAccessPoint();
    void submitCredentials(const QString &ssid, const QString &password);
    void cancel();

public slots:
    // Connected to ConnectionManager signals in main()/AppController
    void onPairingConnectionChanged(DeviceState state);
    void onPairingConnectionFailed();
    void onPairingSocketClosed();
    void onPairingStatusChanged(int status, const QString &deviceId);

signals:
    void pairingStateChanged(PairingState state);
    void pairingSucceeded(const QString &deviceId);
    void accessPointNotReachable();   // "I'm connected" button pressed, but the socket did not open; user can retry

private slots:
    void onStepTimeout();

private:
    void setState(PairingState state);
    void finish(PairingState result);   // closes the pairing socket, stops the timer, publishes the final state
    QJsonObject buildCredentialsFrame(const QString &ssid, const QString &password) const;

    static constexpr int kAccessPointWindowMs = 180000;  // time to join the SoftAP network
    static constexpr int kCredentialsWindowMs = 180000;  // time to type SSID and password
    static constexpr int kResultTimeoutMs = 60000;       // time for the device to answer

    ConnectionManager *m_connectionManager = nullptr;
    DatabaseManager *m_databaseManager = nullptr;
    QTimer *m_stepTimer = nullptr;   // one timer, restarted with a different interval at each step
    PairingState m_state = PairingState::Idle;
};