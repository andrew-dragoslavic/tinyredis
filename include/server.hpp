#pragma once
#include <cstdint>
#include "kvstore.hpp"
#include "repl.hpp"

namespace tr
{
    int run_server(const uint16_t port);

    void handle_client(int client_fd, KVStore &db);

    bool write_all(int fd, const std::string &s);
}