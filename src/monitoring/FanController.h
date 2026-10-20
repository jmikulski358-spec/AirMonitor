#pragma once

#include <QJsonObject>
#include <QDateTime>
#include <QObject>
#include <optional>
#include <QString>

#include "core/DataTypes.h"

class ConnectionManager;

class FanController : public QObject {

    Q_OBJECT

public:
    explicit FanController(ConnectionManager *connectionManager, QObject *parent = nullptr);

    std::optional<FanMode> currentFanMode() const;
    std::optional<int> currentFanSpeed() const;

    void setNewMode(FanMode newMode, std::optional<int> newSpeed);

public slots:
    void onDataReceived(const QJsonObject &payload, const QString &deviceId, const QDateTime &timestamp);
    void onActiveDeviceChanged();
signals:
    void fanStateCleared();
    void fanModeChanged(const FanMode newMode, const QString &deviceId, const QDateTime &timestamp);
    void fanSpeedChanged(const int newSpeed, const QString &deviceId, const QDateTime &timestamp);
private:
    std::optional<FanMode> m_fanMode;
    std::optional<int> m_fanSpeed;
    ConnectionManager *m_connectionManager = nullptr;

    QJsonObject buildFanRegulationRequest(FanMode newFanMode, std::optional<int> newSpeed);
};
