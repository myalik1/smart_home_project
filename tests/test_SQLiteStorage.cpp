#include "../include/SQLiteStorage.h"

#include <gtest/gtest.h>
#include <cstdio>

class SQLiteStorageTest : public ::testing::Test {
protected:
    const std::string TEST_DB = "test_storage.db";
    SQLiteStorage storage;

    void SetUp() override {
        std::remove(TEST_DB.c_str());
        ASSERT_TRUE(storage.open(TEST_DB));
    }

    void TearDown() override {
        storage.close();
        std::remove(TEST_DB.c_str());
    }
};

TEST_F(SQLiteStorageTest, OpenAndClose) {
    EXPECT_TRUE(storage.isOpen());
    storage.close();
    EXPECT_FALSE(storage.isOpen());
}

TEST_F(SQLiteStorageTest, OpenInvalidPath) {
    SQLiteStorage bad;
    EXPECT_FALSE(bad.open("/nonexistent/dir/test.db"));
    EXPECT_FALSE(bad.isOpen());
}

TEST_F(SQLiteStorageTest, CreateTableAndInsert) {
    EXPECT_TRUE(storage.execute(
        "CREATE TABLE devices ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  type TEXT NOT NULL,"
        "  status TEXT DEFAULT 'off'"
        ");"
    ));

    EXPECT_TRUE(storage.execute(
        "INSERT INTO devices (name, type, status) VALUES (?, ?, ?);",
        {"Lamp1", "lamp", "on"}
    ));

    EXPECT_TRUE(storage.execute(
        "INSERT INTO devices (name, type, status) VALUES (?, ?, ?);",
        {"Radiator1", "radiator", "off"}
    ));
}

TEST_F(SQLiteStorageTest, QueryAll) {
    storage.execute(
        "CREATE TABLE devices (id INTEGER PRIMARY KEY, name TEXT, type TEXT);"
    );
    storage.execute("INSERT INTO devices (name, type) VALUES (?, ?);", {"Lamp1", "lamp"});
    storage.execute("INSERT INTO devices (name, type) VALUES (?, ?);", {"Plug1", "plug"});

    auto rows = storage.query("SELECT * FROM devices;");
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0]["name"], "Lamp1");
    EXPECT_EQ(rows[1]["name"], "Plug1");
}

TEST_F(SQLiteStorageTest, ParameterizedQuery) {
    storage.execute(
        "CREATE TABLE devices (id INTEGER PRIMARY KEY, name TEXT, type TEXT);"
    );
    storage.execute("INSERT INTO devices (name, type) VALUES (?, ?);", {"Lamp1", "lamp"});
    storage.execute("INSERT INTO devices (name, type) VALUES (?, ?);", {"Plug1", "plug"});

    auto rows = storage.query("SELECT * FROM devices WHERE type = ?;", {"lamp"});
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0]["name"], "Lamp1");
}

TEST_F(SQLiteStorageTest, UpdateAndDelete) {
    storage.execute("CREATE TABLE devices (id INTEGER PRIMARY KEY, name TEXT, status TEXT);");
    storage.execute("INSERT INTO devices (name, status) VALUES (?, ?);", {"Lamp1", "on"});

    EXPECT_TRUE(storage.execute("UPDATE devices SET status = ? WHERE name = ?;", {"off", "Lamp1"}));

    auto rows = storage.query("SELECT status FROM devices WHERE name = ?;", {"Lamp1"});
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0]["status"], "off");

    EXPECT_TRUE(storage.execute("DELETE FROM devices WHERE name = ?;", {"Lamp1"}));
    rows = storage.query("SELECT * FROM devices;");
    EXPECT_TRUE(rows.empty());
}

TEST_F(SQLiteStorageTest, TransactionCommit) {
    storage.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, val TEXT);");

    EXPECT_TRUE(storage.beginTransaction());
    storage.execute("INSERT INTO t (val) VALUES (?);", {"a"});
    storage.execute("INSERT INTO t (val) VALUES (?);", {"b"});
    EXPECT_TRUE(storage.commitTransaction());

    auto rows = storage.query("SELECT * FROM t;");
    EXPECT_EQ(rows.size(), 2u);
}

TEST_F(SQLiteStorageTest, TransactionRollback) {
    storage.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, val TEXT);");
    storage.execute("INSERT INTO t (val) VALUES (?);", {"keeper"});

    EXPECT_TRUE(storage.beginTransaction());
    storage.execute("INSERT INTO t (val) VALUES (?);", {"ghost"});
    EXPECT_TRUE(storage.rollbackTransaction());

    auto rows = storage.query("SELECT * FROM t;");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0]["val"], "keeper");
}

TEST_F(SQLiteStorageTest, MoveConstructor) {
    SQLiteStorage moved = std::move(storage);
    EXPECT_TRUE(moved.isOpen());
    EXPECT_FALSE(storage.isOpen());

    auto rows = moved.query("SELECT 1 AS val;");
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0]["val"], "1");
}

TEST_F(SQLiteStorageTest, ExecuteWithoutOpen) {
    SQLiteStorage closed;
    EXPECT_FALSE(closed.execute("SELECT 1;"));
}

TEST_F(SQLiteStorageTest, InvalidSQL) {
    EXPECT_FALSE(storage.execute("INVALID SQL;"));
    EXPECT_FALSE(storage.getLastError().empty());
}
