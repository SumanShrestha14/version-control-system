#pragma once
#include <string>
#include <vector>
#include <curl/curl.h>

struct HttpResponse {
    long statusCode;
    std::string body;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Owns a curl handle -- no accidental copies of the underlying resource.
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    HttpResponse get(const std::string& url, const std::vector<std::string>& headers);
    HttpResponse post(const std::string& url, const std::string& body,
                       const std::vector<std::string>& headers);

private:
    CURL* curl_;
    HttpResponse perform(const std::string& url, const std::vector<std::string>& headers,
                         const std::string* postBody);
};
