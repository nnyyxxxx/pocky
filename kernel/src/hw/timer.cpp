#include "timer.hpp"

#include <cstdio>
#include <cstring>
#include <atomic>

#include "core/scheduler.hpp"
#include "idt.hpp"
#include "io.hpp"
#include "pic.hpp"
#include "printf.hpp"

namespace {

static std::atomic<uint64_t> timer_ticks{0};
uint32_t timer_frequency = 0;

extern "C" void timer_handler();

extern "C" void timer_callback() {
    timer_ticks.fetch_add(1, std::memory_order_relaxed);
    kernel::Scheduler::instance().tick();
}

void append_number(char* buffer, size_t& pos, size_t max_size, uint64_t num) {
    char num_str[20] = {0};
    size_t i = 0;

    if (num == 0)
        num_str[i++] = '0';
    else {
        while (num > 0 && i < sizeof(num_str) - 1) {
            num_str[i++] = '0' + (num % 10);
            num /= 10;
        }
    }

    while (i > 0 && pos < max_size - 1) {
        buffer[pos++] = num_str[--i];
    }
    buffer[pos] = '\0';
}

void append_string(char* buffer, size_t& pos, size_t max_size, const char* str) {
    while (*str && pos < max_size - 1) {
        buffer[pos++] = *str++;
    }
    buffer[pos] = '\0';
}

}  // namespace

void init_timer(uint32_t frequency) {
    timer_frequency = frequency;

    uint32_t divisor = PIT_FREQUENCY / frequency;

    outb(PIT_COMMAND, 0x36);

    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);

    set_interrupt_handler(32, timer_handler, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT_GATE);
}

uint64_t get_ticks() {
    return timer_ticks.load(std::memory_order_relaxed);
}

uint64_t get_uptime_seconds() {
    return get_ticks() / timer_frequency;
}

void format_uptime(char* buffer, size_t size) {
    uint64_t seconds = get_uptime_seconds();
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;
    uint64_t days = hours / 24;

    seconds %= 60;
    minutes %= 60;
    hours %= 24;

    size_t pos = 0;

    if (days > 0) {
        append_number(buffer, pos, size, days);
        append_string(buffer, pos, size, " days, ");
        append_number(buffer, pos, size, hours);
        append_string(buffer, pos, size, " hours, ");
        append_number(buffer, pos, size, minutes);
        append_string(buffer, pos, size, " minutes, ");
        append_number(buffer, pos, size, seconds);
        append_string(buffer, pos, size, " seconds");
    } else if (hours > 0) {
        append_number(buffer, pos, size, hours);
        append_string(buffer, pos, size, " hours, ");
        append_number(buffer, pos, size, minutes);
        append_string(buffer, pos, size, " minutes, ");
        append_number(buffer, pos, size, seconds);
        append_string(buffer, pos, size, " seconds");
    } else if (minutes > 0) {
        append_number(buffer, pos, size, minutes);
        append_string(buffer, pos, size, " minutes, ");
        append_number(buffer, pos, size, seconds);
        append_string(buffer, pos, size, " seconds");
    } else {
        append_number(buffer, pos, size, seconds);
        append_string(buffer, pos, size, " seconds");
    }
}