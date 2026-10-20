#include "monitoring/FilterMonitor.h"

#include <QDebug>

#include <QStringList>

#include "network/ConnectionManager.h"

FilterMonitor::FilterMonitor(ConnectionManager *connectionManager, QObject *parent)
    : QObject(parent)
    , m_connectionManager(connectionManager)
{
}

void FilterMonitor::changeFilter()
{
    if (!m_connectionManager) {
        qDebug() << "changeFilter: no connection Manager";
        return;
    }

    const QString deviceId = m_connectionManager->currentDeviceId();
    if (deviceId.isEmpty()) {
        qDebug() << "changeFilter: no active device";
        return;
    }

    emit filterReplaced(deviceId, QDateTime::currentDateTime());
}

std::optional<int> FilterMonitor::currentFilterLifePercentage() const
{
    return m_filterLifePercentage;
}

void FilterMonitor::onDataReceived(const QJsonObject &payload, const QString &deviceId, const QDateTime &timestamp)
{
    const QJsonObject filterState = payload.value(QStringLiteral("filter_state")).toObject();
    const QJsonValue value = filterState.value(QStringLiteral("filter_life_percentage"));

    if (!value.isDouble()) {
        return;
    }

    const int percentage = qRound(value.toDouble());
    if (percentage < 0 || percentage > 100) {
        return;
    }

    if (m_filterLifePercentage == percentage) {
        return; // If there are no changes, we do not emit the signal.
        //This condtion comes after save, because we want to see new data on the chart
        //even if its the same as it was.
    }

    m_filterLifePercentage = percentage;
    emit filterPercentageChanged(percentage, deviceId, timestamp);
}

void FilterMonitor::onActiveDeviceChanged()
{
    //Reseting data, so we do not mix up device readings.
    m_filterLifePercentage.reset();
    emit filterStateCleared();
}