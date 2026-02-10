#pragma once

#include "can/can_frame.h"
#include "dbc/signal_codec.h"
#include "module/charge_module.h"
#include <QWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QCheckBox>

namespace ccs {

/// Expert/debug view showing raw CAN frames and fully decoded signals
class ExpertWidget : public QWidget {
    Q_OBJECT
public:
    explicit ExpertWidget(QWidget* parent = nullptr);

    void setChargeModule(ChargeModule* module);

public slots:
    void onRawFrameReceived(const ccs::CanFrame& frame);
    void onRawFrameSent(const ccs::CanFrame& frame);
    void onDecodedUpdate();

private:
    void setupUi();
    void addRawRow(const QString& dir, const CanFrame& frame);

    ChargeModule* m_module = nullptr;

    // Raw frames table
    QTableWidget* m_rawTable = nullptr;
    QCheckBox* m_autoScrollCheck = nullptr;
    QPushButton* m_clearRawBtn = nullptr;

    // Decoded signals table
    QTableWidget* m_decodedTable = nullptr;

    int m_rawRowCount = 0;
    static constexpr int MAX_RAW_ROWS = 5000;
};

} // namespace ccs
