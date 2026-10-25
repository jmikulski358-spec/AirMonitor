#include "database/DatabaseManager.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
    + QStringLiteral("/database");
    if (!QDir().mkpath(folder)) {
        qWarning() << "Cannot create database folder:" << folder;
        return;
    }
    const QString filePath = QDir(folder).filePath(QStringLiteral("airmonitor.db"));

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    m_db.setDatabaseName(filePath);

    if (!m_db.open()) {
        qWarning() << "Cannot open database:" << m_db.lastError().text();
        return;
    }

    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS paired_devices ("
                       "id INTEGER NOT NULL PRIMARY KEY, "
                       "device_id TEXT UNIQUE NOT NULL, "
                       "nickname TEXT);"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS readings ("
                       "id INTEGER NOT NULL PRIMARY KEY, "
                       "device_id TEXT NOT NULL, "
                       "timestamp INTEGER NOT NULL, "
                       "particular_matter REAL, "
                       "voc REAL, "
                       "carbon_dioxide INTEGER, "
                       "temperature REAL, "
                       "humidity INTEGER);"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS notifications ("
                       "id INTEGER NOT NULL PRIMARY KEY, "
                       "device_id TEXT NOT NULL, "
                       "category TEXT NOT NULL, "
                       "parameter INTEGER NOT NULL, "
                       "value REAL, "
                       "threshold REAL, "
                       "timestamp INTEGER NOT NULL, "
                       "severity INTEGER NOT NULL, "
                       "description TEXT NOT NULL, "
                       "was_read INTEGER NOT NULL CHECK(was_read IN (0,1)) DEFAULT 0);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_readings_device_time "
                       "ON readings(device_id, timestamp);"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_notifications_device_time "
                       "ON notifications(device_id, timestamp);")
    };

    bool allOk = true;
    for (const QString &sql : statements) {
        allOk = runSql(sql) && allOk;
    }

    m_purgeTimer = new QTimer(this);
    m_purgeTimer->setInterval(60 * 60 * 1000); // once an hour
    connect(m_purgeTimer, &QTimer::timeout, this, &DatabaseManager::purgeOldReadings);
    m_purgeTimer->start();
    purgeOldReadings();

    if (allOk) {
        m_ready = true;
        qDebug() << "Database ready:" << filePath;
    }
}

bool DatabaseManager::runSql(const QString &sql)
{
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        qWarning() << "SQL error:" << query.lastError().text() << "in:" << sql;
        return false;
    }
    return true;
}


QStringList DatabaseManager::pairedDevices() const
{
    QStringList result;
    if (!m_db.isOpen()) {
        return result;
    }

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT device_id FROM paired_devices ORDER BY id"))) {
        qWarning() << "pairedDevices failed:" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        result.append(query.value(0).toString());
    }
    return result;
}

bool DatabaseManager::addPairedDevice(const QString &deviceId, const QString &nickname)
{
    if (!m_db.isOpen()) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO paired_devices (device_id, nickname) VALUES (:device, :nickname)"));
    query.bindValue(QStringLiteral(":device"), deviceId);
    query.bindValue(QStringLiteral(":nickname"), nickname);

    if (!query.exec()) {
        qWarning() << "addPairedDevice failed:" << query.lastError().text();
        return false;
    }
    return true;
}

int DatabaseManager::saveNotification(const StoredNotification &notification)
{
    if (!m_db.isOpen()) {
        return -1;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO notifications "
        "(device_id, category, parameter, value, threshold, timestamp, severity, description, was_read) "
        "VALUES (:device, :category, :parameter, :value, :threshold, :ts, :severity, :description, :read)"));
    query.bindValue(QStringLiteral(":device"), notification.deviceId);
    query.bindValue(QStringLiteral(":category"), notification.event.category);
    query.bindValue(QStringLiteral(":parameter"), static_cast<int>(notification.event.parameter));
    query.bindValue(QStringLiteral(":value"), notification.event.value);
    query.bindValue(QStringLiteral(":threshold"), notification.event.threshold);
    query.bindValue(QStringLiteral(":ts"), notification.event.timestamp.toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":severity"), static_cast<int>(notification.event.severity));
    query.bindValue(QStringLiteral(":description"), notification.description);
    query.bindValue(QStringLiteral(":read"), notification.wasRead ? 1 : 0);

    if (!query.exec()) {
        qWarning() << "saveNotification failed:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

QList<StoredNotification> DatabaseManager::notifications(const QString &deviceId) const
{
    QList<StoredNotification> result;
    if (!m_db.isOpen()) {
        return result;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT id, device_id, category, parameter, value, threshold, timestamp, severity, "
        "description, was_read FROM notifications WHERE device_id = :device "
        "ORDER BY timestamp DESC, id DESC LIMIT :limit"));
    query.bindValue(QStringLiteral(":device"), deviceId);
    query.bindValue(QStringLiteral(":limit"), kMaxNotifications);

    if (!query.exec()) {
        qWarning() << "notifications failed:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        StoredNotification n;
        n.id = query.value(0).toInt();
        n.deviceId = query.value(1).toString();
        n.event.category = query.value(2).toString();
        n.event.parameter = static_cast<Parameter>(query.value(3).toInt());
        n.event.value = query.value(4).toDouble();
        n.event.threshold = query.value(5).toDouble();
        n.event.timestamp = QDateTime::fromMSecsSinceEpoch(query.value(6).toLongLong());
        n.event.severity = static_cast<Severity>(query.value(7).toInt());
        n.description = query.value(8).toString();
        n.wasRead = query.value(9).toInt() != 0;
        result.append(n);
    }
    return result;
}

bool DatabaseManager::markAsRead(int notificationId)
{
    if (!m_db.isOpen()) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE notifications SET was_read = 1 WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), notificationId);

    if (!query.exec()) {
        qWarning() << "markAsRead failed:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::markAllAsRead(const QString &deviceId)
{
    if (!m_db.isOpen()) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE notifications SET was_read = 1 WHERE device_id = :device AND was_read = 0"));
    query.bindValue(QStringLiteral(":device"), deviceId);

    if (!query.exec()) {
        qWarning() << "markAllAsRead failed:" << query.lastError().text();
        return false;
    }
    return true;
}


void DatabaseManager::saveReading(const SensorReading &reading, const QString &deviceId,
                                  const QDateTime &timestamp)
{
    if (!m_db.isOpen()) {
        return;
    }

    // Throttling: one stored reading per interval max, counted separately for each device
    const qint64 ms = timestamp.toMSecsSinceEpoch();
    if (ms - m_lastReadingSaveMs.value(deviceId, 0) < kReadingSaveIntervalMs) {
        return;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO readings "
        "(device_id, timestamp, particular_matter, voc, carbon_dioxide, temperature, humidity) "
        "VALUES (:device, :ts, :pm, :voc, :co2, :temperature, :humidity)"));
    query.bindValue(QStringLiteral(":device"), deviceId);
    query.bindValue(QStringLiteral(":ts"), ms);
    query.bindValue(QStringLiteral(":pm"), reading.particulateMatter);
    query.bindValue(QStringLiteral(":voc"), reading.voc);
    query.bindValue(QStringLiteral(":co2"), reading.carbonDioxide);
    query.bindValue(QStringLiteral(":temperature"), reading.temperature);
    query.bindValue(QStringLiteral(":humidity"), reading.humidity);

    if (!query.exec()) {
        qWarning() << "saveReading failed:" << query.lastError().text();
        return;
    }
    m_lastReadingSaveMs.insert(deviceId, ms);
}

QList<QPointF> DatabaseManager::readingSeries(const QString &deviceId, Parameter parameter,
                                              const DateRange &range, qint64 bucketMs) const
{
    QList<QPointF> points;
    if (!m_db.isOpen() || bucketMs <= 0) {
        return points;
    }

    // A column name cannot be a query placeholder, so it comes from a closed list (never from user text)
    QString column;
    switch (parameter) {
    case Parameter::ParticulateMatter: column = QStringLiteral("particular_matter"); break;
    case Parameter::Voc:               column = QStringLiteral("voc"); break;
    case Parameter::Co2:               column = QStringLiteral("carbon_dioxide"); break;
    case Parameter::Temperature:       column = QStringLiteral("temperature"); break;
    case Parameter::Humidity:          column = QStringLiteral("humidity"); break;
    default:                           return points;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
                      "SELECT timestamp / :bucket AS bucket_index, AVG(%1) FROM readings "
                      "WHERE device_id = :device AND timestamp >= :from AND timestamp <= :to "
                      "GROUP BY bucket_index ORDER BY bucket_index").arg(column));
    query.bindValue(QStringLiteral(":bucket"), bucketMs);
    query.bindValue(QStringLiteral(":device"), deviceId);
    query.bindValue(QStringLiteral(":from"), range.from.toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":to"), range.to.toMSecsSinceEpoch());

    if (!query.exec()) {
        qWarning() << "readingSeries failed:" << query.lastError().text();
        return points;
    }

    while (query.next()) {
        if (query.value(1).isNull()) {
            continue;
        }
        const qint64 bucketIndex = query.value(0).toLongLong();
        points.append(QPointF(static_cast<double>(bucketIndex * bucketMs),
                              query.value(1).toDouble()));
    }
    return points;
}

void DatabaseManager::purgeOldReadings()
{
    if (!m_db.isOpen()) {
        return;
    }

    const qint64 cutoff = QDateTime::currentDateTime().addDays(-kRetentionDays).toMSecsSinceEpoch();

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM readings WHERE timestamp < :cutoff"));
    query.bindValue(QStringLiteral(":cutoff"), cutoff);

    if (!query.exec()) {
        qWarning() << "purgeOldReadings failed:" << query.lastError().text();
    }
}