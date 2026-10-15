/**
 * @file sqlite_test.cpp
 * @brief Тест SQLiteStorage — проверяет основные операции с БД.
 *
 * Запуск: ./sqlite_test
 * Создаёт временную БД test_storage.db, выполняет CRUD и удаляет файл.
 */

#include "../include/SQLiteStorage.h"

#include <cassert>
#include <iostream>
#include <cstdio>

const std::string TEST_DB = "test_storage.db";

void test_open_close() {
    std::cout << "  test_open_close... ";
    SQLiteStorage storage;

    assert(!storage.isOpen());
    assert(storage.open(TEST_DB));
    assert(storage.isOpen());

    storage.close();
    assert(!storage.isOpen());

    std::cout << "OK\n";
}

void test_create_and_insert() {
    std::cout << "  test_create_and_insert... ";
    SQLiteStorage storage;
    storage.open(TEST_DB);

    bool ok = storage.execute(
        "CREATE TABLE IF NOT EXISTS devices ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  type TEXT NOT NULL,"
        "  status TEXT DEFAULT 'off'"
        ");"
    );
    assert(ok);

    ok = storage.execute(
        "INSERT INTO devices (name, type, status) VALUES (?, ?, ?);",
        {"Lamp1", "lamp", "on"}
    );
    assert(ok);

    ok = storage.execute(
        "INSERT INTO devices (name, type, status) VALUES (?, ?, ?);",
        {"Radiator1", "radiator", "off"}
    );
    assert(ok);

    std::cout << "OK\n";
}

void test_query() {
    std::cout << "  test_query... ";
    SQLiteStorage storage;
    storage.open(TEST_DB);

    auto rows = storage.query("SELECT * FROM devices;");
    assert(rows.size() == 2);

    assert(rows[0]["name"] == "Lamp1");
    assert(rows[0]["type"] == "lamp");
    assert(rows[0]["status"] == "on");

    assert(rows[1]["name"] == "Radiator1");
    assert(rows[1]["type"] == "radiator");

    std::cout << "OK\n";
}

void test_parameterized_query() {
    std::cout << "  test_parameterized_query... ";
    SQLiteStorage storage;
    storage.open(TEST_DB);

    auto rows = storage.query(
        "SELECT * FROM devices WHERE type = ?;",
        {"lamp"}
    );
    assert(rows.size() == 1);
    assert(rows[0]["name"] == "Lamp1");

    std::cout << "OK\n";
}

void test_update_and_delete() {
    std::cout << "  test_update_and_delete... ";
    SQLiteStorage storage;
    storage.open(TEST_DB);

    bool ok = storage.execute(
        "UPDATE devices SET status = ? WHERE name = ?;",
        {"off", "Lamp1"}
    );
    assert(ok);

    auto rows = storage.query("SELECT status FROM devices WHERE name = ?;", {"Lamp1"});
    assert(rows.size() == 1);
    assert(rows[0]["status"] == "off");

    ok = storage.execute("DELETE FROM devices WHERE name = ?;", {"Radiator1"});
    assert(ok);

    rows = storage.query("SELECT * FROM devices;");
    assert(rows.size() == 1);

    std::cout << "OK\n";
}

void test_transaction_commit() {
    std::cout << "  test_transaction_commit... ";
    SQLiteStorage storage;
    storage.open(TEST_DB);

    storage.execute("DELETE FROM devices;");

    assert(storage.beginTransaction());
    storage.execute("INSERT INTO devices (name, type) VALUES (?, ?);", {"TxDevice1", "test"});
    storage.execute("INSERT INTO devices (name, type) VALUES (?, ?);", {"TxDevice2", "test"});
    assert(storage.commitTransaction());

    auto rows = storage.query("SELECT * FROM devices WHERE type = 'test';");
    assert(rows.size() == 2);

    std::cout << "OK\n";
}

void test_transaction_rollback() {
    std::cout << "  test_transaction_rollback... ";
    SQLiteStorage storage;
    storage.open(TEST_DB);

    storage.execute("DELETE FROM devices;");
    storage.execute("INSERT INTO devices (name, type) VALUES (?, ?);", {"Keeper", "test"});

    assert(storage.beginTransaction());
    storage.execute("INSERT INTO devices (name, type) VALUES (?, ?);", {"Ghost1", "test"});
    storage.execute("INSERT INTO devices (name, type) VALUES (?, ?);", {"Ghost2", "test"});
    assert(storage.rollbackTransaction());

    auto rows = storage.query("SELECT * FROM devices;");
    assert(rows.size() == 1);
    assert(rows[0]["name"] == "Keeper");

    std::cout << "OK\n";
}

void test_move_semantics() {
    std::cout << "  test_move_semantics... ";
    SQLiteStorage storage1;
    storage1.open(TEST_DB);
    assert(storage1.isOpen());

    SQLiteStorage storage2 = std::move(storage1);
    assert(storage2.isOpen());
    assert(!storage1.isOpen());

    auto rows = storage2.query("SELECT * FROM devices;");
    assert(!rows.empty());

    std::cout << "OK\n";
}

void test_error_handling() {
    std::cout << "  test_error_handling... ";
    SQLiteStorage storage;

    // Запрос без открытого соединения
    assert(!storage.execute("SELECT 1;"));

    storage.open(TEST_DB);

    // Невалидный SQL
    assert(!storage.execute("INVALID SQL QUERY;"));
    assert(!storage.getLastError().empty());

    std::cout << "OK\n";
}

int main() {
    // Удаляем файл от предыдущего запуска
    std::remove(TEST_DB.c_str());

    std::cout << "[SQLiteStorage Tests]\n";

    test_open_close();
    test_create_and_insert();
    test_query();
    test_parameterized_query();
    test_update_and_delete();
    test_transaction_commit();
    test_transaction_rollback();
    test_move_semantics();
    test_error_handling();

    // Чистим за собой
    std::remove(TEST_DB.c_str());

    std::cout << "\n✅ All tests passed!\n";
    return 0;
}
