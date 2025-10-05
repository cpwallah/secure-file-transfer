#include "session_manager.h"

std::string SessionManager::createSession(const std::string &clientUUID, const std::vector<BYTE> &aesKey, const std::vector<BYTE> &aesIV)
{
    std::lock_guard<std::mutex> lock(sessionMutex);

    Session session;
    session.sessionId = CryptoUtils::generateUUID();
    session.clientUUID = clientUUID;
    session.aesKey = aesKey;
    session.aesIV = aesIV;
    session.createdAt = std::chrono::steady_clock::now();
    session.lastActivity = std::chrono::steady_clock::now();

    sessions[session.sessionId] = session;
    return session.sessionId;
}

Session *SessionManager::getSession(const std::string &sessionId)
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    auto it = sessions.find(sessionId);
    if (it != sessions.end())
    {
        return &it->second;
    }
    return nullptr;
}

bool SessionManager::validateSession(const std::string &sessionId)
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    auto it = sessions.find(sessionId);
    if (it != sessions.end())
    {
        // Check if session expired (30 minutes)
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - it->second.lastActivity);
        return duration.count() < 30;
    }
    return false;
}

void SessionManager::updateActivity(const std::string &sessionId)
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    auto it = sessions.find(sessionId);
    if (it != sessions.end())
    {
        it->second.lastActivity = std::chrono::steady_clock::now();
    }
}

void SessionManager::removeSession(const std::string &sessionId)
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    sessions.erase(sessionId);
}

void SessionManager::cleanupExpiredSessions(int timeoutMinutes)
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    auto now = std::chrono::steady_clock::now();

    for (auto it = sessions.begin(); it != sessions.end();)
    {
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - it->second.lastActivity);
        if (duration.count() > timeoutMinutes)
        {
            it = sessions.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

size_t SessionManager::getActiveSessionCount()
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    return sessions.size();
}