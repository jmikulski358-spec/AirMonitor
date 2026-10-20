#include "notifications/NotificationBuilder.h"

NotificationBuilder::NotificationBuilder(QObject *parent)
    : QObject(parent)
{
}

void NotificationBuilder::onNotificationRaised(const NotificationEvent &event, const QString &deviceId)
{
    emit notificationSend(buildNotification(event, deviceId));
}

StoredNotification NotificationBuilder::buildNotification(const NotificationEvent &event, const QString &deviceId) const
{
    StoredNotification notification;
    notification.event = event;
    notification.description = buildDescription(event);
    notification.deviceId = deviceId;
    return notification;
}

QString NotificationBuilder::buildDescription(const NotificationEvent &event) const
{
    const bool critical = (event.severity == Severity::Critical);

    if (event.category == QStringLiteral("filter_replaced")) {
        return tr("Filter replacement recorded.");
    }

    if (event.category == QStringLiteral("filter_state")) {
        const int life = qRound(event.value);
        return critical
                   ? tr("Filter life is at %1%. Replace the filter now.").arg(life)
                   : tr("Filter life is at %1%. Plan to replace the filter soon.").arg(life);
    }

    if (event.category == QStringLiteral("parameter_level")) {
        switch (event.parameter) {
        case Parameter::ParticulateMatter:
            return critical
                       ? tr("PM2.5 levels are critically high: %1 \u00B5g/m\u00B3 (limit %2). Use Turbo mode.")
                             .arg(event.value, 0, 'f', 1).arg(event.threshold, 0, 'f', 1)
                       : tr("PM2.5 levels are high: %1 \u00B5g/m\u00B3 (limit %2).")
                             .arg(event.value, 0, 'f', 1).arg(event.threshold, 0, 'f', 1);
        case Parameter::Voc:
            return critical
                       ? tr("VOC level is high: %1 mg/m\u00B3 (limit %2). Ventilate the room if possible.")
                             .arg(event.value, 0, 'f', 1).arg(event.threshold, 0, 'f', 1)
                       : tr("VOC level is elevated: %1 mg/m\u00B3 (limit %2). Consider opening a window and ventilating the room.")
                             .arg(event.value, 0, 'f', 1).arg(event.threshold, 0, 'f', 1);
        case Parameter::Co2:
            return critical
                       ? tr("CO2 level is very high: %1 ppm (limit %2). Ventilate the room as soon as possible.")
                             .arg(qRound(event.value)).arg(qRound(event.threshold))
                       : tr("CO2 level is elevated: %1 ppm (limit %2). Open a window and ventilate the room.")
                             .arg(qRound(event.value)).arg(qRound(event.threshold));
        default:
            break;
        }
    }

    return tr("Unknown event.");
}