#pragma once
#include <cstdint>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

constexpr float operator"" _mhz(long double val) { return static_cast<float>(val * 1'000'000.0L); }
constexpr float operator"" _khz(long double val) { return static_cast<float>(val * 1'000.0f); }
constexpr float operator"" _hz(long double val) { return static_cast<float>(val); }

constexpr uint32_t operator"" _kb(unsigned long long val) { return val * 1024; }
