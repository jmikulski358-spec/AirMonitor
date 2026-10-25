#pragma once

#include <QObject>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <optional>

class ConnectionManager;

class FilterMonitor : public QObject
{
    Q_OBJECT

public:
    explicit FilterMonitor(ConnectionManager *connectionManager, QObject *parent = nullptr);

    std::optional<int> currentFilterLifePercentage() const;

    void changeFilter();

public slots:
    void onDataReceived(const QJsonObject &payload, const QString &deviceId, const QDateTime &timestamp);
    void onActiveDeviceChanged();

signals:
    void filterPercentageChanged(const int percentage, const QString &deviceId, const QDateTime &timestamp);
    void filterReplaced(const QString &deviceId, const QDateTime &timestamp);
    void filterStateCleared();

private:
    std::optional<int> m_filterLifePercentage;
    ConnectionManager *m_connectionManager = nullptr;
    static constexpr int kReplacedJumpPercent = 20;
};