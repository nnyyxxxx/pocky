#include "core/ipc.hpp"

#include <cstdio>
#include <cstring>

#include "../shell.hpp"
#include "commands.hpp"
#include "core/process.hpp"
#include "printf.hpp"
#include "timer.hpp"

namespace commands {

void cmd_ipc_test() {
    auto& pm = kernel::ProcessManager::instance();
    pid_t test_pid = pm.create_process("ipc_test", shell_pid);
    auto& ipc = kernel::IPCManager::instance();

    char queue_name[64];
    snprintf(queue_name, sizeof(queue_name), "test_queue_%d_%lu", test_pid, get_ticks());

    int32_t queue_id = ipc.create_message_queue(test_pid, queue_name);
    if (queue_id < 0) {
        printf("Failed to create message queue\n");
        pm.terminate_process(test_pid);
        return;
    }

    printf("Created message queue with ID: %d\n", queue_id);

    const char* message = "Hello from IPC test!";
    if (!ipc.send_message(queue_id, test_pid, message, strlen(message) + 1)) {
        printf("Failed to send message\n");
        ipc.destroy_message_queue(queue_id);
        pm.terminate_process(test_pid);
        return;
    }

    printf("Sent message: \"%s\"\n", message);

    struct {
        pid_t sender;
        uint64_t timestamp;
        size_t size;
        uint8_t data[1024];
    } received_msg;

    if (!ipc.receive_message(queue_id, *reinterpret_cast<kernel::IPCMessage*>(&received_msg),
                             false)) {
        printf("Failed to receive message\n");
        ipc.destroy_message_queue(queue_id);
        pm.terminate_process(test_pid);
        return;
    }

    printf("Received message from PID %d: \"%s\"\n", received_msg.sender, received_msg.data);

    int32_t shm_id = ipc.create_shared_memory(test_pid, 4096);
    if (shm_id < 0) {
        printf("Failed to create shared memory\n");
        ipc.destroy_message_queue(queue_id);
        pm.terminate_process(test_pid);
        return;
    }

    printf("Created shared memory with ID: %d\n", shm_id);

    void* shm_addr = ipc.attach_shared_memory(shm_id, test_pid);
    if (!shm_addr) {
        printf("Failed to attach shared memory\n");
        ipc.destroy_shared_memory(shm_id);
        ipc.destroy_message_queue(queue_id);
        pm.terminate_process(test_pid);
        return;
    }

    printf("Attached shared memory at address: 0x%llx\n", reinterpret_cast<uint64_t>(shm_addr));

    const char* shm_message = "Hello from shared memory!";
    memcpy(shm_addr, shm_message, strlen(shm_message) + 1);

    printf("Wrote to shared memory: \"%s\"\n", shm_message);
    printf("Read from shared memory: \"%s\"\n", static_cast<char*>(shm_addr));

    if (!ipc.detach_shared_memory(shm_id, test_pid))
        printf("Failed to detach shared memory\n");
    else
        printf("Detached shared memory\n");

    if (!ipc.destroy_shared_memory(shm_id))
        printf("Failed to destroy shared memory\n");
    else
        printf("Destroyed shared memory\n");

    if (!ipc.destroy_message_queue(queue_id))
        printf("Failed to destroy message queue\n");
    else
        printf("Destroyed message queue\n");

    printf("IPC test completed successfully!\n");
    pm.terminate_process(test_pid);
}

}  // namespace commands