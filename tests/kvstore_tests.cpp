#include <gtest/gtest.h>
#include "kvstore.hpp"
#include "repl.hpp"
#include <thread>
#include <chrono>

TEST(KVStore, SetGetDelBasics)
{
    tr::KVStore store;

    store.set("key1", "value1");
    auto result = store.get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value1");

    auto missing = store.get("nonexistent");
    EXPECT_FALSE(missing.has_value());

    EXPECT_TRUE(store.del("key1"));
    auto deleted = store.get("key1");
    EXPECT_FALSE(deleted.has_value());

    EXPECT_FALSE(store.del("nonexistent"));
}

TEST(Repl, ParseLine_BasicWhitespace)
{
    auto tokens = tr::parse_line("  SET  a  b  ");
    EXPECT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "SET");
    EXPECT_EQ(tokens[1], "a");
    EXPECT_EQ(tokens[2], "b");
}

TEST(ReplEval, FullWorkflow)
{
    tr::KVStore db;

    // Set multiple keys
    EXPECT_EQ(tr::eval_command(db, {"SET", "name", "John"}), "OK");
    EXPECT_EQ(tr::eval_command(db, {"SET", "age", "25"}), "OK");

    // Get them back
    EXPECT_EQ(tr::eval_command(db, {"GET", "name"}), "John");
    EXPECT_EQ(tr::eval_command(db, {"GET", "age"}), "25");

    // Delete one
    EXPECT_EQ(tr::eval_command(db, {"DEL", "name"}), "1");

    // Verify deletion
    EXPECT_EQ(tr::eval_command(db, {"GET", "name"}), "(nil)");
    EXPECT_EQ(tr::eval_command(db, {"GET", "age"}), "25");

    // Test ping still works
    EXPECT_EQ(tr::eval_command(db, {"PING"}), "PONG");
}

TEST(ReplEval, UnknownCommand)
{
    tr::KVStore db;
    auto result = tr::eval_command(db, {"UNKNOWN"});
    EXPECT_EQ(result, "(error) ERR unknown command 'unknown'");

    auto result2 = tr::eval_command(db, {"INVALID", "arg1"});
    EXPECT_EQ(result2, "(error) ERR unknown command 'invalid'");
}

TEST(ReplEval, DelCommand_WrongArgs)
{
    tr::KVStore db;

    // Too few arguments
    auto result1 = tr::eval_command(db, {"DEL"});
    EXPECT_EQ(result1, "(error) ERR wrong number of arguments for 'del'");

    // Too many arguments
    auto result2 = tr::eval_command(db, {"DEL", "key1", "key2"});
    EXPECT_EQ(result2, "(error) ERR wrong number of arguments for 'del'");
}

TEST(KVStoreExpiry, TTL_NoExpiryIsMinus1)
{
    tr::KVStore db;
    db.set("k", "v");
    EXPECT_EQ(db.ttl("k"), -1);
}

TEST(KVStoreExpiry, Expire_MissingKeyReturnsZero)
{
    tr::KVStore db;
    EXPECT_FALSE(db.expire("no_such_key", 5)); // 0
}

TEST(KVStoreExpiry, Expire_NonPositiveDeletesImmediately)
{
    tr::KVStore db;
    db.set("k", "v");
    EXPECT_TRUE(db.expire("k", 0));        // 1, delete now
    EXPECT_FALSE(db.get("k").has_value()); // gone
    EXPECT_EQ(db.ttl("k"), -2);            // missing
}

TEST(KVStoreExpiry, Set_ClearsOldExpiry)
{
    tr::KVStore db;
    db.set("k", "v1");
    EXPECT_TRUE(db.expire("k", 5)); // set a deadline
    db.set("k", "v2");              // fresh value clears TTL
    EXPECT_EQ(db.ttl("k"), -1);     // exists, no expiry now
    EXPECT_EQ(db.get("k").value(), "v2");
}

TEST(KVStoreExpiry, TTL_CountsDownThenMinus2)
{
    tr::KVStore db;
    db.set("k", "v");
    ASSERT_TRUE(db.expire("k", 1)); // 1 second
    long long t = db.ttl("k");
    EXPECT_GE(t, 0); // non-negative
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    EXPECT_EQ(db.ttl("k"), -2); // now expired & gone
}

TEST(KVStoreExpiry, Del_RemovesValueAndExpiry)
{
    tr::KVStore db;
    db.set("k", "v");
    db.expire("k", 5);
    EXPECT_TRUE(db.del("k"));              // value deleted
    EXPECT_EQ(db.ttl("k"), -2);            // missing
    EXPECT_FALSE(db.get("k").has_value()); // missing
}

TEST(KVStoreExpiry, PurgeOnTouchViaGet)
{
    tr::KVStore db;
    db.set("k", "v");
    db.expire("k", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    EXPECT_FALSE(db.get("k").has_value()); // get triggers purge
    EXPECT_EQ(db.ttl("k"), -2);
}

// RESP array parsing tests

TEST(RespParse, Ok_SimplePing)
{
    std::string in = "*1\r\n$4\r\nPING\r\n";
    std::size_t consumed = 999;
    std::vector<std::string> out = {"pre"};
    auto st = tr::parse_resp_array(in, consumed, out);

    EXPECT_EQ(st, tr::RespParseStatus::Ok);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], "PING");
    EXPECT_EQ(consumed, in.size());
}

TEST(RespParse, Ok_GetKey)
{
    std::string in = "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n";
    std::size_t consumed = 999;
    std::vector<std::string> out;
    auto st = tr::parse_resp_array(in, consumed, out);

    EXPECT_EQ(st, tr::RespParseStatus::Ok);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0], "GET");
    EXPECT_EQ(out[1], "key");
    EXPECT_EQ(consumed, in.size());
}

TEST(RespParse, Ok_ZeroLengthBulk)
{
    std::string in = "*1\r\n$0\r\n\r\n";
    std::size_t consumed = 999;
    std::vector<std::string> out;
    auto st = tr::parse_resp_array(in, consumed, out);

    EXPECT_EQ(st, tr::RespParseStatus::Ok);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], "");
    EXPECT_EQ(consumed, in.size());
}

TEST(RespParse, Ok_EmptyArray)
{
    std::string in = "*0\r\n";
    std::size_t consumed = 999;
    std::vector<std::string> out = {"x"};
    auto st = tr::parse_resp_array(in, consumed, out);

    EXPECT_EQ(st, tr::RespParseStatus::Ok);
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(consumed, in.size());
}

TEST(RespParse, NeedMore_HeaderOnly)
{
    std::string in = "*2\r\n";
    std::size_t consumed = 123;
    std::vector<std::string> out = {"x"};
    auto st = tr::parse_resp_array(in, consumed, out);

    EXPECT_EQ(st, tr::RespParseStatus::NeedMore);
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(consumed, 0u);
}

TEST(RespParse, NeedMore_PartialBulkLenLine)
{
    std::string in = "*1\r\n$4";
    std::size_t consumed = 123;
    std::vector<std::string> out = {"x"};
    auto st = tr::parse_resp_array(in, consumed, out);

    EXPECT_EQ(st, tr::RespParseStatus::NeedMore);
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(consumed, 0u);
}

TEST(RespParse, NeedMore_PartialBulkData)
{
    std::string in = "*1\r\n$4\r\nPI";
    std::size_t consumed = 123;
    std::vector<std::string> out;
    auto st = tr::parse_resp_array(in, consumed, out);

    EXPECT_EQ(st, tr::RespParseStatus::NeedMore);
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(consumed, 0u);
}

TEST(RespParse, Error_NotArrayPrefix)
{
    std::string in = "$3\r\nGET\r\n";
    std::size_t consumed = 123;
    std::vector<std::string> out = {"x"};
    auto st = tr::parse_resp_array(in, consumed, out);

    EXPECT_EQ(st, tr::RespParseStatus::Error);
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(consumed, 0u);
}

TEST(RespParse, Error_NegativeArrayLen)
{
    std::string in = "*-1\r\n";
    std::size_t consumed = 123;
    std::vector<std::string> out;
    auto st = tr::parse_resp_array(in, consumed, out);

    EXPECT_EQ(st, tr::RespParseStatus::Error);
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(consumed, 0u);
}

TEST(RespParse, Error_NonNumericBulkLen)
{
    std::string in = "*1\r\n$X\r\n";
    std::size_t consumed = 123;
    std::vector<std::string> out;
    auto st = tr::parse_resp_array(in, consumed, out);

    EXPECT_EQ(st, tr::RespParseStatus::Error);
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(consumed, 0u);
}

TEST(RespParse, Error_BadTrailingCRLF_AfterData)
{
    // Has enough bytes, but the two bytes after data are not \r\n
    std::string in = "*1\r\n$4\r\nPINGxx";
    std::size_t consumed = 123;
    std::vector<std::string> out;
    auto st = tr::parse_resp_array(in, consumed, out);

    EXPECT_EQ(st, tr::RespParseStatus::Error);
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(consumed, 0u);
}

TEST(RespParse, Pipeline_TwoArrays)
{
    std::string first = "*1\r\n$4\r\nPING\r\n";
    std::string second = "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n";
    std::string in = first + second;

    std::size_t consumed = 0;
    std::vector<std::string> out;
    auto st = tr::parse_resp_array(in, consumed, out);

    EXPECT_EQ(st, tr::RespParseStatus::Ok);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], "PING");
    EXPECT_EQ(consumed, first.size());

    // Parse the second frame
    std::size_t consumed2 = 0;
    std::vector<std::string> out2;
    auto st2 = tr::parse_resp_array(in.substr(consumed), consumed2, out2);

    EXPECT_EQ(st2, tr::RespParseStatus::Ok);
    ASSERT_EQ(out2.size(), 2u);
    EXPECT_EQ(out2[0], "GET");
    EXPECT_EQ(out2[1], "key");
    EXPECT_EQ(consumed2, second.size());
}
