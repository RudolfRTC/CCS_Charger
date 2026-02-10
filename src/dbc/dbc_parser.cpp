#include "dbc/dbc_parser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

namespace ccs {

bool DbcParser::parse(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = "Cannot open file: " + filePath;
        return false;
    }

    QTextStream stream(&file);
    m_currentMessage = nullptr;

    // Read all lines and process
    QString fullText = stream.readAll();
    QStringList lines = fullText.split('\n');

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (line.isEmpty()) {
            m_currentMessage = nullptr;
            continue;
        }
        parseLine(line);
    }

    return true;
}

void DbcParser::parseLine(const QString& line)
{
    if (line.startsWith("BU_:")) {
        parseNodeList(line);
    }
    else if (line.startsWith("BO_ ")) {
        parseMessage(line);
    }
    else if (line.startsWith(" SG_ ") || line.startsWith("SG_ ")) {
        if (m_currentMessage) {
            parseSignal(line, *m_currentMessage);
        }
    }
    else if (line.startsWith("CM_ ")) {
        parseComment(line);
    }
    else if (line.startsWith("BA_DEF_DEF_ ")) {
        parseAttributeDefault(line);
    }
    else if (line.startsWith("BA_ ")) {
        parseAttribute(line);
    }
    else if (line.startsWith("VAL_ ")) {
        parseValueDescription(line);
    }
    else if (line.startsWith("BA_DEF_ ") && line.contains("\"DBName\"")) {
        // Skip definitions
    }
}

void DbcParser::parseNodeList(const QString& line)
{
    // BU_: VCU CMS
    QString rest = line.mid(4).trimmed();
    m_db.nodes = rest.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
}

void DbcParser::parseMessage(const QString& line)
{
    // BO_ 2147488512 EVDCMaxLimits: 8 VCU
    static QRegularExpression rx(R"(BO_\s+(\d+)\s+(\w+)\s*:\s*(\d+)\s+(\w+))");
    auto match = rx.match(line);
    if (!match.hasMatch()) return;

    DbcMessage msg;
    msg.id = match.captured(1).toUInt();
    msg.name = match.captured(2);
    msg.dlc = match.captured(3).toUInt();
    msg.transmitter = match.captured(4);

    // Bit 31 of DBC ID indicates extended frame
    if (msg.id & 0x80000000) {
        msg.extended = true;
        msg.canId = msg.id & 0x1FFFFFFF;
    } else {
        msg.extended = false;
        msg.canId = msg.id;
    }

    m_db.messages[msg.canId] = msg;
    m_currentMessage = &m_db.messages[msg.canId];
}

void DbcParser::parseSignal(const QString& line, DbcMessage& msg)
{
    // SG_ EVMaxCurrent : 0|16@1+ (0.1,0) [0|6500] "A" CMS
    static QRegularExpression rx(
        R"(SG_\s+(\w+)\s*:\s*(\d+)\|(\d+)@([01])([+-])\s*\(([^,]+),([^)]+)\)\s*\[([^|]+)\|([^\]]+)\]\s*\"([^\"]*)\"\s*(.*))");

    auto match = rx.match(line.trimmed());
    if (!match.hasMatch()) return;

    DbcSignal sig;
    sig.name = match.captured(1);
    sig.startBit = match.captured(2).toUInt();
    sig.bitLength = match.captured(3).toUInt();
    sig.littleEndian = (match.captured(4) == "1");
    sig.isSigned = (match.captured(5) == "-");
    sig.factor = match.captured(6).toDouble();
    sig.offset = match.captured(7).toDouble();
    sig.minimum = match.captured(8).toDouble();
    sig.maximum = match.captured(9).toDouble();
    sig.unit = match.captured(10);

    msg.dbcSignals.append(sig);
}

void DbcParser::parseComment(const QString& line)
{
    // CM_ BO_ 2147488512 "description";
    // CM_ SG_ 2147488512 EVMaxCurrent "description";
    static QRegularExpression rxBo(R"(CM_\s+BO_\s+(\d+)\s+\"(.*)\"\s*;)");
    static QRegularExpression rxSg(R"(CM_\s+SG_\s+(\d+)\s+(\w+)\s+\"(.*)\"\s*;)");

    auto matchBo = rxBo.match(line);
    if (matchBo.hasMatch()) {
        uint32_t rawId = matchBo.captured(1).toUInt();
        uint32_t canId = rawId & 0x1FFFFFFF;
        if (m_db.messages.contains(canId)) {
            m_db.messages[canId].comment = matchBo.captured(2);
        }
        return;
    }

    auto matchSg = rxSg.match(line);
    if (matchSg.hasMatch()) {
        uint32_t rawId = matchSg.captured(1).toUInt();
        uint32_t canId = rawId & 0x1FFFFFFF;
        QString sigName = matchSg.captured(2);
        if (m_db.messages.contains(canId)) {
            auto& msg = m_db.messages[canId];
            for (auto& sig : msg.dbcSignals) {
                if (sig.name == sigName) {
                    sig.comment = matchSg.captured(3);
                    break;
                }
            }
        }
    }
}

void DbcParser::parseAttributeDefault(const QString& /*line*/)
{
    // BA_DEF_DEF_ "GenMsgCycleTime" 0;
    // We store defaults but use them only when specific BA_ values aren't set
}

void DbcParser::parseAttribute(const QString& line)
{
    // BA_ "GenMsgCycleTime" BO_ 2147488512 100;
    static QRegularExpression rxCycle(R"(BA_\s+\"GenMsgCycleTime\"\s+BO_\s+(\d+)\s+(\d+)\s*;)");
    static QRegularExpression rxSendType(R"(BA_\s+\"GenMsgSendType\"\s+BO_\s+(\d+)\s+(\d+)\s*;)");
    static QRegularExpression rxStartValue(R"(BA_\s+\"GenSigStartValue\"\s+SG_\s+(\d+)\s+(\w+)\s+(\d+)\s*;)");
    static QRegularExpression rxDbName(R"(BA_\s+\"DBName\"\s+\"([^\"]*)\"\s*;)");
    static QRegularExpression rxBusType(R"(BA_\s+\"BusType\"\s+\"([^\"]*)\"\s*;)");

    auto matchDb = rxDbName.match(line);
    if (matchDb.hasMatch()) {
        m_db.name = matchDb.captured(1);
        return;
    }

    auto matchBus = rxBusType.match(line);
    if (matchBus.hasMatch()) {
        m_db.busType = matchBus.captured(1);
        return;
    }

    auto matchCycle = rxCycle.match(line);
    if (matchCycle.hasMatch()) {
        uint32_t rawId = matchCycle.captured(1).toUInt();
        uint32_t canId = rawId & 0x1FFFFFFF;
        int cycle = matchCycle.captured(2).toInt();
        if (m_db.messages.contains(canId)) {
            m_db.messages[canId].cycleTimeMs = cycle;
        }
        return;
    }

    auto matchSend = rxSendType.match(line);
    if (matchSend.hasMatch()) {
        uint32_t rawId = matchSend.captured(1).toUInt();
        uint32_t canId = rawId & 0x1FFFFFFF;
        int type = matchSend.captured(2).toInt();
        if (m_db.messages.contains(canId)) {
            static const char* types[] = {"Cyclic", "Event-driven", "On request", "dummy"};
            m_db.messages[canId].sendType = types[std::min(type, 3)];
        }
        return;
    }

    auto matchStart = rxStartValue.match(line);
    if (matchStart.hasMatch()) {
        uint32_t rawId = matchStart.captured(1).toUInt();
        uint32_t canId = rawId & 0x1FFFFFFF;
        QString sigName = matchStart.captured(2);
        int val = matchStart.captured(3).toInt();
        if (m_db.messages.contains(canId)) {
            auto& msg = m_db.messages[canId];
            for (auto& sig : msg.dbcSignals) {
                if (sig.name == sigName) {
                    sig.startValue = val;
                    break;
                }
            }
        }
    }
}

void DbcParser::parseValueDescription(const QString& line)
{
    // VAL_ 2147487744 StateMachineState 15 "SNA" 0 "Default" 1 "Init" ...;
    static QRegularExpression rxHeader(R"(VAL_\s+(\d+)\s+(\w+)\s+)");
    auto matchHeader = rxHeader.match(line);
    if (!matchHeader.hasMatch()) return;

    uint32_t rawId = matchHeader.captured(1).toUInt();
    uint32_t canId = rawId & 0x1FFFFFFF;
    QString sigName = matchHeader.captured(2);

    if (!m_db.messages.contains(canId)) return;
    auto& msg = m_db.messages[canId];

    DbcSignal* targetSig = nullptr;
    for (auto& sig : msg.dbcSignals) {
        if (sig.name == sigName) {
            targetSig = &sig;
            break;
        }
    }
    if (!targetSig) return;

    // Parse value-description pairs: number "string"
    QString remainder = line.mid(matchHeader.capturedEnd());
    static QRegularExpression rxPair(R"((\d+)\s+\"([^\"]*)\")");
    auto it = rxPair.globalMatch(remainder);
    while (it.hasNext()) {
        auto m = it.next();
        targetSig->valueDescriptions[m.captured(1).toInt()] = m.captured(2);
    }
}

} // namespace ccs
