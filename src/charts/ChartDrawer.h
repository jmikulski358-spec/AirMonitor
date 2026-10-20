#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QString>
#include <optional>

#include "core/DataTypes.h"

class DatabaseManager;

class ChartDrawer : public QObject {
    Q_OBJECT

public:
    explicit ChartDrawer(DatabaseManager *database, QObject *parent = nullptr);

    void loadChart(Parameter parameter, TimeRange range);

    QList<QPointF> points() const;   // x = ms since epoch, y = value
    double minValue() const;
    double maxValue() const;
    DateRange visibleRange() const;

public slots:
    void onActiveDeviceChanged(const QString &deviceId);

signals:
    void chartDataChanged();

private:
    void reload();
    DateRange rangeToDates(TimeRange range) const;
    qint64 bucketMs(TimeRange range) const;

    DatabaseManager *m_database = nullptr;
    QString m_deviceId;
    std::optional<Parameter> m_parameter;
    TimeRange m_timeRange = TimeRange::LastHour;

    QList<QPointF> m_points;
    double m_minValue = 0.0;
    double m_maxValue = 0.0;
    DateRange m_visibleRange;
};