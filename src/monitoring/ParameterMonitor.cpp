#include "monitoring/ParameterMonitor.h"

#include <QStringList>

ParameterMonitor::ParameterMonitor(QObject *parent)
    : QObject(parent)
{
}

std::optional<SensorReading> ParameterMonitor::currentReading() const
{
    return m_reading;
}

void ParameterMonitor::onDataReceived(const QJsonObject &payload, const QString &deviceId, const QDateTime &timestamp)
{

    //Checking if all the data are correct
    const QJsonObject sensor = payload.value(QStringLiteral("sensor_reading")).toObject();

    const QStringList requiredKeys = {
        QStringLiteral("pm25"), QStringLiteral("voc"), QStringLiteral("co2"),
        QStringLiteral("temperature"), QStringLiteral("humidity")
    };
    for (const QString &key : requiredKeys) {
        if (!sensor.contains(key)) {
            return; // incomplete frame? Ignored completly.
        }
    }

    SensorReading reading;
    reading.particulateMatter = sensor.value(QStringLiteral("pm25")).toDouble();
    reading.voc = sensor.value(QStringLiteral("voc")).toDouble();
    reading.carbonDioxide = qRound(sensor.value(QStringLiteral("co2")).toDouble());
    reading.temperature = sensor.value(QStringLiteral("temperature")).toDouble();
    reading.humidity = qRound(sensor.value(QStringLiteral("humidity")).toDouble());

    m_reading = reading;
    emit dataUpdated(reading, deviceId, timestamp);
}

void ParameterMonitor::onActiveDeviceChanged()
{
    //Reseting data, so we do not mix up device readings.
    m_reading.reset();
    emit readingCleared();
}