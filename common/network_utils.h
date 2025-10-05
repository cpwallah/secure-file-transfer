#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

// Windows headers first
#include <winsock2.h>
#include <ws2tcpip.h>

// Then C++ headers
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

class NetworkUtils
{
public:
    static bool initialize();
    static void cleanup();
    static bool sendData(SOCKET socket, const std::vector<BYTE> &data);
    static bool receiveData(SOCKET socket, std::vector<BYTE> &data);
    static std::string getTimestamp();
    static void printMessage(const std::string &type, const std::string &message);

    // Helper functions for better error handling
    static std::string getSocketErrorString(int errorCode);
    static int getLastSocketError();
    static bool isSocketConnected(SOCKET socket);
};

#endif