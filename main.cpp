#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include "network/NetworkScanner.h"
#include "network/ConnectionManager.h"
#include "monitoring/FilterMonitor.h"
#include "monitoring/ParameterMonitor.h"
#include "monitoring/FanController.h"
#include "notifications/EventMonitor.h"
#include "notifications/NotificationBuilder.h"
#include "database/DatabaseManager.h"
#include "charts/ChartDrawer.h"
#include "notifications/NotificationInbox.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("AirMonitor");

    DatabaseManager databaseManager;
    databaseManager.addPairedDevice(QStringLiteral("A1B2C3"), QStringLiteral("Mock 1"));
    databaseManager.addPairedDevice(QStringLiteral("D4E5F6"), QStringLiteral("Mock 2")); // temporary: normally after pairing


    NetworkScanner scanner;
    scanner.setPairedDevices({"A1B2C3", "D4E5F6"});
    qDebug() << "PAIRED:" << scanner.pairedDevices();
    NotificationInbox notificationInbox(&databaseManager);
    ChartDrawer chartDrawer(&databaseManager);

    ConnectionManager connectionManager;

    ParameterMonitor parameterMonitor;
    FilterMonitor filterMonitor(&connectionManager);
    FanController fanController(&connectionManager);
    EventMonitor eventMonitor;
    NotificationBuilder notificationBuilder;

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("AirMonitorApp", "Main");

    QObject::connect(&app, &QGuiApplication::lastWindowClosed, []() {
        qDebug() << "lastWindowClosed";
    });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        qDebug() << "aboutToQuit";
    });

    QObject::connect(&connectionManager, &ConnectionManager::dataReceived,
                    &parameterMonitor, &ParameterMonitor::onDataReceived);
    QObject::connect(&connectionManager, &ConnectionManager::activeDeviceChanged,
                     &parameterMonitor, &ParameterMonitor::onActiveDeviceChanged);

    QObject::connect(&connectionManager, &ConnectionManager::dataReceived,
                     &filterMonitor, &FilterMonitor::onDataReceived);
    QObject::connect(&connectionManager, &ConnectionManager::activeDeviceChanged,
                     &filterMonitor, &FilterMonitor::onActiveDeviceChanged);

    QObject::connect(&connectionManager, &ConnectionManager::dataReceived,
                     &fanController, &FanController::onDataReceived);
    QObject::connect(&connectionManager, &ConnectionManager::activeDeviceChanged,
                     &fanController, &FanController::onActiveDeviceChanged);

    QObject::connect(&parameterMonitor, &ParameterMonitor::dataUpdated,
                     &eventMonitor, &EventMonitor::onDataUpdated);
    QObject::connect(&filterMonitor, &FilterMonitor::filterPercentageChanged,
                     &eventMonitor, &EventMonitor::onFilterPercentageChanged);
    QObject::connect(&connectionManager, &ConnectionManager::activeDeviceChanged,
                     &eventMonitor, &EventMonitor::onActiveDeviceChanged);
    QObject::connect(&filterMonitor, &FilterMonitor::filterReplaced,
                     &eventMonitor, &EventMonitor::onFilterReplaced);
    QObject::connect(&eventMonitor, &EventMonitor::notificationRaised,
                     &notificationBuilder, &NotificationBuilder::onNotificationRaised);

    QObject::connect(&parameterMonitor, &ParameterMonitor::dataUpdated,
                     &databaseManager, &DatabaseManager::saveReading);

    QObject::connect(&notificationBuilder, &NotificationBuilder::notificationSend,
                     &notificationInbox, &NotificationInbox::onNotificationSend);
    QObject::connect(&connectionManager, &ConnectionManager::activeDeviceChanged,
                     &notificationInbox, &NotificationInbox::onActiveDeviceChanged);
    QObject::connect(&connectionManager, &ConnectionManager::activeDeviceChanged,
                     &chartDrawer, &ChartDrawer::onActiveDeviceChanged);

    QObject::connect(&notificationInbox, &NotificationInbox::notificationsChanged,
                    [&notificationInbox]() {
        /*qDebug() << "INBOX:" << notificationInbox.notifications().size() << "items,"
        << notificationInbox.unreadCount() << "unread";*/
    });
    QObject::connect(&chartDrawer, &ChartDrawer::chartDataChanged, [&chartDrawer]() {
        /*qDebug() << "CHART:" << chartDrawer.points().size() << "points, min"
        << chartDrawer.minValue() << "max" << chartDrawer.maxValue();*/
    });

    QObject::connect(&parameterMonitor, &ParameterMonitor::dataUpdated,
                     [](const SensorReading &r, const QString &id, const QDateTime &ts) {
        /*qDebug() << ts.toString(Qt::ISODateWithMs) << id
        << "PM2.5:" << r.particulateMatter << "VOC:" << r.voc
        << "CO2:" << r.carbonDioxide << "T:" << r.temperature
        << "H:" << r.humidity;*/
    });

    QObject::connect(&filterMonitor, &FilterMonitor::filterPercentageChanged,
                     [](int percentage, const QString &id, const QDateTime &ts) {
        /*qDebug() << "Filter:" << percentage << "%" << id << ts.toString(Qt::ISODateWithMs);*/
    });

    QObject::connect(&filterMonitor, &FilterMonitor::filterStateCleared, []() {
        /*qDebug() << "Filter state cleared";*/
    });

    QObject::connect(&filterMonitor, &FilterMonitor::filterReplaced,
                     [](const QString &id, const QDateTime &ts) {
        /*qDebug() << "Filter replaced:" << id << ts.toString(Qt::ISODateWithMs);*/
    });

    QObject::connect(&fanController, &FanController::fanModeChanged,
                     [](FanMode mode, const QString &id, const QDateTime &ts) {
        /*qDebug() << "Fan: Mode changed to " << static_cast<int>(mode) << id << ts.toString(Qt::ISODateWithMs);*/
    });

    QObject::connect(&fanController, &FanController::fanSpeedChanged,
                 [](int speed, const QString &id, const QDateTime &ts){
        /*qDebug() << "Fan: speed changed to " << speed << id << ts.toString((Qt::ISODateWithMs));*/
    });

    QObject::connect(&eventMonitor, &EventMonitor::notificationRaised,
                     [](const NotificationEvent &e, const QString &id) {
        /*qDebug() << "NOTIFICATION" << e.category << "param:" << static_cast<int>(e.parameter)
        << "value:" << e.value << "threshold:" << e.threshold
        << "severity:" << static_cast<int>(e.severity) << id;*/
    });

    QObject::connect(&notificationBuilder, &NotificationBuilder::notificationSend,
                     [](const StoredNotification &n) {
        /*qDebug() << "SENT:" << n.deviceId << "|" << n.description
        << "| id:" << n.id << "| read:" << n.wasRead;*/
    });

    QObject::connect(&scanner, &NetworkScanner::devicesChanged,
                     [](const QList<DiscoveredDevice> &devices) {
                         qDebug() << "DEVICE LIST:" << devices.size();
                         for (const DiscoveredDevice &d : devices) {
                             qDebug() << "  -" << d.deviceId << d.address << d.port;
                         }
                     });
    QObject::connect(&connectionManager, &ConnectionManager::connectionStatusChanged,
                     [](const QString &id, DeviceState s) {
                         qDebug() << "STATE:" << id << static_cast<int>(s);   // 0 Disconnected, 1 Connecting, 2 Connected
                     });
    QObject::connect(&connectionManager, &ConnectionManager::connectionLost,
                     [](const QString &id) { qDebug() << "LOST:" << id; });
    QObject::connect(&connectionManager, &ConnectionManager::connectionFailed,
                     [](const QString &id) { qDebug() << "FAILED:" << id; });

    scanner.startScanning();

    /*auto connectToListed = [&scanner, &connectionManager](int index) {
        const QList<DiscoveredDevice> list = scanner.activeDevices();
        if (index < list.size()) {
            const DiscoveredDevice &d = list.at(index);
            connectionManager.connectToDevice(d.deviceId, d.address, d.port);
        } else {
            qDebug() << "TEST: no device at index" << index;
        }
    };
    QTimer::singleShot(4000,  [connectToListed]() { connectToListed(0); });
    QTimer::singleShot(14000, [connectToListed]() { connectToListed(1); });
    QTimer::singleShot(14000, [&connectionManager]() { connectionManager.disconnectFromDevice(); }); */

    connectionManager.connectToDevice(QStringLiteral("SILENT"), QHostAddress(QStringLiteral("127.0.0.1")), 83);

    scanner.scanNetwork();


    return QGuiApplication::exec();
}
