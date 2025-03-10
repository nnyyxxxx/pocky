#pragma once

#include <cstddef>
#include <cstring>
#include <new>
#include "memory/heap.hpp"

namespace kernel {

class String {
public:
    String() : m_data(nullptr), m_size(0), m_capacity(0) {}

    String(const char* str) {
        if (!str) {
            m_data = nullptr;
            m_size = 0;
            m_capacity = 0;
            return;
        }

        m_size = strlen(str);
        m_capacity = m_size + 1;
        m_data = new char[m_capacity];
        memcpy(m_data, str, m_size + 1);
    }

    String(const String& other) {
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        if (m_capacity > 0) {
            m_data = new char[m_capacity];
            memcpy(m_data, other.m_data, m_size + 1);
        } else
            m_data = nullptr;
    }

    String(String&& other) noexcept : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity) {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    ~String() {
        delete[] m_data;
    }

    String& operator=(const String& other) {
        if (this != &other) {
            delete[] m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            if (m_capacity > 0) {
                m_data = new char[m_capacity];
                memcpy(m_data, other.m_data, m_size + 1);
            } else
                m_data = nullptr;
        }
        return *this;
    }

    String& operator=(String&& other) noexcept {
        if (this != &other) {
            delete[] m_data;
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    String& operator=(const char* str) {
        delete[] m_data;
        if (!str) {
            m_data = nullptr;
            m_size = 0;
            m_capacity = 0;
            return *this;
        }

        m_size = strlen(str);
        m_capacity = m_size + 1;
        m_data = new char[m_capacity];
        memcpy(m_data, str, m_size + 1);
        return *this;
    }

    String& operator+=(const String& other) {
        if (other.m_size == 0) return *this;

        size_t new_size = m_size + other.m_size;
        if (new_size + 1 > m_capacity) {
            size_t new_capacity = new_size + 1;
            char* new_data = new char[new_capacity];
            if (m_data) {
                memcpy(new_data, m_data, m_size);
                delete[] m_data;
            }
            m_data = new_data;
            m_capacity = new_capacity;
        }

        memcpy(m_data + m_size, other.m_data, other.m_size + 1);
        m_size = new_size;
        return *this;
    }

    String& operator+=(const char* str) {
        if (!str || *str == '\0') return *this;

        size_t str_len = strlen(str);
        size_t new_size = m_size + str_len;
        if (new_size + 1 > m_capacity) {
            size_t new_capacity = new_size + 1;
            char* new_data = new char[new_capacity];
            if (m_data) {
                memcpy(new_data, m_data, m_size);
                delete[] m_data;
            }
            m_data = new_data;
            m_capacity = new_capacity;
        }

        memcpy(m_data + m_size, str, str_len + 1);
        m_size = new_size;
        return *this;
    }

    bool operator==(const String& other) const {
        if (m_size != other.m_size) return false;
        if (m_size == 0) return true;
        return memcmp(m_data, other.m_data, m_size) == 0;
    }

    bool operator==(const char* str) const {
        if (!str) return m_size == 0;
        if (!m_data) return *str == '\0';
        return strcmp(m_data, str) == 0;
    }

    bool operator!=(const String& other) const {
        return !(*this == other);
    }

    bool operator!=(const char* str) const {
        return !(*this == str);
    }

    char& operator[](size_t index) {
        return m_data[index];
    }

    const char& operator[](size_t index) const {
        return m_data[index];
    }

    const char* c_str() const {
        return m_data ? m_data : "";
    }

    size_t size() const {
        return m_size;
    }

    bool empty() const {
        return m_size == 0;
    }

    void reserve(size_t new_capacity) {
        if (new_capacity <= m_capacity) return;

        char* new_data = new char[new_capacity];
        if (m_data) {
            memcpy(new_data, m_data, m_size + 1);
            delete[] m_data;
        } else
            new_data[0] = '\0';

        m_data = new_data;
        m_capacity = new_capacity;
    }

    void clear() {
        if (m_data)
            m_data[0] = '\0';
        m_size = 0;
    }

    String substr(size_t pos, size_t len = npos) const {
        if (pos >= m_size) return String();

        if (len == npos || pos + len > m_size)
            len = m_size - pos;

        String result;
        result.reserve(len + 1);
        result.m_size = len;
        memcpy(result.m_data, m_data + pos, len);
        result.m_data[len] = '\0';

        return result;
    }

    size_t find(const char* str, size_t pos = 0) const {
        if (!m_data || !str || pos >= m_size) return npos;

        const char* found = strstr(m_data + pos, str);
        if (!found) return npos;

        return found - m_data;
    }

    size_t find(const String& str, size_t pos = 0) const {
        return find(str.c_str(), pos);
    }

    static const size_t npos = static_cast<size_t>(-1);

private:
    char* m_data;
    size_t m_size;
    size_t m_capacity;
};

} // namespace kernel