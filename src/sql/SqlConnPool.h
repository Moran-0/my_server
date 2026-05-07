#pragma once
#include <mysql/mysql.h>
#include <iostream>
#include <string>
#include <memory>
#include <functional>
#include <utility>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unordered_map>

#include "Common.h"

/// @brief SELECT 查询结果集封装
class ResultSet : NoCopy {
  public:
    using Row = std::vector<std::string>;

    ResultSet() = default;
    ResultSet(ResultSet&& other) noexcept
        : m_ok(other.m_ok), m_error(std::move(other.m_error)),
          m_rows(std::move(other.m_rows)), m_columns(std::move(other.m_columns)),
          m_colMap(std::move(other.m_colMap)) {
        other.m_ok = true;
    }

    /// @brief 查询是否成功执行（无 SQL 错误）
    bool Ok() const { return m_ok; }
    /// @brief 错误信息（Ok() 为 false 时有效）
    const std::string& Error() const { return m_error; }
    /// @brief 是否有数据行
    bool Empty() const { return m_rows.empty(); }
    /// @brief 行数
    size_t Rows() const { return m_rows.size(); }
    /// @brief 列数
    size_t Columns() const { return m_columns.size(); }

    /// @brief 按行号访问整行
    const Row& operator[](size_t row) const { return m_rows[row]; }

    /// @brief 按行号和列名取值
    const std::string& Get(size_t row, const std::string& col) const {
        auto it = m_colMap.find(col);
        if (it == m_colMap.end()) {
            static const std::string s_empty;
            return s_empty;
        }
        return m_rows[row][it->second];
    }

  private:
    friend class MysqlConnection;

    bool m_ok = true;
    std::string m_error;
    std::vector<Row> m_rows;
    std::vector<std::string> m_columns;
    std::unordered_map<std::string, size_t> m_colMap;

    void SetError(const std::string& err) {
        m_ok = false;
        m_error = err;
    }
    void SetColumns(std::vector<std::string> cols) {
        m_columns = std::move(cols);
        m_colMap.clear();
        for (size_t i = 0; i < m_columns.size(); ++i) {
            m_colMap[m_columns[i]] = i;
        }
    }
    void PushRow(Row row) { m_rows.push_back(std::move(row)); }
};

/// @brief RAII封装MySQL连接
class MysqlConnection : NoCopy {
  public:
    MysqlConnection();
    bool Connect(const std::string& host, int port, const std::string& user,
                 const std::string& passwd, const std::string& db);
    /// @brief 执行 SELECT 查询，返回结构化结果集
    [[nodiscard]] ResultSet Query(const std::string& sql);
    /// @brief 执行 INSERT/UPDATE/DELETE，返回受影响行数，失败返回-1
    long long Execute(const std::string& sql);
    /// @brief 转义字符串防止SQL注入
    std::string Escape(const std::string& value);
    [[nodiscard]] std::string LastError() const { return m_lastError; }
    bool Ping() { return mysql_ping(m_conn.get()) == 0; }

  private:
    std::unique_ptr<MYSQL, decltype(&mysql_close)> m_conn;
    std::string m_lastError;
    bool m_connected = false;
};

/// @brief 连接池RAII借用
class PoolGuard : NoCopy {
  public:
    PoolGuard(MysqlConnection* conn, std::function<void(MysqlConnection*)> release);
    ~PoolGuard();
    PoolGuard(PoolGuard&& other) noexcept
        : m_conn(std::exchange(other.m_conn, nullptr)), m_release(std::move(other.m_release)){};
    MysqlConnection* operator->() { return m_conn; }
    MysqlConnection& operator*() { return *m_conn; }

  private:
    MysqlConnection* m_conn;
    std::function<void(MysqlConnection*)> m_release;
};

/// @brief MySQL连接池-单例模式
class MysqlPool : NoCopy {
  public:
    static MysqlPool& Instance();
    void Init(const std::string& host, int port, const std::string& user,
              const std::string& passwd, const std::string& db, size_t poolSize = 8);
    PoolGuard Borrow();
    size_t AvailableConns() const { return m_pool.size(); };
    void Shutdown();

  private:
    MysqlPool() = default;
    ~MysqlPool();
    void ReturnConn(MysqlConnection* conn);

    std::queue<std::unique_ptr<MysqlConnection>> m_pool;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_shutdown{false};
    std::atomic<bool> m_initialed{false};
    size_t m_poolSize{0};
};
