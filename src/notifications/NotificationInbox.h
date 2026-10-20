#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "core/DataTypes.h"

class DatabaseManager;

class NotificationInbox : public QObject {
    Q_OBJECT

public:
    explicit NotificationInbox(DatabaseManager *database, QObject *parent = nullptr);

    QList<StoredNotification> notifications() const;
    int unreadCount() const;

    void markAsRead(int notificationId);
    void markAllAsRead();

public slots:
    void onNotificationSend(const StoredNotification &notification);
    void onActiveDeviceChanged(const QString &deviceId);

signals:
    void notificationsChanged();

private:
    void reload();

    DatabaseManager *m_database = nullptr;
    QString m_activeDeviceId;
    QList<StoredNotification> m_notifications;
};