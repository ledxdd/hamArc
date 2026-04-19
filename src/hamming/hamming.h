#ifndef HAMMING_H
#define HAMMING_H

#include <vector>
#include <cstdint>

// Структура для представления отдельных бит (нужна, если вы планируете 
// использовать ExtractBits вне этого модуля)
struct DecodedBits {
    uint8_t p0, p1, p2, d1, p4, d2, d3, d4;
};

// Основные функции кодирования и декодирования
std::vector<uint8_t> Encode(const std::vector<uint8_t>& data);
std::vector<uint8_t> Decode(const std::vector<uint8_t>& data);

// Вспомогательные функции (интерфейс низкого уровня)
uint8_t EncodeNibble(uint8_t nibble);
int DecodeNibble(uint8_t code, uint8_t& corrected);
uint8_t DecodeBytePair(uint8_t low_code, uint8_t high_code);

// Математические функции (обычно их не выносят в .h, но если нужно для тестов):
uint8_t CalculateParityBits(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4);
uint8_t CalculateOverallParity(uint8_t code);
uint8_t CalculateSyndrome(const DecodedBits& bits);
DecodedBits ExtractBits(uint8_t code);
uint8_t BuildNibble(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4);

#endif // HAMMING_H