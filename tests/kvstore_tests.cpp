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