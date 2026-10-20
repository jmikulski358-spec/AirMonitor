#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "core/DataTypes.h"

class DatabaseManager : public QObject {
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);

    // Devices
    QStringList pairedDevices() const;
    bool addPairedDevice(const QString &deviceId, const QString &nickname);

    // Notifications
    int saveNotification(const StoredNotification &notification); // returns new id, -1 on failure
    QList<StoredNotification> notifications(const QString &deviceId) const;
    bool markAsRead(int notificationId);
    bool markAllAsRead(const QString &deviceId);

    // Readings (for charts): points are (ms since epoch, average value)
    QList<QPointF> readingSeries(const QString &deviceId, Parameter parameter,
                                 const DateRange &range, qint64 bucketMs) const;

public slots:
    void saveReading(const SensorReading &reading, const QString &deviceId, const QDateTime &timestamp);

private slots:
    void purgeOldReadings();

private:
    bool runSql(const QString &sql);

    static constexpr qint64 kReadingSaveIntervalMs = 60 * 1000;
    static constexpr int kRetentionDays = 15;
    static constexpr int kMaxNotifications = 200;

    QSqlDatabase m_db;
    QHash<QString, qint64> m_lastReadingSaveMs;
    QTimer *m_purgeTimer = nullptr;
};