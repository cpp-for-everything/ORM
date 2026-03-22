#pragma once
#include <chrono>

namespace orm {

    using datetime    = std::chrono::system_clock::time_point;
#if ORM_HAS_UTC_CLOCK
    using timestamp   = std::chrono::utc_clock::time_point;
#else
    using timestamp   = std::chrono::system_clock::time_point;
#endif
    using date        = std::chrono::year_month_day;
    using time_of_day = std::chrono::hh_mm_ss<std::chrono::seconds>;
    using year        = std::chrono::year;

} // namespace orm
