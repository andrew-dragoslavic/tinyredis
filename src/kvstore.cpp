#include "kvstore.hpp"

namespace tr
{
    bool KVStore::purge_if_expired(const std::string &key)
    {
        auto it = expiry.find(key);
        if (it == expiry.end())
        {
            return false;
        }
        std::chrono::steady_clock::time_point deadline = it->second;
        if (deadline <= std::chrono::steady_clock::now())
        {
            memory.erase(key);
            expiry.erase(key);
            return true;
        }
        return false;
    }

    bool KVStore::expire(const std::string &key, long long seconds)
    {
        purge_if_expired(key);
        if (memory.find(key) == memory.end())
        {
            return false;
        }
        if (seconds <= 0)
        {
            memory.erase(key);
            expiry.erase(key);
            return true;
        }
        auto now = std::chrono::steady_clock::now();
        expiry[key] = now + std::chrono::seconds(seconds);
        return true;
    }

    long long KVStore::ttl(const std::string &key)
    {
        purge_if_expired(key);
        auto it = memory.find(key);
        if (it == memory.end())
        {
            return -2;
        }
        auto it2 = expiry.find(key);
        if (it2 == expiry.end())
        {
            return -1;
        }
        std::chrono::steady_clock::time_point deadline = it2->second;
        std::chrono::steady_clock::duration remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero())
        {
            memory.erase(it);
            expiry.erase(it2);
            return -2;
        }

        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(remaining);
        return seconds.count();
    }

    void KVStore::set(const std::string &key, const std::string &value)
    {
        purge_if_expired(key);
        memory[key] = value;
        auto it = expiry.find(key);
        if (it != expiry.end())
        {
            expiry.erase(it);
        }
    }

    std::optional<std::string> KVStore::get(const std::string &key)
    {
        purge_if_expired(key);
        auto it = memory.find(key);
        if (it != memory.end())
        {
            return it->second;
        }
        return std::nullopt;
    }
    bool KVStore::del(const std::string &key)
    {
        purge_if_expired(key);
        auto it = memory.find(key);
        if (it != memory.end())
        {
            bool removed = memory.erase(key) > 0;
            expiry.erase(key);
            return removed;
        }
        return false;
    }
}