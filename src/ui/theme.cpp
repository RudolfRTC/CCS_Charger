#include "ui/theme.h"
#include <QFontDatabase>

namespace ccs {

QString Theme::globalStyleSheet()
{
    return QString(R"(
        * {
            font-family: 'Segoe UI', 'Roboto', 'Helvetica Neue', sans-serif;
            color: %1;
        }

        QMainWindow {
            background-color: %2;
        }

        QWidget {
            background-color: transparent;
        }

        QTabWidget::pane {
            border: 1px solid %3;
            background-color: %4;
            border-radius: 4px;
        }

        QTabBar::tab {
            background-color: %5;
            color: %6;
            border: 1px solid %3;
            border-bottom: none;
            padding: 8px 20px;
            margin-right: 2px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            font-weight: bold;
            font-size: 12px;
        }

        QTabBar::tab:selected {
            background-color: %4;
            color: %7;
            border-bottom: 2px solid %7;
        }

        QTabBar::tab:hover:!selected {
            background-color: %8;
            color: %1;
        }

        QPushButton {
            background-color: %5;
            color: %1;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 8px 16px;
            font-weight: bold;
            font-size: 12px;
            min-height: 20px;
        }

        QPushButton:hover {
            background-color: %8;
            border-color: %7;
        }

        QPushButton:pressed {
            background-color: %3;
        }

        QPushButton:disabled {
            color: %9;
            border-color: %9;
        }

        QPushButton#emergencyButton {
            background-color: rgba(255, 53, 77, 40);
            border: 2px solid %10;
            color: %10;
            font-size: 14px;
            font-weight: bold;
            min-height: 40px;
            border-radius: 8px;
        }

        QPushButton#emergencyButton:hover {
            background-color: rgba(255, 53, 77, 80);
        }

        QPushButton#startButton {
            background-color: rgba(0, 255, 136, 30);
            border: 2px solid %11;
            color: %11;
            font-size: 13px;
        }

        QPushButton#startButton:hover {
            background-color: rgba(0, 255, 136, 60);
        }

        QPushButton#stopButton {
            background-color: rgba(255, 140, 0, 30);
            border: 2px solid %12;
            color: %12;
            font-size: 13px;
        }

        QPushButton#stopButton:hover {
            background-color: rgba(255, 140, 0, 60);
        }

        QComboBox {
            background-color: %5;
            color: %1;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 6px 12px;
            font-size: 12px;
            min-height: 20px;
        }

        QComboBox::drop-down {
            border: none;
            width: 24px;
        }

        QComboBox QAbstractItemView {
            background-color: %4;
            color: %1;
            border: 1px solid %3;
            selection-background-color: %8;
        }

        QSpinBox, QDoubleSpinBox {
            background-color: %5;
            color: %1;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 12px;
            min-height: 20px;
        }

        QSpinBox:focus, QDoubleSpinBox:focus {
            border-color: %7;
        }

        QLineEdit {
            background-color: %5;
            color: %1;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 6px 8px;
            font-size: 12px;
        }

        QLineEdit:focus {
            border-color: %7;
        }

        QGroupBox {
            border: 1px solid %3;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 16px;
            font-weight: bold;
            font-size: 12px;
            color: %6;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
            color: %7;
        }

        QLabel {
            color: %6;
            font-size: 11px;
        }

        QLabel#valueLabel {
            color: %1;
            font-size: 28px;
            font-weight: bold;
        }

        QLabel#unitLabel {
            color: %6;
            font-size: 14px;
        }

        QLabel#stateLabel {
            color: %7;
            font-size: 16px;
            font-weight: bold;
        }

        QLabel#errorLabel {
            color: %10;
            font-size: 12px;
            font-weight: bold;
        }

        QLabel#titleLabel {
            color: %7;
            font-size: 11px;
            font-weight: bold;
            text-transform: uppercase;
        }

        QTableWidget {
            background-color: %4;
            alternate-background-color: %5;
            color: %1;
            border: 1px solid %3;
            gridline-color: %3;
            font-size: 11px;
            font-family: 'Consolas', 'Courier New', monospace;
        }

        QTableWidget::item {
            padding: 4px 8px;
        }

        QTableWidget::item:selected {
            background-color: %8;
        }

        QHeaderView::section {
            background-color: %5;
            color: %7;
            border: 1px solid %3;
            padding: 6px 8px;
            font-weight: bold;
            font-size: 11px;
        }

        QScrollBar:vertical {
            background-color: %4;
            width: 10px;
            border: none;
        }

        QScrollBar::handle:vertical {
            background-color: %3;
            border-radius: 5px;
            min-height: 20px;
        }

        QScrollBar::handle:vertical:hover {
            background-color: %6;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }

        QCheckBox {
            color: %6;
            font-size: 12px;
            spacing: 8px;
        }

        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border: 1px solid %3;
            border-radius: 3px;
            background-color: %5;
        }

        QCheckBox::indicator:checked {
            background-color: %7;
            border-color: %7;
        }

        QStatusBar {
            background-color: %5;
            color: %6;
            border-top: 1px solid %3;
            font-size: 11px;
        }

        QToolTip {
            background-color: %4;
            color: %1;
            border: 1px solid %7;
            padding: 4px;
            font-size: 11px;
        }

        QSplitter::handle {
            background-color: %3;
        }

        QProgressBar {
            background-color: %5;
            border: 1px solid %3;
            border-radius: 4px;
            text-align: center;
            color: %1;
            font-size: 11px;
            min-height: 16px;
        }

        QProgressBar::chunk {
            background-color: %7;
            border-radius: 3px;
        }
    )")
    .arg(TextPrimary.name())       // %1
    .arg(BgDark.name())            // %2
    .arg(Border.name())            // %3
    .arg(BgMedium.name())          // %4
    .arg(BgLight.name())           // %5
    .arg(TextSecondary.name())     // %6
    .arg(AccentCyan.name())        // %7
    .arg(BgHighlight.name())       // %8
    .arg(TextDisabled.name())      // %9
    .arg(AccentRed.name())         // %10
    .arg(AccentGreen.name())       // %11
    .arg(AccentOrange.name());     // %12
}

QFont Theme::dashboardValueFont()
{
    QFont f("Segoe UI", 32, QFont::Bold);
    return f;
}

QFont Theme::labelFont()
{
    QFont f("Segoe UI", 11);
    return f;
}

QFont Theme::monoFont()
{
    QFont f("Consolas", 10);
    f.setStyleHint(QFont::Monospace);
    return f;
}

} // namespace ccs
