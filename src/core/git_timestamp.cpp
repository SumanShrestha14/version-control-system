#include "git_timestamp.h"
#include <ctime>
#include <cstdio>
#include <cstdlib>

std::string GitTimestamp::now() {
    std::time_t t = std::time(nullptr);
    std::tm localTm{}, utcTm{};
#ifdef _WIN32
    localtime_s(&localTm, &t);
    gmtime_s(&utcTm, &t);
#else
    localtime_r(&t, &localTm);
    gmtime_r(&t, &utcTm);
#endif
    std::time_t localAsTimeT = std::mktime(&localTm);
    std::time_t utcAsTimeT = std::mktime(&utcTm);
    long offsetSeconds = static_cast<long>(localAsTimeT - utcAsTimeT);

    char sign = offsetSeconds >= 0 ? '+' : '-';
    long absMinutes = std::abs(offsetSeconds) / 60;
    long hours = absMinutes / 60;
    long minutes = absMinutes % 60;

    char buf[8];
    std::snprintf(buf, sizeof(buf), "%c%02ld%02ld", sign, hours, minutes);
    return std::to_string(static_cast<long long>(t)) + " " + buf;
}
