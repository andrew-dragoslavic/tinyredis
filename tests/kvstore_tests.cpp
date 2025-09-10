#include <gtest/gtest.h>
#include "kvstore.hpp"
#include "repl.hpp"

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