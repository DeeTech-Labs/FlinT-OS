#pragma once
#include <chrono>
#include <ctime>
namespace flint::hardware {
inline bool rtcFromEpoch(uint64_t epoch, std::tm &utc) {
    using namespace std::chrono;
    if (epoch < 946684800 || epoch >= 4102444800)
        return false; // PCF85063 2000..2099
    const sys_seconds time{seconds{int64_t(epoch)}};
    const auto date = floor<days>(time);
    const year_month_day calendar{date};
    const hh_mm_ss clock{time - date};
    utc = {};
    utc.tm_year = int(calendar.year()) - 1900;
    utc.tm_mon = unsigned(calendar.month()) - 1;
    utc.tm_mday = unsigned(calendar.day());
    utc.tm_wday = weekday{date}.c_encoding();
    utc.tm_hour = clock.hours().count();
    utc.tm_min = clock.minutes().count();
    utc.tm_sec = clock.seconds().count();
    return true;
}
inline uint64_t rtcToEpoch(const std::tm &utc) {
    using namespace std::chrono;
    const year_month_day calendar{
        year{utc.tm_year + 1900}, month{unsigned(utc.tm_mon + 1)}, day{unsigned(utc.tm_mday)}};
    return duration_cast<seconds>(sys_days{calendar}.time_since_epoch()).count() +
        utc.tm_hour * 3600 + utc.tm_min * 60 + utc.tm_sec;
}
} // namespace flint::hardware
