#include "../include/smtp_sender.hpp"
#include "../include/logger.hpp"
#include <thread>
#include <chrono>
#include <cstdlib>

SmtpConfig SmtpConfig::from_env() {
    SmtpConfig cfg;
    cfg.host     = std::getenv("SMTP_HOST") ? std::getenv("SMTP_HOST") : "localhost";
    cfg.port     = std::getenv("SMTP_PORT") ? std::stoi(std::getenv("SMTP_PORT")) : 1025;
    cfg.username = std::getenv("SMTP_USER") ? std::getenv("SMTP_USER") : "";
    cfg.password = std::getenv("SMTP_PASS") ? std::getenv("SMTP_PASS") : "";
    return cfg;
}

// Simula SMTP com 20% de falha para testar retry/DLQ
SmtpResult send_email(const SmtpConfig&, const Event& event) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // latência simulada

    if (std::rand() % 5 == 0) // 20% de chance de falha
        return {false, "Connection refused (mock)"};

    Logger::info("[MOCK SMTP] Email sent: " + event.type + " -> " + event.email);
    return {true, ""};
}
