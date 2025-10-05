#include "file_transfer.h"
#include <filesystem>

namespace fs = std::filesystem;

bool FileTransfer::sendFile(SOCKET socket, const std::vector<BYTE> &key, const std::vector<BYTE> &iv, const std::string &filePath)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        NetworkUtils::printMessage("ERROR", "Cannot open file: " + filePath);
        return false;
    }

    uint64_t fileSize = file.tellg();
    file.seekg(0);
    std::string fileName = fs::path(filePath).filename().string();

    NetworkUtils::printMessage("SENDING", "File: " + fileName + " (" + std::to_string(fileSize) + " bytes)");

    // Send file info
    std::vector<BYTE> fileInfo;
    uint32_t nameSize = fileName.size();
    fileInfo.insert(fileInfo.end(), (BYTE *)&nameSize, (BYTE *)&nameSize + sizeof(nameSize));
    fileInfo.insert(fileInfo.end(), fileName.begin(), fileName.end());
    fileInfo.insert(fileInfo.end(), (BYTE *)&fileSize, (BYTE *)&fileSize + sizeof(fileSize));

    auto encryptedInfo = CryptoUtils::aesEncrypt(key, iv, fileInfo);
    if (!NetworkUtils::sendData(socket, encryptedInfo))
    {
        NetworkUtils::printMessage("ERROR", "Failed to send file info");
        return false;
    }

    // Send file data in chunks
    const uint32_t CHUNK_SIZE = 4096;
    uint32_t totalChunks = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;
    uint32_t chunksSent = 0;

    for (uint32_t i = 0; i < totalChunks; i++)
    {
        std::vector<BYTE> chunk(CHUNK_SIZE);
        file.read((char *)chunk.data(), CHUNK_SIZE);
        std::streamsize bytesRead = file.gcount();
        chunk.resize(bytesRead);

        auto encryptedChunk = CryptoUtils::aesEncrypt(key, iv, chunk);
        if (!NetworkUtils::sendData(socket, encryptedChunk))
        {
            NetworkUtils::printMessage("ERROR", "Failed to send chunk " + std::to_string(i));
            return false;
        }

        chunksSent++;
        if (chunksSent % 10 == 0 || chunksSent == totalChunks)
        {
            NetworkUtils::printMessage("PROGRESS", "Sent " + std::to_string(chunksSent) + "/" + std::to_string(totalChunks) + " chunks");
        }
    }

    file.close();
    NetworkUtils::printMessage("SUCCESS", "File sent successfully: " + fileName);
    return true;
}

bool FileTransfer::receiveFile(SOCKET socket, const std::vector<BYTE> &key, const std::vector<BYTE> &iv, const std::string &saveDir)
{
    // Receive file info
    std::vector<BYTE> encryptedInfo;
    if (!NetworkUtils::receiveData(socket, encryptedInfo))
    {
        NetworkUtils::printMessage("ERROR", "Failed to receive file info");
        return false;
    }

    auto fileInfo = CryptoUtils::aesDecrypt(key, iv, encryptedInfo);
    if (fileInfo.empty())
    {
        NetworkUtils::printMessage("ERROR", "Failed to decrypt file info");
        return false;
    }

    size_t offset = 0;
    uint32_t nameSize = *(uint32_t *)fileInfo.data();
    offset += sizeof(nameSize);

    std::string fileName(fileInfo.begin() + offset, fileInfo.begin() + offset + nameSize);
    offset += nameSize;

    uint64_t fileSize = *(uint64_t *)(fileInfo.data() + offset);

    NetworkUtils::printMessage("RECEIVING", "File: " + fileName + " (" + std::to_string(fileSize) + " bytes)");

    // Create save directory
    fs::create_directories(saveDir);
    std::string savePath = saveDir + "\\" + fileName;

    std::ofstream file(savePath, std::ios::binary);
    if (!file.is_open())
    {
        NetworkUtils::printMessage("ERROR", "Cannot create file: " + savePath);
        return false;
    }

    // Receive file data
    uint64_t totalReceived = 0;
    uint32_t chunksReceived = 0;
    uint32_t expectedChunks = (fileSize + 4096 - 1) / 4096;

    while (totalReceived < fileSize)
    {
        std::vector<BYTE> encryptedChunk;
        if (!NetworkUtils::receiveData(socket, encryptedChunk))
        {
            NetworkUtils::printMessage("ERROR", "Failed to receive chunk");
            return false;
        }

        auto chunk = CryptoUtils::aesDecrypt(key, iv, encryptedChunk);
        if (chunk.empty())
        {
            NetworkUtils::printMessage("ERROR", "Failed to decrypt chunk");
            return false;
        }

        file.write((const char *)chunk.data(), chunk.size());
        totalReceived += chunk.size();
        chunksReceived++;

        if (chunksReceived % 10 == 0 || chunksReceived == expectedChunks)
        {
            int progress = (totalReceived * 100) / fileSize;
            NetworkUtils::printMessage("PROGRESS", "Received " + std::to_string(chunksReceived) +
                                                       "/" + std::to_string(expectedChunks) + " chunks (" +
                                                       std::to_string(progress) + "%)");
        }
    }

    file.close();
    NetworkUtils::printMessage("SUCCESS", "File received: " + savePath);
    return true;
}