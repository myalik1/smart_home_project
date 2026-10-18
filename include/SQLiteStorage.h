#pragma once

#include "IStorage.h"

#include <sqlite3.h>
#include <string>
#include <vector>
#include <map>
#include <iostream>

/**
 * @brief Реализация IStorage на базе SQLite.
 *
 * Управляет жизненным циклом соединения с файловой БД SQLite.
 * Поддерживает параметризованные запросы и транзакции.
 */
class SQLiteStorage : public IStorage {
public:
    SQLiteStorage();
    ~SQLiteStorage() override;

    // Запрещаем копирование (sqlite3* не копируется)
    SQLiteStorage(const SQLiteStorage&) = delete;
    SQLiteStorage& operator=(const SQLiteStorage&) = delete;

    // Разрешаем перемещение
    SQLiteStorage(SQLiteStorage&& other) noexcept;
    SQLiteStorage& operator=(SQLiteStorage&& other) noexcept;

    bool open(const std::string& connectionString) override;
    void close() override;
    bool isOpen() const override;

    bool execute(const std::string& sql) override;
    QueryResult query(const std::string& sql) override;

    bool execute(const std::string& sql,
                 const std::vector<std::string>& params) override;
    QueryResult query(const std::string& sql,
                      const std::vector<std::string>& params) override;

    bool beginTransaction() override;
    bool commitTransaction() override;
    bool rollbackTransaction() override;

    /**
     * @brief Получить текст последней ошибки SQLite.
     */
    std::string getLastError() const;

private:
    sqlite3* db_;
    std::string lastError_;

    sqlite3_stmt* prepareStatement(const std::string& sql,
                                   const std::vector<std::string>& params);
    QueryResult extractRows(sqlite3_stmt* stmt);
    void setLastError(const std::string& context);
};
