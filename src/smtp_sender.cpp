#include "../include/smtp_sender.hpp"
#include <curl/curl.h>
#include <cstdlib>
#include <sstream>
#include <ctime>

SmtpConfig SmtpConfig::from_env() {
    SmtpConfig cfg;
    cfg.host     = std::getenv("SMTP_HOST") ? std::getenv("SMTP_HOST") : "localhost";
    cfg.port     = std::getenv("SMTP_PORT") ? std::stoi(std::getenv("SMTP_PORT")) : 1025;
    cfg.username = std::getenv("SMTP_USER") ? std::getenv("SMTP_USER") : "";
    cfg.password = std::getenv("SMTP_PASS") ? std::getenv("SMTP_PASS") : "";
    return cfg;
}

// libcurl precisa de um callback pra ler o body do email
struct MailPayload { std::string data; size_t pos = 0; };

static size_t read_callback(char* buf, size_t size, size_t nmemb, void* userp) {
    auto* p = static_cast<MailPayload*>(userp);
    size_t avail = p->data.size() - p->pos;
    size_t copy  = std::min(avail, size * nmemb);
    memcpy(buf, p->data.data() + p->pos, copy);
    p->pos += copy;
    return copy;
}

SmtpResult send_email(const SmtpConfig& cfg, const Event& event) {
    CURL* curl = curl_easy_init();
    if (!curl) return {false, "curl_easy_init failed"};

    std::string url = "smtp://" + cfg.host + ":" + std::to_string(cfg.port);

    // monta o body do email
    std::ostringstream body;
    body << "To: "      << event.email << "\r\n"
         << "From: notifications@sistema.com\r\n"
         << "Subject: " << event.type << "\r\n"
         << "\r\n"
         << "Notification: " << event.type << "\r\n";

    MailPayload payload{body.str()};

    curl_slist* recipients = curl_slist_append(nullptr, event.email.c_str());

    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM,      "notifications@sistema.com");
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT,      recipients);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION,   read_callback);
    curl_easy_setopt(curl, CURLOPT_READDATA,       &payload);
    curl_easy_setopt(curl, CURLOPT_UPLOAD,         1L);

    if (!cfg.username.empty()) {
        curl_easy_setopt(curl, CURLOPT_USERNAME, cfg.username.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, cfg.password.c_str());
    }

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        return {false, curl_easy_strerror(res)};
    return {true, ""};
}
