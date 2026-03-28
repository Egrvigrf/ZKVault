#include "crypto/hex.hpp"

#include <stdexcept>

namespace {

unsigned char HexCharToValue(char c) {
    if (c >= '0' && c <= '9') {
        return static_cast<unsigned char>(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return static_cast<unsigned char>(c - 'a' + 10);
    }
    if (c >= 'A' && c <= 'F') {
        return static_cast<unsigned char>(c - 'A' + 10);
    }

    throw std::runtime_error("invalid hex character");
}

}  // namespace

std::string BytesToHex(const std::vector<unsigned char>& bytes) {
    static const char kHexDigits[] = "0123456789abcdef";

    std::string hex;
    hex.reserve(bytes.size() * 2);

    for (unsigned char byte : bytes) {
        hex.push_back(kHexDigits[byte >> 4]);
        hex.push_back(kHexDigits[byte & 0x0f]);
    }

    return hex;
}

std::vector<unsigned char> HexToBytes(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("hex string must have even length");
    }

    std::vector<unsigned char> bytes;
    bytes.reserve(hex.size() / 2);

    for (std::size_t i = 0; i < hex.size(); i += 2) {
        unsigned char high = HexCharToValue(hex[i]);
        unsigned char low = HexCharToValue(hex[i + 1]);
        bytes.push_back(static_cast<unsigned char>((high << 4) | low));
    }

    return bytes;
}
