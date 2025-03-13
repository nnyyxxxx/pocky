#pragma once

#include <cstdint>
#include <cstring>

namespace fs {

constexpr size_t MAX_PATH = 256;
constexpr uint32_t ROOT_CLUSTER = 2;

struct SFat32BPB {
    uint8_t m_bootJmp[3];
    uint8_t m_oemName[8];
    uint16_t m_bytesPerSector;
    uint8_t m_sectorsPerCluster;
    uint16_t m_reservedSectorCount;
    uint8_t m_tableCount;
    uint16_t m_rootEntryCount;
    uint16_t m_totalSectors16;
    uint8_t m_mediaType;
    uint16_t m_tableSize16;
    uint16_t m_sectorsPerTrack;
    uint16_t m_headSideCount;
    uint32_t m_hiddenSectorCount;
    uint32_t m_totalSectors32;
    uint32_t m_tableSize32;
    uint16_t m_extFlags;
    uint16_t m_fatVersion;
    uint32_t m_rootCluster;
    uint16_t m_fsInfo;
    uint16_t m_backupBootSector;
    uint8_t m_reserved[12];
    uint8_t m_driveNumber;
    uint8_t m_reserved1;
    uint8_t m_bootSignature;
    uint32_t m_volumeId;
    uint8_t m_volumeLabel[11];
    uint8_t m_fatTypeLabel[8];
} __attribute__((packed));

struct SFat32FSInfo {
    uint32_t m_leadSignature;
    uint8_t m_reserved1[480];
    uint32_t m_structSignature;
    uint32_t m_freeCount;
    uint32_t m_nextFree;
    uint8_t m_reserved2[12];
    uint32_t m_trailSignature;
} __attribute__((packed));

struct SFat32LFNEntry {
    uint8_t m_order;
    uint16_t m_name1[5];
    uint8_t m_attributes;
    uint8_t m_type;
    uint8_t m_checksum;
    uint16_t m_name2[6];
    uint16_t m_zero;
    uint16_t m_name3[2];
} __attribute__((packed));

class CFat32FileSystem {
public:
    static CFat32FileSystem& instance() {
        static CFat32FileSystem instance;
        return instance;
    }

    bool initialize(const uint8_t* bootSector);
    bool mount();
    void unmount();
    void readFile(uint32_t startCluster, uint8_t* buffer, size_t bufferSize);
    bool writeFile(uint32_t startCluster, const uint8_t* data, size_t dataSize);
    const char* get_current_path() const {
        return m_currentPath;
    }
    void set_current_path(const char* path) {
        strncpy(m_currentPath, path, MAX_PATH - 1);
    }

    uint32_t get_current_directory_cluster() const {
        return m_currentDirectoryCluster;
    }

    void set_current_directory_cluster(uint32_t cluster) {
        m_currentDirectoryCluster = cluster;
    }

    bool createFile(const char* name, uint8_t attributes);
    bool createDirectory(const char* name);
    bool deleteFile(const char* name);
    bool deleteDirectory(const char* name);
    bool renameFile(const char* oldName, const char* newName);
    bool findFile(const char* name, uint32_t& cluster, uint32_t& size, uint8_t& attributes);
    bool findFileInDirectory(uint32_t dirCluster, const char* name, uint32_t& cluster,
                             uint32_t& size, uint8_t& attributes);

private:
    CFat32FileSystem() {
        strncpy(m_currentPath, "/", MAX_PATH - 1);
        m_currentDirectoryCluster = 2;
    }
    ~CFat32FileSystem() = default;
    CFat32FileSystem(const CFat32FileSystem&) = delete;
    CFat32FileSystem& operator=(const CFat32FileSystem&) = delete;

    bool validateBootSector(const SFat32BPB* bpb);
    bool readFSInfo();
    bool readSector(uint32_t sectorNumber, uint8_t* buffer);
    uint32_t readFatEntry(uint32_t cluster);
    void readDirectoryEntry(uint32_t startCluster);
    void followClusterChain(uint32_t startCluster);
    void parseDirectoryEntries(uint8_t* sectorData);
    bool diskRead(uint32_t sector, uint8_t* buffer, size_t size);
    bool diskWrite(uint32_t sector, const uint8_t* data, size_t size);

    uint32_t allocateCluster();
    void freeCluster(uint32_t cluster);
    bool updateFatEntry(uint32_t cluster, uint32_t value);
    uint8_t calculateShortNameChecksum(const char* shortName);
    bool writeDirectoryEntry(uint32_t cluster, const char* name, uint8_t attributes,
                             uint32_t fileCluster, uint32_t fileSize);
    bool writeLFNEntries(uint32_t cluster, const char* longName, uint8_t checksum);
    bool findFreeDirectoryEntry(uint32_t cluster, size_t& entryIndex);
    bool validateFileName(const char* name);
    bool isDirectoryEmpty(uint32_t cluster);
    void clearDirectory(uint32_t cluster);

    SFat32BPB m_bpb;
    SFat32FSInfo m_fsInfo;
    uint32_t m_firstDataSector;
    uint32_t m_dataSectors;
    uint32_t m_totalClusters;
    uint32_t m_firstFatSector;
    char m_currentPath[MAX_PATH];
    uint32_t m_currentDirectoryCluster;
};
}  // namespace fs