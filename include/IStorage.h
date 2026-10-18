#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

class IStorage {
public:
    using Row = std::map<std::string, std::string>;
    using QueryResult = std::vector<Row>;

    virtual ~IStorage() = default;

    virtual bool open(const std::string& connectionString) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual bool execute(const std::string& sql) = 0;
    virtual QueryResult query(const std::string& sql) = 0;

    virtual bool execute(const std::string& sql,
                         const std::vector<std::string>& params) = 0;
    virtual QueryResult query(const std::string& sql,
                              const std::vector<std::string>& params) = 0;

    virtual bool beginTransaction() = 0;
    virtual bool commitTransaction() = 0;
    virtual bool rollbackTransaction() = 0;
};
