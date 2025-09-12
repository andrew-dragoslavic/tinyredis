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