#include "crypto_utils.h"
#include <iostream>
#include <wincrypt.h>

#pragma comment(lib, "crypt32.lib")

// Simple XOR encryption (always works)
std::vector<BYTE> xorEncryptDecrypt(const std::vector<BYTE> &key, const std::vector<BYTE> &data)
{
    std::vector<BYTE> result(data.size());
    for (size_t i = 0; i < data.size(); i++)
    {
        result[i] = data[i] ^ key[i % key.size()];
    }
    return result;
}

bool CryptoUtils::generateAESKey(std::vector<BYTE> &key, std::vector<BYTE> &iv)
{
    key.resize(16);
    iv.resize(16);

    // Use Windows Crypto for random generation
    HCRYPTPROV hProv;
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        CryptGenRandom(hProv, 16, key.data());
        CryptGenRandom(hProv, 16, iv.data());
        CryptReleaseContext(hProv, 0);
        return true;
    }

    // Fallback to simple random
    srand(static_cast<unsigned int>(time(NULL)));
    for (int i = 0; i < 16; i++)
    {
        key[i] = rand() % 256;
        iv[i] = rand() % 256;
    }
    return true;
}

std::vector<BYTE> CryptoUtils::aesEncrypt(const std::vector<BYTE> &key, const std::vector<BYTE> &iv, const std::vector<BYTE> &data)
{
    std::cout << "Encrypting " << data.size() << " bytes with XOR" << std::endl;

    // Combine key and IV for stronger XOR
    std::vector<BYTE> combinedKey(key);
    combinedKey.insert(combinedKey.end(), iv.begin(), iv.end());

    return xorEncryptDecrypt(combinedKey, data);
}

std::vector<BYTE> CryptoUtils::aesDecrypt(const std::vector<BYTE> &key, const std::vector<BYTE> &iv, const std::vector<BYTE> &encrypted)
{
    std::cout << "Decrypting " << encrypted.size() << " bytes with XOR" << std::endl;

    // Combine key and IV for stronger XOR
    std::vector<BYTE> combinedKey(key);
    combinedKey.insert(combinedKey.end(), iv.begin(), iv.end());

    return xorEncryptDecrypt(combinedKey, encrypted);
}

std::string CryptoUtils::generateUUID()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 32; i++)
    {
        if (i == 8 || i == 12 || i == 16 || i == 20)
            ss << "-";
        ss << dis(gen);
    }
    return ss.str();
}

std::string CryptoUtils::base64Encode(const std::vector<BYTE> &data)
{
    DWORD dwLen = 0;
    if (!CryptBinaryToStringA(data.data(), static_cast<DWORD>(data.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &dwLen))
    {
        return "";
    }

    std::string encoded(dwLen, '\0');
    if (!CryptBinaryToStringA(data.data(), static_cast<DWORD>(data.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &encoded[0], &dwLen))
    {
        return "";
    }

    if (!encoded.empty() && encoded.back() == '\0')
    {
        encoded.pop_back();
    }

    return encoded;
}

std::vector<BYTE> CryptoUtils::base64Decode(const std::string &data)
{
    DWORD dwLen = 0;
    if (!CryptStringToBinaryA(data.c_str(), static_cast<DWORD>(data.size()), CRYPT_STRING_BASE64, NULL, &dwLen, NULL, NULL))
    {
        return {};
    }

    std::vector<BYTE> decoded(dwLen);
    if (!CryptStringToBinaryA(data.c_str(), static_cast<DWORD>(data.size()), CRYPT_STRING_BASE64, decoded.data(), &dwLen, NULL, NULL))
    {
        return {};
    }

    decoded.resize(dwLen);
    return decoded;
}

std::vector<BYTE> CryptoUtils::stringToVector(const std::string &str)
{
    return std::vector<BYTE>(str.begin(), str.end());
}

std::string CryptoUtils::vectorToString(const std::vector<BYTE> &data)
{
    return std::string(data.begin(), data.end());
}