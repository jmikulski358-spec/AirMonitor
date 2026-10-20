#include "notifications/NotificationInbox.h"

#include <QDebug>

#include "database/DatabaseManager.h"

NotificationInbox::NotificationInbox(DatabaseManager *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
}

QList<StoredNotification> NotificationInbox::notifications() const
{
    return m_notifications;
}

int NotificationInbox::unreadCount() const
{
    int count = 0;
    for (const StoredNotification &n : m_notifications) {
        if (!n.wasRead) {
            ++count;
        }
    }
    return count;
}

void NotificationInbox::onNotificationSend(const StoredNotification &notification)
{
    StoredNotification stored = notification;

    if (m_database) {
        stored.id = m_database->saveNotification(notification);
        if (stored.id < 0) {
            qWarning() << "Notification was not saved to the database";
        }
    }

    // Always stored, but shown only if it belongs to the device currently on screen
    if (stored.deviceId != m_activeDeviceId) {
        return;
    }

    m_notifications.prepend(stored); // newest first
    emit notificationsChanged();
}

void NotificationInbox::onActiveDeviceChanged(const QString &deviceId)
{
    m_activeDeviceId = deviceId;
    reload();
}

void NotificationInbox::markAsRead(int notificationId)
{
    if (m_database && !m_database->markAsRead(notificationId)) {
        return;
    }

    for (StoredNotification &n : m_notifications) {
        if (n.id == notificationId) {
            if (!n.wasRead) {
                n.wasRead = true;
                emit notificationsChanged();
            }
            return;
        }
    }
}

void NotificationInbox::markAllAsRead()
{
    if (m_activeDeviceId.isEmpty()) {
        return;
    }
    if (m_database && !m_database->markAllAsRead(m_activeDeviceId)) {
        return;
    }

    bool changed = false;
    for (StoredNotification &n : m_notifications) {
        if (!n.wasRead) {
            n.wasRead = true;
            changed = true;
        }
    }
    if (changed) {
        emit notificationsChanged();
    }
}

void NotificationInbox::reload()
{
    if (m_database && !m_activeDeviceId.isEmpty()) {
        m_notifications = m_database->notifications(m_activeDeviceId);
    } else {
        m_notifications.clear();
    }
    emit notificationsChanged();
}