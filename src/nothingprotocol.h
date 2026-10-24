/*
 * harbour-pinch - ANC control for Nothing earbuds on Sailfish OS
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Plain C++ (no Qt) implementation of the Nothing earbud serial protocol,
 * so it can be unit-tested on any desktop with a bare compiler.
 *
 * Wire format (all multi-byte values little endian):
 *
 *   0x55 0x60 0x01 | cmd lo | cmd hi | len | 0x00 | op id | payload[len] | crc lo | crc hi
 *
 * The CRC is CRC-16/MODBUS (init 0xFFFF, poly 0xA001 reflected) over
 * everything before the CRC. Protocol knowledge comes from the community
 * projects ear-web and earctl; this is an independent implementation.
 */

#ifndef NOTHINGPROTOCOL_H
#define NOTHINGPROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using std::size_t;

namespace nothing {

// RFCOMM service class UUID the earbuds expose for the control channel.
static const char *const SppServiceUuid = "aeac4a03-dff5-498f-843a-34487cf133eb";

namespace cmd {
static const uint16_t RequestSerial = 0xC006;
static const uint16_t RequestBattery = 0xC007;
static const uint16_t RequestInEar = 0xC00E;
static const uint16_t RequestGestures = 0xC018;
static const uint16_t RequestAnc = 0xC01E;
static const uint16_t RequestPersonalizedAnc = 0xC020;
static const uint16_t RequestDualEnable = 0xC027;
static const uint16_t RequestDualList = 0xC028;
static const uint16_t RequestLatency = 0xC041;
static const uint16_t RequestFirmware = 0xC042;
static const uint16_t RequestEnhancedBass = 0xC04E;
static const uint16_t RequestSuperMic = 0xC05E;

static const uint16_t Ring = 0xF002;
static const uint16_t SetGesture = 0xF003;
static const uint16_t SetInEar = 0xF004;
static const uint16_t SetAnc = 0xF00F;
static const uint16_t SetPersonalizedAnc = 0xF011;
static const uint16_t StartFitTest = 0xF014;
static const uint16_t SetDualEnable = 0xF01A;
static const uint16_t SetDualConnect = 0xF01B;
static const uint16_t SetLatency = 0xF040;
static const uint16_t SetEnhancedBass = 0xF051;
static const uint16_t SetSpatialAudio = 0xF052;
static const uint16_t SetSuperMic = 0xF05F;
}

namespace resp {
// The earbuds answer a RequestAnc with one of these, and also push them
// unsolicited when the mode is changed on the buds (pinch gesture).
static const uint16_t AncPrimary = 0xE003;
static const uint16_t AncSecondary = 0x401E;
// Battery report, sent as answer to RequestBattery and unsolicited on change.
static const uint16_t BatteryPrimary = 0xE001;
static const uint16_t BatterySecondary = 0x4007;
static const uint16_t Serial = 0x4006;
static const uint16_t InEar = 0x400E;
static const uint16_t Gestures = 0x4018;
static const uint16_t PersonalizedAnc = 0x4020;
static const uint16_t Latency = 0x4041;
static const uint16_t Firmware = 0x4042;
static const uint16_t EnhancedBass = 0x404E;
static const uint16_t SuperMic = 0x405E;
static const uint16_t FitTestResult = 0xE00D;
static const uint16_t DualEnable = 0x4027;
static const uint16_t DualList = 0x4028;
static const uint16_t DualDeviceEvent = 0xE00E;
}

// Device ids used by battery reports, gestures and the ring command.
enum DeviceId : uint8_t {
    DeviceLeft = 0x02,
    DeviceRight = 0x03,
    DeviceCase = 0x04
};

// Gesture types (byte 3 of a gesture slot).
enum GestureType : uint8_t {
    GestureDouble = 2,
    GestureTriple = 3,
    GestureHold = 7,
    GestureDoubleHold = 9
};

// Gesture actions. 10/20/21/22 are all "noise control"; they differ in the
// modes the gesture cycles through.
enum GestureAction : uint8_t {
    ActionNone = 1,
    ActionSkipBack = 8,
    ActionSkipForward = 9,
    ActionNoiseControlAll = 10,      // noise cancellation, transparency, off
    ActionVoiceAssistant = 11,
    ActionVolumeUp = 18,
    ActionVolumeDown = 19,
    ActionNoiseControlAncOff = 20,   // noise cancellation, off
    ActionNoiseControlTranspOff = 21,// transparency, off
    ActionNoiseControlAncTransp = 22,// noise cancellation, transparency
    ActionMicToggle = 29
};

// Ear tip fit test result per side.
enum FitResult : uint8_t {
    FitGood = 0,
    FitAdjust = 1,
    FitNotDetected = 2
};

// Device-side ANC codes. Values come straight from the protocol.
enum AncCode : uint8_t {
    AncHigh = 0x01,
    AncMid = 0x02,
    AncLow = 0x03,
    AncAdaptive = 0x04,
    AncOff = 0x05,
    AncTransparency = 0x07
};

bool isValidAncCode(uint8_t code);

struct Packet {
    uint16_t command = 0;
    uint8_t operationId = 0;
    std::vector<uint8_t> payload;
};

uint16_t crc16(const uint8_t *data, size_t length);

std::vector<uint8_t> encode(uint16_t command, uint8_t operationId,
                            const std::vector<uint8_t> &payload);

enum class ParseResult {
    NeedMoreData, // buffer holds no complete packet yet
    Ok,           // a packet was removed from the buffer into 'out'
    CrcError      // a complete packet with a bad CRC was dropped
};

// Consumes at most one packet from the front of 'buffer'. Garbage in front
// of a header is discarded. Call repeatedly until NeedMoreData.
ParseResult tryParse(std::vector<uint8_t> &buffer, Packet &out);

struct BatteryReading {
    bool present = false; // not reported, e.g. bud out of range or case closed
    uint8_t percent = 0;
    bool charging = false;
};

struct BatteryStatus {
    BatteryReading left;
    BatteryReading right;
    BatteryReading caseBattery;
    BatteryReading single; // over-ear headphones report one battery (id 0x06)
};

// Payload: count, then per component (id, level). Ids 0x02 left, 0x03 right,
// 0x04 case. Level bit 7 = charging, bits 0-6 = percent.
// Returns false if the packet is not a battery report.
bool parseBattery(const Packet &packet, BatteryStatus &status);

struct GestureSlot {
    uint8_t device = 0;
    uint8_t common = 1; // purpose unknown, echoed back unchanged when setting
    uint8_t type = 0;
    uint8_t action = 0;
};

// Payload: count, then per slot (device, common, type, action).
bool parseGestures(const Packet &packet, std::vector<GestureSlot> &out);
std::vector<uint8_t> setGesturePayload(const GestureSlot &slot);

// Simple on/off states. Each returns false if the packet is not the
// matching report.
bool parseInEar(const Packet &packet, bool &enabled);          // payload[2]
bool parseLatency(const Packet &packet, bool &lowLatency);     // payload[0] == 1
bool parseSuperMic(const Packet &packet, bool &enabled);       // payload[0] == 1
bool parsePersonalizedAnc(const Packet &packet, bool &enabled);// payload[0] == 1
// Level 0..5 as shown in the app; the wire value is level * 2.
bool parseEnhancedBass(const Packet &packet, bool &enabled, uint8_t &level);
bool parseFirmware(const Packet &packet, std::string &version);
bool parseFitResult(const Packet &packet, uint8_t &left, uint8_t &right);

// Serial report: text after byte 7, lines "x,<field>,<value>"; field 4 is
// the serial number.
bool parseSerial(const Packet &packet, std::string &serial);
// The model's SKU as encoded in the serial number, empty if unknown.
std::string skuFromSerial(const std::string &serial);

// Dual connection (multipoint).
struct DualDevice {
    uint8_t mac[6] = { 0, 0, 0, 0, 0, 0 }; // in display order
    std::string name;
    bool self = false;      // the device this app runs on
    bool connected = false;
};

bool parseDualEnable(const Packet &packet, bool &enabled); // payload[0] == 1
// One page of remembered devices: payload[2] = count, then per device
// flags (high nibble self, low nibble connected), MAC (6), name length
// (bits 0-6), name (UTF-8).
bool parseDualList(const Packet &packet, std::vector<DualDevice> &devices);
// A remembered device connected or disconnected: payload[0] state, [1..6] MAC.
bool parseDualDeviceEvent(const Packet &packet, bool &connected, uint8_t mac[6]);
std::vector<uint8_t> dualConnectPayload(const uint8_t mac[6], bool connect);

std::vector<uint8_t> setInEarPayload(bool enabled);
std::vector<uint8_t> setLatencyPayload(bool lowLatency);
std::vector<uint8_t> setEnhancedBassPayload(bool enabled, uint8_t level);
std::vector<uint8_t> setSpatialAudioPayload(bool enabled);
std::vector<uint8_t> ringPayload(uint8_t device, bool on);

// Payload for SetAnc.
std::vector<uint8_t> setAncPayload(uint8_t ancCode);

// Extracts the ANC code from an ANC response/notification. Returns false if
// the packet is not an ANC packet or carries an unknown value.
bool parseAnc(const Packet &packet, uint8_t &ancCode);

} // namespace nothing

#endif // NOTHINGPROTOCOL_H
