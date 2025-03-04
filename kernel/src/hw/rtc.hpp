#pragma once
#include <cstdint>

constexpr uint16_t CMOS_ADDRESS = 0x70;
constexpr uint16_t CMOS_DATA = 0x71;

constexpr uint8_t RTC_SECONDS = 0x00;
constexpr uint8_t RTC_MINUTES = 0x02;
constexpr uint8_t RTC_HOURS = 0x04;
constexpr uint8_t RTC_STATUS_B = 0x0B;

struct RTCTime {
    uint8_t seconds = 0;
    uint8_t minutes = 0;
    uint8_t hours = 0;
};

void init_rtc();
RTCTime get_rtc_time();