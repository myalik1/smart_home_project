#include "../include/SQLiteStorage.h"

#include <stdexcept>

// ─── Конструкторы / деструктор ───────────────────────────────────────────────

SQLiteStorage::SQLiteStorage() : db_(nullptr) {}

SQLiteStorage::~SQLiteStorage() {
    close();
}

SQLiteStorage::SQLiteStorage(SQLiteStorage&& other) noexcept
    : db_(other.db_), lastError_(std::move(other.lastError_)) {
    other.db_ = nullptr;
}

SQLiteStorage& SQLiteStorage::operator=(SQLiteStorage&& other) noexcept {
    if (this != &other) {
        close();
        db_ = other.db_;
        lastError_ = std::move(other.lastError_);
        other.db_ = nullptr;
    }
    return *this;
}

// ─── Управление соединением ──────────────────────────────────────────────────

bool SQLiteStorage::open(const std::string& connectionString) {
    if (db_) {
        close();
    }

    int rc = sqlite3_open(connectionString.c_str(), &db_);
    if (rc != SQLITE_OK) {
        setLastError("open");
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    // Включаем WAL-режим для лучшей производительности при параллельных чтениях
    execute("PRAGMA journal_mode=WAL;");
    // Включаем проверку внешних ключей
    execute("PRAGMA foreign_keys=ON;");

    return true;
}

void SQLiteStorage::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool SQLiteStorage::isOpen() const {
    return db_ != nullptr;
}

// ─── Выполнение запросов ─────────────────────────────────────────────────────

bool SQLiteStorage::execute(const std::string& sql) {
    if (!db_) {
        lastError_ = "Database is not open";
        return false;
    }

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);

    if (rc != SQLITE_OK) {
        lastError_ = errMsg ? std::string(errMsg) : "Unknown error";
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

IStorage::QueryResult SQLiteStorage::query(const std::string& sql) {
    return query(sql, {});
}

bool SQLiteStorage::execute(const std::string& sql,
                            const std::vector<std::string>& params) {
    sqlite3_stmt* stmt = prepareStatement(sql, params);
    if (!stmt) {
        return false;
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        setLastError("execute");
        return false;
    }

    return true;
}

IStorage::QueryResult SQLiteStorage::query(const std::string& sql,
                                           const std::vector<std::string>& params) {
    sqlite3_stmt* stmt = prepareStatement(sql, params);
    if (!stmt) {
        return {};
    }

    QueryResult result = extractRows(stmt);
    sqlite3_finalize(stmt);
    return result;
}

// ─── Транзакции ──────────────────────────────────────────────────────────────

bool SQLiteStorage::beginTransaction() {
    return execute("BEGIN TRANSACTION;");
}

bool SQLiteStorage::commitTransaction() {
    return execute("COMMIT;");
}

bool SQLiteStorage::rollbackTransaction() {
    return execute("ROLLBACK;");
}

// ─── Вспомогательные методы ──────────────────────────────────────────────────

std::string SQLiteStorage::getLastError() const {
    return lastError_;
}

sqlite3_stmt* SQLiteStorage::prepareStatement(const std::string& sql,
                                              const std::vector<std::string>& params) {
    if (!db_) {
        lastError_ = "Database is not open";
        return nullptr;
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

    if (rc != SQLITE_OK) {
        setLastError("prepare");
        return nullptr;
    }

    // Привязываем параметры (индексация с 1)
    for (size_t i = 0; i < params.size(); ++i) {
        rc = sqlite3_bind_text(stmt,
                               static_cast<int>(i + 1),
                               params[i].c_str(),
                               -1,
                               SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            setLastError("bind");
            sqlite3_finalize(stmt);
            return nullptr;
        }
    }

    return stmt;
}

IStorage::QueryResult SQLiteStorage::extractRows(sqlite3_stmt* stmt) {
    QueryResult result;

    int colCount = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Row row;
        for (int i = 0; i < colCount; ++i) {
            const char* colName = sqlite3_column_name(stmt, i);
            const unsigned char* colValue = sqlite3_column_text(stmt, i);

            row[colName] = colValue ? reinterpret_cast<const char*>(colValue) : "";
        }
        result.push_back(std::move(row));
    }

    return result;
}

void SQLiteStorage::setLastError(const std::string& context) {
    if (db_) {
        lastError_ = context + ": " + sqlite3_errmsg(db_);
    } else {
        lastError_ = context + ": database handle is null";
    }
}
