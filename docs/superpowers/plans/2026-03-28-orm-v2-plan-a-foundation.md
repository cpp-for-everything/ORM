# ORM v2 — Plan A: Foundation (Types, Reflection, Entity Model)

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish the complete ORM type vocabulary, reflection adapter, and entity model (`property<>`, `relationship<>`, all `orm::` wrappers) that all subsequent plans depend on.

**Architecture:** Pure header-only C++20/23 core. Reflection gated on `__cpp_impl_reflection` (C++26 Boost.PFR fallback). No database code in this plan — purely the C++ type layer that describes entities.

**Tech Stack:** C++20 minimum, C++26 optional; Boost.PFR (fallback reflection); CMake 3.20+; GoogleTest for unit tests.

**Spec:** `docs/superpowers/specs/2026-03-28-orm-v2-architecture.md` §3, §4, §7, §8

**Depends on:** nothing (this is the foundation)  
**Required by:** Plan B (Query IR), Plan C (Connector + Result), Plan D (MockDB)

---

## Chunk 1: CMake scaffold and Boost.PFR integration

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Create | `lib/CMakeLists.txt` | ORM library target, C++ standard, PFR, optional C++26 detection |
| Create | `lib/include/ORM/ORM.hpp` | Top-level umbrella include |
| Create | `tests/CMakeLists.txt` | GoogleTest harness, links `orm` target |
| Create | `tests/foundation/CMakeLists.txt` | Foundation test suite |
| Modify | `CMakeLists.txt` | Add `lib/` and `tests/` subdirectories |

---

### Task 1: Root CMakeLists.txt and library scaffold

**Files:**
- Modify: `CMakeLists.txt`
- Create: `lib/CMakeLists.txt`
- Create: `lib/include/ORM/ORM.hpp`

- [ ] **Step 1: Check existing root CMakeLists.txt**

Read `CMakeLists.txt` to understand the existing structure before modifying.

- [ ] **Step 2: Create the library CMakeLists.txt**

Create `lib/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)

# ── ORM interface library ────────────────────────────────────────────────────
add_library(orm INTERFACE)
add_library(orm::orm ALIAS orm)

target_include_directories(orm INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(orm INTERFACE cxx_std_20)

# ── C++26 reflection detection ───────────────────────────────────────────────
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
    #include <meta>
    int main() { constexpr auto r = ^^int; return 0; }
" ORM_HAS_CPP26_REFLECTION)

if(ORM_HAS_CPP26_REFLECTION)
    message(STATUS "ORM: C++26 static reflection available")
    target_compile_definitions(orm INTERFACE ORM_HAS_REFLECTION=1)
else()
    message(STATUS "ORM: C++26 reflection not available, using Boost.PFR fallback")
    target_compile_definitions(orm INTERFACE ORM_HAS_REFLECTION=0)
endif()

# ── Boost.PFR (always included as fallback) ───────────────────────────────────
find_package(Boost REQUIRED)
target_link_libraries(orm INTERFACE Boost::headers)
```

- [ ] **Step 3: Create the top-level umbrella header**

Create `lib/include/ORM/ORM.hpp`:

```cpp
#pragma once

#include "ORM/details/reflection.hpp"
#include "ORM/types/wrappers.hpp"
#include "ORM/types/chrono.hpp"
#include "ORM/types/constrained.hpp"
#include "ORM/entity/property.hpp"
#include "ORM/entity/relationship.hpp"
#include "ORM/entity/table.hpp"
```

- [ ] **Step 4: Wire lib/ into root CMakeLists.txt**

Add to root `CMakeLists.txt` (after project() declaration):

```cmake
add_subdirectory(lib)
```

- [ ] **Step 5: Commit**

```bash
git add lib/CMakeLists.txt lib/include/ORM/ORM.hpp CMakeLists.txt
git commit -m "build: scaffold ORM v2 library target with C++26 reflection detection"
```

---

### Task 2: Test harness

**Files:**
- Create: `tests/CMakeLists.txt`
- Create: `tests/foundation/CMakeLists.txt`

- [ ] **Step 1: Create tests/CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

add_subdirectory(foundation)
```

- [ ] **Step 2: Create tests/foundation/CMakeLists.txt**

```cmake
add_executable(test_foundation
    test_types.cpp
    test_property.cpp
    test_relationship.cpp
    test_reflection.cpp
)

target_link_libraries(test_foundation PRIVATE
    orm::orm
    GTest::gtest_main
)

gtest_discover_tests(test_foundation)
```

- [ ] **Step 3: Wire tests/ into root CMakeLists.txt**

```cmake
add_subdirectory(tests)
```

- [ ] **Step 4: Commit**

```bash
git add tests/CMakeLists.txt tests/foundation/CMakeLists.txt CMakeLists.txt
git commit -m "build: add GoogleTest harness and foundation test target"
```

---

## Chunk 2: Reflection adapter

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Create | `lib/include/ORM/details/reflection.hpp` | `#ifdef` gate; C++26 `std::meta` vs PFR adapter; `orm::detail::field_name_of<&T::m>()` |
| Create | `lib/include/ORM/details/pfr_adapter.hpp` | Boost.PFR wrappers used by property<> on PFR path |
| Create | `tests/foundation/test_reflection.cpp` | Tests for field name extraction |

---

### Task 3: Reflection adapter header

**Files:**
- Create: `lib/include/ORM/details/reflection.hpp`
- Create: `lib/include/ORM/details/pfr_adapter.hpp`

- [ ] **Step 1: Write the failing test first**

Create `tests/foundation/test_reflection.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/details/reflection.hpp"

struct SampleEntity {
    int user_id;
    double score;
};

TEST(ReflectionAdapter, FieldNameFromMemberPointer) {
    constexpr auto name = orm::detail::field_name_of<&SampleEntity::user_id>();
    EXPECT_EQ(name, "user_id");
}

TEST(ReflectionAdapter, FieldNameSecondMember) {
    constexpr auto name = orm::detail::field_name_of<&SampleEntity::score>();
    EXPECT_EQ(name, "score");
}
```

- [ ] **Step 2: Run test to confirm it fails**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure -R test_foundation
```

Expected: compile error — `orm::detail::field_name_of` not defined.

- [ ] **Step 3: Create the PFR adapter**

Create `lib/include/ORM/details/pfr_adapter.hpp`:

```cpp
#pragma once
#include <boost/pfr.hpp>
#include <string_view>
#include <cstddef>

namespace orm::detail::pfr {

template <typename T, std::size_t I>
constexpr std::string_view field_name() {
    return boost::pfr::get_name<I, T>();
}

} // namespace orm::detail::pfr
```

- [ ] **Step 4: Create the reflection adapter**

Create `lib/include/ORM/details/reflection.hpp`:

```cpp
#pragma once
#include <string_view>

#if ORM_HAS_REFLECTION
  #include <meta>
#else
  #include "pfr_adapter.hpp"
  #include "member_pointer_index.hpp"
#endif

namespace orm::detail {

#if ORM_HAS_REFLECTION

template <auto MemberPtr>
consteval std::string_view field_name_of() {
    return std::meta::name_v<^^[:std::meta::reflect_value(MemberPtr):]>;
}

#else

// Forward declaration — member_pointer_index.hpp resolves the index
template <auto MemberPtr>
struct member_pointer_index;

template <auto MemberPtr>
constexpr std::string_view field_name_of() {
    using Table = typename member_pointer_traits<MemberPtr>::table_type;
    constexpr std::size_t idx = member_pointer_index<MemberPtr>::value;
    return pfr::field_name<Table, idx>();
}

#endif

} // namespace orm::detail
```

- [ ] **Step 5: Create member_pointer_index helper (PFR path)**

Create `lib/include/ORM/details/member_pointer_index.hpp`:

```cpp
#pragma once
#include <boost/pfr.hpp>
#include <cstddef>
#include <type_traits>

namespace orm::detail {

template <auto MemberPtr>
struct member_pointer_traits;

template <typename T, typename Class>
struct member_pointer_traits<static_cast<T Class::*>(nullptr)> {
    using value_type = T;
    using table_type = Class;
};

// Resolve the 0-based index of a member pointer within its class via PFR
// by comparing offsets at compile time.
template <auto MemberPtr, typename Table = typename member_pointer_traits<decltype(MemberPtr)>::table_type>
struct member_pointer_index {
private:
    template <std::size_t... Is>
    static constexpr std::size_t find(std::index_sequence<Is...>) {
        std::size_t result = 0;
        bool found = false;
        // We compare the address of each PFR field in a default-constructed
        // Table to the offset encoded in MemberPtr by constructing a dummy.
        // This is a constexpr-compatible approach via offsetof-style reasoning.
        // We use pfr::get to get references and compare member pointer offsets.
        ((void)((!found && &(boost::pfr::get<Is>(std::declval<Table&>())) ==
                 &(std::declval<Table&>().*MemberPtr))
                    ? (result = Is, found = true, 0)
                    : 0),
         ...);
        return result;
    }

public:
    static constexpr std::size_t value =
        find(std::make_index_sequence<boost::pfr::tuple_size_v<Table>>{});
};

} // namespace orm::detail
```

- [ ] **Step 6: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_foundation
```

Expected: `ReflectionAdapter` tests PASS.

- [ ] **Step 7: Commit**

```bash
git add lib/include/ORM/details/
git commit -m "feat: reflection adapter with C++26/PFR dual path and field_name_of<>"
```

---

## Chunk 3: ORM type wrappers

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Create | `lib/include/ORM/types/wrappers.hpp` | `orm::fixed_string<N>`, `orm::binary<N>`, `orm::varbinary<N>` |
| Create | `lib/include/ORM/types/chrono.hpp` | `orm::datetime`, `orm::timestamp`, `orm::date`, `orm::time_of_day`, `orm::year` |
| Create | `lib/include/ORM/types/constrained.hpp` | `orm::enum_t<vals...>`, `orm::set_t<vals...>` |
| Create | `tests/foundation/test_types.cpp` | Type trait checks, construction, comparison |

---

### Task 4: Size-constrained wrappers

**Files:**
- Create: `lib/include/ORM/types/wrappers.hpp`
- Create: `tests/foundation/test_types.cpp` (first section)

- [ ] **Step 1: Write failing tests**

Create `tests/foundation/test_types.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/types/wrappers.hpp"
#include "ORM/types/chrono.hpp"
#include "ORM/types/constrained.hpp"
#include <cstring>

// ── fixed_string ──────────────────────────────────────────────────────────────

TEST(FixedString, SizeTemplateParam) {
    static_assert(orm::fixed_string<10>::max_size == 10);
}

TEST(FixedString, ConstructFromStringView) {
    orm::fixed_string<10> s(u8"hello");
    EXPECT_EQ(s.as_u8string_view(), u8"hello");
}

TEST(FixedString, TruncatesAtMaxSize) {
    orm::fixed_string<3> s(u8"hello");
    EXPECT_EQ(s.size(), 3u);
}

TEST(FixedString, EqualityComparison) {
    orm::fixed_string<10> a(u8"abc"), b(u8"abc"), c(u8"xyz");
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// ── binary / varbinary ────────────────────────────────────────────────────────

TEST(Binary, SizeTemplateParam) {
    static_assert(orm::binary<16>::fixed_size == 16);
}

TEST(Binary, ConstructFromSpan) {
    std::array<uint8_t, 4> data{0x01, 0x02, 0x03, 0x04};
    orm::binary<4> b(data);
    EXPECT_EQ(b.data()[0], 0x01);
    EXPECT_EQ(b.size(), 4u);
}

TEST(Varbinary, MaxSizeTemplateParam) {
    static_assert(orm::varbinary<255>::max_size == 255);
}

TEST(Varbinary, ConstructFromVector) {
    std::vector<uint8_t> v{0xAA, 0xBB};
    orm::varbinary<255> vb(v);
    EXPECT_EQ(vb.size(), 2u);
    EXPECT_EQ(vb.data()[1], 0xBB);
}
```

- [ ] **Step 2: Run test — confirm compile error**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_foundation
```

Expected: compile error — wrappers not defined.

- [ ] **Step 3: Implement wrappers**

Create `lib/include/ORM/types/wrappers.hpp`:

```cpp
#pragma once
#include <array>
#include <cstring>
#include <span>
#include <string_view>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <algorithm>

namespace orm {

// ── fixed_string<N> ───────────────────────────────────────────────────────────
// Represents a fixed-length UTF-8 string (maps to SQL CHAR(N)).
template <std::size_t N>
struct fixed_string {
    static constexpr std::size_t max_size = N;

    constexpr fixed_string() noexcept { _data.fill(0); }

    explicit constexpr fixed_string(std::u8string_view sv) noexcept {
        _data.fill(0);
        auto len = std::min(sv.size(), N);
        std::copy_n(sv.data(), len, _data.data());
        _len = len;
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept { return _len; }
    [[nodiscard]] constexpr std::u8string_view as_u8string_view() const noexcept {
        return {reinterpret_cast<const char8_t*>(_data.data()), _len};
    }

    constexpr bool operator==(const fixed_string&) const noexcept = default;

private:
    std::array<char8_t, N> _data{};
    std::size_t _len{0};
};

// ── binary<N> ────────────────────────────────────────────────────────────────
// Fixed-size binary buffer (maps to SQL BINARY(N)).
template <std::size_t N>
struct binary {
    static constexpr std::size_t fixed_size = N;

    constexpr binary() noexcept { _data.fill(0); }

    explicit constexpr binary(std::span<const uint8_t, N> s) noexcept {
        std::copy_n(s.data(), N, _data.data());
    }

    explicit constexpr binary(const std::array<uint8_t, N>& arr) noexcept : _data(arr) {}

    [[nodiscard]] constexpr std::size_t size() const noexcept { return N; }
    [[nodiscard]] constexpr const uint8_t* data() const noexcept { return _data.data(); }
    [[nodiscard]] constexpr uint8_t* data() noexcept { return _data.data(); }

    constexpr bool operator==(const binary&) const noexcept = default;

private:
    std::array<uint8_t, N> _data{};
};

// ── varbinary<N> ──────────────────────────────────────────────────────────────
// Variable-size binary up to N bytes (maps to SQL VARBINARY(N)).
template <std::size_t N>
struct varbinary {
    static constexpr std::size_t max_size = N;

    varbinary() = default;

    explicit varbinary(std::span<const uint8_t> s) {
        auto len = std::min(s.size(), N);
        _data.assign(s.data(), s.data() + len);
    }

    explicit varbinary(const std::vector<uint8_t>& v) {
        auto len = std::min(v.size(), N);
        _data.assign(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(len));
    }

    [[nodiscard]] std::size_t size() const noexcept { return _data.size(); }
    [[nodiscard]] const uint8_t* data() const noexcept { return _data.data(); }
    [[nodiscard]] uint8_t* data() noexcept { return _data.data(); }

    bool operator==(const varbinary&) const noexcept = default;

private:
    std::vector<uint8_t> _data;
};

} // namespace orm
```

- [ ] **Step 4: Run tests — confirm pass**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_foundation
```

Expected: `FixedString.*`, `Binary.*`, `Varbinary.*` PASS.

- [ ] **Step 5: Commit**

```bash
git add lib/include/ORM/types/wrappers.hpp tests/foundation/test_types.cpp
git commit -m "feat: orm::fixed_string<N>, orm::binary<N>, orm::varbinary<N>"
```

---

### Task 5: Chrono type aliases

**Files:**
- Create: `lib/include/ORM/types/chrono.hpp`
- Modify: `tests/foundation/test_types.cpp` (append chrono tests)

- [ ] **Step 1: Append chrono tests**

Append to `tests/foundation/test_types.cpp`:

```cpp
#include <chrono>

TEST(ChronoAliases, DatetimeIsSystemClockTimePoint) {
    static_assert(std::is_same_v<
        orm::datetime,
        std::chrono::system_clock::time_point>);
}

TEST(ChronoAliases, TimestampIsUtcClockTimePoint) {
    static_assert(std::is_same_v<
        orm::timestamp,
        std::chrono::utc_clock::time_point>);
}

TEST(ChronoAliases, DateIsYearMonthDay) {
    static_assert(std::is_same_v<
        orm::date,
        std::chrono::year_month_day>);
}

TEST(ChronoAliases, TimeOfDayIsHhMmSs) {
    static_assert(std::is_same_v<
        orm::time_of_day,
        std::chrono::hh_mm_ss<std::chrono::seconds>>);
}

TEST(ChronoAliases, YearIsChronoYear) {
    static_assert(std::is_same_v<orm::year, std::chrono::year>);
}
```

- [ ] **Step 2: Confirm compile error**

```bash
cmake --build build 2>&1 | head -20
```

Expected: `orm::datetime` not defined.

- [ ] **Step 3: Implement chrono aliases**

Create `lib/include/ORM/types/chrono.hpp`:

```cpp
#pragma once
#include <chrono>

namespace orm {

using datetime    = std::chrono::system_clock::time_point;
using timestamp   = std::chrono::utc_clock::time_point;
using date        = std::chrono::year_month_day;
using time_of_day = std::chrono::hh_mm_ss<std::chrono::seconds>;
using year        = std::chrono::year;

} // namespace orm
```

- [ ] **Step 4: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_foundation
```

Expected: `ChronoAliases.*` PASS.

- [ ] **Step 5: Commit**

```bash
git add lib/include/ORM/types/chrono.hpp tests/foundation/test_types.cpp
git commit -m "feat: orm::datetime, timestamp, date, time_of_day, year chrono aliases"
```

---

### Task 6: Constrained value types

**Files:**
- Create: `lib/include/ORM/types/constrained.hpp`
- Modify: `tests/foundation/test_types.cpp` (append constrained tests)

- [ ] **Step 1: Append tests**

Append to `tests/foundation/test_types.cpp`:

```cpp
TEST(EnumT, AcceptsValidValue) {
    using Status = orm::enum_t<"active", "banned", "pending">;
    Status s(u8"active");
    EXPECT_EQ(s.value(), u8"active");
}

TEST(EnumT, RejectsInvalidValueAtRuntime) {
    using Status = orm::enum_t<"active", "banned">;
    EXPECT_THROW(Status(u8"unknown"), std::invalid_argument);
}

TEST(EnumT, ValidValueCheck) {
    using Status = orm::enum_t<"a", "b">;
    EXPECT_TRUE(Status::is_valid(u8"a"));
    EXPECT_FALSE(Status::is_valid(u8"c"));
}

TEST(SetT, AcceptsSubsetOfValues) {
    using Perms = orm::set_t<"read", "write", "admin">;
    Perms p({u8"read", u8"write"});
    EXPECT_TRUE(p.contains(u8"read"));
    EXPECT_FALSE(p.contains(u8"admin"));
}

TEST(SetT, RejectsUnknownValue) {
    using Perms = orm::set_t<"read", "write">;
    EXPECT_THROW((Perms({u8"read", u8"execute"})), std::invalid_argument);
}
```

- [ ] **Step 2: Confirm compile error**

```bash
cmake --build build 2>&1 | head -20
```

- [ ] **Step 3: Implement constrained types**

Create `lib/include/ORM/types/constrained.hpp`:

```cpp
#pragma once
#include <array>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <set>
#include <cstddef>
#include <string_view>
#include <initializer_list>

namespace orm {

namespace detail {
// Compile-time string literal wrapper for NTTP use
template <std::size_t N>
struct ct_string {
    char8_t data[N]{};
    constexpr ct_string(const char8_t (&s)[N]) { std::copy_n(s, N, data); }
    constexpr std::u8string_view view() const { return {data, N - 1}; }
    constexpr bool operator==(const ct_string&) const = default;
};
} // namespace detail

// ── enum_t<"val1", "val2", ...> ───────────────────────────────────────────────
template <detail::ct_string... Values>
struct enum_t {
    static constexpr std::array<std::u8string_view, sizeof...(Values)> allowed{
        Values.view()...};

    static constexpr bool is_valid(std::u8string_view v) noexcept {
        return std::find(allowed.begin(), allowed.end(), v) != allowed.end();
    }

    explicit enum_t(std::u8string_view v) {
        if (!is_valid(v))
            throw std::invalid_argument("orm::enum_t: invalid value");
        _value = std::u8string(v);
    }

    [[nodiscard]] std::u8string_view value() const noexcept { return _value; }

    bool operator==(const enum_t&) const noexcept = default;

private:
    std::u8string _value;
};

// ── set_t<"val1", "val2", ...> ────────────────────────────────────────────────
template <detail::ct_string... Values>
struct set_t {
    static constexpr std::array<std::u8string_view, sizeof...(Values)> allowed{
        Values.view()...};

    static constexpr bool is_valid(std::u8string_view v) noexcept {
        return std::find(allowed.begin(), allowed.end(), v) != allowed.end();
    }

    explicit set_t(std::initializer_list<std::u8string_view> vals) {
        for (auto& v : vals) {
            if (!is_valid(v))
                throw std::invalid_argument("orm::set_t: invalid value");
            _values.insert(std::u8string(v));
        }
    }

    [[nodiscard]] bool contains(std::u8string_view v) const {
        return _values.contains(std::u8string(v));
    }

    [[nodiscard]] const std::set<std::u8string>& values() const noexcept { return _values; }

    bool operator==(const set_t&) const noexcept = default;

private:
    std::set<std::u8string> _values;
};

} // namespace orm
```

- [ ] **Step 4: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_foundation
```

Expected: `EnumT.*`, `SetT.*` PASS. All previous tests still PASS.

- [ ] **Step 5: Commit**

```bash
git add lib/include/ORM/types/constrained.hpp tests/foundation/test_types.cpp
git commit -m "feat: orm::enum_t<> and orm::set_t<> constrained value types"
```

---

## Chunk 4: Entity model — `property<>` and `relationship<>`

### File map

| Action | Path | Responsibility |
|--------|------|---------------|
| Create | `lib/include/ORM/entity/property.hpp` | `property<CppType, "name">` template; C++26 path drops string arg |
| Create | `lib/include/ORM/entity/relationship.hpp` | `store_as` enum, `relationship<store_as::X, T, "name">` |
| Create | `lib/include/ORM/entity/table.hpp` | `orm::table_name<T>` trait, entity concept |
| Create | `tests/foundation/test_property.cpp` | property<> tests |
| Create | `tests/foundation/test_relationship.cpp` | relationship<> tests |

---

### Task 7: `property<>` template

**Files:**
- Create: `lib/include/ORM/entity/property.hpp`
- Create: `tests/foundation/test_property.cpp`

- [ ] **Step 1: Write failing tests**

Create `tests/foundation/test_property.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/entity/property.hpp"
#include "ORM/types/wrappers.hpp"
#include "ORM/types/chrono.hpp"

// property<> stores its column name
TEST(Property, ColumnName) {
    using P = orm::property<int, "user_id">;
    EXPECT_EQ(P::column_name(), "user_id");
}

TEST(Property, ColumnNameFixedString) {
    using P = orm::property<orm::fixed_string<50>, "code">;
    EXPECT_EQ(P::column_name(), "code");
}

// property<> exposes the C++ type
TEST(Property, NativeType) {
    static_assert(std::is_same_v<orm::property<int, "id">::value_type, int>);
    static_assert(std::is_same_v<
        orm::property<std::u8string, "name">::value_type, std::u8string>);
}

// property<> holds and retrieves a value
TEST(Property, ValueRoundtrip) {
    orm::property<int, "id"> p;
    p.value = 42;
    EXPECT_EQ(p.value, 42);
}

// is_property concept
TEST(Property, IsPropertyConcept) {
    static_assert(orm::is_property<orm::property<int, "id">>);
    static_assert(!orm::is_property<int>);
}
```

- [ ] **Step 2: Confirm compile error**

```bash
cmake --build build 2>&1 | head -20
```

- [ ] **Step 3: Implement `property<>`**

Create `lib/include/ORM/entity/property.hpp`:

```cpp
#pragma once
#include <string_view>
#include <type_traits>
#include "ORM/details/reflection.hpp"

namespace orm {

namespace detail {
// Compile-time string for NTTP column name
template <std::size_t N>
struct col_name_str {
    char data[N]{};
    constexpr col_name_str(const char (&s)[N]) { std::copy_n(s, N, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};
} // namespace detail

// ── property<CppType, "column_name"> ──────────────────────────────────────────
// On C++26: string arg is optional — inferred via reflection if omitted.
// On PFR path: string arg is mandatory.
template <typename T, detail::col_name_str Name>
struct property {
    using value_type = T;

    static constexpr std::string_view column_name() noexcept { return Name.view(); }

    T value{};

    constexpr property() = default;
    explicit constexpr property(T v) : value(std::move(v)) {}
};

// ── is_property concept ───────────────────────────────────────────────────────
template <typename T>
concept is_property = requires {
    typename T::value_type;
    { T::column_name() } -> std::convertible_to<std::string_view>;
    requires std::is_same_v<T, property<typename T::value_type,
        detail::col_name_str<T::column_name().size() + 1>{
            // reconstructed from string_view — concept just checks shape
        }>>;
};

// Simpler trait-based check (works without reconstructing NTTP)
template <typename T>
struct is_property_trait : std::false_type {};

template <typename T, detail::col_name_str N>
struct is_property_trait<property<T, N>> : std::true_type {};

template <typename T>
inline constexpr bool is_property_v = is_property_trait<T>::value;

// Redefine concept using trait
template <typename T>
concept is_property = is_property_v<T>;

} // namespace orm
```

- [ ] **Step 4: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_foundation
```

Expected: `Property.*` PASS.

- [ ] **Step 5: Commit**

```bash
git add lib/include/ORM/entity/property.hpp tests/foundation/test_property.cpp
git commit -m "feat: orm::property<T, name> entity field template"
```

---

### Task 8: `relationship<>` and `store_as`

**Files:**
- Create: `lib/include/ORM/entity/relationship.hpp`
- Create: `tests/foundation/test_relationship.cpp`

- [ ] **Step 1: Write failing tests**

Create `tests/foundation/test_relationship.cpp`:

```cpp
#include <gtest/gtest.h>
#include "ORM/entity/relationship.hpp"
#include "ORM/entity/property.hpp"
#include <vector>

struct Post {
    orm::property<int, "id"> id;
};

// store_as enum values are accessible
TEST(StoreAs, ValuesExist) {
    static_assert(orm::store_as::reference != orm::store_as::embed);
}

// relationship<> exposes its store strategy
TEST(Relationship, StoreAsReference) {
    using R = orm::relationship<orm::store_as::reference, Post, "posts">;
    static_assert(R::strategy == orm::store_as::reference);
    EXPECT_EQ(R::field_name(), "posts");
}

TEST(Relationship, StoreAsEmbed) {
    using R = orm::relationship<orm::store_as::embed, Post, "archived">;
    static_assert(R::strategy == orm::store_as::embed);
}

// relationship<> exposes the related type
TEST(Relationship, RelatedType) {
    using R = orm::relationship<orm::store_as::reference, Post, "posts">;
    static_assert(std::is_same_v<R::related_type, Post>);
}

// relationship<> for one-to-many (vector)
TEST(Relationship, OneToMany) {
    using R = orm::relationship<orm::store_as::reference, std::vector<Post>, "likes">;
    static_assert(std::is_same_v<R::element_type, Post>);
    static_assert(R::is_collection);
}

// is_relationship concept
TEST(Relationship, IsRelationshipConcept) {
    using R = orm::relationship<orm::store_as::reference, Post, "posts">;
    static_assert(orm::is_relationship_v<R>);
    static_assert(!orm::is_relationship_v<int>);
}

// Inferred relationship kind from C++ member shape
TEST(InferredRelationship, SingleStructIsOneToOne) {
    static_assert(orm::infer_relationship_v<Post> == orm::inferred_kind::one_to_one);
}

TEST(InferredRelationship, VectorIsOneToMany) {
    static_assert(orm::infer_relationship_v<std::vector<Post>> == orm::inferred_kind::one_to_many);
}
```

- [ ] **Step 2: Confirm compile error**

```bash
cmake --build build 2>&1 | head -20
```

- [ ] **Step 3: Implement `relationship<>`**

Create `lib/include/ORM/entity/relationship.hpp`:

```cpp
#pragma once
#include <string_view>
#include <type_traits>
#include <vector>
#include <list>
#include "ORM/entity/property.hpp"

namespace orm {

// ── store_as enum ─────────────────────────────────────────────────────────────
enum class store_as {
    reference,  // FK join (SQL) or $lookup (NoSQL)
    embed,      // denormalized embed in SQL (JSON col) or embedded doc (NoSQL)
};

// ── inferred_kind ─────────────────────────────────────────────────────────────
enum class inferred_kind { one_to_one, one_to_many };

// infer_relationship from C++ type shape
template <typename T>
struct infer_relationship_trait {
    static constexpr inferred_kind value = inferred_kind::one_to_one;
};

template <typename T, typename A>
struct infer_relationship_trait<std::vector<T, A>> {
    static constexpr inferred_kind value = inferred_kind::one_to_many;
};

template <typename T, typename A>
struct infer_relationship_trait<std::list<T, A>> {
    static constexpr inferred_kind value = inferred_kind::one_to_many;
};

template <typename T>
inline constexpr inferred_kind infer_relationship_v =
    infer_relationship_trait<T>::value;

// ── element_type helper ───────────────────────────────────────────────────────
template <typename T>
struct element_type_trait { using type = T; };

template <typename T, typename A>
struct element_type_trait<std::vector<T, A>> { using type = T; };

template <typename T, typename A>
struct element_type_trait<std::list<T, A>> { using type = T; };

// ── col_name_str (reuse from property) ───────────────────────────────────────
namespace detail { template <std::size_t N> struct col_name_str; }

// ── relationship<strategy, T, "name"> ────────────────────────────────────────
template <store_as Strategy, typename T, detail::col_name_str Name>
struct relationship {
    static constexpr store_as strategy = Strategy;
    using related_type  = T;
    using element_type  = typename element_type_trait<T>::type;
    static constexpr bool is_collection =
        (infer_relationship_v<T> == inferred_kind::one_to_many);

    static constexpr std::string_view field_name() noexcept { return Name.view(); }
};

// ── is_relationship trait ─────────────────────────────────────────────────────
template <typename T>
struct is_relationship_trait : std::false_type {};

template <store_as S, typename T, detail::col_name_str N>
struct is_relationship_trait<relationship<S, T, N>> : std::true_type {};

template <typename T>
inline constexpr bool is_relationship_v = is_relationship_trait<T>::value;

} // namespace orm
```

- [ ] **Step 4: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_foundation
```

Expected: `StoreAs.*`, `Relationship.*`, `InferredRelationship.*` PASS. All previous tests still PASS.

- [ ] **Step 5: Commit**

```bash
git add lib/include/ORM/entity/relationship.hpp tests/foundation/test_relationship.cpp
git commit -m "feat: orm::relationship<> + store_as + inferred relationship kinds"
```

---

### Task 9: `table.hpp` entity concept and `orm::table_name<T>`

**Files:**
- Create: `lib/include/ORM/entity/table.hpp`
- Modify: `tests/foundation/test_property.cpp` (append table tests)

- [ ] **Step 1: Append tests**

Append to `tests/foundation/test_property.cpp`:

```cpp
#include "ORM/entity/table.hpp"

struct UserTable {
    orm::property<int, "id">           id;
    orm::property<std::u8string, "name"> name;
};

// Entities are detected by having at least one property<> member (via PFR)
TEST(Table, IsEntityConcept) {
    static_assert(orm::is_entity<UserTable>);
    static_assert(!orm::is_entity<int>);
}

// Default table name is the struct name (lowercased)
TEST(Table, DefaultTableName) {
    EXPECT_EQ(orm::table_name<UserTable>(), "usertable");
}

// Custom table name via specialization
namespace orm { template<> struct table_name_trait<UserTable> {
    static constexpr std::string_view value = "users";
}; }

TEST(Table, CustomTableName) {
    EXPECT_EQ(orm::table_name<UserTable>(), "users");
}
```

- [ ] **Step 2: Confirm compile error**

```bash
cmake --build build 2>&1 | head -20
```

- [ ] **Step 3: Implement `table.hpp`**

Create `lib/include/ORM/entity/table.hpp`:

```cpp
#pragma once
#include <string_view>
#include <algorithm>
#include <cctype>
#include "ORM/details/reflection.hpp"
#include "ORM/entity/property.hpp"

namespace orm {

// ── table_name_trait<T> — specialize to override the table name ───────────────
template <typename T>
struct table_name_trait {
    // Default: derive from type name via reflection or PFR
    static constexpr std::string_view value = orm::detail::type_name_of<T>();
};

// ── table_name<T>() ───────────────────────────────────────────────────────────
template <typename T>
constexpr std::string_view table_name() {
    return table_name_trait<T>::value;
}

// ── is_entity concept ─────────────────────────────────────────────────────────
// An entity is an aggregate struct whose first member is a property<>.
// We detect this via PFR or reflection.
template <typename T>
concept is_entity = std::is_aggregate_v<T> && requires {
    // At least one member must be accessible (PFR-style check)
    requires (boost::pfr::tuple_size_v<T> > 0);
};

} // namespace orm
```

Also add `type_name_of<T>()` to `lib/include/ORM/details/reflection.hpp`:

```cpp
// Append inside namespace orm::detail:
template <typename T>
constexpr std::string_view type_name_of() {
#if ORM_HAS_REFLECTION
    return std::meta::name_v<^^T>;
#else
    // Extract from __PRETTY_FUNCTION__ or compiler-specific
    // For now: return a placeholder — users should specialize table_name_trait<T>
    return "unknown_table";
#endif
}
```

- [ ] **Step 4: Run tests**

```bash
cmake --build build
ctest --test-dir build --output-on-failure -R test_foundation
```

Expected: `Table.*` PASS. Full suite PASS.

- [ ] **Step 5: Commit**

```bash
git add lib/include/ORM/entity/table.hpp tests/foundation/test_property.cpp
git commit -m "feat: orm::is_entity concept and orm::table_name<T> with specialization support"
```

---

### Task 10: Final integration — update ORM.hpp and run full suite

**Files:**
- Modify: `lib/include/ORM/ORM.hpp` (verify all includes are correct)

- [ ] **Step 1: Verify ORM.hpp includes are complete and ordered**

`lib/include/ORM/ORM.hpp` should include exactly:

```cpp
#pragma once

#include "ORM/details/reflection.hpp"
#include "ORM/details/pfr_adapter.hpp"
#include "ORM/types/wrappers.hpp"
#include "ORM/types/chrono.hpp"
#include "ORM/types/constrained.hpp"
#include "ORM/entity/property.hpp"
#include "ORM/entity/relationship.hpp"
#include "ORM/entity/table.hpp"
```

- [ ] **Step 2: Build and run full test suite**

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all tests PASS, zero failures.

- [ ] **Step 3: Final commit**

```bash
git add lib/include/ORM/ORM.hpp
git commit -m "feat: complete Plan A foundation — types, reflection, entity model"
```

---

## Plan A completion checklist

- [ ] CMake scaffold with C++26 detection and Boost.PFR
- [ ] GoogleTest harness wired
- [ ] Reflection adapter (`field_name_of<>`, C++26/PFR dual path)
- [ ] `orm::fixed_string<N>`, `orm::binary<N>`, `orm::varbinary<N>`
- [ ] `orm::datetime`, `orm::timestamp`, `orm::date`, `orm::time_of_day`, `orm::year`
- [ ] `orm::enum_t<vals...>`, `orm::set_t<vals...>`
- [ ] `orm::property<T, "name">`
- [ ] `orm::relationship<store_as::X, T, "name">`
- [ ] `orm::is_entity`, `orm::table_name<T>`
- [ ] All tests pass, zero warnings at `-Wall -Wextra`

**Next:** Plan B — Query IR (`field<>`, `Rule<>`, `Placeholder<T>`, CRUD builders)
