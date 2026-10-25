#include "app/AppController.h"

#include <QDebug>
#include <QVariantMap>
#include <algorithm>
#include <limits>

#include "database/DatabaseManager.h"
#include "monitoring/FanController.h"
#include "monitoring/FilterMonitor.h"
#include "monitoring/ParameterMonitor.h"
#include "network/ConnectionManager.h"
#include "network/NetworkScanner.h"
#include "network/PairingController.h"

namespace {
const double kNoData = std::numeric_limits<double>::quiet_NaN();
}

AppController::AppController(ConnectionManager *connectionManager,
                             DatabaseManager *databaseManager,
                             NetworkScanner *scanner,
                             PairingController *pairingController,
                             ParameterMonitor *parameterMonitor,
                             FanController *fanController,
                             FilterMonitor *filterMonitor,
                             QObject *parent)
    : QObject(parent)
    , m_connectionManager(connectionManager)
    , m_databaseManager(databaseManager)
    , m_scanner(scanner)
    , m_pairing(pairingController)
    , m_parameterMonitor(parameterMonitor)
    , m_fanController(fanController)
    , m_filterMonitor(filterMonitor)
    , m_fanCommandTimer(new QTimer(this))
{
    m_fanCommandTimer->setSingleShot(true);
    m_fanCommandTimer->setInterval(kFanCommandTimeoutMs);
    connect(m_fanCommandTimer, &QTimer::timeout, this, &AppController::onFanCommandTimeout);

    // The state may already exist if something connected before us.
    m_reading = m_parameterMonitor->currentReading();
    m_fanMode = m_fanController->currentFanMode();
    m_fanSpeed = m_fanController->currentFanSpeed();
    m_filterLife = m_filterMonitor->currentFilterLifePercentage();

    connect(m_connectionManager, &ConnectionManager::connectionStatusChanged,
            this, &AppController::onConnectionStatusChanged);
    connect(m_connectionManager, &ConnectionManager::connectionFailed,
            this, &AppController::onConnectionFailed);
    connect(m_connectionManager, &ConnectionManager::connectionLost,
            this, &AppController::onConnectionLost);

    connect(m_scanner, &NetworkScanner::devicesChanged, this, &AppController::onDevicesChanged);
    connect(m_scanner, &NetworkScanner::scanFinished, this, &AppController::onScanFinished);

    connect(m_pairing, &PairingController::pairingStateChanged, this, &AppController::onPairingStateChanged);
    connect(m_pairing, &PairingController::pairingSucceeded, this, &AppController::onPairingSucceeded);
    connect(m_pairing, &PairingController::accessPointNotReachable, this, &AppController::onAccessPointNotReachable);

    connect(m_parameterMonitor, &ParameterMonitor::dataUpdated, this, &AppController::onReadingUpdated);
    connect(m_parameterMonitor, &ParameterMonitor::readingCleared, this, &AppController::onReadingCleared);

    connect(m_fanController, &FanController::fanModeChanged, this, &AppController::onFanModeChanged);
    connect(m_fanController, &FanController::fanSpeedChanged, this, &AppController::onFanSpeedChanged);
    connect(m_fanController, &FanController::fanStateCleared, this, &AppController::onFanStateCleared);

    connect(m_filterMonitor, &FilterMonitor::filterStateCleared, this, &AppController::onFilterStateCleared);
    connect(m_filterMonitor, &FilterMonitor::filterPercentageChanged, this, &AppController::onFilterPercentageChanged);
    connect(m_filterMonitor, &FilterMonitor::filterReplaced, this, &AppController::onFilterReplaced);
}

void AppController::start()
{
    if (!m_databaseManager->isReady()) {
        // Nothing works without the database (no paired devices), so this is a fatal error screen.
        m_startupError = tr("Cannot open the database. Check free disk space and permissions, then restart the app.");
        setScreen(Screen::Error);
        return;
    }
    if (m_databaseManager->pairedDevices().isEmpty()) {
        startPairing();            // first run: nothing to connect to yet
    } else {
        setScreen(Screen::DeviceList);
    }
}

//getters

AppController::Screen AppController::screen() const { return m_screen; }
QString AppController::startupError() const { return m_startupError; }
QVariantList AppController::devices() const { return m_devices; }
bool AppController::scanning() const { return m_scanning; }
bool AppController::connecting() const { return m_connecting; }
int AppController::pairingState() const { return static_cast<int>(m_pairingState); }

double AppController::pm25() const { return m_reading ? m_reading->particulateMatter : kNoData; }
double AppController::voc() const { return m_reading ? m_reading->voc : kNoData; }
double AppController::co2() const { return m_reading ? m_reading->carbonDioxide : kNoData; }
double AppController::temperature() const { return m_reading ? m_reading->temperature : kNoData; }
double AppController::humidity() const { return m_reading ? m_reading->humidity : kNoData; }
int AppController::filterLifePercentage() const { return m_filterLife.value_or(-1); }
int AppController::fanMode() const { return m_fanMode ? static_cast<int>(*m_fanMode) : 0; }
int AppController::fanSpeed() const { return m_fanSpeed ? *m_fanSpeed : -1; }
bool AppController::fanCommandPending() const { return m_pendingMode.has_value(); }

//Commands from the UI

void AppController::refreshDevices()
{
    if (m_screen != Screen::DeviceList) {
        qWarning() << "refreshDevices ignored on screen" << static_cast<int>(m_screen);
        return;
    }
    if (!m_scanning) {
        m_scanning = true;
        emit scanningChanged();
    }
    m_scanner->scanNetwork();   // if a background scan is running, its scanFinished ends the spinner
}

void AppController::connectToDevice(const QString &deviceId)
{
    if (m_screen != Screen::DeviceList || m_connecting) {
        qWarning() << "connectToDevice ignored (screen" << static_cast<int>(m_screen)
                   << ", connecting" << m_connecting << ")";
        return;
    }

    // Address and port come from the scanner: the device may have disappeared since the last scan.
    const QList<DiscoveredDevice> available = m_scanner->activeDevices();
    for (const DiscoveredDevice &device : available) {
        if (device.deviceId == deviceId) {
            m_connectionManager->connectToDevice(device.deviceId, device.address, device.port);
            return;
        }
    }
    emit errorMessage(tr("The device is no longer available. Refresh the list."));
}

void AppController::goBack()
{
    switch (m_screen) {
    case Screen::Control:
        clearPendingCommand();
        m_connectionManager->disconnectFromDevice();   // a requested close: no "connection lost"
        setScreen(Screen::DeviceList);
        break;
    case Screen::Pairing:
        cancelPairing();
        break;
    default:
        break;   // on the list there is nothing "behind" us: QML decides what the system Back does
    }
}

void AppController::startPairing()
{
    if ((m_screen != Screen::Startup && m_screen != Screen::DeviceList && m_screen != Screen::Pairing)
        || m_pairing->isActive()) {
        qWarning() << "startPairing ignored (screen" << static_cast<int>(m_screen) << ")";
        return;
    }
    m_pairing->startPairing();
    setScreen(Screen::Pairing);   // also stops the scanner
}

void AppController::confirmApConnected()
{
    if (m_screen == Screen::Pairing) {
        m_pairing->connectToAccessPoint();
    }
}

void AppController::submitCredentials(const QString &ssid, const QString &password)
{
    if (m_screen == Screen::Pairing) {
        m_pairing->submitCredentials(ssid, password);
    }
}

void AppController::cancelPairing()
{
    if (m_screen != Screen::Pairing) {
        return;
    }
    if (m_pairing->isActive()) {
        m_pairing->cancel();
    }
    setScreen(Screen::DeviceList);   // with an empty database this is an empty list with a "+" button
}

void AppController::setFanMode(int mode)
{
    switch (mode) {
    case static_cast<int>(FanMode::Auto):
    case static_cast<int>(FanMode::Turbo):
    case static_cast<int>(FanMode::Sleep):
        sendFanCommand(static_cast<FanMode>(mode), std::nullopt);
        break;
    case static_cast<int>(FanMode::Manual): {
        // Take over the speed the device is running at now, within the allowed range
        const int speed = std::clamp(m_fanSpeed.value_or(kManualMinSpeed), kManualMinSpeed, kManualMaxSpeed);
        sendFanCommand(FanMode::Manual, speed);
        break;
    }
    default:
        qWarning() << "setFanMode: unknown mode" << mode;
        break;
    }
}

void AppController::setManualSpeed(int speed)
{
    sendFanCommand(FanMode::Manual, std::clamp(speed, kManualMinSpeed, kManualMaxSpeed));
}

//Connection

void AppController::onConnectionStatusChanged(const QString &deviceId, DeviceState state)
{
    Q_UNUSED(deviceId)

    const bool connecting = (state == DeviceState::Connecting);
    if (m_connecting != connecting) {
        m_connecting = connecting;
        emit connectingChanged();
    }
    if (state == DeviceState::Connected && m_screen == Screen::DeviceList) {
        setScreen(Screen::Control);
    }
}

void AppController::onConnectionFailed(const QString &deviceId)
{
    Q_UNUSED(deviceId)
    emit errorMessage(tr("Connection error: couldn't connect to the device."));   // we stay on the list
}

void AppController::onConnectionLost(const QString &deviceId)
{
    Q_UNUSED(deviceId)
    clearPendingCommand();
    emit errorMessage(tr("Connection lost."));
    if (m_screen == Screen::Control) {
        setScreen(Screen::DeviceList);
    }
}

//Scanner

void AppController::onDevicesChanged(const QList<DiscoveredDevice> &devices)
{
    m_devices.clear();
    for (const DiscoveredDevice &device : devices) {
        QVariantMap item;
        item.insert(QStringLiteral("deviceId"), device.deviceId);
        item.insert(QStringLiteral("name"), tr("AirMonitor %1").arg(device.deviceId));  //Nickname from the databse for tests at least
        item.insert(QStringLiteral("address"), device.address.toString());
        m_devices.append(item);
    }
    emit devicesChanged();
}

void AppController::onScanFinished()
{
    if (m_scanning) {
        m_scanning = false;
        emit scanningChanged();
    }
}

// Pairing

void AppController::onPairingStateChanged(PairingState state)
{
    m_pairingState = state;
    emit pairingStateChanged();
}

void AppController::onPairingSucceeded(const QString &deviceId)
{
    Q_UNUSED(deviceId)
    setScreen(Screen::DeviceList);   // entering the list starts scanning right away
}

void AppController::onAccessPointNotReachable()
{
    emit errorMessage(tr("Not connected to the device's Wi-Fi yet. Join its network and try again."));
}

// readings, fan, filter and fun. :D

void AppController::onReadingUpdated(const SensorReading &reading)
{
    m_reading = reading;
    emit readingChanged();
}

void AppController::onReadingCleared()
{
    m_reading.reset();
    emit readingChanged();
}

void AppController::onFanModeChanged(FanMode mode)
{
    m_fanMode = mode;
    checkPendingCommand();
    emit fanChanged();
}

void AppController::onFanSpeedChanged(int speed)
{
    m_fanSpeed = speed;
    checkPendingCommand();
    emit fanChanged();
}

void AppController::onFanStateCleared()
{
    m_fanMode.reset();
    m_fanSpeed.reset();
    clearPendingCommand();   // emits fanChanged()
}

void AppController::onFilterStateCleared(){
    m_filterLife.reset();
    emit filterLifeChanged();
}

void AppController::onFilterPercentageChanged(int percentage){
    m_filterLife = percentage;
    emit filterLifeChanged();
}

void AppController::onFilterReplaced(const QString &deviceId)
{
    Q_UNUSED(deviceId)
    emit filterReplaced();
}

void AppController::sendFanCommand(FanMode mode, std::optional<int> speed)
{
    if (m_screen != Screen::Control || m_connectionManager->state() != DeviceState::Connected) {
        emit errorMessage(tr("Error: couldn't execute command: not connected to a device."));
        return;
    }
    // Already in the requested state: no data frame would ever "confirm" it and the timeout would fire for nothing.
    if (m_fanMode == mode && (!speed || m_fanSpeed == speed)) {
        return;
    }

    m_pendingMode = mode;
    m_pendingSpeed = speed;
    m_fanController->setNewMode(mode, speed);
    m_fanCommandTimer->start();
    emit fanChanged();   // fanCommandPending became true :|
}

void AppController::checkPendingCommand()
{
    if (!m_pendingMode) {
        return;
    }
    const bool modeApplied = (m_fanMode == m_pendingMode);
    const bool speedApplied = !m_pendingSpeed || (m_fanSpeed == m_pendingSpeed);
    if (modeApplied && speedApplied) {
        m_fanCommandTimer->stop();
        m_pendingMode.reset();
        m_pendingSpeed.reset();
        // the caller emits fanChanged()
    }
}

void AppController::clearPendingCommand()
{
    m_fanCommandTimer->stop();
    m_pendingMode.reset();
    m_pendingSpeed.reset();
    emit fanChanged();
}

void AppController::onFanCommandTimeout()
{
    if (!m_pendingMode) {
        return;
    }
    clearPendingCommand();
    emit errorMessage(tr("Error: the device did not apply the requested fan setting."));
}

// Navigation

void AppController::setScreen(Screen screen)
{
    if (m_screen == screen) {
        return;
    }
    m_screen = screen;

    // The scanner runs only while the list is on screen
    if (m_screen == Screen::DeviceList) {
        m_scanner->startScanning();
    } else {
        m_scanner->stopScanning();
        if (m_scanning) {
            m_scanning = false;
            emit scanningChanged();
        }
    }
    emit screenChanged();
}

void AppController::replaceFilter()
{
    if (m_screen != Screen::Control || m_connectionManager->state() != DeviceState::Connected) {
        emit errorMessage(tr("Error: couldn't execute command: not connected to a device."));
        return;
    }
    m_filterMonitor->changeFilter();
}
