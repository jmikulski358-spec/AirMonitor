#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUdpSocket>

#include "core/DataTypes.h"

class DatabaseManager;

class NetworkScanner : public QObject
{
    Q_OBJECT

public:
    explicit NetworkScanner(DatabaseManager *databse, QObject *parent = nullptr);
    QStringList pairedDevices() const;
    QList<DiscoveredDevice> activeDevices() const;

    void scanNetwork();     // one scan (refresh button)
    void startScanning();   // periodic: call when the device list appears on screen
    void stopScanning();    // call when it disappears

signals:
    void devicesChanged(const QList<DiscoveredDevice> &devices); // only when the list really changed
    void scanFinished();

private slots:
    void onReadyRead();
    void onScanTimeout();

private:
    bool isSeenThisScan(const QString &deviceId) const;

    static constexpr quint16 kDiscoveryPort = 45678;
    static constexpr int kScanDurationMs = 2500;
    static constexpr int kScanIntervalMs = 7000;
    static constexpr int kMaxMissedScans = 2;

    DatabaseManager *m_database = nullptr;
    QStringList m_pairedDevices;
    QList<DiscoveredDevice> m_knownDevices;
    QList<DiscoveredDevice> m_seenThisScan;
    QHash<QString, int> m_missedScans;
    QUdpSocket *m_socket = nullptr;
    QTimer *m_scanTimer = nullptr;
    QTimer *m_repeatTimer = nullptr;
};