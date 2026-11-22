/*
 * harbour-pinch - ANC control for Nothing earbuds on Sailfish OS
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nothingprotocol.h"

#include <algorithm>
#include <cstring>

namespace nothing {

namespace {
const uint8_t Magic[3] = { 0x55, 0x60, 0x01 };
const size_t HeaderLength = 8;
const size_t CrcLength = 2;
}

bool isValidAncCode(uint8_t code)
{
    switch (code) {
    case AncHigh:
    case AncMid:
    case AncLow:
    case AncAdaptive:
    case AncOff:
    case AncTransparency:
        return true;
    default:
        return false;
    }
}

uint16_t crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 1)
                crc = static_cast<uint16_t>((crc >> 1) ^ 0xA001);
            else
                crc >>= 1;
        }
    }
    return crc;
}

std::vector<uint8_t> encode(uint16_t command, uint8_t operationId,
                            const std::vector<uint8_t> &payload)
{
    std::vector<uint8_t> packet;
    packet.reserve(HeaderLength + payload.size() + CrcLength);
    packet.insert(packet.end(), Magic, Magic + 3);
    packet.push_back(static_cast<uint8_t>(command & 0xFF));
    packet.push_back(static_cast<uint8_t>(command >> 8));
    packet.push_back(static_cast<uint8_t>(payload.size()));
    packet.push_back(0x00);
    packet.push_back(operationId);
    packet.insert(packet.end(), payload.begin(), payload.end());
    const uint16_t crc = crc16(packet.data(), packet.size());
    packet.push_back(static_cast<uint8_t>(crc & 0xFF));
    packet.push_back(static_cast<uint8_t>(crc >> 8));
    return packet;
}

ParseResult tryParse(std::vector<uint8_t> &buffer, Packet &out)
{
    for (;;) {
        // Drop everything before the first possible header byte.
        size_t start = 0;
        while (start < buffer.size() && buffer[start] != Magic[0])
            ++start;
        buffer.erase(buffer.begin(), buffer.begin() + static_cast<long>(start));

        if (buffer.size() < HeaderLength)
            return ParseResult::NeedMoreData;

        if (buffer[1] != Magic[1] || buffer[2] != Magic[2]) {
            // False start, skip this byte and search again.
            buffer.erase(buffer.begin());
            continue;
        }

        const size_t payloadLength = buffer[5];
        const size_t total = HeaderLength + payloadLength + CrcLength;
        if (buffer.size() < total)
            return ParseResult::NeedMoreData;

        const uint16_t expected = static_cast<uint16_t>(buffer[total - 2] | (buffer[total - 1] << 8));
        const uint16_t actual = crc16(buffer.data(), total - CrcLength);

        if (expected != actual) {
            buffer.erase(buffer.begin(), buffer.begin() + static_cast<long>(total));
            return ParseResult::CrcError;
        }

        out.command = static_cast<uint16_t>(buffer[3] | (buffer[4] << 8));
        out.operationId = buffer[7];
        out.payload.assign(buffer.begin() + HeaderLength,
                           buffer.begin() + static_cast<long>(HeaderLength + payloadLength));
        buffer.erase(buffer.begin(), buffer.begin() + static_cast<long>(total));
        return ParseResult::Ok;
    }
}

std::vector<uint8_t> setAncPayload(uint8_t ancCode)
{
    return std::vector<uint8_t>{ 0x01, ancCode, 0x00 };
}

bool parseAnc(const Packet &packet, uint8_t &ancCode)
{
    if (packet.command != resp::AncPrimary && packet.command != resp::AncSecondary)
        return false;
    if (packet.payload.size() < 2)
        return false;
    if (!isValidAncCode(packet.payload[1]))
        return false;
    ancCode = packet.payload[1];
    return true;
}

bool parseBattery(const Packet &packet, BatteryStatus &status)
{
    if (packet.command != resp::BatteryPrimary && packet.command != resp::BatterySecondary)
        return false;

    status = BatteryStatus();
    if (packet.payload.empty())
        return true;

    const size_t count = packet.payload[0];
    for (size_t i = 0; i < count; ++i) {
        const size_t index = 1 + i * 2;
        if (index + 1 >= packet.payload.size())
            break;
        const uint8_t id = packet.payload[index];
        const uint8_t level = packet.payload[index + 1];

        BatteryReading reading;
        reading.present = true;
        reading.percent = static_cast<uint8_t>(level & 0x7F);
        reading.charging = (level & 0x80) != 0;
        if (reading.percent > 100)
            reading.percent = 100;

        switch (id) {
        case 0x02: status.left = reading; break;
        case 0x03: status.right = reading; break;
        case 0x04: status.caseBattery = reading; break;
        case 0x06: status.single = reading; break; // over-ear headphones
        default: break;
        }
    }
    return true;
}

namespace {
bool boolAt(const Packet &packet, uint16_t command, size_t index, bool &value)
{
    if (packet.command != command || packet.payload.size() <= index)
        return false;
    value = packet.payload[index] == 1;
    return true;
}
}

bool parseGestures(const Packet &packet, std::vector<GestureSlot> &out)
{
    if (packet.command != resp::Gestures)
        return false;
    out.clear();
    if (packet.payload.empty())
        return true;
    const size_t count = packet.payload[0];
    for (size_t i = 0; i < count; ++i) {
        const size_t base = 1 + i * 4;
        if (base + 3 >= packet.payload.size())
            break;
        GestureSlot slot;
        slot.device = packet.payload[base];
        slot.common = packet.payload[base + 1];
        slot.type = packet.payload[base + 2];
        slot.action = packet.payload[base + 3];
        out.push_back(slot);
    }
    return true;
}

std::vector<uint8_t> setGesturePayload(const GestureSlot &slot)
{
    return std::vector<uint8_t>{ 0x01, slot.device, slot.common, slot.type, slot.action };
}

bool parseInEar(const Packet &packet, bool &enabled)
{
    return boolAt(packet, resp::InEar, 2, enabled);
}

bool parseLatency(const Packet &packet, bool &lowLatency)
{
    return boolAt(packet, resp::Latency, 0, lowLatency);
}

bool parseSuperMic(const Packet &packet, bool &enabled)
{
    return boolAt(packet, resp::SuperMic, 0, enabled);
}

bool parsePersonalizedAnc(const Packet &packet, bool &enabled)
{
    return boolAt(packet, resp::PersonalizedAnc, 0, enabled);
}

bool parseEnhancedBass(const Packet &packet, bool &enabled, uint8_t &level)
{
    if (packet.command != resp::EnhancedBass || packet.payload.size() < 2)
        return false;
    enabled = packet.payload[0] > 0;
    level = static_cast<uint8_t>(packet.payload[1] / 2);
    return true;
}

bool parseFirmware(const Packet &packet, std::string &version)
{
    if (packet.command != resp::Firmware)
        return false;
    version.clear();
    for (uint8_t c : packet.payload) {
        if (c >= 0x20 && c < 0x7F)
            version.push_back(static_cast<char>(c));
    }
    // trim spaces
    while (!version.empty() && version.back() == ' ')
        version.pop_back();
    while (!version.empty() && version.front() == ' ')
        version.erase(version.begin());
    return true;
}

bool parseFitResult(const Packet &packet, uint8_t &left, uint8_t &right)
{
    if (packet.command != resp::FitTestResult || packet.payload.size() < 2)
        return false;
    left = packet.payload[0];
    right = packet.payload[1];
    return true;
}

bool parseSerial(const Packet &packet, std::string &serial)
{
    if (packet.command != resp::Serial || packet.payload.size() < 8)
        return false;
    const std::string text(packet.payload.begin() + 7, packet.payload.end());
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // expect exactly three comma separated parts
        const size_t c1 = line.find(',');
        const size_t c2 = c1 == std::string::npos ? std::string::npos : line.find(',', c1 + 1);
        if (c2 != std::string::npos && line.find(',', c2 + 1) == std::string::npos) {
            std::string field = line.substr(c1 + 1, c2 - c1 - 1);
            std::string value = line.substr(c2 + 1);
            auto trim = [](std::string &v) {
                while (!v.empty() && (v.back() == ' ' || v.back() == '\0'))
                    v.pop_back();
                while (!v.empty() && v.front() == ' ')
                    v.erase(v.begin());
            };
            trim(field);
            trim(value);
            if (field == "4" && !value.empty()) {
                serial = value;
                return true;
            }
        }
        start = end + 1;
    }
    return false;
}

std::string skuFromSerial(const std::string &serial)
{
    if (serial == "12345678901234567")
        return "01";
    if (serial.size() < 6)
        return std::string();
    const std::string head = serial.substr(0, 2);
    if (head == "MA") {
        if (serial.size() < 8)
            return std::string();
        const std::string year = serial.substr(6, 2);
        if (year == "22" || year == "23")
            return "14";
        if (year == "24")
            return "11200005";
        return std::string();
    }
    if (head == "SH" || head == "13")
        return serial.substr(4, 2);
    return std::string();
}

bool parseDualEnable(const Packet &packet, bool &enabled)
{
    return boolAt(packet, resp::DualEnable, 0, enabled);
}

bool parseDualList(const Packet &packet, std::vector<DualDevice> &devices)
{
    if (packet.command != resp::DualList)
        return false;
    devices.clear();
    if (packet.payload.size() < 3)
        return true;
    const size_t count = packet.payload[2];
    size_t offset = 3;
    for (size_t i = 0; i < count; ++i) {
        if (offset + 8 > packet.payload.size())
            break;
        DualDevice d;
        const uint8_t flags = packet.payload[offset];
        d.self = (flags & 0xF0) != 0;
        d.connected = (flags & 0x0F) != 0;
        for (int b = 0; b < 6; ++b)
            d.mac[b] = packet.payload[offset + 1 + b];
        const size_t nameLength = packet.payload[offset + 7] & 0x7F;
        const size_t nameStart = offset + 8;
        const size_t nameEnd = std::min(nameStart + nameLength, packet.payload.size());
        d.name.assign(packet.payload.begin() + static_cast<long>(nameStart),
                      packet.payload.begin() + static_cast<long>(nameEnd));
        devices.push_back(d);
        offset = nameStart + nameLength;
    }
    return true;
}

bool parseDualDeviceEvent(const Packet &packet, bool &connected, uint8_t mac[6])
{
    if (packet.command != resp::DualDeviceEvent || packet.payload.size() < 7)
        return false;
    connected = packet.payload[0] == 1;
    for (int b = 0; b < 6; ++b)
        mac[b] = packet.payload[1 + b];
    return true;
}

std::vector<uint8_t> dualConnectPayload(const uint8_t mac[6], bool connect)
{
    std::vector<uint8_t> payload{ static_cast<uint8_t>(connect ? 1 : 0) };
    payload.insert(payload.end(), mac, mac + 6);
    return payload;
}

std::vector<uint8_t> setInEarPayload(bool enabled)
{
    return std::vector<uint8_t>{ 0x01, 0x01, static_cast<uint8_t>(enabled ? 0x01 : 0x00) };
}

std::vector<uint8_t> setLatencyPayload(bool lowLatency)
{
    return std::vector<uint8_t>{ static_cast<uint8_t>(lowLatency ? 0x01 : 0x02), 0x00 };
}

std::vector<uint8_t> setEnhancedBassPayload(bool enabled, uint8_t level)
{
    return std::vector<uint8_t>{ static_cast<uint8_t>(enabled ? 0x01 : 0x00),
                                 static_cast<uint8_t>(level * 2) };
}

std::vector<uint8_t> setSpatialAudioPayload(bool enabled)
{
    return std::vector<uint8_t>{ static_cast<uint8_t>(enabled ? 0x01 : 0x00), 0x00 };
}

std::vector<uint8_t> ringPayload(uint8_t device, bool on)
{
    return std::vector<uint8_t>{ device, static_cast<uint8_t>(on ? 0x01 : 0x00) };
}

bool parseEq(const Packet &packet, uint8_t &mode)
{
    if (packet.command != resp::Eq || packet.payload.empty())
        return false;
    mode = packet.payload[0];
    return true;
}

std::vector<uint8_t> setEqPayload(uint8_t mode)
{
    return std::vector<uint8_t>{ mode, 0x00 };
}

namespace {
// Custom EQ values are encoded as little-endian IEEE 754 floats, with one
// quirk ported verbatim from earctl: while writing the packet's "total
// gain" field, an exact zero is marked by setting the top bit of the last
// (most significant, after the little-endian swap) byte instead of using
// all-zero bytes. encodeEqFloat/decodeEqFloat keep that behaviour so the
// packet matches what earctl sends, without claiming to know why the
// firmware wants it that way.
std::vector<uint8_t> encodeEqFloat(float value, bool isTotalGain)
{
    if (isTotalGain && value >= 0.0f)
        return std::vector<uint8_t>{ 0x00, 0x00, 0x00, 0x80 };
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    uint8_t b[4] = {
        static_cast<uint8_t>((bits >> 24) & 0xFF),
        static_cast<uint8_t>((bits >> 16) & 0xFF),
        static_cast<uint8_t>((bits >> 8) & 0xFF),
        static_cast<uint8_t>(bits & 0xFF)
    };
    if (value != 0.0f && b[0] == 0 && b[1] == 0 && b[2] == 0)
        b[3] |= 0x80;
    // Reverse big-endian -> little-endian for the wire.
    return std::vector<uint8_t>{ b[3], b[2], b[1], b[0] };
}

float decodeEqFloat(const uint8_t *bytes)
{
    uint8_t s[4] = { bytes[3], bytes[2], bytes[1], bytes[0] };
    bool negate = false;
    if (s[0] == 0 && s[1] == 0 && s[2] == 0 && (s[3] & 0x80)) {
        s[3] &= 0x7F;
        negate = true;
    }
    const uint32_t bits = (static_cast<uint32_t>(s[0]) << 24) | (static_cast<uint32_t>(s[1]) << 16)
                         | (static_cast<uint32_t>(s[2]) << 8) | static_cast<uint32_t>(s[3]);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return negate ? -value : value;
}
}

bool parseCustomEq(const Packet &packet, CustomEq &eq)
{
    if (packet.command != resp::CustomEq || packet.payload.size() < 36)
        return false;
    // Layout: count(1) totalGain(4) then 3 bands of 13 bytes (filter type,
    // gain as float, 8 bytes unused by us); band order on the wire is
    // mid, treble, bass.
    float bands[3];
    for (int i = 0; i < 3; ++i) {
        const size_t offset = 6 + static_cast<size_t>(i) * 13;
        if (offset + 4 > packet.payload.size())
            return false;
        bands[i] = decodeEqFloat(&packet.payload[offset]);
    }
    eq.mid = bands[0];
    eq.treble = bands[1];
    eq.bass = bands[2];
    return true;
}

std::vector<uint8_t> setCustomEqPayload(const CustomEq &eq)
{
    // Fixed packet template for a 3-band EQ, ported from earctl: filter
    // type and a fixed frequency/Q per band; only the gain floats and the
    // total-gain field below are replaced.
    std::vector<uint8_t> payload{
        0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x75, 0x44, 0xc3,
        0xf5, 0x28, 0x3f, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x5a, 0x45, 0x00, 0x00, 0x80,
        0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x43, 0xcd, 0xcc, 0x4c, 0x3f, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    const float values[3] = { eq.mid, eq.treble, eq.bass };
    float highest = 0.0f;
    for (float v : values) {
        const float a = v < 0.0f ? -v : v;
        if (a > highest)
            highest = a;
    }
    const std::vector<uint8_t> total = encodeEqFloat(-highest, true);
    std::copy(total.begin(), total.end(), payload.begin() + 1);
    for (int i = 0; i < 3; ++i) {
        const std::vector<uint8_t> bytes = encodeEqFloat(values[i], false);
        std::copy(bytes.begin(), bytes.end(), payload.begin() + 6 + i * 13);
    }
    return payload;
}

} // namespace nothing
