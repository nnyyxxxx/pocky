#include "ipc.hpp"

#include <cstring>

#include "hw/timer.hpp"
#include "lib/string.hpp"
#include "lib/vector.hpp"
#include "memory/physical_memory.hpp"
#include "memory/virtual_memory.hpp"
#include "process.hpp"
#include "scheduler.hpp"

namespace kernel {

MessageQueue::MessageQueue(pid_t owner, const char* name) : m_owner(owner) {
    size_t len = strlen(name) + 1;
    m_name = new char[len];
    memcpy(m_name, name, len);
}

MessageQueue::~MessageQueue() {
    delete[] m_name;

    for (size_t i = 0; i < m_waiting_processes.size(); i++) {
        Process* process = m_waiting_processes[i];
        if (process->state == ProcessState::Waiting && process->waiting_on == this) {
            process->state = ProcessState::Ready;
            process->waiting_on = nullptr;
        }
    }

    m_waiting_processes.clear();
    m_messages.clear();
}

bool MessageQueue::send_message(pid_t sender, const void* data, size_t size) {
    if (is_full() || size > MAX_MESSAGE_SIZE) return false;

    IPCMessage message;
    message.sender = sender;
    message.timestamp = get_ticks();
    message.size = size;

    memcpy(message.data, data, size);

    m_messages.push_back(message);

    if (m_waiting_processes.size() > 0) {
        Process* waiting_process = m_waiting_processes[0];

        for (size_t i = 0; i < m_waiting_processes.size() - 1; i++) {
            m_waiting_processes[i] = m_waiting_processes[i + 1];
        }
        m_waiting_processes.pop_back();

        waiting_process->state = ProcessState::Ready;
        waiting_process->waiting_on = nullptr;

        Scheduler::instance().add_process(waiting_process);
    }

    return true;
}

bool MessageQueue::receive_message(IPCMessage& message, bool wait) {
    if (m_messages.size() == 0) {
        if (!wait) return false;

        auto& pm = ProcessManager::instance();
        Process* current = pm.get_current_process();

        if (!current) return false;

        current->state = ProcessState::Waiting;
        current->waiting_on = this;
        m_waiting_processes.push_back(current);

        Scheduler::instance().schedule();

        if (m_messages.size() == 0) return false;
    }

    message = m_messages[0];

    if (m_messages.size() > 1) {
        for (size_t i = 0; i < m_messages.size() - 1; i++) {
            m_messages[i] = m_messages[i + 1];
        }
    }
    m_messages.pop_back();

    return true;
}

IPCManager& IPCManager::instance() {
    static IPCManager instance;
    return instance;
}

IPCManager::~IPCManager() {
    for (size_t i = 0; i < m_message_queues.size(); i++) {
        delete m_message_queues[i];
    }
    m_message_queues.clear();

    for (size_t i = 0; i < m_shared_memory_regions.size(); i++) {
        auto* region = m_shared_memory_regions[i];
        if (region->address) {
            auto& pmm = PhysicalMemoryManager::instance();
            size_t num_pages = (region->size + 4095) / 4096;

            for (size_t i = 0; i < num_pages; i++) {
                uint64_t addr = reinterpret_cast<uint64_t>(region->address) + i * 4096;
                auto& vmm = VirtualMemoryManager::instance();
                uintptr_t phys_addr = vmm.get_physical_address(addr);

                if (phys_addr) {
                    pmm.free_frame(reinterpret_cast<void*>(phys_addr));
                    vmm.unmap_page(addr);
                }
            }
        }

        delete region;
    }
    m_shared_memory_regions.clear();
}

int32_t IPCManager::create_message_queue(pid_t owner, const char* name) {
    for (size_t i = 0; i < m_message_queues.size(); i++) {
        auto* queue = m_message_queues[i];
        if (!queue) continue;

        if (strcmp(queue->get_name(), name) == 0) return -1;
    }

    auto* queue = new MessageQueue(owner, name);

    for (size_t i = 0; i < m_message_queues.size(); i++) {
        if (m_message_queues[i] == nullptr) {
            m_message_queues[i] = queue;
            return i + 1;
        }
    }

    int32_t id = m_next_queue_id++;
    m_message_queues.push_back(queue);

    return id;
}

bool IPCManager::destroy_message_queue(int32_t id) {
    if (id <= 0 || id >= m_next_queue_id || id > static_cast<int32_t>(m_message_queues.size()))
        return false;

    auto* queue = m_message_queues[id - 1];
    if (!queue) return false;

    delete queue;
    m_message_queues[id - 1] = nullptr;

    return true;
}

int32_t IPCManager::open_message_queue(const char* name) {
    for (size_t i = 0; i < m_message_queues.size(); i++) {
        auto* queue = m_message_queues[i];
        if (queue && strcmp(queue->get_name(), name) == 0) return i + 1;
    }

    return -1;
}

bool IPCManager::send_message(int32_t queue_id, pid_t sender, const void* data, size_t size) {
    if (queue_id <= 0 || queue_id >= m_next_queue_id ||
        queue_id > static_cast<int32_t>(m_message_queues.size())) {
        return false;
    }

    auto* queue = m_message_queues[queue_id - 1];
    if (!queue) return false;

    return queue->send_message(sender, data, size);
}

bool IPCManager::receive_message(int32_t queue_id, IPCMessage& message, bool wait) {
    if (queue_id <= 0 || queue_id >= m_next_queue_id ||
        queue_id > static_cast<int32_t>(m_message_queues.size())) {
        return false;
    }

    auto* queue = m_message_queues[queue_id - 1];
    if (!queue) return false;

    return queue->receive_message(message, wait);
}

int32_t IPCManager::create_shared_memory(pid_t creator, size_t size) {
    if (size == 0 || size > MAX_SHARED_MEMORY_SIZE) return -1;

    size = (size + 4095) & ~4095;

    auto* region = new SharedMemoryRegion;
    region->id = m_next_shared_memory_id++;
    region->creator = creator;
    region->size = size;

    auto& pmm = PhysicalMemoryManager::instance();
    auto& vmm = VirtualMemoryManager::instance();

    uint64_t shared_mem_base = 0x700000000000 + (region->id - 1) * MAX_SHARED_MEMORY_SIZE;
    region->address = reinterpret_cast<void*>(shared_mem_base);

    size_t num_pages = size / 4096;
    for (size_t i = 0; i < num_pages; i++) {
        uint64_t addr = shared_mem_base + i * 4096;
        uintptr_t phys_page = reinterpret_cast<uintptr_t>(pmm.allocate_frame());

        if (!phys_page) {
            for (size_t j = 0; j < i; j++) {
                uint64_t cleanup_addr = shared_mem_base + j * 4096;
                uintptr_t phys_addr = vmm.get_physical_address(cleanup_addr);
                if (phys_addr) {
                    pmm.free_frame(reinterpret_cast<void*>(phys_addr));
                    vmm.unmap_page(cleanup_addr);
                }
            }

            delete region;
            return -1;
        }

        vmm.map_page(addr, phys_page, true);
    }

    memset(region->address, 0, size);

    region->attached_processes.push_back(creator);

    m_shared_memory_regions.push_back(region);

    return region->id;
}

bool IPCManager::destroy_shared_memory(int32_t id) {
    for (size_t i = 0; i < m_shared_memory_regions.size(); i++) {
        auto* region = m_shared_memory_regions[i];

        if (!region) continue;

        if (region->id == id) {
            if (region->address) {
                auto& pmm = PhysicalMemoryManager::instance();
                auto& vmm = VirtualMemoryManager::instance();

                size_t num_pages = (region->size + 4095) / 4096;

                for (size_t j = 0; j < num_pages; j++) {
                    uint64_t addr = reinterpret_cast<uint64_t>(region->address) + j * 4096;
                    uintptr_t phys_addr = vmm.get_physical_address(addr);

                    if (phys_addr) {
                        pmm.free_frame(reinterpret_cast<void*>(phys_addr));
                        vmm.unmap_page(addr);
                    }
                }
            }

            delete region;

            for (size_t j = i; j < m_shared_memory_regions.size() - 1; j++) {
                m_shared_memory_regions[j] = m_shared_memory_regions[j + 1];
            }
            m_shared_memory_regions.pop_back();

            return true;
        }
    }

    return false;
}

void* IPCManager::attach_shared_memory(int32_t id, pid_t pid) {
    if (id <= 0) return nullptr;

    for (size_t i = 0; i < m_shared_memory_regions.size(); i++) {
        auto* region = m_shared_memory_regions[i];
        if (!region || region->id != id) continue;

        for (size_t j = 0; j < region->attached_processes.size(); j++) {
            pid_t attached_pid = region->attached_processes[j];
            if (attached_pid == pid) return region->address;
        }

        region->attached_processes.push_back(pid);
        return region->address;
    }

    return nullptr;
}

bool IPCManager::detach_shared_memory(int32_t id, pid_t pid) {
    for (size_t i = 0; i < m_shared_memory_regions.size(); i++) {
        auto* region = m_shared_memory_regions[i];
        if (!region) continue;

        if (region->id == id) {
            for (size_t j = 0; j < region->attached_processes.size(); j++) {
                if (region->attached_processes[j] == pid) {
                    for (size_t k = j; k < region->attached_processes.size() - 1; k++) {
                        region->attached_processes[k] = region->attached_processes[k + 1];
                    }
                    region->attached_processes.pop_back();
                    return true;
                }
            }

            return false;
        }
    }

    return false;
}

void IPCManager::wake_waiting_processes(void* resource) {
    auto& pm = ProcessManager::instance();

    Process* current = pm.get_first_process();

    while (current) {
        if (current->state == ProcessState::Waiting && current->waiting_on == resource) {
            current->state = ProcessState::Ready;
            current->waiting_on = nullptr;

            Scheduler::instance().add_process(current);
        }

        current = current->next;
    }
}

}  // namespace kernel