#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include "memory/heap.hpp"

namespace kernel {

template <typename T>
class Vector {
public:
    Vector() : m_data(nullptr), m_size(0), m_capacity(0) {}

    Vector(size_t initial_capacity) : m_size(0), m_capacity(initial_capacity) {
        if (initial_capacity > 0)
            m_data = static_cast<T*>(operator new(sizeof(T) * initial_capacity));
        else
            m_data = nullptr;
    }

    ~Vector() {
        clear();
        if (m_data)
            operator delete(m_data);
    }

    Vector(const Vector& other) : m_size(other.m_size), m_capacity(other.m_capacity) {
        if (m_capacity > 0) {
            m_data = static_cast<T*>(operator new(sizeof(T) * m_capacity));
            for (size_t i = 0; i < m_size; i++) {
                new (&m_data[i]) T(other.m_data[i]);
            }
        } else
            m_data = nullptr;
    }

    Vector(Vector&& other) noexcept : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity) {
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    Vector& operator=(const Vector& other) {
        if (this != &other) {
            clear();
            if (m_capacity < other.m_size) {
                if (m_data)
                    operator delete(m_data);
                m_capacity = other.m_capacity;
                m_data = static_cast<T*>(operator new(sizeof(T) * m_capacity));
            }
            m_size = other.m_size;
            for (size_t i = 0; i < m_size; i++) {
                new (&m_data[i]) T(other.m_data[i]);
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            clear();
            if (m_data)
                operator delete(m_data);
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    void push_back(const T& value) {
        if (m_size >= m_capacity)
            reserve(m_capacity == 0 ? 1 : m_capacity * 2);
        new (&m_data[m_size]) T(value);
        m_size++;
    }

    void push_back(T&& value) {
        if (m_size >= m_capacity)
            reserve(m_capacity == 0 ? 1 : m_capacity * 2);
        new (&m_data[m_size]) T(static_cast<T&&>(value));
        m_size++;
    }

    template <typename... Args>
    T& emplace_back(Args&&... args) {
        if (m_size >= m_capacity)
            reserve(m_capacity == 0 ? 1 : m_capacity * 2);
        new (&m_data[m_size]) T(static_cast<Args&&>(args)...);
        return m_data[m_size++];
    }

    void pop_back() {
        if (m_size > 0) {
            m_size--;
            m_data[m_size].~T();
        }
    }

    void reserve(size_t new_capacity) {
        if (new_capacity <= m_capacity) return;

        T* new_data = static_cast<T*>(operator new(sizeof(T) * new_capacity));
        for (size_t i = 0; i < m_size; i++) {
            new (&new_data[i]) T(static_cast<T&&>(m_data[i]));
            m_data[i].~T();
        }

        if (m_data)
            operator delete(m_data);

        m_data = new_data;
        m_capacity = new_capacity;
    }

    void resize(size_t new_size) {
        if (new_size > m_capacity)
            reserve(new_size);

        for (size_t i = m_size; i < new_size; i++) {
            new (&m_data[i]) T();
        }

        for (size_t i = new_size; i < m_size; i++) {
            m_data[i].~T();
        }

        m_size = new_size;
    }

    void clear() {
        for (size_t i = 0; i < m_size; i++) {
            m_data[i].~T();
        }
        m_size = 0;
    }

    T& operator[](size_t index) {
        return m_data[index];
    }

    const T& operator[](size_t index) const {
        return m_data[index];
    }

    size_t size() const {
        return m_size;
    }

    bool empty() const {
        return m_size == 0;
    }

    T* begin() {
        return m_data;
    }

    const T* begin() const {
        return m_data;
    }

    T* end() {
        return m_data + m_size;
    }

    const T* end() const {
        return m_data + m_size;
    }

    T& front() {
        return m_data[0];
    }

    const T& front() const {
        return m_data[0];
    }

    T& back() {
        return m_data[m_size - 1];
    }

    const T& back() const {
        return m_data[m_size - 1];
    }

    void erase(T* position) {
        if (position < begin() || position >= end())
            return;

        position->~T();

        size_t index = position - begin();
        for (size_t i = index; i < m_size - 1; i++) {
            new (&m_data[i]) T(static_cast<T&&>(m_data[i + 1]));
            m_data[i + 1].~T();
        }

        m_size--;
    }

private:
    T* m_data;
    size_t m_size;
    size_t m_capacity;
};

} // namespace kernel