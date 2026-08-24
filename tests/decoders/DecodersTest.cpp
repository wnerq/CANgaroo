/*

  Copyright (c) 2026 Schildkroet

  This file is part of cangaroo.

  cangaroo is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  cangaroo is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with cangaroo.  If not, see <http://www.gnu.org/licenses/>.

*/

// UDS and J1939 protocol decoders.
//
// The single-frame, multi-frame, negative-response and J1939 PGN cases below
// started life as src/decoders/test/DecoderTest.cpp, an assert()-based main()
// that was never referenced by any .pri and so never compiled or ran. They are
// converted to Qt Test here, with the stateful paths (multi-frame reassembly,
// reset, interleaved sessions) extended -- that is where a decoder normally
// breaks.

#include <QtTest>

#include "core/BusMessage.h"
#include "decoders/J1939Decoder.h"
#include "decoders/UdsDecoder.h"

class DecodersTest : public QObject
{
    Q_OBJECT

private slots:
    // --- UDS ---
    void udsSingleFrame();
    void udsMultiFrame();
    void udsNegativeResponse();
    void udsIgnoresRtrAndErrorFrames();
    void udsIgnoresEmptyFrame();
    void udsIgnoresUnknownService();
    void udsConsecutiveFrameWithoutFirstFrameIsIgnored();
    void udsResetDropsPendingReassembly();
    void udsInterleavedSessionsAreKeptApart();

    // --- J1939 ---
    void j1939SingleFrame();
    void j1939IgnoresStandardFrames();
    void j1939ExtractsAddressMetadata();
    void j1939IgnoresDiagnosticPgnSoUdsCanClaimIt();

    // --- UDS on 29-bit (extended) identifiers ---
    void udsExtendedSingleFrameExtractsAddressMetadata();
    void udsExtendedMultiFrameExtractsAddressMetadata();
};

// Original DecoderTest case: 0x02 0x10 0x01 -> DiagnosticSessionControl.
void DecodersTest::udsSingleFrame()
{
    UdsDecoder decoder;
    BusMessage msg(0x7E0);
    msg.setLength(8);
    msg.setByte(0, 0x02);   // single frame, 2 data bytes
    msg.setByte(1, 0x10);   // SID 0x10
    msg.setByte(2, 0x01);   // sub-function

    ProtocolMessage out;
    QCOMPARE(decoder.tryDecode(msg, out), DecodeStatus::Completed);
    QCOMPARE(out.name, QString("DiagnosticSessionControl"));
    QCOMPARE(out.protocol, QString("uds"));
    QCOMPARE(out.id, 0x10u);
    QCOMPARE(out.type, MessageType::Request);
    QCOMPARE(out.payload.size(), 2);
    QCOMPARE(static_cast<uint8_t>(out.payload[0]), uint8_t(0x10));
}

// Original DecoderTest case: first frame then one consecutive frame completes a
// 10-byte ReadDataByIdentifier request.
void DecodersTest::udsMultiFrame()
{
    UdsDecoder decoder;
    ProtocolMessage out;

    BusMessage ff(0x7E0);
    ff.setLength(8);
    ff.setByte(0, 0x10);   // first frame
    ff.setByte(1, 0x0A);   // total size 10
    ff.setByte(2, 0x22);   // SID 0x22
    for (int i = 3; i < 8; i++) { ff.setByte(static_cast<uint8_t>(i), static_cast<uint8_t>(i)); }

    QCOMPARE(decoder.tryDecode(ff, out), DecodeStatus::Consumed);

    BusMessage cf(0x7E0);
    cf.setLength(8);
    cf.setByte(0, 0x21);   // consecutive frame, sequence 1
    for (int i = 1; i < 6; i++)
    {
        cf.setByte(static_cast<uint8_t>(i), static_cast<uint8_t>(0xA0 + i));
    }

    QCOMPARE(decoder.tryDecode(cf, out), DecodeStatus::Completed);
    QCOMPARE(out.name, QString("ReadDataByIdentifier"));
    QCOMPARE(out.payload.size(), 10);
    QCOMPARE(out.id, 0x22u);
    QCOMPARE(out.type, MessageType::Request);
}

// Original DecoderTest case: 0x7F with NRC 0x33.
void DecodersTest::udsNegativeResponse()
{
    UdsDecoder decoder;
    BusMessage msg(0x7E8);
    msg.setLength(8);
    msg.setByte(0, 0x03);
    msg.setByte(1, 0x7F);   // negative response
    msg.setByte(2, 0x22);   // rejected service
    msg.setByte(3, 0x33);   // NRC: security access denied

    ProtocolMessage out;
    QCOMPARE(decoder.tryDecode(msg, out), DecodeStatus::Completed);
    QCOMPARE(out.type, MessageType::NegativeResponse);
    QCOMPARE(out.name, QString("NegativeResponse"));
    QCOMPARE(out.description, QString("negative response: Security Access Denied"));
}

void DecodersTest::udsIgnoresRtrAndErrorFrames()
{
    UdsDecoder decoder;
    ProtocolMessage out;

    BusMessage rtr(0x7E0);
    rtr.setLength(8);
    rtr.setByte(0, 0x02);
    rtr.setByte(1, 0x10);
    rtr.setRTR(true);
    QCOMPARE(decoder.tryDecode(rtr, out), DecodeStatus::Ignored);

    BusMessage err(0x7E0);
    err.setLength(8);
    err.setByte(0, 0x02);
    err.setByte(1, 0x10);
    err.setErrorFrame(true);
    QCOMPARE(decoder.tryDecode(err, out), DecodeStatus::Ignored);
}

void DecodersTest::udsIgnoresEmptyFrame()
{
    UdsDecoder decoder;
    ProtocolMessage out;

    BusMessage empty(0x7E0);
    empty.setLength(0);
    QCOMPARE(decoder.tryDecode(empty, out), DecodeStatus::Ignored);
}

void DecodersTest::udsIgnoresUnknownService()
{
    UdsDecoder decoder;
    ProtocolMessage out;

    BusMessage msg(0x7E0);
    msg.setLength(8);
    msg.setByte(0, 0x02);
    msg.setByte(1, 0x00);   // not a valid UDS SID
    msg.setByte(2, 0x01);

    QCOMPARE(decoder.tryDecode(msg, out), DecodeStatus::Ignored);
}

// A consecutive frame with no session in progress must not be mistaken for data.
void DecodersTest::udsConsecutiveFrameWithoutFirstFrameIsIgnored()
{
    UdsDecoder decoder;
    ProtocolMessage out;

    BusMessage cf(0x7E0);
    cf.setLength(8);
    cf.setByte(0, 0x21);
    for (int i = 1; i < 8; i++) { cf.setByte(static_cast<uint8_t>(i), 0xFF); }

    QCOMPARE(decoder.tryDecode(cf, out), DecodeStatus::Ignored);
}

// reset() is called when a measurement starts; a half-received transfer from the
// previous run must not complete afterwards and emit a bogus message.
void DecodersTest::udsResetDropsPendingReassembly()
{
    UdsDecoder decoder;
    ProtocolMessage out;

    BusMessage ff(0x7E0);
    ff.setLength(8);
    ff.setByte(0, 0x10);
    ff.setByte(1, 0x0A);
    ff.setByte(2, 0x22);
    for (int i = 3; i < 8; i++) { ff.setByte(static_cast<uint8_t>(i), static_cast<uint8_t>(i)); }
    QCOMPARE(decoder.tryDecode(ff, out), DecodeStatus::Consumed);

    decoder.reset();

    BusMessage cf(0x7E0);
    cf.setLength(8);
    cf.setByte(0, 0x21);
    for (int i = 1; i < 6; i++)
    {
        cf.setByte(static_cast<uint8_t>(i), static_cast<uint8_t>(0xA0 + i));
    }
    QCOMPARE(decoder.tryDecode(cf, out), DecodeStatus::Ignored);
}

// Two ECUs transferring at once must not have their payloads mixed.
void DecodersTest::udsInterleavedSessionsAreKeptApart()
{
    UdsDecoder decoder;
    ProtocolMessage out;

    const auto firstFrame = [](uint32_t id, uint8_t sid, uint8_t fill)
    {
        BusMessage ff(id);
        ff.setLength(8);
        ff.setByte(0, 0x10);
        ff.setByte(1, 0x0A);
        ff.setByte(2, sid);
        for (int i = 3; i < 8; i++) { ff.setByte(static_cast<uint8_t>(i), fill); }
        return ff;
    };

    const auto consecutive = [](uint32_t id, uint8_t fill)
    {
        BusMessage cf(id);
        cf.setLength(8);
        cf.setByte(0, 0x21);
        for (int i = 1; i < 6; i++) { cf.setByte(static_cast<uint8_t>(i), fill); }
        return cf;
    };

    // Start both transfers, then finish them in the opposite order.
    QCOMPARE(decoder.tryDecode(firstFrame(0x7E0, 0x22, 0x11), out), DecodeStatus::Consumed);
    QCOMPARE(decoder.tryDecode(firstFrame(0x7E1, 0x2E, 0x22), out), DecodeStatus::Consumed);

    QCOMPARE(decoder.tryDecode(consecutive(0x7E1, 0xBB), out), DecodeStatus::Completed);
    QCOMPARE(out.id, 0x2Eu);
    QCOMPARE(out.payload.size(), 10);
    QCOMPARE(static_cast<uint8_t>(out.payload[1]), uint8_t(0x22));

    QCOMPARE(decoder.tryDecode(consecutive(0x7E0, 0xAA), out), DecodeStatus::Completed);
    QCOMPARE(out.id, 0x22u);
    QCOMPARE(out.payload.size(), 10);
    QCOMPARE(static_cast<uint8_t>(out.payload[1]), uint8_t(0x11));
}

// Original DecoderTest case: PGN 65263 with source address 1, priority 6.
void DecodersTest::j1939SingleFrame()
{
    J1939Decoder decoder;
    BusMessage msg(0x18FEEF01);
    msg.setExtended(true);
    msg.setLength(8);
    for (int i = 0; i < 8; i++) { msg.setByte(static_cast<uint8_t>(i), static_cast<uint8_t>(i)); }

    ProtocolMessage out;
    QCOMPARE(decoder.tryDecode(msg, out), DecodeStatus::Completed);
    QCOMPARE(out.name, QString("Engine Fluid Level/Pressure"));
    QCOMPARE(out.protocol, QString("J1939"));
    QCOMPARE(out.id, 0xFEEFu);
    QCOMPARE(out.type, MessageType::Request);
}

// J1939 is an extended-identifier protocol; 11-bit frames are not its business.
void DecodersTest::j1939IgnoresStandardFrames()
{
    J1939Decoder decoder;
    ProtocolMessage out;

    BusMessage msg(0x123);
    msg.setLength(8);
    for (int i = 0; i < 8; i++) { msg.setByte(static_cast<uint8_t>(i), static_cast<uint8_t>(i)); }

    QCOMPARE(decoder.tryDecode(msg, out), DecodeStatus::Ignored);
}

void DecodersTest::j1939ExtractsAddressMetadata()
{
    J1939Decoder decoder;
    BusMessage msg(0x18FEEF2A);   // source address 0x2A
    msg.setExtended(true);
    msg.setLength(8);

    ProtocolMessage out;
    QCOMPARE(decoder.tryDecode(msg, out), DecodeStatus::Completed);
    QVERIFY(!out.metadata.isEmpty());
    QCOMPARE(out.metadata.value("Source Address").toUInt(), 0x2Au);
    QCOMPARE(out.metadata.value("Priority").toUInt(), 6u);
    QCOMPARE(out.metadata.value("PDU Format").toUInt(), 0xFEu);
}

// Issue #38: J1939's catch-all "single-packet PGN" branch used to claim
// every 29-bit frame outright (including PGN 0xDA00/0xDB00, which ISO 15765-4
// UDS-on-CAN extended addressing reuses), so UdsDecoder was never reached and
// the UDS Protocol window stayed empty for 29-bit setups. J1939 must yield on
// these PGNs instead of completing a bogus "J1939" message.
void DecodersTest::j1939IgnoresDiagnosticPgnSoUdsCanClaimIt()
{
    J1939Decoder decoder;
    ProtocolMessage out;

    BusMessage physical(0x18DAFEF9);   // SA 0xF9 -> DA 0xFE, physical addressing
    physical.setExtended(true);
    physical.setLength(8);
    QCOMPARE(decoder.tryDecode(physical, out), DecodeStatus::Ignored);

    BusMessage functional(0x18DBFEF9); // PGN 0xDB00, functional addressing
    functional.setExtended(true);
    functional.setLength(8);
    QCOMPARE(decoder.tryDecode(functional, out), DecodeStatus::Ignored);
}

void DecodersTest::udsExtendedSingleFrameExtractsAddressMetadata()
{
    UdsDecoder decoder;
    BusMessage msg(0x18DAFEF9);   // SA 0xF9 -> DA 0xFE
    msg.setExtended(true);
    msg.setLength(8);
    msg.setByte(0, 0x02);   // single frame, 2 data bytes
    msg.setByte(1, 0x10);   // SID 0x10
    msg.setByte(2, 0x01);   // sub-function

    ProtocolMessage out;
    QCOMPARE(decoder.tryDecode(msg, out), DecodeStatus::Completed);
    QCOMPARE(out.protocol, QString("uds"));
    QVERIFY(!out.metadata.isEmpty());
    QCOMPARE(out.metadata.value("Source Address").toUInt(), 0xF9u);
    QCOMPARE(out.metadata.value("Target Address").toUInt(), 0xFEu);
}

void DecodersTest::udsExtendedMultiFrameExtractsAddressMetadata()
{
    UdsDecoder decoder;
    ProtocolMessage out;

    BusMessage ff(0x18DAFEF9);
    ff.setExtended(true);
    ff.setLength(8);
    ff.setByte(0, 0x10);   // first frame
    ff.setByte(1, 0x0A);   // total size 10
    ff.setByte(2, 0x22);   // SID 0x22
    for (int i = 3; i < 8; i++) { ff.setByte(static_cast<uint8_t>(i), static_cast<uint8_t>(i)); }
    QCOMPARE(decoder.tryDecode(ff, out), DecodeStatus::Consumed);

    BusMessage cf(0x18DAFEF9);
    cf.setExtended(true);
    cf.setLength(8);
    cf.setByte(0, 0x21);   // consecutive frame, sequence 1
    for (int i = 1; i < 6; i++) { cf.setByte(static_cast<uint8_t>(i), static_cast<uint8_t>(0xA0 + i)); }

    QCOMPARE(decoder.tryDecode(cf, out), DecodeStatus::Completed);
    QCOMPARE(out.metadata.value("Source Address").toUInt(), 0xF9u);
    QCOMPARE(out.metadata.value("Target Address").toUInt(), 0xFEu);
}

QTEST_APPLESS_MAIN(DecodersTest)

#include "DecodersTest.moc"
