#pragma once
#include <string>
#include <vector>
#include "kvstore.hpp"

namespace tr
{
    std::vector<std::string> parse_line(const std::string &line);

    std::string eval_command(KVStore &db, const std::vector<std::string> &args);

    enum class RespParseStatus
    {
        NeedMore,
        Ok,
        Error
    };

    RespParseStatus parse_resp_array(const std::string &in, std::size_t &consumed, std::vector<std::string> &out);
}