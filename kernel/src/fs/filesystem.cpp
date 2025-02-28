#include "filesystem.hpp"

#include <cstring>

#include "memory/heap.hpp"

namespace fs {

FileNode::FileNode(const char* name, FileType type)
    : type(type),
      size(0),
      data(nullptr),
      parent(nullptr),
      first_child(nullptr),
      next_sibling(nullptr) {
    strncpy(this->name, name, MAX_FILENAME - 1);
    this->name[MAX_FILENAME - 1] = '\0';
}

FileNode::~FileNode() {
    if (data) delete[] data;

    FileNode* child = first_child;
    while (child) {
        FileNode* next = child->next_sibling;
        delete child;
        child = next;
    }
}

FileSystem& FileSystem::instance() {
    static FileSystem instance;
    return instance;
}

bool FileSystem::initialize() {
    if (m_root) return false;

    m_root = create_node("/", FileType::Directory);
    m_current_dir = m_root;
    return true;
}

FileNode* FileSystem::create_node(const char* name, FileType type) {
    return new FileNode(name, type);
}

FileNode* FileSystem::resolve_path(const char* path) {
    if (!path || !*path) return nullptr;

    FileNode* current = *path == '/' ? m_root : m_current_dir;
    if (!*path || (*path == '/' && !path[1])) return current;

    char component[MAX_FILENAME];
    const char* start = *path == '/' ? path + 1 : path;
    const char* end = start;

    while (*start) {
        end = strchr(start, '/');
        if (!end) end = start + strlen(start);

        size_t len = end - start;
        if (len >= MAX_FILENAME) return nullptr;

        strncpy(component, start, len);
        component[len] = '\0';

        if (strcmp(component, ".") == 0) {
            // stay in current directory
        } else if (strcmp(component, "..") == 0) {
            if (current->parent) current = current->parent;
        } else {
            FileNode* child = current->first_child;
            FileNode* found_dir = nullptr;

            bool is_last = !*end;
            bool has_trailing_slash = is_last && (path[strlen(path) - 1] == '/');

            while (child) {
                if (strcmp(child->name, component) == 0) {
                    if (child->type == FileType::Regular) {
                        if (is_last && !has_trailing_slash) {
                            current = child;
                            break;
                        }
                    } else if (child->type == FileType::Directory) {
                        found_dir = child;
                        if (!is_last || has_trailing_slash) {
                            current = child;
                            break;
                        }
                    }
                }
                child = child->next_sibling;
            }

            if (!child && found_dir)
                current = found_dir;
            else if (!child && !found_dir && *end)
                return nullptr;
        }

        if (!*end) break;
        start = end + 1;
    }

    return current;
}

FileNode* FileSystem::create_file(const char* path, FileType type) {
    if (!path || !*path) return nullptr;

    char parent_path[MAX_PATH];
    const char* filename = strrchr(path, '/');

    if (!filename) {
        filename = path;
        parent_path[0] = '.';
        parent_path[1] = '\0';
    } else {
        size_t len = filename - path;
        if (len >= MAX_PATH) return nullptr;
        strncpy(parent_path, path, len);
        parent_path[len] = '\0';
        filename++;
    }

    FileNode* parent = resolve_path(parent_path);
    if (!parent || parent->type != FileType::Directory) return nullptr;

    FileNode* existing = parent->first_child;
    while (existing) {
        if (strcmp(existing->name, filename) == 0 && existing->type == type) return existing;
        existing = existing->next_sibling;
    }

    FileNode* node = create_node(filename, type);
    if (!node) return nullptr;

    node->parent = parent;
    node->next_sibling = parent->first_child;
    parent->first_child = node;

    return node;
}

bool FileSystem::delete_file(const char* path) {
    FileNode* node = resolve_path(path);
    if (!node || node == m_root) return false;

    FileNode* parent = node->parent;
    if (!parent) return false;

    if (parent->first_child == node) {
        parent->first_child = node->next_sibling;
    } else {
        FileNode* prev = parent->first_child;
        while (prev && prev->next_sibling != node) {
            prev = prev->next_sibling;
        }
        if (prev) prev->next_sibling = node->next_sibling;
    }

    delete node;
    return true;
}

FileNode* FileSystem::get_file(const char* path) {
    return resolve_path(path);
}

bool FileSystem::write_file(const char* path, const uint8_t* data, size_t size) {
    FileNode* node = resolve_path(path);
    if (!node || node->type != FileType::Regular) return false;

    uint8_t* new_data = new uint8_t[size];
    if (!new_data) return false;

    memcpy(new_data, data, size);
    delete[] node->data;
    node->data = new_data;
    node->size = size;

    return true;
}

bool FileSystem::read_file(const char* path, uint8_t* buffer, size_t size) {
    FileNode* node = resolve_path(path);
    if (!node || node->type != FileType::Regular || !node->data) return false;

    size_t to_read = size < node->size ? size : node->size;
    memcpy(buffer, node->data, to_read);
    return true;
}

bool FileSystem::change_directory(const char* path) {
    FileNode* node = resolve_path(path);
    if (!node || node->type != FileType::Directory) return false;

    m_current_dir = node;
    return true;
}

void FileSystem::list_directory(const char* path, void (*callback)(const FileNode*)) {
    FileNode* dir = path ? resolve_path(path) : m_current_dir;
    if (!dir || dir->type != FileType::Directory) return;

    FileNode* child = dir->first_child;
    while (child) {
        callback(child);
        child = child->next_sibling;
    }
}

}  // namespace fs