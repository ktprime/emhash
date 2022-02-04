#pragma once

#include <iostream>
#include <string>

namespace qc::log
{
    inline void error_break() {}

    inline void warning_break() {}

    inline void exception_break() {}
}

#define QC_ERROR(msg) do { \
    std::cerr << "ERROR [" << __FILE__ << ":" << __LINE__ << " " << __func__ << "]: " << msg << std::endl; \
    qc::log::error_break(); \
} while (false)

#define QC_WARNING(msg) do { \
    std::cerr << "WARNING [" << __FILE__ << ":" << __LINE__ << " " << __func__ << "]: " << msg << std::endl; \
    qc::log::warning_break(); \
} while (false)

#define QC_INFO(msg) do { \
    std::cerr << "INFO [" << __FILE__ << ":" << __LINE__ << " " << __func__ << "]: " << msg << std::endl; \
} while (false)

#define QC_DEBUG(msg) do { \
    std::cerr << "DEBUG [" << __FILE__ << ":" << __LINE__ << " " << __func__ << "]: " << msg << std::endl; \
} while (false)

#define QC_EXCEPTION(msg, exception) do { \
    std::cerr << "EXCEPTION [" << __FILE__ << ":" << __LINE__ << " " << __func__ << "]: " << msg << std::endl; \
    qc::log::exception_break(); \
    throw exception; \
} while (false)
