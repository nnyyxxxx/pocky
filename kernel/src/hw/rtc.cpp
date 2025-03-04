#include "rtc.hpp"

#include "io.hpp"

namespace {

uint8_t read_cmos(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

uint8_t bcd_to_binary(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

}  // namespace

void init_rtc() {
    outb(CMOS_ADDRESS, RTC_STATUS_B);
    uint8_t status = inb(CMOS_DATA);
    outb(CMOS_ADDRESS, RTC_STATUS_B);
    outb(CMOS_DATA, status | 0x02 | 0x04);
}

RTCTime get_rtc_time() {
    RTCTime time;

    uint8_t seconds = read_cmos(RTC_SECONDS);
    uint8_t minutes = read_cmos(RTC_MINUTES);
    uint8_t hours = read_cmos(RTC_HOURS);

    outb(CMOS_ADDRESS, RTC_STATUS_B);
    uint8_t status = inb(CMOS_DATA);
    bool is_bcd = !(status & 0x04);

    if (is_bcd) {
        seconds = bcd_to_binary(seconds);
        minutes = bcd_to_binary(minutes);
        hours = bcd_to_binary(hours);
    }

    if (!(status & 0x02) && (hours & 0x80)) hours = ((hours & 0x7F) + 12) % 24;

    time.seconds = seconds;
    time.minutes = minutes;
    time.hours = hours;

    return time;
}