#pragma once

#include <cstddef>
#include <cstdint>

namespace fs {

constexpr size_t MAX_FILENAME = 256;
constexpr size_t MAX_PATH = 4096;
constexpr size_t BLOCK_SIZE = 4096;

enum class FileType {
    Regular,
    Directory,
};

struct FileNode {
    char name[MAX_FILENAME];
    FileType type;
    size_t size;
    uint8_t* data;
    FileNode* parent;
    FileNode* first_child;
    FileNode* next_sibling;

    FileNode(const char* name, FileType type);
    ~FileNode();
};

class FileSystem {
public:
    static FileSystem& instance();

    bool initialize();
    FileNode* create_file(const char* path, FileType type);
    bool delete_file(const char* path);
    FileNode* get_file(const char* path);
    bool write_file(const char* path, const uint8_t* data, size_t size);
    bool read_file(const char* path, uint8_t* buffer, size_t size);
    FileNode* get_current_directory() const { return m_current_dir; }
    bool change_directory(const char* path);
    void list_directory(const char* path, void (*callback)(const FileNode*));
    const char* get_current_path() const;

private:
    FileSystem() = default;
    ~FileSystem() = default;
    FileSystem(const FileSystem&) = delete;
    FileSystem& operator=(const FileSystem&) = delete;

    FileNode* resolve_path(const char* path);
    FileNode* create_node(const char* name, FileType type);
    void build_path(FileNode* node, char* buffer, size_t size) const;

    FileNode* m_root = nullptr;
    FileNode* m_current_dir = nullptr;
    mutable char m_path_buffer[MAX_PATH] = {0};
};

} // namespace fs