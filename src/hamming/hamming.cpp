#include "hamming.h"

#include <cstdint>
#include <vector>

uint8_t CalculateParityBits(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4) {
    uint8_t p1 = d1 ^ d2 ^ d4;
    uint8_t p2 = d1 ^ d3 ^ d4;
    uint8_t p4 = d2 ^ d3 ^ d4;
    return (p1 << 1) | (p2 << 2) | (p4 << 4);
}

uint8_t CalculateOverallParity(uint8_t code) {
    uint8_t overall = 0;
    while (code) {
        overall ^= code & 1;
        code >>= 1;
    }
    return overall;
}

uint8_t EncodeNibble(uint8_t nibble) {
    uint8_t d1 = (nibble >> 0) & 1;
    uint8_t d2 = (nibble >> 1) & 1;
    uint8_t d3 = (nibble >> 2) & 1;
    uint8_t d4 = (nibble >> 3) & 1;
    uint8_t parity = CalculateParityBits(d1, d2, d3, d4);
    uint8_t code = parity | (d1 << 3) | (d2 << 5) | (d3 << 6) | (d4 << 7);
    return code | CalculateOverallParity(code);
}

std::vector<uint8_t> Encode(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> out;
    out.reserve(data.size() * 2);
    for (uint8_t byte : data) {
        out.push_back(EncodeNibble(byte & 0x0F));
        out.push_back(EncodeNibble(byte >> 4));
    }
    return out;
}

DecodedBits ExtractBits(uint8_t code) {
    DecodedBits bits{};
    bits.p0 = (code >> 0) & 1;
    bits.p1 = (code >> 1) & 1;
    bits.p2 = (code >> 2) & 1;
    bits.d1 = (code >> 3) & 1;
    bits.p4 = (code >> 4) & 1;
    bits.d2 = (code >> 5) & 1;
    bits.d3 = (code >> 6) & 1;
    bits.d4 = (code >> 7) & 1;
    return bits;
}

uint8_t CalculateSyndrome(const DecodedBits& bits) {
    uint8_t s1 = bits.p1 ^ bits.d1 ^ bits.d2 ^ bits.d4;
    uint8_t s2 = bits.p2 ^ bits.d1 ^ bits.d3 ^ bits.d4;
    uint8_t s4 = bits.p4 ^ bits.d2 ^ bits.d3 ^ bits.d4;
    return (s4 << 2) | (s2 << 1) | s1;
}

uint8_t BuildNibble(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4) {
    return (d4 << 3) | (d3 << 2) | (d2 << 1) | d1;
}

int DecodeNibble(uint8_t code, uint8_t& corrected) {
    DecodedBits bits = ExtractBits(code);
    uint8_t syndrome = CalculateSyndrome(bits);
    uint8_t overall = bits.p0 ^ bits.p1 ^ bits.p2 ^ bits.d1 ^ bits.p4 ^ bits.d2 ^ bits.d3 ^ bits.d4;

    if (syndrome) {
        code ^= (1 << (syndrome - 1));
    }
    
    bits = ExtractBits(code);
    
    corrected = BuildNibble(bits.d1, bits.d2, bits.d3, bits.d4);
    return overall;
}

uint8_t DecodeBytePair(uint8_t low_code, uint8_t high_code) {
    uint8_t low = 0, high = 0;
    DecodeNibble(low_code, low);
    DecodeNibble(high_code, high);
    return (high << 4) | low;
}

std::vector<uint8_t> Decode(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> out;
    out.reserve(data.size() / 2);
    for (size_t i = 0; i < data.size(); i += 2) {
        out.push_back(DecodeBytePair(data[i], data[i + 1]));
    }
    return out;
}