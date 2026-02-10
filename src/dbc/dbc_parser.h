#pragma once

#include <QString>
#include <QMap>
#include <QVector>
#include <cstdint>
#include <optional>

namespace ccs {

struct DbcSignal {
    QString name;
    uint32_t startBit = 0;
    uint32_t bitLength = 0;
    bool littleEndian = true; // @1+ = little endian
    bool isSigned = false;
    double factor = 1.0;
    double offset = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    QString unit;
    QString comment;
    QMap<int, QString> valueDescriptions; // enumeration values
    int startValue = 0; // GenSigStartValue (often SNA indicator)
};

struct DbcMessage {
    uint32_t id = 0;         // Raw DBC ID (includes bit 31 for extended)
    uint32_t canId = 0;      // Actual CAN ID (without bit 31)
    bool extended = false;
    QString name;
    uint8_t dlc = 0;
    QString transmitter;
    QString comment;
    int cycleTimeMs = 0;     // GenMsgCycleTime
    QString sendType;        // Cyclic, Event-driven, etc.
    QVector<DbcSignal> dbcSignals;
};

struct DbcDatabase {
    QString name;
    QString busType;
    QVector<QString> nodes;
    QMap<uint32_t, DbcMessage> messages; // key = canId

    const DbcMessage* findMessage(uint32_t canId) const {
        auto it = messages.find(canId);
        return (it != messages.end()) ? &it.value() : nullptr;
    }

    const DbcSignal* findSignal(uint32_t canId, const QString& signalName) const {
        const auto* msg = findMessage(canId);
        if (!msg) return nullptr;
        for (const auto& sig : msg->dbcSignals) {
            if (sig.name == signalName) return &sig;
        }
        return nullptr;
    }
};

class DbcParser {
public:
    DbcParser() = default;

    /// Parse a .dbc file and return the database
    bool parse(const QString& filePath);

    /// Get the parsed database
    const DbcDatabase& database() const { return m_db; }
    DbcDatabase& database() { return m_db; }

    QString lastError() const { return m_lastError; }

private:
    void parseLine(const QString& line);
    void parseMessage(const QString& line);
    void parseSignal(const QString& line, DbcMessage& msg);
    void parseComment(const QString& line);
    void parseAttributeDefault(const QString& line);
    void parseAttribute(const QString& line);
    void parseValueDescription(const QString& line);
    void parseNodeList(const QString& line);

    DbcDatabase m_db;
    DbcMessage* m_currentMessage = nullptr;
    QString m_lastError;
};

} // namespace ccs
