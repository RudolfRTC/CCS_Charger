#include "ui/chart_widget.h"
#include "ui/theme.h"
#include <QVBoxLayout>

namespace ccs {

ChartWidget::ChartWidget(QWidget* parent)
    : QWidget(parent)
{
    setupChart();
    m_elapsed.start();
}

void ChartWidget::setupChart()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_chart = new QChart();
    m_chart->setBackgroundBrush(QBrush(Theme::BgMedium));
    m_chart->setPlotAreaBackgroundBrush(QBrush(Theme::BgLight));
    m_chart->setPlotAreaBackgroundVisible(true);
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setLabelColor(Theme::TextSecondary);
    m_chart->legend()->setAlignment(Qt::AlignTop);
    m_chart->setTitle("");
    m_chart->setMargins(QMargins(4, 4, 4, 4));

    // Voltage series
    m_voltageSeries = new QLineSeries();
    m_voltageSeries->setName("Voltage (V)");
    m_voltageSeries->setColor(Theme::AccentCyan);
    m_voltageSeries->setPen(QPen(Theme::AccentCyan, 2));
    m_chart->addSeries(m_voltageSeries);

    // Current series
    m_currentSeries = new QLineSeries();
    m_currentSeries->setName("Current (A)");
    m_currentSeries->setColor(Theme::AccentBlue);
    m_currentSeries->setPen(QPen(Theme::AccentBlue, 2));
    m_chart->addSeries(m_currentSeries);

    // Time axis
    m_timeAxis = new QValueAxis();
    m_timeAxis->setTitleText("Time (s)");
    m_timeAxis->setTitleBrush(QBrush(Theme::TextSecondary));
    m_timeAxis->setLabelsColor(Theme::TextSecondary);
    m_timeAxis->setGridLineColor(Theme::Border);
    m_timeAxis->setRange(0, m_timeWindowS);
    m_chart->addAxis(m_timeAxis, Qt::AlignBottom);
    m_voltageSeries->attachAxis(m_timeAxis);
    m_currentSeries->attachAxis(m_timeAxis);

    // Voltage axis (left)
    m_voltageAxis = new QValueAxis();
    m_voltageAxis->setTitleText("Voltage (V)");
    m_voltageAxis->setTitleBrush(QBrush(Theme::AccentCyan));
    m_voltageAxis->setLabelsColor(Theme::AccentCyan);
    m_voltageAxis->setGridLineColor(Theme::Border);
    m_voltageAxis->setRange(0, 500);
    m_chart->addAxis(m_voltageAxis, Qt::AlignLeft);
    m_voltageSeries->attachAxis(m_voltageAxis);

    // Current axis (right)
    m_currentAxis = new QValueAxis();
    m_currentAxis->setTitleText("Current (A)");
    m_currentAxis->setTitleBrush(QBrush(Theme::AccentBlue));
    m_currentAxis->setLabelsColor(Theme::AccentBlue);
    m_currentAxis->setGridLineColor(QColor(Theme::Border.red(), Theme::Border.green(), Theme::Border.blue(), 80));
    m_currentAxis->setRange(0, 200);
    m_chart->addAxis(m_currentAxis, Qt::AlignRight);
    m_currentSeries->attachAxis(m_currentAxis);

    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setStyleSheet(QString("background-color: %1; border: none;").arg(Theme::BgMedium.name()));

    layout->addWidget(m_chartView);
}

void ChartWidget::addDataPoint(double voltage, double current)
{
    double t = m_elapsed.elapsed() / 1000.0;

    m_voltageSeries->append(t, voltage);
    m_currentSeries->append(t, current);
    m_pointCount++;

    // Auto-scale time axis
    if (t > m_timeAxis->max()) {
        double newMin = t - m_timeWindowS;
        if (newMin < 0) newMin = 0;
        m_timeAxis->setRange(newMin, t + 5);
    }

    // Auto-scale voltage axis
    if (voltage > m_voltageAxis->max() * 0.9) {
        m_voltageAxis->setMax(voltage * 1.2);
    }

    // Auto-scale current axis
    if (current > m_currentAxis->max() * 0.9) {
        m_currentAxis->setMax(current * 1.2);
    }

    // Limit points to avoid memory issues (keep last ~2000 points)
    if (m_pointCount > 2000) {
        m_voltageSeries->remove(0);
        m_currentSeries->remove(0);
    }
}

void ChartWidget::reset()
{
    m_voltageSeries->clear();
    m_currentSeries->clear();
    m_elapsed.restart();
    m_pointCount = 0;
    m_timeAxis->setRange(0, m_timeWindowS);
    m_voltageAxis->setRange(0, 500);
    m_currentAxis->setRange(0, 200);
}

void ChartWidget::setTimeWindowSeconds(int seconds)
{
    m_timeWindowS = seconds;
}

} // namespace ccs
