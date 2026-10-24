/*
 * Standalone protocol tests, no Qt needed:
 *   g++ -std=c++11 -Wall -Wextra -I../src ../src/nothingprotocol.cpp protocol_test.cpp -o protocol_test && ./protocol_test
 * Test vectors mirror the ones in earctl's protocol.rs.
 */

#include "nothingprotocol.h"

#include <cstdio>
#include <cstdlib>

using namespace nothing;

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void testCrcKnownValue()
{
    const uint8_t bytes[] = { 0x55, 0x60, 0x01, 0x34, 0x12, 0x02, 0x00, 0x01, 0xAA, 0xBB };
    CHECK(crc16(bytes, sizeof(bytes)) == 0xFA6A);
}

static void testRoundTrip()
{
    std::vector<uint8_t> payload{ 0xAA, 0x55, 0x01 };
    std::vector<uint8_t> buffer = encode(0xC007, 0x10, payload);
    CHECK(buffer[0] == 0x55 && buffer[1] == 0x60 && buffer[2] == 0x01);
    Packet p;
    CHECK(tryParse(buffer, p) == ParseResult::Ok);
    CHECK(p.command == 0xC007);
    CHECK(p.operationId == 0x10);
    CHECK(p.payload == payload);
    CHECK(buffer.empty());
}

static void testFragmentedAndGarbage()
{
    std::vector<uint8_t> a = encode(0x1234, 1, { 0x01, 0x02 });
    std::vector<uint8_t> b = encode(0xABCD, 2, { 0x03 });

    std::vector<uint8_t> buffer{ 0x00, 0x55, 0x13 }; // noise, incl. a fake magic byte
    buffer.insert(buffer.end(), a.begin(), a.begin() + 5);
    Packet p;
    CHECK(tryParse(buffer, p) == ParseResult::NeedMoreData);

    buffer.insert(buffer.end(), a.begin() + 5, a.end());
    buffer.insert(buffer.end(), b.begin(), b.end());
    CHECK(tryParse(buffer, p) == ParseResult::Ok);
    CHECK(p.command == 0x1234 && p.payload == std::vector<uint8_t>({ 0x01, 0x02 }));
    CHECK(tryParse(buffer, p) == ParseResult::Ok);
    CHECK(p.command == 0xABCD && p.payload == std::vector<uint8_t>({ 0x03 }));
    CHECK(buffer.empty());
    CHECK(tryParse(buffer, p) == ParseResult::NeedMoreData);
}

static void testBadCrc()
{
    std::vector<uint8_t> buffer = encode(0x1234, 1, { 0x01 });
    buffer.back() ^= 0xFF;
    std::vector<uint8_t> good = encode(0x5678, 2, {});
    buffer.insert(buffer.end(), good.begin(), good.end());
    Packet p;
    CHECK(tryParse(buffer, p) == ParseResult::CrcError);
    CHECK(tryParse(buffer, p) == ParseResult::Ok);
    CHECK(p.command == 0x5678 && p.payload.empty());
}

static void testSetAnc()
{
    std::vector<uint8_t> packet = encode(cmd::SetAnc, 7, setAncPayload(AncTransparency));
    // 55 60 01 | 0F F0 | 03 00 07 | 01 07 00 | crc
    CHECK(packet.size() == 13);
    CHECK(packet[3] == 0x0F && packet[4] == 0xF0);
    CHECK(packet[5] == 3 && packet[7] == 7);
    CHECK(packet[8] == 0x01 && packet[9] == 0x07 && packet[10] == 0x00);
}

static void testParseAnc()
{
    Packet p;
    p.command = resp::AncSecondary;
    p.payload = { 0x01, AncAdaptive };
    uint8_t code = 0;
    CHECK(parseAnc(p, code) && code == AncAdaptive);

    p.command = resp::AncPrimary;
    p.payload = { 0x01, AncOff, 0x00 };
    CHECK(parseAnc(p, code) && code == AncOff);

    p.payload = { 0x01, 0x06 }; // unknown value
    CHECK(!parseAnc(p, code));

    p.command = 0x4007; // some other notification
    p.payload = { 0x01, AncHigh };
    CHECK(!parseAnc(p, code));
}

static void testParseBattery()
{
    Packet p;
    p.command = resp::BatteryPrimary;
    // 3 components: left 80 %, right 75 % charging, case 50 %
    p.payload = { 0x03, 0x02, 80, 0x03, 0x80 | 75, 0x04, 50 };
    BatteryStatus s;
    CHECK(parseBattery(p, s));
    CHECK(s.left.present && s.left.percent == 80 && !s.left.charging);
    CHECK(s.right.present && s.right.percent == 75 && s.right.charging);
    CHECK(s.caseBattery.present && s.caseBattery.percent == 50);

    // only the left bud reported, truncated trailing entry ignored
    p.command = resp::BatterySecondary;
    p.payload = { 0x02, 0x02, 0x80 | 20, 0x04 };
    CHECK(parseBattery(p, s));
    CHECK(s.left.present && s.left.percent == 20 && s.left.charging);
    CHECK(!s.right.present && !s.caseBattery.present);

    // over-ear headphones: one battery with id 0x06
    p.command = resp::BatteryPrimary;
    p.payload = { 0x01, 0x06, 0x80 | 64 };
    CHECK(parseBattery(p, s));
    CHECK(s.single.present && s.single.percent == 64 && s.single.charging);
    CHECK(!s.left.present && !s.right.present && !s.caseBattery.present);

    p.command = resp::AncSecondary;
    CHECK(!parseBattery(p, s));
}

static Packet makePacket(uint16_t command, std::vector<uint8_t> payload)
{
    Packet p;
    p.command = command;
    p.payload = payload;
    return p;
}

static void testSimpleStates()
{
    bool on = false;
    CHECK(parseInEar(makePacket(resp::InEar, { 0x01, 0x01, 0x01 }), on) && on);
    CHECK(parseInEar(makePacket(resp::InEar, { 0x01, 0x01, 0x00 }), on) && !on);
    CHECK(!parseInEar(makePacket(resp::InEar, { 0x01 }), on));
    CHECK(parseLatency(makePacket(resp::Latency, { 0x01 }), on) && on);
    CHECK(parseLatency(makePacket(resp::Latency, { 0x02 }), on) && !on);
    CHECK(parseSuperMic(makePacket(resp::SuperMic, { 0x01 }), on) && on);
    CHECK(parsePersonalizedAnc(makePacket(resp::PersonalizedAnc, { 0x00 }), on) && !on);
    CHECK(!parseSuperMic(makePacket(resp::Latency, { 0x01 }), on));

    uint8_t level = 0;
    CHECK(parseEnhancedBass(makePacket(resp::EnhancedBass, { 0x01, 0x06 }), on, level));
    CHECK(on && level == 3);

    CHECK(setInEarPayload(true) == std::vector<uint8_t>({ 0x01, 0x01, 0x01 }));
    CHECK(setLatencyPayload(true) == std::vector<uint8_t>({ 0x01, 0x00 }));
    CHECK(setLatencyPayload(false) == std::vector<uint8_t>({ 0x02, 0x00 }));
    CHECK(setEnhancedBassPayload(true, 4) == std::vector<uint8_t>({ 0x01, 0x08 }));
    CHECK(setSpatialAudioPayload(true) == std::vector<uint8_t>({ 0x01, 0x00 }));
    CHECK(ringPayload(DeviceLeft, true) == std::vector<uint8_t>({ 0x02, 0x01 }));
}

static void testGestures()
{
    std::vector<GestureSlot> slots;
    Packet p = makePacket(resp::Gestures, { 0x02, 0x02, 0x01, 0x02, 0x08, 0x03, 0x01, 0x07, 0x16, 0x02 });
    CHECK(parseGestures(p, slots));
    CHECK(slots.size() == 2);
    CHECK(slots[0].device == DeviceLeft && slots[0].type == GestureDouble && slots[0].action == ActionSkipBack);
    CHECK(slots[1].device == DeviceRight && slots[1].type == GestureHold
          && slots[1].action == ActionNoiseControlAncTransp);

    GestureSlot slot;
    slot.device = DeviceRight;
    slot.common = 1;
    slot.type = GestureTriple;
    slot.action = ActionVoiceAssistant;
    CHECK(setGesturePayload(slot) == std::vector<uint8_t>({ 0x01, 0x03, 0x01, 0x03, 0x0B }));
}

static void testFirmwareFitSerial()
{
    std::string version;
    CHECK(parseFirmware(makePacket(resp::Firmware, { '1', '.', '0', '.', '2', ' ' }), version));
    CHECK(version == "1.0.2");

    uint8_t left = 9, right = 9;
    CHECK(parseFitResult(makePacket(resp::FitTestResult, { FitGood, FitAdjust }), left, right));
    CHECK(left == FitGood && right == FitAdjust);

    const std::string text = "1,1,abc\n2,4,SH1021ABCDEF1234\n3,5,x";
    std::vector<uint8_t> payload(7, 0);
    payload.insert(payload.end(), text.begin(), text.end());
    std::string serial;
    CHECK(parseSerial(makePacket(resp::Serial, payload), serial));
    CHECK(serial == "SH1021ABCDEF1234");
    CHECK(skuFromSerial(serial) == "21");
    CHECK(skuFromSerial("MA01AB24XYZ") == "11200005");
    CHECK(skuFromSerial("XY") == "");
}

static void testDualConnection()
{
    bool on = false;
    CHECK(parseDualEnable(makePacket(resp::DualEnable, { 0x01 }), on) && on);

    // two devices: this phone (self, connected) "Jolla", other "Pixel" not connected
    std::vector<uint8_t> payload{ 0x00, 0x00, 0x02,
        0x11, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 5, 'J', 'o', 'l', 'l', 'a',
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x80 | 5, 'P', 'i', 'x', 'e', 'l' };
    std::vector<DualDevice> devices;
    CHECK(parseDualList(makePacket(resp::DualList, payload), devices));
    CHECK(devices.size() == 2);
    CHECK(devices[0].self && devices[0].connected && devices[0].name == "Jolla" && devices[0].mac[0] == 0xAA);
    CHECK(!devices[1].self && !devices[1].connected && devices[1].name == "Pixel" && devices[1].mac[5] == 0x06);

    uint8_t mac[6];
    CHECK(parseDualDeviceEvent(makePacket(resp::DualDeviceEvent, { 0x01, 1, 2, 3, 4, 5, 6 }), on, mac));
    CHECK(on && mac[0] == 1 && mac[5] == 6);
    CHECK(dualConnectPayload(mac, false) == std::vector<uint8_t>({ 0x00, 1, 2, 3, 4, 5, 6 }));
}

int main()
{
    testCrcKnownValue();
    testRoundTrip();
    testFragmentedAndGarbage();
    testBadCrc();
    testSetAnc();
    testParseAnc();
    testParseBattery();
    testSimpleStates();
    testGestures();
    testFirmwareFitSerial();
    testDualConnection();
    if (failures) {
        std::printf("%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("All protocol tests passed\n");
    return EXIT_SUCCESS;
}
