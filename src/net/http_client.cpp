#include "http_client.h"
#include <curl/curl.h>
#include <stdexcept>
namespace {
size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}
}

HttpClient::HttpClient() {
    curl_ = curl_easy_init();
    if (!curl_) {
        throw std::runtime_error("HttpClient: curl_easy_init failed");
    }
}

HttpClient::~HttpClient() {
    curl_easy_cleanup(curl_);   // always runs, even if an exception unwinds through here
}

HttpResponse HttpClient::perform(const std::string& url,
                                  const std::vector<std::string>& headers,
                                  const std::string* postBody) {
    std::string body;
    struct curl_slist* headerList = nullptr;
    for (const auto& h : headers) {
        headerList = curl_slist_append(headerList, h.c_str());
    }

    curl_easy_reset(curl_);   // clear any options left over from a previous call on this handle
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_, CURLOPT_USERAGENT, "git/2.40.0");
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl_, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(curl_, CURLOPT_LOW_SPEED_TIME, 30L);
    if (headerList) {
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headerList);
    }
    if (postBody) {
        curl_easy_setopt(curl_, CURLOPT_POST, 1L);
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, postBody->data());
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, static_cast<long>(postBody->size()));
    }

    CURLcode res = curl_easy_perform(curl_);
    long statusCode = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_slist_free_all(headerList);

    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("HttpClient: curl request failed: ") +
                                  curl_easy_strerror(res));
    }
    return {statusCode, body};
}

HttpResponse HttpClient::get(const std::string& url, const std::vector<std::string>& headers) {
    return perform(url, headers, nullptr);
}

HttpResponse HttpClient::post(const std::string& url, const std::string& body,
                               const std::vector<std::string>& headers) {
    return perform(url, headers, &body);
}
