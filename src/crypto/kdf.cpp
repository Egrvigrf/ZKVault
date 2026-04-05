#include "crypto/kdf.hpp"
#include "crypto/secure_memory.hpp"

#include <openssl/evp.h>

#include <stdexcept>

std::vector<unsigned char> DeriveKeyScrypt(
    const std::string& password,
    const std::vector<unsigned char>& salt,
    std::size_t key_size) {
    constexpr std::size_t kScryptMaxMem = 64 * 1024 * 1024;

    std::vector<unsigned char> key(key_size);
    auto key_guard = MakeScopedCleanse(key);

    if (EVP_PBE_scrypt(password.c_str(),
                       password.size(),
                       salt.data(),
                       salt.size(),
                       1 << 15,
                       8,
                       1,
                       kScryptMaxMem,
                       key.data(),
                       key.size()) != 1) {
        throw std::runtime_error("EVP_PBE_scrypt failed");
    }

    key_guard.Release();
    return key;
}
