#include "http.h"
#include <curl/curl.h>
#include <stdexcept>

namespace {
size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
  auto *out = static_cast<std::string *>(userdata);
  out->append(ptr, size * nmemb);
  return size * nmemb;
}

CURL *setup_common(const std::string &url, std::string &body_out,
                   const std::vector<std::string> &headers,
                   struct curl_slist *&header_list) {
  CURL *curl = curl_easy_init();
  if (!curl)
    throw std::runtime_error("curl_easy_init failed");
  for (const auto &h : headers)
    header_list = curl_slist_append(header_list, h.c_str());

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body_out);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,
                   "git/2.40.0"); // some servers gate on this
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT,
                   15L); // fail fast if we can't even connect
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L); // bytes/sec
  curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME,
                   30L); // abort if under that rate for 30s straight
  if (header_list)
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
  return curl;
}
} // namespace

HttpResponse http_get(const std::string &url,
                      const std::vector<std::string> &headers) {
  std::string body;
  struct curl_slist *header_list = nullptr;
  CURL *curl = setup_common(url, body, headers, header_list);

  CURLcode res = curl_easy_perform(curl);
  long status_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

  curl_slist_free_all(header_list);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK)
    throw std::runtime_error(std::string("curl GET failed: ") +
                             curl_easy_strerror(res));
  return {status_code, body};
}

HttpResponse http_post(const std::string &url, const std::string &post_body,
                       const std::vector<std::string> &headers) {
  std::string body;
  struct curl_slist *header_list = nullptr;
  CURL *curl = setup_common(url, body, headers, header_list);

  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body.data());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)post_body.size());

  CURLcode res = curl_easy_perform(curl);
  long status_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

  curl_slist_free_all(header_list);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK)
    throw std::runtime_error(std::string("curl POST failed: ") +
                             curl_easy_strerror(res));
  return {status_code, body};
}
