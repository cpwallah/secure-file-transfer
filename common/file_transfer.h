#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include <string>
#include <vector>
#include <fstream>
#include "network_utils.h"
#include "crypto_utils.h"

class FileTransfer
{
public:
    static bool sendFile(SOCKET socket, const std::vector<BYTE> &key, const std::vector<BYTE> &iv, const std::string &filePath);
    static bool receiveFile(SOCKET socket, const std::vector<BYTE> &key, const std::vector<BYTE> &iv, const std::string &saveDir = "received_files");
};

#endif