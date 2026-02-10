#pragma once

#include <QWidget>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QElapsedTimer>

namespace ccs {

/// Real-time voltage/current chart
class ChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChartWidget(QWidget* parent = nullptr);

    void addDataPoint(double voltage, double current);
    void reset();
    void setTimeWindowSeconds(int seconds);

private:
    void setupChart();

    QChartView* m_chartView = nullptr;
    QChart* m_chart = nullptr;
    QLineSeries* m_voltageSeries = nullptr;
    QLineSeries* m_currentSeries = nullptr;
    QValueAxis* m_timeAxis = nullptr;
    QValueAxis* m_voltageAxis = nullptr;
    QValueAxis* m_currentAxis = nullptr;

    QElapsedTimer m_elapsed;
    int m_timeWindowS = 120;
    int m_pointCount = 0;
};

} // namespace ccs
