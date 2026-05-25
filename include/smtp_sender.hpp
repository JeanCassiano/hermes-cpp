#pragma once
#include <string>
#include "event.hpp"


struct SmtpConfig {
    std::string host;
    int port;
    std::string username;
    std::string password;


    static SmtpConfig from_env();
};

struct SmtpResult {
    bool success;
    std::string error;
};

SmtpResult send_email(const SmtpConfig& cfg, const Event& event);