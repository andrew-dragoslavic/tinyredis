#pragma once
#include <string>
#include <unordered_map>
#include <optional>
#include <chrono>
#include <vector>
// using namespace std;

namespace tr
{
    class KVStore
    {
    public:
        bool expire(const std::string &key, long long seconds);

        long long ttl(const std::string &key);

        void set(const std::string &key, const std::string &value);

        std::optional<std::string> get(const std::string &key);

        bool del(const std::string &key);

        std::optional<long long> incrby(const std::string &key, long long delta);

        int exists(const std::vector<std::string> &keys);

    private:
        std::unordered_map<std::string, std::string> memory;

        std::unordered_map<std::string, std::chrono::steady_clock::time_point> expiry;

        bool purge_if_expired(const std::string &key);
    };
}
