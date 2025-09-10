#include "kvstore.hpp"

namespace tr
{
    void KVStore::set(const std::string &key, const std::string &value)
    {
        memory[key] = value;
    }

    std::optional<std::string> KVStore::get(const std::string &key) const
    {
        auto it = memory.find(key);
        if (it != memory.end())
        {
            return it->second;
        }
        return std::nullopt;
    }
    bool KVStore::del(const std::string &key)
    {
        auto it = memory.find(key);
        if (it != memory.end())
        {
            memory.erase(key);
            return true;
        }
        return false;
    }
}