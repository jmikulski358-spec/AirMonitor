#pragma once

#include <QObject>
#include <QString>

#include "core/DataTypes.h"

class NotificationBuilder : public QObject{
    Q_OBJECT

public:
    explicit NotificationBuilder(QObject *parent = nullptr);

public slots:
    void onNotificationRaised(const NotificationEvent &event, const QString &deviceId);

signals:
    void notificationSend(const StoredNotification &notification);

private:
    StoredNotification buildNotification(const NotificationEvent &event, const QString &deviceId) const;
    QString buildDescription(const NotificationEvent &event) const;
};
