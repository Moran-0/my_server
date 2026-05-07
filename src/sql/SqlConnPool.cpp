#include "SqlConnPool.h"
#include "Logging.h"
MysqlConnection::MysqlConnection() : m_conn(mysql_init(nullptr), mysql_close) {
    if (!m_conn) {
        throw std::runtime_error("Failed to initialize MySQL connection");
    }
}

bool MysqlConnection::Connect(const std::string& host, int port, const std::string& user, const std::string& passwd, const std::string& db) {
    // 设置字符集
    mysql_options(m_conn.get(), MYSQL_SET_CHARSET_NAME, "utf8mb4");
    if (!mysql_real_connect(m_conn.get(), host.c_str(), user.c_str(), passwd.c_str(), db.c_str(), port, nullptr, 0)) {
        m_lastError = mysql_error(m_conn.get());
        LOG_ERROR << "Failed to connect to MySQL: " << m_lastError;
        return false;
    }
    m_connected = true;
    LOG_INFO << "Connected to MySQL successfully";
    return true;
}

ResultSet MysqlConnection::Query(const std::string& sql) {
    ResultSet result;

    if (mysql_query(m_conn.get(), sql.c_str())) {
        m_lastError = mysql_error(m_conn.get());
        result.SetError(m_lastError);
        return result;
    }

    MYSQL_RES* rawResult = mysql_store_result(m_conn.get());
    if (!rawResult) {
        if (mysql_field_count(m_conn.get()) == 0) {
            // 非 SELECT 语句（INSERT/UPDATE/DELETE），返回空成功结果集
            return result;
        }
        m_lastError = mysql_error(m_conn.get());
        result.SetError(m_lastError);
        return result;
    }

    std::unique_ptr<MYSQL_RES, decltype(&mysql_free_result)> res(rawResult, mysql_free_result);

    // 读取列名
    int numFields = mysql_num_fields(res.get());
    MYSQL_FIELD* fields = mysql_fetch_fields(res.get());
    std::vector<std::string> columns;
    columns.reserve(numFields);
    for (int i = 0; i < numFields; ++i) {
        columns.emplace_back(fields[i].name);
    }
    result.SetColumns(std::move(columns));

    // 读取所有行
    MYSQL_ROW row;
    unsigned long* lengths;
    while ((row = mysql_fetch_row(res.get()))) {
        lengths = mysql_fetch_lengths(res.get());
        ResultSet::Row r;
        r.reserve(numFields);
        for (int i = 0; i < numFields; ++i) {
            r.emplace_back(row[i] ? std::string(row[i], lengths[i]) : "");
        }
        result.PushRow(std::move(r));
    }

    return result;
}

long long MysqlConnection::Execute(const std::string& sql) {
    if (mysql_query(m_conn.get(), sql.c_str())) {
        m_lastError = mysql_error(m_conn.get());
        return -1;
    }
    // 释放可能的结果集（如果是SELECT类语句）
    MYSQL_RES* result = mysql_store_result(m_conn.get());
    if (result) {
        mysql_free_result(result);
    }
    return static_cast<long long>(mysql_affected_rows(m_conn.get()));
}

std::string MysqlConnection::Escape(const std::string& value) {
    std::string out(value.size() * 2 + 1, '\0');
    unsigned long len = mysql_real_escape_string(m_conn.get(), out.data(), value.data(), value.size());
    out.resize(len);
    return out;
}

PoolGuard::PoolGuard(MysqlConnection* conn, std::function<void(MysqlConnection*)> release) : m_conn(conn), m_release(std::move(release)) {}

PoolGuard::~PoolGuard() {
    if (m_release && m_conn) {
        m_release(m_conn);
    }
}

MysqlPool& MysqlPool::Instance() {
    static MysqlPool instance;
    return instance;
}

void MysqlPool::Init(const std::string& host, int port, const std::string& user, const std::string& passwd, const std::string& db, size_t poolSize) {
    if (m_initialed.exchange(true)) {
        return;
    }
    for (size_t i = 0; i < poolSize; ++i) {
        auto conn = std::make_unique<MysqlConnection>();
        if (!conn->Connect(host, port, user, passwd, db)) {
            LOG_ERROR << "Failed to connect to MySQL";
            throw std::runtime_error("MysqlPool initialization failed");
        }
        m_pool.push(std::move(conn));
    }
    m_poolSize = poolSize;
}

PoolGuard MysqlPool::Borrow() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return !m_pool.empty(); });
    if (m_shutdown) {
        LOG_ERROR << "Borrowing while MysqlPool is shutting down";
        throw std::runtime_error("MysqlPool is shutting down");
    }
    auto conn = std::move(m_pool.front());
    m_pool.pop();
    return {conn.release(), [this](MysqlConnection* conn) { this->ReturnConn(conn); }};
}

void MysqlPool::Shutdown() {
    m_shutdown.store(true);
    m_cv.notify_all();
}

MysqlPool::~MysqlPool() {
    Shutdown();
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_pool.empty()) {
        m_pool.pop();
    }
}

void MysqlPool::ReturnConn(MysqlConnection* conn) {
    auto connPtr = std::unique_ptr<MysqlConnection>(conn); // 重新获取所有权，自动释放
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_shutdown) {
        LOG_ERROR << "Returning connection while MysqlPool is shutting down";
        return;
    }
    if (m_pool.size() < m_poolSize) {
        m_pool.push(std::move(connPtr));
        m_cv.notify_one();
    }
}
