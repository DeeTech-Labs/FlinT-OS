#pragma once
#ifdef _WIN32
#include <ctime>
#include <io.h>
inline std::tm *gmtime_r(const std::time_t *time, std::tm *output) {
    return gmtime_s(output, time) == 0 ? output : nullptr;
}
#define fsync _commit
#define fileno _fileno
#define unlink _unlink
#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
#endif
#else
#include <unistd.h>
#endif
