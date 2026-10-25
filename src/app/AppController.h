#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <optional>

#include "core/DataTypes.h"

class ConnectionManager;
class DatabaseManager;
class FanController;
class NetworkScanner;
class ParameterMonitor;
class PairingController;
class FilterMonitor;

/*

The bridge between the C++ logic and QML. It owns nothing: all dependencies are created in main()
and only used here.

- State for the UI -> Q_PROPERTY (QML refreshes itself), used for e.g. parameter values.
- One-time events -> signals, used for e.g. errorMessages.
- Commands from UI -> Q_INVOKABLE, e.g. switchinf fan mode.

The current screen lives here (not in QML), so navigation can be tested from the console.

*/
class AppController : public QObject
{
    Q_OBJECT

public:
    enum class Screen {
        Startup,      // before start() was called
        Error,        // the database did not open: nothing works without it (CRITICAL)
        Pairing,
        DeviceList,
        Control
    };
    Q_ENUM(Screen)

private:
    Q_PROPERTY(Screen screen READ screen NOTIFY screenChanged)
    Q_PROPERTY(QString startupError READ startupError NOTIFY screenChanged)

    //device list
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)   // [{deviceId, name, address}]
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)        // manual refresh in progress
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectingChanged)

    //pairing (value of PairingState, see DataTypes.h)
    Q_PROPERTY(int pairingState READ pairingState NOTIFY pairingStateChanged)

    //readings: NaN means "no data", QML shows "--", not 0, for some data values can be zero (e.g. temperature), though it's not likely.
    Q_PROPERTY(double pm25 READ pm25 NOTIFY readingChanged)
    Q_PROPERTY(double voc READ voc NOTIFY readingChanged)
    Q_PROPERTY(double co2 READ co2 NOTIFY readingChanged)
    Q_PROPERTY(double temperature READ temperature NOTIFY readingChanged)
    Q_PROPERTY(double humidity READ humidity NOTIFY readingChanged)

    //Fan, as reported by the device: mode 0 = unknown, otherwise a FanMode value (1..4); speed -1 = unknown
    Q_PROPERTY(int fanMode READ fanMode NOTIFY fanChanged)
    Q_PROPERTY(int fanSpeed READ fanSpeed NOTIFY fanChanged)
    Q_PROPERTY(bool fanCommandPending READ fanCommandPending NOTIFY fanChanged)

    // -1 means no data (QML sows "--"), otherwise it shows filter life percentage.
   Q_PROPERTY(int filterLife READ filterLifePercentage NOTIFY filterLifeChanged)

public:
    AppController(ConnectionManager *connectionManager,
                DatabaseManager *databaseManager,
                NetworkScanner *scanner,
                PairingController *pairingController,
                ParameterMonitor *parameterMonitor,
                FanController *fanController,
                FilterMonitor *filterMonitor,
                QObject *parent = nullptr
                );

    void start(); //Call once, after everything is wired: picks the first screen.

    Screen screen() const;
    QString startupError() const;
    QVariantList devices() const;
    bool scanning() const;
    bool connecting() const;
    int pairingState() const;
    double pm25() const;
    double voc() const;
    double co2() const;
    double temperature() const;
    double humidity() const;
    int fanMode() const;
    int fanSpeed() const;
    bool fanCommandPending() const;
    int filterLifePercentage() const;

    // Commands from the UI
    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void connectToDevice(const QString &deviceId);
    Q_INVOKABLE void goBack();                       // "<": "go back" button from the control panel / from pairing
    Q_INVOKABLE void startPairing();                 // "+" (also used for "try again")
    Q_INVOKABLE void confirmApConnected();           // "I'm connected"
    Q_INVOKABLE void submitCredentials(const QString &ssid, const QString &password);
    Q_INVOKABLE void cancelPairing();
    Q_INVOKABLE void setFanMode(int mode);           // FanMode value 1..4
    Q_INVOKABLE void setManualSpeed(int speed);
    Q_INVOKABLE void replaceFilter();

signals:
    void screenChanged();
    void devicesChanged();
    void scanningChanged();
    void connectingChanged();
    void pairingStateChanged();
    void readingChanged();
    void fanChanged();
    void filterLifeChanged();   // state: the percentage changed (or disappeared)
    void filterReplaced();
    void errorMessage(const QString &text);   // one-time: dialog in QML

private slots:
    void onConnectionStatusChanged(const QString &deviceId, DeviceState state);
    void onConnectionFailed(const QString &deviceId);
    void onConnectionLost(const QString &deviceId);
    void onDevicesChanged(const QList<DiscoveredDevice> &devices);
    void onScanFinished();
    void onPairingStateChanged(PairingState state);
    void onPairingSucceeded(const QString &deviceId);
    void onAccessPointNotReachable();
    void onReadingUpdated(const SensorReading &reading);
    void onReadingCleared();
    void onFanModeChanged(FanMode mode);
    void onFanSpeedChanged(int speed);
    void onFanStateCleared();
    void onFanCommandTimeout();
    void onFilterPercentageChanged(int percentage);
    void onFilterStateCleared();
    void onFilterReplaced(const QString &deviceId);

private:
    void setScreen(Screen screen);
    void sendFanCommand(FanMode mode, std::optional<int> speed);
    void checkPendingCommand();
    void clearPendingCommand();

    // The device does not confirm commands, so we wait for the next data frames to show the change.
    static constexpr int kFanCommandTimeoutMs = 5000;  // about 2-3 data frames; adjust to the real interval
    static constexpr int kManualMinSpeed = 65;
    static constexpr int kManualMaxSpeed = 80;

    ConnectionManager *m_connectionManager = nullptr; // not owned
    DatabaseManager *m_databaseManager = nullptr;   // not owned
    NetworkScanner *m_scanner = nullptr;     // not owned
    PairingController *m_pairing = nullptr;   // not owned
    ParameterMonitor *m_parameterMonitor = nullptr;     // not owned
    FanController *m_fanController = nullptr;    // not owned
    FilterMonitor *m_filterMonitor = nullptr;   // not owned

    Screen m_screen = Screen::Startup;
    QString m_startupError;
    QVariantList m_devices;
    bool m_scanning = false;
    bool m_connecting = false;
    PairingState m_pairingState = PairingState::Idle;

    std::optional<SensorReading> m_reading;
    std::optional<FanMode> m_fanMode;
    std::optional<int> m_fanSpeed;

    std::optional<int> m_filterLife;
    std::optional<FanMode> m_pendingMode;
    std::optional<int> m_pendingSpeed;
    QTimer *m_fanCommandTimer = nullptr;
};
