#include <iostream>
#include <string>
#include <vector>

// Windows headers - CORRECT ORDER
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <direct.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")

#include "../common/network_utils.h"
#include "../common/crypto_utils.h"
#include "../common/file_transfer.h"

class SimpleClient
{
private:
    SOCKET clientSocket;
    std::vector<BYTE> aesKey;
    std::vector<BYTE> aesIV;
    std::string clientUUID;
    bool connected;

public:
    SimpleClient() : clientSocket(INVALID_SOCKET), connected(false)
    {
        _mkdir("received_files");
        _mkdir("files_to_send");
    }

    ~SimpleClient()
    {
        disconnect();
    }

    bool connectToServer(const std::string &ip, int port)
    {
        std::cout << "Connecting to " << ip << ":" << port << "..." << std::endl;

        if (!NetworkUtils::initialize())
        {
            std::cout << "Network initialization failed" << std::endl;
            return false;
        }

        clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket == INVALID_SOCKET)
        {
            std::cout << "Socket creation failed" << std::endl;
            return false;
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

        if (::connect(clientSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        {
            std::cout << "Connection failed! Make sure server is running." << std::endl;
            closesocket(clientSocket);
            return false;
        }

        std::cout << "Connected to server successfully!" << std::endl;

        // Receive session keys
        std::vector<BYTE> keyData;
        std::cout << "Receiving encryption keys..." << std::endl;

        if (!NetworkUtils::receiveData(clientSocket, keyData))
        {
            std::cout << "Failed to receive session keys" << std::endl;
            closesocket(clientSocket);
            return false;
        }

        std::cout << "Received key data size: " << keyData.size() << " bytes" << std::endl;

        if (keyData.size() < 32)
        {
            std::cout << "Invalid key data received (size: " << keyData.size() << ")" << std::endl;
            closesocket(clientSocket);
            return false;
        }

        // Extract AES key and IV
        aesKey.assign(keyData.begin(), keyData.begin() + 16);
        aesIV.assign(keyData.begin() + 16, keyData.begin() + 32);

        std::cout << "Key size: " << aesKey.size() << ", IV size: " << aesIV.size() << std::endl;

        // Extract UUID if present
        if (keyData.size() > 32)
        {
            size_t offset = 32;
            if (offset + sizeof(uint32_t) <= keyData.size())
            {
                uint32_t uuidSize = *(uint32_t *)(keyData.data() + offset);
                offset += sizeof(uint32_t);
                if (offset + uuidSize <= keyData.size())
                {
                    clientUUID.assign(keyData.begin() + offset, keyData.begin() + offset + uuidSize);
                    std::cout << "UUID: " << clientUUID << std::endl;
                }
            }
        }

        std::cout << "Secure connection established!" << std::endl;
        connected = true;
        return true;
    }

    void disconnect()
    {
        connected = false;
        if (clientSocket != INVALID_SOCKET)
        {
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
        }
        NetworkUtils::cleanup();
    }

    void run()
    {
        if (!connected)
        {
            std::cout << "Not connected to server!" << std::endl;
            return;
        }

        std::cout << "\n=== CLIENT READY ===" << std::endl;
        std::cout << "Waiting for server commands..." << std::endl;

        while (connected)
        {
            std::vector<BYTE> encryptedCommand;

            std::cout << "\n[Waiting for command from server...]" << std::endl;

            if (!NetworkUtils::receiveData(clientSocket, encryptedCommand))
            {
                std::cout << "Failed to receive command from server" << std::endl;
                break;
            }

            std::cout << "Received encrypted command, size: " << encryptedCommand.size() << " bytes" << std::endl;

            if (encryptedCommand.empty())
            {
                std::cout << "ERROR: Received empty command!" << std::endl;
                break;
            }

            // Decrypt the command
            auto commandData = CryptoUtils::aesDecrypt(aesKey, aesIV, encryptedCommand);
            if (commandData.empty())
            {
                std::cout << "Failed to decrypt command" << std::endl;
                break;
            }

            if (commandData.size() < sizeof(int))
            {
                std::cout << "Invalid command data size: " << commandData.size() << std::endl;
                break;
            }

            int command = *(int *)commandData.data();
            std::cout << "Server command: " << command << std::endl;

            switch (command)
            {
            case 1:
                handleUpload();
                break;
            case 2:
                handleDownload();
                break;
            case 6:
                std::cout << "Server requested disconnection. Goodbye!" << std::endl;
                connected = false;
                break;
            default:
                std::cout << "Unknown command: " << command << std::endl;
                break;
            }
        }

        disconnect();
        std::cout << "Client stopped." << std::endl;
    }

private:
    void handleUpload()
    {
        std::cout << "UPLOAD: Enter file path: ";
        std::string filePath;
        std::getline(std::cin, filePath);

        if (filePath.empty())
        {
            std::cout << "Upload cancelled" << std::endl;
            return;
        }

        // Check if file exists
        DWORD attrs = GetFileAttributesA(filePath.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES)
        {
            std::cout << "File not found: " << filePath << std::endl;
            return;
        }

        std::cout << "Uploading: " << filePath << std::endl;

        if (FileTransfer::sendFile(clientSocket, aesKey, aesIV, filePath))
        {
            std::cout << "Upload successful!" << std::endl;
        }
        else
        {
            std::cout << "Upload failed" << std::endl;
            connected = false;
        }
    }

    void handleDownload()
    {
        std::cout << "DOWNLOAD: Waiting for file..." << std::endl;

        if (FileTransfer::receiveFile(clientSocket, aesKey, aesIV, "received_files"))
        {
            std::cout << "Download successful!" << std::endl;
        }
        else
        {
            std::cout << "Download failed" << std::endl;
            connected = false;
        }
    }
};

int main()
{
    std::cout << "=== SECURE FILE TRANSFER CLIENT ===" << std::endl;

    SimpleClient client;
    if (client.connectToServer("127.0.0.1", 8080))
    {
        client.run();
    }
    else
    {
        std::cout << "Failed to connect to server" << std::endl;
    }

    std::cout << "Press Enter to exit...";
    std::cin.get();
    return 0;
}