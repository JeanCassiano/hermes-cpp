#include "../include/httplib.h"
#include "../include/json.hpp"
#include "../include/thread_pool.hpp"
#include "../include/event.hpp"
#include <sqlite3.h>
#include <iostream>
#include <mutex>
#include <memory>
#include <vector>
#include <functional>

using json = nlohmann::json;

class DatabaseConnection {
public:
    explicit DatabaseConnection(const char* filename) : db(nullptr) {
        sqlite3_open(filename, &db);
    }
    ~DatabaseConnection() {
        if (db) {
            sqlite3_close(db);
        }
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
            id         INTEGER PRIMARY KEY AUTOINCREMENT,
            type       TEXT NOT NULL,
            email      TEXT NOT NULL,
            status     TEXT DEFAULT 'pending',
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    )", nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
    }
}

int save_event(sqlite3* db, const Event& e) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db,
        "INSERT INTO events (type, email) VALUES (?, ?)",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, e.type.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, e.email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    int id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    return id;
}

void update_status(sqlite3* db, int id, const std::string& status) {
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db,
        "UPDATE events SET status = ? WHERE id = ?",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,  2, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

// ─── Worker ──────────────────────────────────────────────────────────────────

void process_event(const Event& e) {
    std::cout << "[WORKER] Sending '" << e.type
              << "' notification to " << e.email << "\n";
}

// ─── Main ────────────────────────────────────────────────────────────────────

int main() {
    DatabaseConnection db_conn("notifications.db");
    sqlite3* db = db_conn.get();
    init_db(db);

    auto db_mutex = std::make_shared<std::mutex>();
    ThreadPool pool(4);
    httplib::Server server;

    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
    });

    server.Post("/notify", [&, db_mutex](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);

            if (!body.contains("type") || !body.contains("email")) {
                json error = {{"error", "Missing required fields: type, email"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            Event e;
            e.type  = body["type"].get<std::string>();
            e.email = body["email"].get<std::string>();

            int event_id;
            {
                std::unique_lock<std::mutex> lock(*db_mutex);
                event_id = save_event(db, e);
            }

            pool.enqueue([e, event_id, db, db_mutex]() {
                process_event(e);
                std::unique_lock<std::mutex> lock(*db_mutex);
                update_status(db, event_id, "sent");
            });

            json response = {
                {"status", "queued"},
                {"id",     event_id},
                {"event",  e.type}
            };
            res.set_content(response.dump(), "application/json");
        } catch (const json::exception& e) {
            json error = {{"error", "Invalid JSON"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });

    server.Get("/events", [&, db_mutex](const httplib::Request&, httplib::Response& res) {
        try {
            std::unique_lock<std::mutex> lock(*db_mutex);

            json events = json::array();
            sqlite3_stmt* stmt;
            int rc = sqlite3_prepare_v2(db, "SELECT id, type, email, status, created_at FROM events", -1, &stmt, nullptr);

            if (rc != SQLITE_OK) {
                json error = {{"error", "Database error"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                events.push_back({
                    {"id",         sqlite3_column_int(stmt,  0)},
                    {"type",       (const char*)sqlite3_column_text(stmt, 1)},
                    {"email",      (const char*)sqlite3_column_text(stmt, 2)},
                    {"status",     (const char*)sqlite3_column_text(stmt, 3)},
                    {"created_at", (const char*)sqlite3_column_text(stmt, 4)}
                });
            }
            sqlite3_finalize(stmt);

            res.set_content(events.dump(2), "application/json");
        } catch (const std::exception& e) {
            json error = {{"error", e.what()}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });

    std::cout << "[SERVER] Running on http://localhost:8080\n";
    server.listen("0.0.0.0", 8080);

    return 0;
}