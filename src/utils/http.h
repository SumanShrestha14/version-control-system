#pragma once
#include <string>
#include <vector>

struct HttpResponse {
    long status_code;
    std::string body;
};

HttpResponse http_get(const std::string& url, const std::vector<std::string>& headers = {});
HttpResponse http_post(const std::string& url, const std::string& body,
                        const std::vector<std::string>& headers = {});
