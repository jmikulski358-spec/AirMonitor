#pragma once

#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <optional>

#include "core/DataTypes.h"

class ParameterMonitor : public QObject
{
    Q_OBJECT

public:
    explicit ParameterMonitor(QObject *parent = nullptr);

    std::optional<SensorReading> currentReading() const;

public slots:
    void onDataReceived(const QJsonObject &payload, const QString &deviceId, const QDateTime &timestamp);
    void onActiveDeviceChanged();

signals:
    void dataUpdated(const SensorReading &reading, const QString &deviceId, const QDateTime &timestamp);
    void readingCleared();

private:
    std::optional<SensorReading> m_reading; //Since 0 is sometime a possible value, we can not use it to indicate no data.
    //therefor we use std::optional<SensorReading>, which either has a value or is "empty".
};