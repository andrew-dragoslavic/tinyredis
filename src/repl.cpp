#include "repl.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace tr
{
    std::vector<std::string> parse_line(const std::string &line)
    {
        std::vector<std::string> tokens;
        std::istringstream ss(line);
        std::string token;

        while (ss >> token)
        {
            tokens.push_back(token);
        }

        return tokens;
    }

    RespParseStatus parse_resp_array(const std::string &in, std::size_t &consumed, std::vector<std::string> &out)
    {
        consumed = 0;
        out.clear();

        if (in.empty())
            return RespParseStatus::NeedMore;

        if (in[0] != '*')
            return RespParseStatus::Error;

        std::size_t crlf = in.find("\r\n");
        if (crlf == std::string::npos)
            return RespParseStatus::NeedMore;

        if (crlf <= 1)
            return RespParseStatus::Error;
        std::string len_str = in.substr(1, crlf - 1);
        std::size_t pos = 0;
        long long n = 0;
        try
        {
            n = std::stoll(len_str, &pos);
        }
        catch (...)
        {
            return RespParseStatus::Error;
        }
        if (pos != len_str.size() || n < 0)
            return RespParseStatus::Error;

        std::size_t cursor = crlf + 2;

        for (long long i = 0; i < n; ++i)
        {
            if (cursor >= in.size())
                return RespParseStatus::NeedMore;

            if (in[cursor] != '$')
                return RespParseStatus::Error;

            std::size_t crlf2 = in.find("\r\n", cursor + 1);
            if (crlf2 == std::string::npos)
                return RespParseStatus::NeedMore;

            const std::string len2_str = in.substr(cursor + 1, crlf2 - (cursor + 1));
            std::size_t pos2 = 0;
            long long len = 0;
            try
            {
                len = std::stoll(len2_str, &pos2, 10);
            }
            catch (...)
            {
                return RespParseStatus::Error;
            }
            if (pos2 != len2_str.size() || len < 0)
                return RespParseStatus::Error;

            std::size_t data_start = crlf2 + 2;
            std::size_t need = data_start + static_cast<std::size_t>(len) + 2;
            if (in.size() < need)
                return RespParseStatus::NeedMore;

            // 6) Slice out the argument bytes

            // 7) Verify trailing "\r\n"
            std::size_t data_end = data_start + static_cast<std::size_t>(len);
            if (in[data_end] != '\r' || in[data_end + 1] != '\n')
                return RespParseStatus::Error;

            out.emplace_back(in.substr(data_start, static_cast<std::size_t>(len)));

            // 8) Advance cursor to the next element
            cursor = data_end + 2;
        }
        consumed = cursor;
        return RespParseStatus::Ok;
    }

    std::string eval_command(KVStore &db, const std::vector<std::string> &args)
    {
        if (args.empty())
        {
            return "";
        }
        std::string cmd = args[0];
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });

        if (cmd == "ping" && args.size() == 1)
        {
            return "PONG";
        }
        else if (cmd == "ping" && args.size() != 1)
        {
            return "(error) ERR wrong number of arguments for 'ping'";
        }
        else if (cmd == "set")
        {
            if (args.size() != 3)
            {
                return "(error) ERR wrong number of arguments for 'set'";
            }
            else
            {
                db.set(args[1], args[2]);
                return "OK";
            }
        }
        else if (cmd == "get")
        {
            if (args.size() != 2)
            {
                return "(error) ERR wrong number of arguments for 'get'";
            }
            else
            {
                std::optional<std::string> value = db.get(args[1]);
                if (value.has_value())
                {
                    return value.value();
                }
                else
                {
                    return "(nil)";
                }
            }
        }
        else if (cmd == "del")
        {
            if (args.size() != 2)
            {
                return "(error) ERR wrong number of arguments for 'del'";
            }
            else
            {
                bool res = db.del(args[1]);
                return std::to_string(static_cast<int>(res));
            }
        }
        else if (cmd == "expire")
        {
            if (args.size() != 3)
            {
                return "(error) ERR wrong number of arguments for 'expire'";
            }
            else
            {
                bool res = db.expire(args[1], std::stoll(args[2]));
                return std::to_string(res);
            }
        }
        else if (cmd == "ttl")
        {
            if (args.size() != 2)
            {
                return "(error) ERR wrong number of arguments for 'ttl'";
            }
            else
            {
                long long res = db.ttl(args[1]);
                return std::to_string(res);
            }
        }
        else if (cmd == "incrby")
        {
            if (args.size() != 3)
            {
                return "(error) ERR wrong number of arguments for 'incrby'";
            }
            else
            {
                try
                {
                    long long delta = std::stoll(args[2]);
                    std::optional<long long> res = db.incrby(args[1], delta);

                    if (res.has_value())
                    {
                        return std::to_string(res.value());
                    }
                    else
                    {
                        return "(error) ERR value is not an integer or out of range";
                    }
                }
                catch (const std::exception &)
                {
                    return "(error) ERR value is not an integer or out of range";
                }
            }
        }
        else if (cmd == "decrby")
        {
            if (args.size() != 3)
            {
                return "(error) ERR wrong number of arguments for 'decrby'";
            }
            else
            {
                try
                {
                    long long delta = -std::stoll(args[2]);
                    std::optional<long long> res = db.incrby(args[1], delta);

                    if (res.has_value())
                    {
                        return std::to_string(res.value());
                    }
                    else
                    {
                        return "(error) ERR value is not an integer or out of range";
                    }
                }
                catch (const std::exception &)
                {
                    return "(error) ERR value is not an integer or out of range";
                }
            }
        }
        else
        {
            return "(error) ERR unknown command '" + cmd + "'";
        }
    }

}