#pragma once

#include "IDecoder.h"
#include <QMap>

class J1939Decoder : public IDecoder {
public:
    J1939Decoder();
    ~J1939Decoder() override = default;

    DecodeStatus tryDecode(const BusMessage& frame, ProtocolMessage& outMsg) override;
    void reset() override;

private:
    struct J1939Session {
        QVector<BusMessage> frames;
        QByteArray data;
        uint32_t pgn = 0;
        int expectedSize = 0;
        int expectedPackets = 0;
        int receivedPackets = 0;
        uint8_t sa = 0;
        uint8_t da = 0; // 0xFF for BAM (broadcast), specific address for RTS
    };

    // Key folds in SA, DA (handles concurrent BAM/RTS sessions from the same
    // SA), RX/TX direction, and interface/channel -- see tpSessionKey().
    QMap<uint64_t, J1939Session> m_sessions;

    static uint64_t tpSessionKey(uint8_t sa, uint8_t da, bool isRX, uint16_t interfaceId) noexcept;
    uint32_t extractPgn(uint32_t id);
};
