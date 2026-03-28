#pragma once

#include <vector>

struct AeadCiphertext {
    std::vector<unsigned char> iv;
    std::vector<unsigned char> ciphertext;
    std::vector<unsigned char> auth_tag;
};

AeadCiphertext EncryptAes256Gcm(
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& plaintext);

std::vector<unsigned char> DecryptAes256Gcm(
    const std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& ciphertext,
    const std::vector<unsigned char>& auth_tag);
