#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

/**
 * @brief Абстрактный интерфейс хранилища данных.
 *
 * Определяет контракт для любого хранилища (SQLite, PostgreSQL и т.д.).
 * Позволяет менять реализацию без изменения бизнес-логики.
 */
class IStorage {
public:
    /// Тип одной строки результата: имя_колонки -> значение
    using Row = std::map<std::string, std::string>;

    /// Тип результата запроса: вектор строк
    using QueryResult = std::vector<Row>;

    virtual ~IStorage() = default;

    /**
     * @brief Открыть соединение с хранилищем.
     * @param connectionString Путь к файлу БД или строка подключения.
     * @return true при успешном подключении.
     */
    virtual bool open(const std::string& connectionString) = 0;

    /**
     * @brief Закрыть соединение с хранилищем.
     */
    virtual void close() = 0;

    /**
     * @brief Проверить, открыто ли соединение.
     */
    virtual bool isOpen() const = 0;

    /**
     * @brief Выполнить SQL-запрос без возврата данных (INSERT, UPDATE, DELETE, CREATE).
     * @param sql SQL-запрос.
     * @return true при успешном выполнении.
     */
    virtual bool execute(const std::string& sql) = 0;

    /**
     * @brief Выполнить SQL-запрос с возвратом данных (SELECT).
     * @param sql SQL-запрос.
     * @return Вектор строк-результатов.
     */
    virtual QueryResult query(const std::string& sql) = 0;

    /**
     * @brief Выполнить параметризованный запрос без возврата данных.
     * @param sql SQL-запрос с плейсхолдерами (?).
     * @param params Значения параметров по порядку.
     * @return true при успешном выполнении.
     */
    virtual bool execute(const std::string& sql,
                         const std::vector<std::string>& params) = 0;

    /**
     * @brief Выполнить параметризованный SELECT-запрос.
     * @param sql SQL-запрос с плейсхолдерами (?).
     * @param params Значения параметров по порядку.
     * @return Вектор строк-результатов.
     */
    virtual QueryResult query(const std::string& sql,
                              const std::vector<std::string>& params) = 0;

    /**
     * @brief Начать транзакцию.
     * @return true при успешном начале.
     */
    virtual bool beginTransaction() = 0;

    /**
     * @brief Зафиксировать транзакцию.
     * @return true при успешном коммите.
     */
    virtual bool commitTransaction() = 0;

    /**
     * @brief Откатить транзакцию.
     * @return true при успешном откате.
     */
    virtual bool rollbackTransaction() = 0;
};
