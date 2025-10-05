#include "network_utils.h"

bool NetworkUtils::initialize()
{
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        std::cout << "WSAStartup failed with error: " << result << std::endl;
        return false;
    }
    return true;
}

void NetworkUtils::cleanup()
{
    WSACleanup();
}

bool NetworkUtils::sendData(SOCKET socket, const std::vector<BYTE> &data)
{
    // First send the size of the data
    uint32_t size = static_cast<uint32_t>(data.size());
    int sent = send(socket, reinterpret_cast<const char *>(&size), sizeof(size), 0);

    if (sent != sizeof(size))
    {
        int error = WSAGetLastError();
        std::cout << "Failed to send data size. Error: " << getSocketErrorString(error) << std::endl;
        return false;
    }

    // Then send the actual data
    if (!data.empty())
    {
        size_t totalSent = 0;
        while (totalSent < data.size())
        {
            sent = send(socket,
                        reinterpret_cast<const char *>(data.data() + totalSent),
                        static_cast<int>(data.size() - totalSent),
                        0);

            if (sent == SOCKET_ERROR)
            {
                int error = WSAGetLastError();
                std::cout << "Failed to send data. Error: " << getSocketErrorString(error) << std::endl;
                return false;
            }
            totalSent += sent;
        }
    }

    return true;
}

bool NetworkUtils::receiveData(SOCKET socket, std::vector<BYTE> &data)
{
    // First receive the size of the data
    uint32_t size = 0;
    int received = recv(socket, reinterpret_cast<char *>(&size), sizeof(size), 0);

    if (received != sizeof(size))
    {
        if (received == 0)
        {
            std::cout << "Connection gracefully closed by peer" << std::endl;
        }
        else if (received == SOCKET_ERROR)
        {
            int error = WSAGetLastError();
            std::cout << "Failed to receive data size. Error: " << getSocketErrorString(error) << std::endl;
        }
        return false;
    }

    // Check for reasonable size to prevent memory exhaustion
    if (size > 100 * 1024 * 1024)
    { // 100MB limit
        std::cout << "Data size too large: " << size << " bytes" << std::endl;
        return false;
    }

    // Receive the actual data
    data.resize(size);
    if (size > 0)
    {
        size_t totalReceived = 0;
        while (totalReceived < size)
        {
            received = recv(socket,
                            reinterpret_cast<char *>(data.data() + totalReceived),
                            static_cast<int>(size - totalReceived),
                            0);

            if (received == 0)
            {
                std::cout << "Connection closed during data transfer" << std::endl;
                return false;
            }
            if (received == SOCKET_ERROR)
            {
                int error = WSAGetLastError();
                std::cout << "Failed to receive data. Error: " << getSocketErrorString(error) << std::endl;
                return false;
            }
            totalReceived += received;
        }
    }

    return true;
}

std::string NetworkUtils::getTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

void NetworkUtils::printMessage(const std::string &type, const std::string &message)
{
    std::cout << "[" << getTimestamp() << "] " << type << ": " << message << std::endl;
}

bool NetworkUtils::isSocketConnected(SOCKET socket)
{
    // Use select to check if socket is still connected
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(socket, &readSet);

    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000; // 1ms

    int result = select(0, &readSet, nullptr, nullptr, &timeout);
    if (result == SOCKET_ERROR)
    {
        return false;
    }
    return true;
}

int NetworkUtils::getLastSocketError()
{
    return WSAGetLastError();
}

std::string NetworkUtils::getSocketErrorString(int errorCode)
{
    switch (errorCode)
    {
    case WSAEINTR:
        return "Interrupted function call";
    case WSAEBADF:
        return "File handle is not valid";
    case WSAEACCES:
        return "Permission denied";
    case WSAEFAULT:
        return "Bad address";
    case WSAEINVAL:
        return "Invalid argument";
    case WSAEMFILE:
        return "Too many open files";
    case WSAEWOULDBLOCK:
        return "Resource temporarily unavailable";
    case WSAEINPROGRESS:
        return "Operation now in progress";
    case WSAEALREADY:
        return "Operation already in progress";
    case WSAENOTSOCK:
        return "Socket operation on nonsocket";
    case WSAEDESTADDRREQ:
        return "Destination address required";
    case WSAEMSGSIZE:
        return "Message too long";
    case WSAEPROTOTYPE:
        return "Protocol wrong type for socket";
    case WSAENOPROTOOPT:
        return "Bad protocol option";
    case WSAEPROTONOSUPPORT:
        return "Protocol not supported";
    case WSAESOCKTNOSUPPORT:
        return "Socket type not supported";
    case WSAEOPNOTSUPP:
        return "Operation not supported";
    case WSAEPFNOSUPPORT:
        return "Protocol family not supported";
    case WSAEAFNOSUPPORT:
        return "Address family not supported";
    case WSAEADDRINUSE:
        return "Address already in use";
    case WSAEADDRNOTAVAIL:
        return "Cannot assign requested address";
    case WSAENETDOWN:
        return "Network is down";
    case WSAENETUNREACH:
        return "Network is unreachable";
    case WSAENETRESET:
        return "Network dropped connection";
    case WSAECONNABORTED:
        return "Connection aborted";
    case WSAECONNRESET:
        return "Connection reset by peer";
    case WSAENOBUFS:
        return "No buffer space available";
    case WSAEISCONN:
        return "Socket is already connected";
    case WSAENOTCONN:
        return "Socket is not connected";
    case WSAESHUTDOWN:
        return "Cannot send after socket shutdown";
    case WSAETIMEDOUT:
        return "Connection timed out";
    case WSAECONNREFUSED:
        return "Connection refused";
    case WSAEHOSTDOWN:
        return "Host is down";
    case WSAEHOSTUNREACH:
        return "No route to host";
    case WSANOTINITIALISED:
        return "WSAStartup not yet performed";
    case WSAEDISCON:
        return "Graceful shutdown in progress";
    default:
        return "Unknown error code: " + std::to_string(errorCode);
    }
}