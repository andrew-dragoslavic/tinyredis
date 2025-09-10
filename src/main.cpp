#include <iostream>
#include <string>
#include "repl.hpp"

int main()
{
    tr::KVStore db;
    std::cout << "tinyredis - type EXIT to quit\n";

    std::string line;
    while (true)
    {
        std::cout << "> "; // Prompt before input
        if (!std::getline(std::cin, line))
        {
            break; // Handle EOF (Ctrl+D)
        }

        if (line == "EXIT" || line == "exit")
        {
            break;
        }

        auto tokens = tr::parse_line(line);
        std::string res = tr::eval_command(db, tokens);
        std::cout << res << std::endl;
    }

    std::cout << "Goodbye!\n";
    return 0;
}
