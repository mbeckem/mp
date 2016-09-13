#ifndef COMMON_UTIL_HPP
#define COMMON_UTIL_HPP

#include <chrono>
#include <fstream>
#include <string>
#include <system_error>

inline void try_open(std::fstream &f, const std::string &path, std::ios_base::openmode mode)
{
    f.open(path, mode);
    if (!f.is_open()) {
        throw std::system_error(errno, std::system_category());
    }
}

// Executes "f" and returns the time it took to execute it (in seconds).
template<typename Func>
double execution_seconds(Func &&f)
{
    using namespace std::chrono;

    auto t1 = high_resolution_clock::now();
    f();
    auto t2 = high_resolution_clock::now();

    double sec = duration_cast<duration<double>>(t2 - t1).count();
    return sec;
}

#endif // COMMON_UTIL_HPP
