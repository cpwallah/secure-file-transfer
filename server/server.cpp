#include <iostream>
#include <string>
#include <thread>
#include <filesystem>
#include <vector>
#include <map>
#include <atomic>
#include "../common/network_utils.h"
#include "../common/crypto_utils.h"
#include "../common/file_transfer.h"
#include "../common/session_manager.h"

namespace fs = std::filesystem;

class FileServer
{
private:
    SOCKET serverSocket;
    SessionManager sessionManager;
    std::map<std::string, std::string> clientUUIDs;
    std::atomic<bool> running{true};
    std::string receivedDir = "received_files";
    std::string serverFilesDir = "server_files";

public:
    FileServer() : serverSocket(INVALID_SOCKET)
    {
        fs::create_directories(receivedDir);
        fs::create_directories(serverFilesDir);
    }

    bool start(int port)
    {
        if (!NetworkUtils::initialize())
        {
            std::cout << "Failed to initialize Winsock" << std::endl;
            return false;
        }

        serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (serverSocket == INVALID_SOCKET)
        {
            std::cout << "Failed to create socket" << std::endl;
            return false;
        }

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(serverSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        {
            std::cout << "Bind failed on port " << port << std::endl;
            closesocket(serverSocket);
            return false;
        }

        if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
        {
            std::cout << "Listen failed" << std::endl;
            closesocket(serverSocket);
            return false;
        }

        std::cout << "=== FILE TRANSFER SERVER ===" << std::endl;
        std::cout << "Server started on port " << port << std::endl;
        std::cout << "Waiting for client connections..." << std::endl;
        return true;
    }

    void run()
    {
        while (running)
        {
            sockaddr_in clientAddr;
            int addrLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(serverSocket, (sockaddr *)&clientAddr, &addrLen);

            if (clientSocket == INVALID_SOCKET)
            {
                if (running)
                    continue;
                else
                    break;
            }

            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            std::string clientAddress = std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port));

            NetworkUtils::printMessage("CONNECTION", "Client connected: " + clientAddress);

            // Handle client in separate thread
            std::thread clientThread(&FileServer::handleClient, this, clientSocket, clientAddress);
            clientThread.detach();
        }
    }

    void stop()
    {
        running = false;
        closesocket(serverSocket);
    }

private:
    void handleClient(SOCKET clientSocket, const std::string &clientAddress)
    {
        std::string clientUUID = CryptoUtils::generateUUID();
        std::vector<BYTE> aesKey, aesIV;
        CryptoUtils::generateAESKey(aesKey, aesIV);

        std::string sessionId = sessionManager.createSession(clientUUID, aesKey, aesIV);
        clientUUIDs[std::to_string((long long)clientSocket)] = clientUUID;

        NetworkUtils::printMessage("SESSION", "Created session for client " + clientUUID);

        try
        {
            // Send session keys to client
            std::vector<BYTE> keyData;
            keyData.insert(keyData.end(), aesKey.begin(), aesKey.end());
            keyData.insert(keyData.end(), aesIV.begin(), aesIV.end());

            uint32_t uuidSize = static_cast<uint32_t>(clientUUID.size());
            keyData.insert(keyData.end(), (BYTE *)&uuidSize, (BYTE *)&uuidSize + sizeof(uuidSize));
            keyData.insert(keyData.end(), clientUUID.begin(), clientUUID.end());

            std::cout << "Sending encryption keys to client (size: " << keyData.size() << " bytes)..." << std::endl;
            if (!NetworkUtils::sendData(clientSocket, keyData))
            {
                NetworkUtils::printMessage("ERROR", "Failed to send keys to client");
                closesocket(clientSocket);
                return;
            }
            std::cout << "Encryption keys sent successfully!" << std::endl;

            bool clientRunning = true;
            while (clientRunning && running)
            {
                showMenu(clientUUID);
                int choice = getMenuChoice();

                if (choice == 0)
                    continue; // Invalid choice

                std::cout << "Sending command " << choice << " to client..." << std::endl;

                // Send choice to client - FIXED: Use simple XOR encryption
                std::vector<BYTE> choiceData(sizeof(int));
                memcpy(choiceData.data(), &choice, sizeof(int));

                std::cout << "Command data size: " << choiceData.size() << " bytes" << std::endl;

                // Use XOR encryption (always works)
                auto encryptedChoice = CryptoUtils::aesEncrypt(aesKey, aesIV, choiceData);

                if (encryptedChoice.empty())
                {
                    std::cout << "ERROR: Encryption returned empty data!" << std::endl;
                    break;
                }

                std::cout << "Encrypted command size: " << encryptedChoice.size() << " bytes" << std::endl;

                if (!NetworkUtils::sendData(clientSocket, encryptedChoice))
                {
                    NetworkUtils::printMessage("ERROR", "Failed to send choice to client");
                    break;
                }
                std::cout << "Command sent successfully!" << std::endl;

                sessionManager.updateActivity(sessionId);

                switch (choice)
                {
                case 1:
                    waitForFile(clientSocket, aesKey, aesIV, clientUUID);
                    break;
                case 2:
                    sendFileToClient(clientSocket, aesKey, aesIV, clientUUID);
                    break;
                case 3:
                    listReceivedFiles();
                    break;
                case 4:
                    listServerFiles();
                    break;
                case 5:
                    listConnectedClients();
                    break;
                case 6:
                    NetworkUtils::printMessage("DISCONNECTION", "Disconnecting client: " + clientUUID);
                    clientRunning = false;
                    break;
                default:
                    std::cout << "Invalid option!" << std::endl;
                }
            }
        }
        catch (const std::exception &e)
        {
            NetworkUtils::printMessage("ERROR", "Client handling error: " + std::string(e.what()));
        }

        // Cleanup
        sessionManager.removeSession(sessionId);
        clientUUIDs.erase(std::to_string((long long)clientSocket));
        closesocket(clientSocket);
        NetworkUtils::printMessage("DISCONNECTION", "Client disconnected: " + clientUUID);
    }

    void showMenu(const std::string &clientUUID)
    {
        std::cout << "\n--- Server Menu ---" << std::endl;
        std::cout << "Client UUID: " << clientUUID << std::endl;
        std::cout << "1. Wait for file from client" << std::endl;
        std::cout << "2. Send file to client" << std::endl;
        std::cout << "3. Show received files" << std::endl;
        std::cout << "4. Show server files" << std::endl;
        std::cout << "5. Show connected clients" << std::endl;
        std::cout << "6. Disconnect client" << std::endl;
        std::cout << "Choose option: ";
    }

    int getMenuChoice()
    {
        std::string input;
        std::getline(std::cin, input);

        try
        {
            return std::stoi(input);
        }
        catch (...)
        {
            std::cout << "Invalid input! Please enter a number 1-6." << std::endl;
            return 0;
        }
    }

    void waitForFile(SOCKET clientSocket, const std::vector<BYTE> &aesKey, const std::vector<BYTE> &aesIV, const std::string &clientUUID)
    {
        std::cout << "Waiting for file from client " << clientUUID << "..." << std::endl;
        std::cout << "Client should now be asking for file selection..." << std::endl;

        if (FileTransfer::receiveFile(clientSocket, aesKey, aesIV, receivedDir))
        {
            std::cout << "File received successfully from " << clientUUID << "!" << std::endl;
        }
        else
        {
            std::cout << "Failed to receive file from " << clientUUID << std::endl;
            std::cout << "Client may have cancelled or encountered an error." << std::endl;
        }
    }

    void sendFileToClient(SOCKET clientSocket, const std::vector<BYTE> &aesKey, const std::vector<BYTE> &aesIV, const std::string &clientUUID)
    {
        // Show available server files
        std::vector<std::string> files;
        std::cout << "\nFiles available in 'server_files' folder:" << std::endl;

        int count = 0;
        for (const auto &entry : fs::directory_iterator(serverFilesDir))
        {
            if (entry.is_regular_file())
            {
                files.push_back(entry.path().string());
                std::cout << "  " << ++count << ". " << entry.path().filename().string();
                std::cout << " (" << entry.file_size() << " bytes)" << std::endl;
            }
        }

        if (count == 0)
        {
            std::cout << "  No files found in server_files folder." << std::endl;
            std::cout << "  Place files there or enter full path." << std::endl;
        }

        std::cout << "\nEnter file path or number: ";
        std::string filePath;
        std::getline(std::cin, filePath);

        // Handle number selection
        try
        {
            int fileNum = std::stoi(filePath);
            if (fileNum >= 1 && fileNum <= count)
            {
                filePath = files[fileNum - 1];
            }
        }
        catch (...)
        {
            // Not a number, use as path
        }

        if (!fs::exists(filePath))
        {
            std::cout << "File not found: " << filePath << std::endl;
            return;
        }

        std::cout << "Sending file to client..." << std::endl;

        if (FileTransfer::sendFile(clientSocket, aesKey, aesIV, filePath))
        {
            std::cout << "File sent successfully to " << clientUUID << "!" << std::endl;
        }
        else
        {
            std::cout << "Failed to send file to " << clientUUID << std::endl;
        }
    }

    void listReceivedFiles()
    {
        std::cout << "\n--- Received Files ---" << std::endl;
        int count = 0;
        for (const auto &entry : fs::directory_iterator(receivedDir))
        {
            if (entry.is_regular_file())
            {
                std::cout << ++count << ". " << entry.path().filename().string();
                std::cout << " (" << entry.file_size() << " bytes)" << std::endl;
            }
        }
        if (count == 0)
        {
            std::cout << "No files received yet." << std::endl;
        }
    }

    void listServerFiles()
    {
        std::cout << "\n--- Server Files ---" << std::endl;
        int count = 0;
        for (const auto &entry : fs::directory_iterator(serverFilesDir))
        {
            if (entry.is_regular_file())
            {
                std::cout << ++count << ". " << entry.path().filename().string();
                std::cout << " (" << entry.file_size() << " bytes)" << std::endl;
            }
        }
        if (count == 0)
        {
            std::cout << "No files in server_files folder." << std::endl;
        }
    }

    void listConnectedClients()
    {
        std::cout << "\n--- Connected Clients ---" << std::endl;
        std::cout << "Total: " << sessionManager.getActiveSessionCount() << " clients" << std::endl;
        for (const auto &[socketStr, uuid] : clientUUIDs)
        {
            std::cout << "UUID: " << uuid << std::endl;
        }
    }
};

int main()
{
    FileServer server;

    std::cout << "Starting file transfer server..." << std::endl;
    if (server.start(8080))
    {
        server.run();
    }
    else
    {
        std::cout << "Failed to start server!" << std::endl;
    }

    server.stop();
    NetworkUtils::cleanup();
    return 0;
}