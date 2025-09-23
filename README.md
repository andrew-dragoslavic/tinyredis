# TinyRedis

TinyRedis is a minimal Redis-style key-value store written in modern C++. It implements a subset of the Redis protocol (RESP) and commands to demonstrate how an in-memory cache server works end-to-end—from parsing client requests to managing data with expirations.

## Features
- In-memory key/value store with optional TTL expiration.
- RESP array parser so real Redis clients can talk to the server.
- Support for core string commands: `PING`, `GET`, `SET`, `DEL`, `EXPIRE`, `TTL`, `INCRBY`, `DECRBY`, and `EXISTS`.
- Line-oriented REPL for quick experimentation from the terminal.
- TCP server that listens on `127.0.0.1:6380` and serves RESP responses.
- GoogleTest suite covering store behavior, command evaluation, and RESP parsing.

## Repository Layout
```
include/        Public headers (KVStore, REPL, server API)
src/            Implementation of the store, REPL, and server front-ends
tests/          GoogleTest-based unit tests
CMakeLists.txt  CMake build configuration
```

## Building
The project uses CMake and requires a C++20 toolchain and GoogleTest.

```bash
cmake -S . -B build
cmake --build build
```

### Running Tests
```bash
ctest --test-dir build
```

## Running
### Interactive REPL
Run the console client to issue commands directly:
```bash
./build/tinyredis
```

Example session:
```
tinyredis - type EXIT to quit
> SET name Alice
OK
> GET name
Alice
> EXIT
Goodbye!
```

### TCP Server
Start the RESP-compatible server:
```bash
./build/tinyredis_server
```

Then use `redis-cli` or `nc` to connect:
```bash
redis-cli -p 6380 ping
redis-cli -p 6380 set counter 5
redis-cli -p 6380 incrby counter 3
redis-cli -p 6380 ttl counter
```

## What I Learned
- **RESP framing:** How Redis frames commands and replies, and how to parse bulk strings and arrays safely.
- **State management:** Implementing TTL-backed storage with periodic purging triggered by reads/writes.
- **Socket programming:** Creating a blocking TCP server, configuring sockets, and handling common error paths.
- **Error handling parity:** Surfacing protocol errors consistently to both REPL users and network clients.
- **Testing discipline:** Using GoogleTest to verify command semantics, expiry behavior, and protocol parsing.

## Next Steps (Ideas)
- Add persistence (append-only log or snapshot) to survive restarts.
- Support additional Redis data types (lists, hashes, sets).
- Move to an event-driven loop to serve multiple clients concurrently.
- Introduce configuration, authentication, and richer logging.

TinyRedis meets its goal as a learning project: it exposes the moving pieces behind Redis-like caches while remaining small enough to understand end-to-end.
