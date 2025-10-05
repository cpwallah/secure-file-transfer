#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <windows.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "bcrypt.lib")

class CryptoUtils
{
public:
    static bool generateAESKey(std::vector<BYTE> &key, std::vector<BYTE> &iv);
    static std::vector<BYTE> aesEncrypt(const std::vector<BYTE> &key, const std::vector<BYTE> &iv, const std::vector<BYTE> &data);
    static std::vector<BYTE> aesDecrypt(const std::vector<BYTE> &key, const std::vector<BYTE> &iv, const std::vector<BYTE> &encrypted);
    static std::string generateUUID();
    static std::string base64Encode(const std::vector<BYTE> &data);
    static std::vector<BYTE> base64Decode(const std::string &data);
    static std::vector<BYTE> stringToVector(const std::string &str);
    static std::string vectorToString(const std::vector<BYTE> &data);

private:
    static bool initializeBCrypt();
};

#endif