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
        else
        {
            return "(error) ERR unknown command '" + cmd + "'";
        }
    }

}