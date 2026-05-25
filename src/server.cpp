#include "../include/httplib.h"
#include "../include/json.hpp"
#include "../include/thread_pool.hpp"
#include "../include/event.hpp"
#include "../include/logger.hpp"
#include "../include/metrics.hpp"
#include "../include/smtp_sender.hpp"
#include "../include/retry_scheduler.hpp"
#include "../include/priority_queue.hpp"
#include "../include/rate_limiter.hpp"
#include <sqlite3.h>
#include <iostream>
#include <mutex>
#include <memory>
#include <vector>
#include <functional>

using json = nlohmann::json;

// ─── Database RAII ───────────────────────────────────────────────────────────

class DatabaseConnection {
public:
    explicit DatabaseConnection(const char* filename) : db(nullptr) {
        sqlite3_open(filename, &db);
    }
    ~DatabaseConnection() {
        if (db) sqlite3_close(db);
    }
    sqlite3* get() const { return db; }
    DatabaseConnection(const DatabaseConnection&) = delete;
    DatabaseConnection& operator=(const DatabaseConnection&) = delete;
private:
    sqlite3* db;
};

// ─── Database ────────────────────────────────────────────────────────────────

void init_db(sqlite3* db) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db, R"(
        CREATE TABLE IF NOT EXISTS events (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            type          TEXT NOT NULL,
            email         TEXT NOT NULL,
            priority      INTEGER DEFAULT 2,
            status        TEXT DEFAULT 'pending',
            retry_count   INTEGER DEFAULT 0,
            max_retries   INTEGER DEFAULT 3,
            next_retry_at DATETIME,
            last_error    TEXT,
            created_at    DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at    DATETIME DEFAULT CURRENT_TIMESTAMP
        );
        CREATE TABLE IF NOT EXISTS dlq (
            id             INTEGER PRIMARY KEY AUTOINCREMENT,
            event_id       INTEGER REFERENCES events(id),
            type           TEXT NOT NULL,
            email          TEXT NOT NULL,
            failure_reason TEXT,
            retry_count    INTEGER,
            moved_at       DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )", nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << "\n";
        sqlite3_free(err_msg);
    }
}

int save_event(sqlite3* db, const Event& e) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db,
        "INSERT INTO events (type, email, priority) VALUES (?, ?, ?)",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, e.type.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, e.email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 3, static_cast<int>(e.priority));
    sqlite3_step(stmt);
    int id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return id;
}

void update_status(sqlite3* db, int id, const std::string& status, const Event& e) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db,
        "UPDATE events SET status=?, retry_count=?, last_error=?, updated_at=CURRENT_TIMESTAMP WHERE id=?",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, status.c_str(),        -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 2, e.retry_count);
    sqlite3_bind_text(stmt, 3, e.last_error.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 4, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void move_to_dlq(sqlite3* db, const Event& e, const std::string& reason) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db,
        "INSERT INTO dlq (event_id, type, email, failure_reason, retry_count) VALUES (?,?,?,?,?)",
        -1, &stmt, nullptr);
    sqlite3_bind_int (stmt, 1, e.id);
    sqlite3_bind_text(stmt, 2, e.type.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, e.email.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, reason.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 5, e.retry_count);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
    DatabaseConnection db_conn("notifications.db");
    sqlite3* db = db_conn.get();
    init_db(db);

    auto db_mutex = std::make_shared<std::mutex>();
    SmtpConfig smtp = SmtpConfig::from_env();

    PriorityEventQueue event_queue;
    RetryScheduler     scheduler(event_queue);
    RateLimiter        rate_limiter(5, 10);
    ThreadPool         pool(4);
    httplib::Server    server;

    // Dispatcher: consome da priority queue e despacha pro ThreadPool
    std::thread dispatcher([&] {
        while (true) {
            Event e = event_queue.pop();
            if (e.id == -1) break; // sentinel de shutdown

            rate_limiter.acquire();

            pool.enqueue([e, smtp, &scheduler, db, db_mutex]() mutable {
                auto start = std::chrono::steady_clock::now();

                auto result = send_email(smtp, e);

                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start).count();

                std::unique_lock<std::mutex> lock(*db_mutex);

                if (result.success) {
                    metrics().record_sent(ms);
                    update_status(db, e.id, "sent", e);
                    Logger::info("Sent '" + e.type + "' to " + e.email);
                } else if (e.retry_count < e.max_retries) {
                    int delay = 1 << e.retry_count; // 2^n: 1s, 2s, 4s
                    e.retry_count++;
                    e.last_error = result.error;
                    update_status(db, e.id, "pending", e);
                    lock.unlock();
                    scheduler.schedule(e, delay);
                    Logger::warn("Retry " + std::to_string(e.retry_count) + "/" +
                                 std::to_string(e.max_retries) + " for event " +
                                 std::to_string(e.id) + " in " + std::to_string(delay) + "s");
                } else {
                    metrics().failed++;
                    metrics().dlq_total++;
                    move_to_dlq(db, e, result.error);
                    update_status(db, e.id, "failed", e);
                    Logger::error("Event " + std::to_string(e.id) + " moved to DLQ: " + result.error);
                }
            });
        }
    });

    // ─── Endpoints ───────────────────────────────────────────────────────────

    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
    });

    server.Post("/notify", [&, db_mutex](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);

            if (!body.contains("type") || !body.contains("email")) {
                res.status = 400;
                res.set_content(json{{"error", "Missing required fields: type, email"}}.dump(), "application/json");
                return;
            }

            Event e;
            e.type  = body["type"].get<std::string>();
            e.email = body["email"].get<std::string>();

            // priority opcional, default = normal
            if (body.contains("priority")) {
                std::string p = body["priority"].get<std::string>();
                if      (p == "critical") e.priority = Priority::CRITICAL;
                else if (p == "high")     e.priority = Priority::HIGH;
                else                      e.priority = Priority::NORMAL;
            }

            e.created_at = std::chrono::system_clock::now();

            {
                std::unique_lock<std::mutex> lock(*db_mutex);
                e.id = save_event(db, e);
            }

            event_queue.push(e);

            Logger::info("Queued event " + std::to_string(e.id) + " [" + e.type + "] -> " + e.email);

            res.set_content(json{
                {"status", "queued"},
                {"id",     e.id},
                {"event",  e.type}
            }.dump(), "application/json");

        } catch (const json::exception&) {
            res.status = 400;
            res.set_content(json{{"error", "Invalid JSON"}}.dump(), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"error", ex.what()}}.dump(), "application/json");
        }
    });

    server.Get("/events", [&, db_mutex](const httplib::Request&, httplib::Response& res) {
        try {
            std::unique_lock<std::mutex> lock(*db_mutex);
            json events = json::array();
            sqlite3_stmt* stmt;
            int rc = sqlite3_prepare_v2(db,
                "SELECT id, type, email, priority, status, retry_count, last_error, created_at FROM events",
                -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                res.status = 500;
                res.set_content(json{{"error", "Database error"}}.dump(), "application/json");
                return;
            }
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                events.push_back({
                    {"id",          sqlite3_column_int (stmt, 0)},
                    {"type",        (const char*)sqlite3_column_text(stmt, 1)},
                    {"email",       (const char*)sqlite3_column_text(stmt, 2)},
                    {"priority",    sqlite3_column_int (stmt, 3)},
                    {"status",      (const char*)sqlite3_column_text(stmt, 4)},
                    {"retry_count", sqlite3_column_int (stmt, 5)},
                    {"last_error",  sqlite3_column_text(stmt, 6) ? (const char*)sqlite3_column_text(stmt, 6) : ""},
                    {"created_at",  (const char*)sqlite3_column_text(stmt, 7)}
                });
            }
            sqlite3_finalize(stmt);
            res.set_content(events.dump(2), "application/json");
        } catch (const std::exception& ex) {
            res.status = 500;
            res.set_content(json{{"error", ex.what()}}.dump(), "application/json");
        }
    });

    server.Get("/dlq", [&, db_mutex](const httplib::Request&, httplib::Response& res) {
        std::unique_lock<std::mutex> lock(*db_mutex);
        json items = json::array();
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db,
            "SELECT id, event_id, type, email, failure_reason, retry_count, moved_at FROM dlq",
            -1, &stmt, nullptr);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            items.push_back({
                {"id",             sqlite3_column_int(stmt, 0)},
                {"event_id",       sqlite3_column_int(stmt, 1)},
                {"type",           (const char*)sqlite3_column_text(stmt, 2)},
                {"email",          (const char*)sqlite3_column_text(stmt, 3)},
                {"failure_reason", sqlite3_column_text(stmt, 4) ? (const char*)sqlite3_column_text(stmt, 4) : ""},
                {"retry_count",    sqlite3_column_int(stmt, 5)},
                {"moved_at",       (const char*)sqlite3_column_text(stmt, 6)}
            });
        }
        sqlite3_finalize(stmt);
        res.set_content(items.dump(2), "application/json");
    });

    server.Post("/dlq/:id/retry", [&, db_mutex](const httplib::Request& req, httplib::Response& res) {
        int dlq_id = std::stoi(req.path_params.at("id"));

        std::unique_lock<std::mutex> lock(*db_mutex);
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db,
            "SELECT event_id, type, email FROM dlq WHERE id = ?",
            -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, dlq_id);

        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            res.status = 404;
            res.set_content(json{{"error", "DLQ entry not found"}}.dump(), "application/json");
            return;
        }

        Event e;
        e.id    = sqlite3_column_int(stmt, 0);
        e.type  = (const char*)sqlite3_column_text(stmt, 1);
        e.email = (const char*)sqlite3_column_text(stmt, 2);
        e.retry_count = 0;
        e.created_at  = std::chrono::system_clock::now();
        sqlite3_finalize(stmt);

        // remove da DLQ e volta para pending
        sqlite3_prepare_v2(db, "DELETE FROM dlq WHERE id = ?", -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, dlq_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        update_status(db, e.id, "pending", e);
        lock.unlock();

        event_queue.push(e);

        Logger::info("Event " + std::to_string(e.id) + " re-queued from DLQ");
        res.set_content(json{{"status", "requeued"}, {"event_id", e.id}}.dump(), "application/json");
    });

    server.Get("/metrics", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(metrics().snapshot().dump(2), "application/json");
    });

    // ─── Shutdown ────────────────────────────────────────────────────────────

    server.set_pre_routing_handler([&](const httplib::Request& req, httplib::Response&) {
        if (req.path == "/shutdown") {
            server.stop();
            return httplib::Server::HandlerResponse::Unhandled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    Logger::info("Server running on http://localhost:8080");
    server.listen("0.0.0.0", 8080);

    // Encerra o dispatcher graciosamente
    Event sentinel;
    sentinel.id = -1;
    event_queue.push(sentinel);
    dispatcher.join();

    pool.wait();

    return 0;
}
