#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include "crypto_utils.h"

struct Session
{
    std::string sessionId;
    std::string clientUUID;
    std::vector<BYTE> aesKey;
    std::vector<BYTE> aesIV;
    std::chrono::steady_clock::time_point createdAt;
    std::chrono::steady_clock::time_point lastActivity;
};

class SessionManager
{
private:
    std::unordered_map<std::string, Session> sessions;
    std::mutex sessionMutex;

public:
    std::string createSession(const std::string &clientUUID, const std::vector<BYTE> &aesKey, const std::vector<BYTE> &aesIV);
    Session *getSession(const std::string &sessionId);
    bool validateSession(const std::string &sessionId);
    void updateActivity(const std::string &sessionId);
    void removeSession(const std::string &sessionId);
    void cleanupExpiredSessions(int timeoutMinutes = 30);
    size_t getActiveSessionCount();
};

#endif