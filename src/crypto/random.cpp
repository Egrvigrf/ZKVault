#include "crypto/random.hpp"

#include <openssl/rand.h>

#include <stdexcept>

std::vector<unsigned char> GenerateRandomBytes(std::size_t size) {
    std::vector<unsigned char> bytes(size);
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }

    return bytes;
}
