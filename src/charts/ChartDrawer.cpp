#include "charts/ChartDrawer.h"

#include <algorithm>

#include "database/DatabaseManager.h"

ChartDrawer::ChartDrawer(DatabaseManager *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
}

void ChartDrawer::loadChart(Parameter parameter, TimeRange range)
{
    m_parameter = parameter;
    m_timeRange = range;
    reload();
}

void ChartDrawer::onActiveDeviceChanged(const QString &deviceId)
{
    m_deviceId = deviceId;
    reload();
}

QList<QPointF> ChartDrawer::points() const { return m_points; }
double ChartDrawer::minValue() const { return m_minValue; }
double ChartDrawer::maxValue() const { return m_maxValue; }
DateRange ChartDrawer::visibleRange() const { return m_visibleRange; }

void ChartDrawer::reload()
{
    m_points.clear();
    m_minValue = 0.0;
    m_maxValue = 0.0;
    m_visibleRange = rangeToDates(m_timeRange);

    if (m_database && m_parameter.has_value() && !m_deviceId.isEmpty()) {
        m_points = m_database->readingSeries(m_deviceId, *m_parameter,
                                             m_visibleRange, bucketMs(m_timeRange));
    }

    if (!m_points.isEmpty()) {
        const auto [minIt, maxIt] = std::minmax_element(
            m_points.cbegin(), m_points.cend(),
            [](const QPointF &a, const QPointF &b) { return a.y() < b.y(); });
        m_minValue = minIt->y();
        m_maxValue = maxIt->y();
    }

    emit chartDataChanged();
}

DateRange ChartDrawer::rangeToDates(TimeRange range) const
{
    DateRange result;
    result.to = QDateTime::currentDateTime();

    switch (range) {
    case TimeRange::LastHour:     result.from = result.to.addSecs(-3600); break;
    case TimeRange::Last24Hours:  result.from = result.to.addDays(-1); break;
    case TimeRange::LastWeek:     result.from = result.to.addDays(-7); break;
    case TimeRange::LastTwoWeeks: result.from = result.to.addDays(-14); break;
    }
    return result;
}

// Averaging buckets keep every chart at roughly 60-340 points
qint64 ChartDrawer::bucketMs(TimeRange range) const
{
    switch (range) {
    case TimeRange::LastHour:     return 60LL * 1000;          // 1 min
    case TimeRange::Last24Hours:  return 5LL * 60 * 1000;      // 5 min
    case TimeRange::LastWeek:     return 30LL * 60 * 1000;     // 30 min
    case TimeRange::LastTwoWeeks: return 60LL * 60 * 1000;     // 1 h
    }
    return 60LL * 1000;
}