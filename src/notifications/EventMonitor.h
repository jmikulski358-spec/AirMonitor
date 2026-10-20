#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>

#include "core/DataTypes.h"

class EventMonitor : public QObject {
    Q_OBJECT

public:
    explicit EventMonitor(QObject *parent = nullptr);

    Severity currentPmSeverity() const;
    Severity currentVocSeverity() const;
    Severity currentCarbonDioxideSeverity() const;
    Severity currentFilterLifeSeverity() const;
public slots:
    void onDataUpdated(const SensorReading &reading, const QString &deviceId, const QDateTime &timestamp);
    void onFilterPercentageChanged(int percentage, const QString &deviceId, const QDateTime &timestamp);
    void onFilterReplaced(const QString &deviceId, const QDateTime &timestamp);
    void onActiveDeviceChanged();

signals:
    void notificationRaised(const NotificationEvent &event, const QString &deviceId);

private:
    void evaluate(Severity &stored, Severity current, Parameter parameter,
                  double value, double badThreshold, double criticalThreshold,
                  const QString &category, const QString &deviceId,
                  const QDateTime &timestamp);

    SensorThresholds m_thresholds;
    int m_filterBadThreshold = 60;
    int m_filterCriticalThreshold = 30;

    Severity m_pmSeverity = Severity::Normal;
    Severity m_vocSeverity = Severity::Normal;
    Severity m_carbonDioxideSeverity = Severity::Normal;
    Severity m_filterLifeSeverity = Severity::Normal;
};
