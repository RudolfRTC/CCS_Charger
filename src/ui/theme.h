#pragma once

#include <QString>
#include <QColor>
#include <QFont>

namespace ccs {

/// Space-like / futuristic dark theme constants
namespace Theme {
    // Background colors
    inline const QColor BgDark       = QColor(0x0A, 0x0E, 0x17);      // Deep space navy
    inline const QColor BgMedium     = QColor(0x11, 0x17, 0x25);      // Panel background
    inline const QColor BgLight      = QColor(0x1A, 0x22, 0x35);      // Card/widget background
    inline const QColor BgHighlight  = QColor(0x22, 0x2D, 0x45);      // Hover/selected

    // Accent colors (neon)
    inline const QColor AccentCyan     = QColor(0x00, 0xE5, 0xFF);    // Primary accent
    inline const QColor AccentBlue     = QColor(0x40, 0x7B, 0xFF);    // Secondary accent
    inline const QColor AccentGreen    = QColor(0x00, 0xFF, 0x88);    // Success / active
    inline const QColor AccentYellow   = QColor(0xFF, 0xD6, 0x00);    // Warning
    inline const QColor AccentRed      = QColor(0xFF, 0x35, 0x4D);    // Error / emergency
    inline const QColor AccentOrange   = QColor(0xFF, 0x8C, 0x00);    // Alert
    inline const QColor AccentPurple   = QColor(0xB0, 0x3A, 0xFF);    // Special

    // Text colors
    inline const QColor TextPrimary   = QColor(0xE8, 0xEB, 0xF0);    // Main text
    inline const QColor TextSecondary = QColor(0x8B, 0x95, 0xA8);    // Dimmed text
    inline const QColor TextDisabled  = QColor(0x4A, 0x52, 0x68);    // Disabled

    // Border
    inline const QColor Border        = QColor(0x2A, 0x35, 0x50);
    inline const QColor BorderActive  = AccentCyan;

    /// Get the full application stylesheet
    QString globalStyleSheet();

    /// Get the font for dashboard big values
    QFont dashboardValueFont();

    /// Get the font for labels
    QFont labelFont();

    /// Get the font for mono/code
    QFont monoFont();
}

} // namespace ccs
