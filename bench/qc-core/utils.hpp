#pragma once

#include <cmath>

#include <fstream>
#include <codecvt>
#include <string>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <vector>

#include <qc-core/core.hpp>

namespace qc::utils
{
    template <typename T>
    inline T pairwiseSum(const size_t n, const T * const vals)
    {
        if (n == 0u) return T(0);
        if (n == 1u) return vals[0];
        if (n == 2u) return vals[0] + vals[1];
        return pairwiseSum(n >> 1, vals) + pairwiseSum((n + 1u) >> 1, vals + (n >> 1));
    }

    // Throws `std::system_error` on failure
    inline std::vector<std::byte> readFile(const std::filesystem::path & path)
    {
        const uintmax_t size{std::filesystem::file_size(path)};
        if (size > qc::min(uintmax_t(std::numeric_limits<size_t>::max()), uintmax_t(std::numeric_limits<std::streamsize>::max()))) {
            throw std::system_error(std::make_error_code(std::errc::file_too_large));
        }

        std::vector<std::byte> data((size_t(size))); // TODO: default initializes its memory - potential performance concern for large files

        std::ifstream ifs(path, std::ios::binary);
        ifs.exceptions(std::ios::badbit | std::ios::failbit);
        ifs.read(reinterpret_cast<char *>(data.data()), std::streamsize(size));

        return data;
    }

    // Throws `std::system_error` on failure
    inline std::string readAsciiFile(const std::filesystem::path & path)
    {
        const uintmax_t size{std::filesystem::file_size(path)};
        if (size > qc::min(uintmax_t(std::numeric_limits<size_t>::max()), uintmax_t(std::numeric_limits<std::streamsize>::max()))) {
            throw std::system_error(std::make_error_code(std::errc::file_too_large));
        }

        std::string str(size_t(size), '\0'); // TODO: explicitly initializes its memory - potential performance concern for large files

        std::ifstream ifs(path, std::ios::binary);
        ifs.exceptions(std::ios::badbit | std::ios::failbit);
        ifs.read(&str.front(), std::streamsize(size));

        return str;
    }

    // Throws `std::system_error` on failure
    inline void writeFile(const std::filesystem::path & path, const void * const data, const size_t size)
    {
        std::ofstream ofs(path, std::ios::out | std::ios::binary);
        ofs.exceptions(std::ios::badbit | std::ios::failbit);
        ofs.write(reinterpret_cast<const char *>(data), size);
    }

    // Throws `std::system_error` on failure
    inline void writeAsciiFile(const std::filesystem::path & path, const std::string_view str)
    {
        writeFile(path, str.data(), str.size());
    }

    //
    // Copies one set of data into another with some stride.
    // `stride` must be at least as large as the size of `T` and must be an even multiple of the alignment of `T`
    //
    template <typename Iter, typename T>
    inline T interlace(const Iter first, const Iter last, T * dst, const size_t stride)
    {
        static_assert(std::is_same_v<std::decay_t<decltype(*first)>, T>);

        for (Iter iter(first); iter != last; ++iter) {
            *dst = *iter;

            reinterpret_cast<std::byte * &>(dst) += stride;
        }
    }

    inline std::string timeString(double seconds)
    {
        static constexpr double secondsPerMinute(60.0);
        static constexpr double secondsPerHour(60.0 * secondsPerMinute);
        static constexpr double secondsPerDay(24.0 * secondsPerHour);

        seconds = std::floor(seconds);
        double days(std::floor(seconds / secondsPerDay)); seconds -= days * secondsPerDay;
        double hours(std::floor(seconds / secondsPerHour)); seconds -= hours * secondsPerHour;
        double minutes(std::floor(seconds / secondsPerMinute)); seconds -= minutes * secondsPerMinute;

        std::stringstream ss;
        ss << std::setw(2) << std::setfill('0') << int(days) << ":";
        ss << std::setw(2) << std::setfill('0') << int(hours) << ":";
        ss << std::setw(2) << std::setfill('0') << int(minutes) << ":";
        ss << std::setw(2) << std::setfill('0') << int(seconds);

        return ss.str();
    }

    namespace print
    {
        template <typename T>
        struct _binary_s
        {
            alignas(T) u8 data[sizeof(T)];
            int blockSize;

            _binary_s(const T & v, const int blockSize) :
                data{},
                blockSize{blockSize}
            {
                std::copy_n(reinterpret_cast<const u8 *>(&v), sizeof(T), data);
            }

            friend std::ostream & operator<<(std::ostream & os, const _binary_s & b)
            {
                const int nBlocks{sizeof(T) / b.blockSize};

                for (int blockI{0}; blockI < nBlocks; ++blockI) {
                    for (int byteI{b.blockSize - 1}; byteI >= 0; --byteI) {
                        u8 byte(b.data[blockI * b.blockSize + byteI]);
                        for (int bitI{0}; bitI < 8; ++bitI) {
                            os << ((byte & 0b10000000) ? "1" : "0");
                            byte <<= 1;
                        }
                    }
                    if (blockI != nBlocks - 1) os << " ";
                }

                return os;
            }
        };

        template <typename T>
        inline _binary_s<T> binary(const T & v, const int blockSize = sizeof(T))
        {
            return _binary_s<T>(v, blockSize);
        }

        struct repeat
        {
            std::string s;
            int n;

            repeat(std::string s, const int n) : s(std::move(s)), n(n) {}

            friend std::ostream & operator<<(std::ostream & os, const repeat & r)
            {
                for (int i{0}; i < r.n; ++i) {
                    os << r.s;
                }
                return os;
            }
        };

        struct line
        {
            int n;

            explicit line(const int n) : n(n) {}

            friend std::ostream & operator<<(std::ostream & os, const line & l)
            {
                return os << repeat("-", l.n);
            }
        };
    }
}
