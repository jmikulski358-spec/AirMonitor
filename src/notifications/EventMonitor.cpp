#include "notifications/EventMonitor.h"

namespace {


Severity severityAbove(double value, double bad, double critical)
{
    if (value > critical) return Severity::Critical;
    if (value > bad) return Severity::Bad;
    return Severity::Normal;
}


Severity severityBelow(double value, double bad, double critical)
{
    if (value < critical) return Severity::Critical;
    if (value < bad) return Severity::Bad;
    return Severity::Normal;
}

} // namespace

EventMonitor::EventMonitor(QObject *parent) : QObject(parent){
}

Severity EventMonitor::currentPmSeverity() const{
    return m_pmSeverity;
}

Severity EventMonitor::currentVocSeverity() const{
    return m_vocSeverity;
}

Severity EventMonitor::currentCarbonDioxideSeverity() const{
    return m_carbonDioxideSeverity;
}

Severity EventMonitor::currentFilterLifeSeverity() const{
    return m_filterLifeSeverity;
}

void EventMonitor::onDataUpdated(const SensorReading &reading, const QString &deviceId, const QDateTime &timestamp){
    const QString category = QStringLiteral("parameter_level");

    evaluate(m_pmSeverity,
            severityAbove(reading.particulateMatter,
                m_thresholds.particulateMatterBad,
                m_thresholds.particulateMatterCritical),
            Parameter::ParticulateMatter, reading.particulateMatter,
            m_thresholds.particulateMatterBad, m_thresholds.particulateMatterCritical,
             category, deviceId, timestamp);

    evaluate(m_vocSeverity,
             severityAbove(reading.voc, m_thresholds.vocBad, m_thresholds.vocCritical),
             Parameter::Voc, reading.voc,
             m_thresholds.vocBad, m_thresholds.vocCritical,
             category, deviceId, timestamp);

    evaluate(m_carbonDioxideSeverity,
             severityAbove(reading.carbonDioxide,
                           m_thresholds.carbonDioxideBad,
                           m_thresholds.carbonDioxideCritical),
             Parameter::Co2, reading.carbonDioxide,
             m_thresholds.carbonDioxideBad, m_thresholds.carbonDioxideCritical,
             category, deviceId, timestamp);
}

void EventMonitor::onFilterPercentageChanged(int percentage, const QString &deviceId, const QDateTime &timestamp)
{
    evaluate(m_filterLifeSeverity,
             severityBelow(percentage, m_filterBadThreshold, m_filterCriticalThreshold),
             Parameter::FilterState, percentage,
             m_filterBadThreshold, m_filterCriticalThreshold,
             QStringLiteral("filter_state"), deviceId, timestamp);
}

void EventMonitor::onFilterReplaced(const QString &deviceId, const QDateTime &timestamp){
    NotificationEvent event;
    event.category = QStringLiteral("filter_replaced");
    event.parameter = Parameter::FilterState;
    event.timestamp = timestamp;
    emit notificationRaised(event, deviceId);
}

void EventMonitor::onActiveDeviceChanged()
{
    m_pmSeverity = Severity::Normal;
    m_vocSeverity = Severity::Normal;
    m_carbonDioxideSeverity = Severity::Normal;
    m_filterLifeSeverity = Severity::Normal;
}

void EventMonitor::evaluate(Severity &stored, Severity current, Parameter parameter,
                            double value, double badThreshold, double criticalThreshold,
                            const QString &category, const QString &deviceId,
                            const QDateTime &timestamp)
{
    if (current > stored) {
        NotificationEvent event;
        event.category = category;
        event.parameter = parameter;
        event.value = value;
        event.threshold = (current == Severity::Critical) ? criticalThreshold : badThreshold;
        event.timestamp = timestamp;
        event.severity = current;
        emit notificationRaised(event, deviceId);
    }
    stored = current;
}