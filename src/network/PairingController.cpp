#include "network/PairingController.h"

#include <QDebug>
#include <QHostAddress>

#include "database/DatabaseManager.h"
#include "network/ConnectionManager.h"

namespace {
    // TEST values: tools/mock_pairing.py listens here.
    // The real ESP32 SoftAP is 192.168.4.1; the port is agreed with the firmware.
    constexpr quint16 kSoftApPort = 8765;

    QHostAddress softApAddress()
    {
        return QHostAddress(QStringLiteral("127.0.0.1"));
    }
}

PairingController::PairingController(ConnectionManager *connectionManager,
                                     DatabaseManager *databaseManager,
                                     QObject *parent)
    : QObject(parent)
    , m_connectionManager(connectionManager)
    , m_databaseManager(databaseManager)
    , m_stepTimer(new QTimer(this))
{
    m_stepTimer->setSingleShot(true);
    connect(m_stepTimer, &QTimer::timeout, this, &PairingController::onStepTimeout);
}

PairingState PairingController::state() const
{
    return m_state;
}

bool PairingController::isActive() const
{
    switch (m_state) {
    case PairingState::ConnectingToAp:
    case PairingState::WaitingForCredentials:
    case PairingState::WaitingForResult:
    case PairingState::WrongPassword:
        return true;
    default:
        return false;
    }
}

void PairingController::startPairing()
{
    if (isActive()) {
        return;
    }
    // The phone is about to leave the home network, so the current device link is gone anyway.
    m_connectionManager->disconnectFromDevice();
    setState(PairingState::ConnectingToAp);
    m_stepTimer->start(kAccessPointWindowMs);
}

void PairingController::connectToAccessPoint()
{
    if (m_state != PairingState::ConnectingToAp) {
        qWarning() << "connectToAccessPoint ignored in state" << static_cast<int>(m_state);
        return;
    }
    m_connectionManager->connectForPairing(softApAddress(), kSoftApPort);
}

void PairingController::submitCredentials(const QString &ssid, const QString &password)
{
    if (m_state != PairingState::WaitingForCredentials && m_state != PairingState::WrongPassword) {
        qWarning() << "submitCredentials ignored in state" << static_cast<int>(m_state);
        return;
    }
    if (ssid.isEmpty()) {
        qWarning() << "submitCredentials ignored: empty SSID";
        return;
    }
    if (!m_connectionManager->sendFrame(buildCredentialsFrame(ssid, password))) {
        finish(PairingState::NetworkError);
        return;
    }
    setState(PairingState::WaitingForResult);
    m_stepTimer->start(kResultTimeoutMs);
}

void PairingController::cancel()
{
    if (!isActive()) {
        return;
    }
    finish(PairingState::Cancelled);
}

void PairingController::onPairingConnectionChanged(DeviceState state)
{
    if (state == DeviceState::Connected && m_state == PairingState::ConnectingToAp) {
        setState(PairingState::WaitingForCredentials);
        m_stepTimer->start(kCredentialsWindowMs);
    }
}

void PairingController::onPairingConnectionFailed()
{
    // Not final: the user probably has not joined the SoftAP network yet and can press the button again.
    if (m_state == PairingState::ConnectingToAp) {
        emit accessPointNotReachable();
    }
}

void PairingController::onPairingSocketClosed()
{
    // After status 1 we have already called finish(), which closes the socket on our side,
    // so a close that reaches us while pairing is still active is always unexpected.
    if (isActive()) {
        finish(PairingState::NetworkError);
    }
}

void PairingController::onPairingStatusChanged(int status, const QString &deviceId)
{
    if (m_state != PairingState::WaitingForResult) {
        return;
    }

    switch (status) {
    case 0:   // device is still joining the home network
        break;
    case 1:
        if (deviceId.isEmpty()) {
            finish(PairingState::NetworkError);   // success without an id is useless to us
            return;
        }
        // Save first, then close the socket and announce the result.
        m_databaseManager->addPairedDevice(deviceId, tr("AirMonitor %1").arg(deviceId));
        finish(PairingState::Succeeded);
        emit pairingSucceeded(deviceId);
        break;
    case 2:   // wrong password: the socket stays open, the user can try again
        setState(PairingState::WrongPassword);
        m_stepTimer->start(kCredentialsWindowMs);
        break;
    case 3:
        finish(PairingState::NetworkError);
        break;
    default:
        qWarning() << "Unknown pairing_status:" << status;
        break;
    }
}

void PairingController::onStepTimeout()
{
    if (isActive()) {
        finish(PairingState::TimedOut);
    }
}

void PairingController::setState(PairingState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit pairingStateChanged(m_state);
}

void PairingController::finish(PairingState result)
{
    m_stepTimer->stop();
    m_connectionManager->disconnectFromDevice();   // no-op if the socket is already closed
    setState(result);
}

QJsonObject PairingController::buildCredentialsFrame(const QString &ssid, const QString &password) const
{
    QJsonObject payload;
    payload.insert(QStringLiteral("ssid"), ssid);
    payload.insert(QStringLiteral("password"), password);

    QJsonObject frame;
    frame.insert(QStringLiteral("type"), QStringLiteral("credentials_submit"));
    frame.insert(QStringLiteral("payload"), payload);
    return frame;
}