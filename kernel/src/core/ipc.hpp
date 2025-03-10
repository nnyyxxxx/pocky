#pragma once

#include <cstdint>
#include "process.hpp"
#include "lib/vector.hpp"
#include "lib/string.hpp"

namespace kernel {

constexpr size_t MAX_MESSAGE_SIZE = 1024;
constexpr size_t MAX_MESSAGES_PER_QUEUE = 64;
constexpr size_t MAX_SHARED_MEMORY_SIZE = 4 * 1024 * 1024;  // 4 MB

struct IPCMessage {
    pid_t sender;
    uint64_t timestamp;
    size_t size;
    uint8_t data[MAX_MESSAGE_SIZE];
};

class MessageQueue {
public:
    MessageQueue(pid_t owner, const char* name);
    ~MessageQueue();

    bool send_message(pid_t sender, const void* data, size_t size);

    bool receive_message(IPCMessage& message, bool wait);

    pid_t get_owner() const { return m_owner; }
    const char* get_name() const { return m_name; }
    size_t get_message_count() const { return m_messages.size(); }
    bool is_full() const { return m_messages.size() >= MAX_MESSAGES_PER_QUEUE; }

private:
    pid_t m_owner = 0;
    char* m_name = nullptr;
    Vector<IPCMessage> m_messages;
    Vector<Process*> m_waiting_processes;
};

struct SharedMemoryRegion {
    int32_t id = 0;
    pid_t creator = 0;
    size_t size = 0;
    void* address = nullptr;
    Vector<pid_t> attached_processes;
};

class IPCManager {
public:
    static IPCManager& instance();

    int32_t create_message_queue(pid_t owner, const char* name);
    bool destroy_message_queue(int32_t id);
    int32_t open_message_queue(const char* name);
    bool send_message(int32_t queue_id, pid_t sender, const void* data, size_t size);
    bool receive_message(int32_t queue_id, IPCMessage& message, bool wait);

    int32_t create_shared_memory(pid_t creator, size_t size);
    bool destroy_shared_memory(int32_t id);
    void* attach_shared_memory(int32_t id, pid_t pid);
    bool detach_shared_memory(int32_t id, pid_t pid);

    void wake_waiting_processes(void* resource);

private:
    IPCManager() = default;
    ~IPCManager();

    IPCManager(const IPCManager&) = delete;
    IPCManager& operator=(const IPCManager&) = delete;

    Vector<MessageQueue*> m_message_queues;
    Vector<SharedMemoryRegion*> m_shared_memory_regions;

    int32_t m_next_queue_id = 1;
    int32_t m_next_shared_memory_id = 1;
};

enum class IPCSyscallNumber : uint64_t {
    MsgCreate = 100,
    MsgDestroy = 101,
    MsgOpen = 102,
    MsgSend = 103,
    MsgReceive = 104,
    ShmCreate = 105,
    ShmDestroy = 106,
    ShmAttach = 107,
    ShmDetach = 108,
};

} // namespace kernel