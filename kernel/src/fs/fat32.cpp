#include "fat32.hpp"

#include <cstdio>
#include <cstring>

#include "drivers/ata.hpp"
#include "printf.hpp"

namespace fs {

CFat32FileSystem::CFat32FileSystem() {
    strncpy(m_currentPath, "/", MAX_PATH - 1);
    strncpy(m_currentDirName, "/", MAX_PATH - 1);
    m_currentDirectoryCluster = 2;
    m_dirStackSize = 1;

    strncpy(m_dirStack[0].name, "/", MAX_PATH - 1);
    strncpy(m_dirStack[0].path, "/", MAX_PATH - 1);
    m_dirStack[0].cluster = ROOT_CLUSTER;
}

CFat32FileSystem& CFat32FileSystem::instance() {
    static CFat32FileSystem instance;
    return instance;
}

bool CFat32FileSystem::push_directory(const char* name, const char* path, uint32_t cluster) {
    if (m_dirStackSize >= MAX_DIR_STACK) return false;

    strncpy(m_dirStack[m_dirStackSize].name, name, MAX_PATH - 1);
    strncpy(m_dirStack[m_dirStackSize].path, path, MAX_PATH - 1);
    m_dirStack[m_dirStackSize].cluster = cluster;
    m_dirStackSize++;

    return true;
}

bool CFat32FileSystem::pop_directory() {
    if (m_dirStackSize <= 1) return false;

    m_dirStackSize--;
    strncpy(m_currentPath, m_dirStack[m_dirStackSize - 1].path, MAX_PATH - 1);
    strncpy(m_currentDirName, m_dirStack[m_dirStackSize - 1].name, MAX_PATH - 1);
    m_currentDirectoryCluster = m_dirStack[m_dirStackSize - 1].cluster;

    return true;
}

const CFat32FileSystem::SDirStackEntry* CFat32FileSystem::get_current_dir_entry() const {
    if (m_dirStackSize == 0) return nullptr;
    return &m_dirStack[m_dirStackSize - 1];
}

const CFat32FileSystem::SDirStackEntry* CFat32FileSystem::get_parent_dir_entry() const {
    if (m_dirStackSize <= 1) return nullptr;
    return &m_dirStack[m_dirStackSize - 2];
}

const char* CFat32FileSystem::get_current_path() const {
    return m_currentPath;
}

void CFat32FileSystem::set_current_path(const char* path) {
    if (!path) {
        m_currentPath[0] = '/';
        m_currentPath[1] = '\0';
        return;
    }

    strncpy(m_currentPath, path, MAX_PATH - 1);
    m_currentPath[MAX_PATH - 1] = '\0';
}

const char* CFat32FileSystem::get_current_dir_name() const {
    return m_currentDirName;
}

void CFat32FileSystem::set_current_dir_name(const char* name) {
    if (!name) {
        m_currentDirName[0] = '/';
        m_currentDirName[1] = '\0';
        return;
    }

    strncpy(m_currentDirName, name, MAX_PATH - 1);
    m_currentDirName[MAX_PATH - 1] = '\0';
}

uint32_t CFat32FileSystem::get_current_directory_cluster() const {
    return m_currentDirectoryCluster;
}

void CFat32FileSystem::set_current_directory_cluster(uint32_t cluster) {
    m_currentDirectoryCluster = cluster;
}

bool CFat32FileSystem::initialize(const uint8_t* bootSector) {
    if (!bootSector) return false;

    memcpy(&m_bpb, bootSector, sizeof(SFat32BPB));
    if (!validateBootSector(&m_bpb)) return false;

    uint32_t fatSize = m_bpb.m_tableSize16 ? m_bpb.m_tableSize16 : m_bpb.m_tableSize32;
    uint32_t rootDirSectors =
        ((m_bpb.m_rootEntryCount * 32) + (m_bpb.m_bytesPerSector - 1)) / m_bpb.m_bytesPerSector;
    uint32_t totalSectors =
        m_bpb.m_totalSectors16 ? m_bpb.m_totalSectors16 : m_bpb.m_totalSectors32;

    m_firstFatSector = m_bpb.m_reservedSectorCount;
    m_firstDataSector =
        m_bpb.m_reservedSectorCount + (m_bpb.m_tableCount * fatSize) + rootDirSectors;
    m_dataSectors = totalSectors - m_firstDataSector;
    m_totalClusters = m_dataSectors / m_bpb.m_sectorsPerCluster;

    return true;
}

bool CFat32FileSystem::validateBootSector(const SFat32BPB* bpb) {
    if (bpb->m_bytesPerSector == 0 || (bpb->m_bytesPerSector & (bpb->m_bytesPerSector - 1))) {
        printf("FAT32: Invalid bytes per sector: %u\n", bpb->m_bytesPerSector);
        return false;
    }

    if (bpb->m_sectorsPerCluster == 0 ||
        (bpb->m_sectorsPerCluster & (bpb->m_sectorsPerCluster - 1))) {
        printf("FAT32: Invalid sectors per cluster: %u\n", bpb->m_sectorsPerCluster);
        return false;
    }

    if (bpb->m_reservedSectorCount == 0) {
        printf("FAT32: Invalid reserved sector count\n");
        return false;
    }

    if (bpb->m_tableCount == 0) {
        printf("FAT32: Invalid FAT count\n");
        return false;
    }

    uint32_t fatSize = bpb->m_tableSize16 ? bpb->m_tableSize16 : bpb->m_tableSize32;
    if (fatSize == 0) {
        printf("FAT32: Invalid FAT size\n");
        return false;
    }

    if (bpb->m_bootSignature != 0x28 && bpb->m_bootSignature != 0x29) {
        printf("FAT32: Invalid boot signature: 0x%x\n", bpb->m_bootSignature);
        return false;
    }

    return true;
}

bool CFat32FileSystem::readFSInfo() {
    if (m_bpb.m_fsInfo == 0) return true;

    uint8_t fsInfoSector[m_bpb.m_bytesPerSector];
    if (!readSector(m_bpb.m_fsInfo, fsInfoSector)) {
        printf("FAT32: Failed to read FSInfo sector\n");
        return false;
    }

    SFat32FSInfo* fsInfo = reinterpret_cast<SFat32FSInfo*>(fsInfoSector);
    if (fsInfo->m_leadSignature != 0x41615252 || fsInfo->m_structSignature != 0x61417272 ||
        fsInfo->m_trailSignature != 0xAA550000) {
        printf("FAT32: Invalid FSInfo structure\n");
        return false;
    }

    m_fsInfo.m_freeCount = fsInfo->m_freeCount;
    m_fsInfo.m_nextFree = fsInfo->m_nextFree;
    return true;
}

bool CFat32FileSystem::mount() {
    uint8_t bootSector[512];

    auto& ata = drivers::CATADriver::instance();
    if (!ata.initialize()) {
        printf("FAT32: Failed to initialize ATA driver\n");
        return false;
    }

    if (!ata.read_sectors(0, 1, bootSector)) {
        printf("FAT32: Failed to read boot sector\n");
        return false;
    }

    if (!initialize(bootSector)) {
        printf("FAT32: Failed to initialize filesystem from boot sector\n");
        return false;
    }

    if (!readFSInfo()) return false;

    uint8_t rootBuffer[m_bpb.m_bytesPerSector];
    uint32_t rootSector = (ROOT_CLUSTER - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;

    if (!readSector(rootSector, rootBuffer)) {
        printf("FAT32: Failed to read root directory\n");
        return false;
    }

    if (rootBuffer[0] == 0) {
        memset(rootBuffer, 0, m_bpb.m_bytesPerSector);

        uint8_t* dotEntry = &rootBuffer[0];
        memcpy(dotEntry, ".          ", 11);
        dotEntry[11] = 0x10;
        dotEntry[26] = ROOT_CLUSTER & 0xFF;
        dotEntry[27] = (ROOT_CLUSTER >> 8) & 0xFF;

        uint8_t* dotdotEntry = &rootBuffer[32];
        memcpy(dotdotEntry, "..         ", 11);
        dotdotEntry[11] = 0x10;
        dotdotEntry[26] = ROOT_CLUSTER & 0xFF;
        dotdotEntry[27] = (ROOT_CLUSTER >> 8) & 0xFF;

        if (!diskWrite(rootSector, rootBuffer, m_bpb.m_bytesPerSector)) {
            printf("FAT32: Failed to initialize root directory\n");
            return false;
        }
    }
    return true;
}

void CFat32FileSystem::unmount() {
    m_firstDataSector = 0;
    m_dataSectors = 0;
    m_totalClusters = 0;
    m_firstFatSector = 0;
    memset(&m_bpb, 0, sizeof(m_bpb));
    memset(&m_fsInfo, 0, sizeof(m_fsInfo));
    printf("FAT32: Filesystem unmounted successfully\n");
}

uint32_t CFat32FileSystem::readFatEntry(uint32_t cluster) {
    if (m_bpb.m_bytesPerSector == 0) {
        printf("FAT32: Invalid bytes per sector parameter\n");
        return 0x0FFFFFF7;
    }

    uint32_t fatOffset = cluster * 4;
    uint32_t fatSector = m_firstFatSector + (fatOffset / m_bpb.m_bytesPerSector);
    uint32_t entOffset = fatOffset % m_bpb.m_bytesPerSector;

    uint8_t sectorData[m_bpb.m_bytesPerSector];
    if (!readSector(fatSector, sectorData)) {
        printf("FAT32: Failed to read FAT sector %u\n", fatSector);
        return 0x0FFFFFF7;
    }

    uint32_t tableValue = *reinterpret_cast<uint32_t*>(&sectorData[entOffset]);
    return tableValue & 0x0FFFFFFF;
}

void CFat32FileSystem::readDirectoryEntry(uint32_t startCluster) {
    uint32_t currentCluster = startCluster;
    uint8_t clusterData[m_bpb.m_bytesPerSector * m_bpb.m_sectorsPerCluster];
    char longFileName[256] = {0};
    size_t lfnIndex = 0;
    while (true) {
        uint32_t firstSector = (currentCluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;
        for (uint32_t i = 0; i < m_bpb.m_sectorsPerCluster; ++i) {
            if (!readSector(firstSector + i, &clusterData[i * m_bpb.m_bytesPerSector])) {
                printf("FAT32: Failed to read directory cluster %u\n", currentCluster);
                return;
            }
        }

        for (size_t i = 0; i < m_bpb.m_bytesPerSector * m_bpb.m_sectorsPerCluster; i += 32) {
            uint8_t* entry = &clusterData[i];
            if (entry[0] == 0x00) break;
            if (entry[0] == 0xE5) continue;

            if (entry[11] == 0x0F) {
                for (int j = 1; j < 11; j += 2) {
                    longFileName[lfnIndex++] = entry[j];
                }
                for (int j = 14; j < 26; j += 2) {
                    longFileName[lfnIndex++] = entry[j];
                }
                for (int j = 28; j < 32; j += 2) {
                    longFileName[lfnIndex++] = entry[j];
                }
                continue;
            }

            char shortName[12] = {0};
            memcpy(shortName, entry, 11);

            printf("FAT32: Found file: %s, LFN: %s\n", shortName, longFileName);
            memset(longFileName, 0, sizeof(longFileName));
            lfnIndex = 0;
        }

        uint32_t nextCluster = readFatEntry(currentCluster);
        if (nextCluster >= 0x0FFFFFF8) break;
        if (nextCluster == 0x0FFFFFF7) {
            printf("FAT32: Bad cluster detected at %u\n", currentCluster);
            break;
        }

        currentCluster = nextCluster;
    }
}

void CFat32FileSystem::followClusterChain(uint32_t startCluster) {
    uint32_t currentCluster = startCluster;
    uint8_t clusterData[m_bpb.m_bytesPerSector * m_bpb.m_sectorsPerCluster];
    while (true) {
        uint32_t nextCluster = readFatEntry(currentCluster);
        if (nextCluster >= 0x0FFFFFF8) break;
        if (nextCluster == 0x0FFFFFF7) {
            printf("FAT32: Bad cluster detected at %u\n", currentCluster);
            break;
        }

        uint32_t firstSector = (currentCluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;
        for (uint32_t i = 0; i < m_bpb.m_sectorsPerCluster; ++i) {
            if (!readSector(firstSector + i, &clusterData[i * m_bpb.m_bytesPerSector])) {
                printf("FAT32: Failed to read cluster %u\n", currentCluster);
                return;
            }
        }

        parseDirectoryEntries(clusterData);

        currentCluster = nextCluster;
    }
}

void CFat32FileSystem::parseDirectoryEntries(uint8_t* sectorData) {
    char longFileName[256] = {0};
    size_t lfnIndex = 0;

    for (size_t i = 0; i < m_bpb.m_bytesPerSector; i += 32) {
        uint8_t* entry = &sectorData[i];
        if (entry[0] == 0x00) break;
        if (entry[0] == 0xE5) continue;

        if (entry[11] == 0x0F) {
            for (int j = 1; j < 11; j += 2) {
                longFileName[lfnIndex++] = entry[j];
            }
            for (int j = 14; j < 26; j += 2) {
                longFileName[lfnIndex++] = entry[j];
            }
            for (int j = 28; j < 32; j += 2) {
                longFileName[lfnIndex++] = entry[j];
            }
            continue;
        }

        char shortName[12] = {0};
        memcpy(shortName, entry, 11);

        printf("FAT32: Found file: %s, LFN: %s\n", shortName, longFileName);
        memset(longFileName, 0, sizeof(longFileName));
        lfnIndex = 0;
    }
}

bool CFat32FileSystem::readSector(uint32_t sectorNumber, uint8_t* buffer) {
    if (m_bpb.m_bytesPerSector == 0) {
        printf("FAT32: Invalid bytes per sector parameter\n");
        return false;
    }

    if (!diskRead(sectorNumber, buffer, m_bpb.m_bytesPerSector)) {
        printf(
            "FAT32: Failed to read sector %u. Please check the disk connection or sector number.\n",
            sectorNumber);
        return false;
    }
    return true;
}

void CFat32FileSystem::readFile(uint32_t startCluster, uint8_t* buffer, size_t bufferSize) {
    if (m_bpb.m_sectorsPerCluster == 0 || m_bpb.m_bytesPerSector == 0) {
        printf("FAT32: Invalid filesystem parameters\n");
        return;
    }

    uint32_t currentCluster = startCluster;
    size_t bytesRead = 0;
    while (bytesRead < bufferSize) {
        uint32_t firstSector = (currentCluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;
        for (uint32_t i = 0; i < m_bpb.m_sectorsPerCluster && bytesRead < bufferSize; ++i) {
            if (!readSector(firstSector + i, &buffer[bytesRead])) {
                printf("FAT32: Failed to read file data at cluster %u\n", currentCluster);
                return;
            }
            bytesRead += m_bpb.m_bytesPerSector;
        }

        uint32_t nextCluster = readFatEntry(currentCluster);
        if (nextCluster >= 0x0FFFFFF8) break;
        if (nextCluster == 0x0FFFFFF7) {
            printf("FAT32: Bad cluster detected at %u during file read\n", currentCluster);
            break;
        }

        currentCluster = nextCluster;
    }
}

bool CFat32FileSystem::writeFile(uint32_t startCluster, const uint8_t* data, size_t dataSize) {
    if (m_bpb.m_sectorsPerCluster == 0 || m_bpb.m_bytesPerSector == 0) {
        printf("FAT32: Invalid filesystem parameters\n");
        return false;
    }

    uint32_t currentCluster = startCluster;
    size_t bytesWritten = 0;
    while (bytesWritten < dataSize) {
        uint32_t firstSector = (currentCluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;
        for (uint32_t i = 0; i < m_bpb.m_sectorsPerCluster && bytesWritten < dataSize; ++i) {
            if (!diskWrite(firstSector + i, &data[bytesWritten], m_bpb.m_bytesPerSector)) {
                printf("FAT32: Failed to write file data at cluster %u\n", currentCluster);
                return false;
            }
            bytesWritten += m_bpb.m_bytesPerSector;
        }

        uint32_t nextCluster = readFatEntry(currentCluster);
        if (nextCluster >= 0x0FFFFFF8) {
            if (bytesWritten < dataSize) {
                uint32_t newCluster = allocateCluster();
                if (newCluster == 0) {
                    printf("FAT32: Failed to allocate new cluster for file write\n");
                    return false;
                }

                if (!updateFatEntry(currentCluster, newCluster)) {
                    printf("FAT32: Failed to update FAT entry\n");
                    return false;
                }

                currentCluster = newCluster;
                continue;
            }
            break;
        }
        if (nextCluster == 0x0FFFFFF7) {
            printf("FAT32: Bad cluster detected at %u during file write\n", currentCluster);
            return false;
        }

        currentCluster = nextCluster;
    }

    if (!updateFatEntry(currentCluster, 0x0FFFFFF8)) {
        printf("FAT32: Failed to mark end of file\n");
        return false;
    }

    return true;
}

bool CFat32FileSystem::diskRead(uint32_t sector, uint8_t* buffer, size_t size) {
    if (size == 0) {
        printf("FAT32: Invalid read size\n");
        return false;
    }

    auto& ata = drivers::CATADriver::instance();
    if (!ata.initialize()) return false;

    uint8_t sectors = (size + 511) / 512;
    if (sectors == 0) sectors = 1;
    return ata.read_sectors(sector, sectors, buffer);
}

bool CFat32FileSystem::diskWrite(uint32_t sector, const uint8_t* data, size_t size) {
    if (size == 0) {
        printf("FAT32: Invalid write size\n");
        return false;
    }

    auto& ata = drivers::CATADriver::instance();
    if (!ata.initialize()) return false;

    uint8_t sectors = (size + 511) / 512;
    if (sectors == 0) sectors = 1;
    return ata.write_sectors(sector, sectors, data);
}

uint32_t CFat32FileSystem::allocateCluster() {
    uint32_t cluster = 2;
    while (cluster < m_totalClusters) {
        uint32_t fatEntry = readFatEntry(cluster);
        if (fatEntry == 0) {
            updateFatEntry(cluster, 0x0FFFFFF8);
            return cluster;
        }
        cluster++;
    }
    return 0;
}

void CFat32FileSystem::freeCluster(uint32_t cluster) {
    if (cluster < 2 || cluster >= m_totalClusters) return;
    updateFatEntry(cluster, 0);
}

bool CFat32FileSystem::updateFatEntry(uint32_t cluster, uint32_t value) {
    uint32_t fatOffset = cluster * 4;
    uint32_t fatSector = m_firstFatSector + (fatOffset / m_bpb.m_bytesPerSector);
    uint32_t entOffset = fatOffset % m_bpb.m_bytesPerSector;

    uint8_t sectorData[m_bpb.m_bytesPerSector];
    if (!readSector(fatSector, sectorData)) return false;

    uint32_t* fatEntry = reinterpret_cast<uint32_t*>(&sectorData[entOffset]);
    *fatEntry = value & 0x0FFFFFFF;

    for (uint8_t i = 0; i < m_bpb.m_tableCount; i++) {
        if (!diskWrite(fatSector + (i * m_bpb.m_tableSize32), sectorData, m_bpb.m_bytesPerSector))
            return false;
    }
    return true;
}

uint8_t CFat32FileSystem::calculateShortNameChecksum(const char* shortName) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + shortName[i];
    }
    return sum;
}

bool CFat32FileSystem::writeDirectoryEntry(uint32_t cluster, const char* name, uint8_t attributes,
                                           uint32_t fileCluster, uint32_t fileSize) {
    size_t entryIndex;
    if (!findFreeDirectoryEntry(cluster, entryIndex)) return false;

    uint8_t sectorData[m_bpb.m_bytesPerSector];
    uint32_t sector = (cluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;
    if (!readSector(sector, sectorData)) return false;

    uint8_t* entry = &sectorData[entryIndex];
    memset(entry, 0, 32);

    char shortName[12] = {0};
    strncpy(shortName, name, 11);
    memcpy(entry, shortName, 11);

    entry[11] = attributes;
    entry[26] = fileCluster & 0xFF;
    entry[27] = (fileCluster >> 8) & 0xFF;
    entry[28] = fileSize & 0xFF;
    entry[29] = (fileSize >> 8) & 0xFF;
    entry[30] = (fileSize >> 16) & 0xFF;
    entry[31] = (fileSize >> 24) & 0xFF;

    return diskWrite(sector, sectorData, m_bpb.m_bytesPerSector);
}

bool CFat32FileSystem::writeLFNEntries(uint32_t cluster, const char* longName, uint8_t checksum) {
    size_t nameLen = strlen(longName);
    size_t numEntries = (nameLen + 12) / 13;
    if (numEntries > 20) return false;

    size_t entryIndex;
    if (!findFreeDirectoryEntry(cluster, entryIndex)) return false;

    uint8_t sectorData[m_bpb.m_bytesPerSector];
    uint32_t sector = (cluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;
    if (!readSector(sector, sectorData)) return false;

    for (size_t i = 0; i < numEntries; i++) {
        size_t entryOffset = entryIndex - (numEntries - 1 - i) * 32;
        if (entryOffset >= m_bpb.m_bytesPerSector) {
            sector++;
            if (!readSector(sector, sectorData)) return false;
            entryOffset = 0;
        }

        uint8_t* entry = &sectorData[entryOffset];
        memset(entry, 0, 32);

        entry[0] = (i == numEntries - 1) ? (0x40 | (i + 1)) : (i + 1);
        entry[11] = 0x0F;

        for (int j = 0; j < 5; j++) {
            size_t nameIndex = i * 13 + j;
            entry[1 + j * 2] = (nameIndex < nameLen) ? longName[nameIndex] : 0;
            entry[2 + j * 2] = 0;
        }

        for (int j = 0; j < 6; j++) {
            size_t nameIndex = i * 13 + j + 5;
            entry[14 + j * 2] = (nameIndex < nameLen) ? longName[nameIndex] : 0;
            entry[15 + j * 2] = 0;
        }

        for (int j = 0; j < 2; j++) {
            size_t nameIndex = i * 13 + j + 11;
            entry[28 + j * 2] = (nameIndex < nameLen) ? longName[nameIndex] : 0;
            entry[29 + j * 2] = 0;
        }

        entry[13] = checksum;
    }

    return diskWrite(sector, sectorData, m_bpb.m_bytesPerSector);
}

bool CFat32FileSystem::findFreeDirectoryEntry(uint32_t cluster, size_t& entryIndex) {
    uint8_t sectorData[m_bpb.m_bytesPerSector];
    uint32_t currentCluster = cluster;

    while (true) {
        uint32_t sector = (currentCluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;
        for (uint32_t i = 0; i < m_bpb.m_sectorsPerCluster; i++) {
            if (!readSector(sector + i, sectorData)) return false;

            for (size_t j = 0; j < m_bpb.m_bytesPerSector; j += 32) {
                uint8_t* entry = &sectorData[j];
                if (entry[0] == 0x00 || entry[0] == 0xE5) {
                    entryIndex = j;
                    return true;
                }
            }
        }

        uint32_t nextCluster = readFatEntry(currentCluster);
        if (nextCluster >= 0x0FFFFFF8) break;
        if (nextCluster == 0x0FFFFFF7) return false;
        currentCluster = nextCluster;
    }

    return false;
}

bool CFat32FileSystem::validateFileName(const char* name) {
    if (!name || !*name) return false;
    if (strlen(name) > 255) return false;

    const char* invalid = "<>:\"/\\|?*";
    for (const char* p = name; *p; p++) {
        if (strchr(invalid, *p)) return false;
    }

    return true;
}

bool CFat32FileSystem::isDirectoryEmpty(uint32_t cluster) {
    uint8_t sectorData[m_bpb.m_bytesPerSector];
    uint32_t currentCluster = cluster;

    while (true) {
        uint32_t sector = (currentCluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;

        for (uint32_t i = 0; i < m_bpb.m_sectorsPerCluster; i++) {
            if (!readSector(sector + i, sectorData)) return false;

            for (size_t j = 0; j < m_bpb.m_bytesPerSector; j += 32) {
                uint8_t* entry = &sectorData[j];
                if (entry[0] == 0x00) return true;
                if (entry[0] == 0xE5) continue;

                if (strncmp(reinterpret_cast<char*>(entry), ".", 11) != 0 &&
                    strncmp(reinterpret_cast<char*>(entry), "..", 11) != 0)
                    return false;
            }
        }

        uint32_t nextCluster = readFatEntry(currentCluster);
        if (nextCluster >= 0x0FFFFFF8) break;
        if (nextCluster == 0x0FFFFFF7) return false;
        currentCluster = nextCluster;
    }

    return true;
}

void CFat32FileSystem::clearDirectory(uint32_t cluster) {
    uint8_t sectorData[m_bpb.m_bytesPerSector];
    uint32_t sector = (cluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;

    if (!readSector(sector, sectorData)) return;

    for (size_t i = 0; i < m_bpb.m_bytesPerSector; i += 32) {
        uint8_t* entry = &sectorData[i];
        if (entry[0] == 0x00) break;
        if (strncmp(reinterpret_cast<char*>(entry), ".", 11) != 0 &&
            strncmp(reinterpret_cast<char*>(entry), "..", 11) != 0)
            entry[0] = 0xE5;
    }

    diskWrite(sector, sectorData, m_bpb.m_bytesPerSector);
}

bool CFat32FileSystem::createFile(const char* name, uint8_t attributes) {
    if (!validateFileName(name)) return false;

    uint32_t parentCluster = m_currentDirectoryCluster;

    if (name[0] == '/') {
        parentCluster = ROOT_CLUSTER;
        name++;
    }

    if (!*name) return false;

    uint32_t existingCluster;
    uint32_t existingSize;
    uint8_t existingAttrs;
    if (findFileInDirectory(parentCluster, name, existingCluster, existingSize, existingAttrs))
        return false;

    uint32_t cluster = allocateCluster();
    if (!cluster) return false;

    char shortName[12] = {0};
    strncpy(shortName, name, 11);
    uint8_t checksum = calculateShortNameChecksum(shortName);

    if (strlen(name) > 11) {
        if (!writeLFNEntries(parentCluster, name, checksum)) {
            freeCluster(cluster);
            return false;
        }
    }

    if (!writeDirectoryEntry(parentCluster, shortName, attributes, cluster, 0)) {
        freeCluster(cluster);
        return false;
    }

    return true;
}

bool CFat32FileSystem::createDirectory(const char* name) {
    if (!validateFileName(name)) return false;

    uint32_t parentCluster = m_currentDirectoryCluster;

    if (name[0] == '/') {
        parentCluster = ROOT_CLUSTER;
        name++;
    }

    if (!*name) return false;

    uint32_t existingCluster;
    uint32_t existingSize;
    uint8_t existingAttrs;
    if (findFileInDirectory(parentCluster, name, existingCluster, existingSize, existingAttrs))
        return false;

    uint32_t cluster = allocateCluster();
    if (!cluster) return false;

    char shortName[12] = {0};
    strncpy(shortName, name, 11);
    uint8_t checksum = calculateShortNameChecksum(shortName);

    if (strlen(name) > 11) {
        if (!writeLFNEntries(parentCluster, name, checksum)) {
            freeCluster(cluster);
            return false;
        }
    }

    if (!writeDirectoryEntry(parentCluster, shortName, 0x10, cluster, 0)) {
        freeCluster(cluster);
        return false;
    }

    uint8_t sectorData[m_bpb.m_bytesPerSector];
    uint32_t sector = (cluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;
    memset(sectorData, 0, m_bpb.m_bytesPerSector);

    uint8_t* dotEntry = &sectorData[0];
    strcpy(reinterpret_cast<char*>(dotEntry), ".");
    dotEntry[11] = 0x10;
    dotEntry[26] = cluster & 0xFF;
    dotEntry[27] = (cluster >> 8) & 0xFF;

    uint8_t* dotdotEntry = &sectorData[32];
    strcpy(reinterpret_cast<char*>(dotdotEntry), "..");
    dotdotEntry[11] = 0x10;
    dotdotEntry[26] = parentCluster & 0xFF;
    dotdotEntry[27] = (parentCluster >> 8) & 0xFF;

    return diskWrite(sector, sectorData, m_bpb.m_bytesPerSector);
}

bool CFat32FileSystem::deleteFile(const char* name) {
    uint32_t cluster, size;
    uint8_t attributes;
    if (!findFile(name, cluster, size, attributes)) return false;

    if (attributes & 0x10) return false;

    uint32_t directoryCluster = m_currentDirectoryCluster;
    const char* fileName = name;

    if (name[0] == '/') {
        directoryCluster = ROOT_CLUSTER;
        fileName++;
    }

    const char* lastSlash = strrchr(fileName, '/');
    if (lastSlash) {
        char dirPath[MAX_PATH] = {0};
        strncpy(dirPath, fileName, lastSlash - fileName);

        uint32_t tempCluster, tempSize;
        uint8_t tempAttributes;
        if (!findFile(dirPath, tempCluster, tempSize, tempAttributes) || !(tempAttributes & 0x10))
            return false;

        directoryCluster = tempCluster;
        fileName = lastSlash + 1;
    }

    uint8_t sectorData[m_bpb.m_bytesPerSector];
    uint32_t currentCluster = directoryCluster;

    while (true) {
        uint32_t sector = (currentCluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;

        for (uint32_t i = 0; i < m_bpb.m_sectorsPerCluster; i++) {
            if (!readSector(sector + i, sectorData)) return false;

            for (size_t j = 0; j < m_bpb.m_bytesPerSector; j += 32) {
                uint8_t* entry = &sectorData[j];
                if (entry[0] == 0x00) break;
                if (entry[0] == 0xE5) continue;
                if (entry[11] == 0x0F) continue;

                char entryName[13] = {0};
                memcpy(entryName, entry, 11);
                for (int k = 10; k >= 0; k--) {
                    if (entryName[k] == ' ')
                        entryName[k] = '\0';
                    else
                        break;
                }

                if (strcmp(entryName, fileName) == 0) {
                    entry[0] = 0xE5;
                    if (!diskWrite(sector + i, sectorData, m_bpb.m_bytesPerSector)) return false;
                    break;
                }
            }
        }

        uint32_t nextCluster = readFatEntry(currentCluster);
        if (nextCluster >= 0x0FFFFFF8) break;
        if (nextCluster == 0x0FFFFFF7) return false;
        currentCluster = nextCluster;
    }

    while (cluster < 0x0FFFFFF8) {
        uint32_t nextCluster = readFatEntry(cluster);
        freeCluster(cluster);
        if (nextCluster >= 0x0FFFFFF8) break;
        if (nextCluster == 0x0FFFFFF7) break;
        cluster = nextCluster;
    }

    return true;
}

bool CFat32FileSystem::deleteDirectory(const char* name) {
    uint32_t cluster, size;
    uint8_t attributes;
    if (!findFile(name, cluster, size, attributes)) return false;

    if (!(attributes & 0x10)) return false;
    if (!isDirectoryEmpty(cluster)) return false;

    uint32_t directoryCluster = m_currentDirectoryCluster;
    const char* dirName = name;

    if (name[0] == '/') {
        directoryCluster = ROOT_CLUSTER;
        dirName++;
    }

    const char* lastSlash = strrchr(dirName, '/');
    if (lastSlash) {
        char parentPath[MAX_PATH] = {0};
        strncpy(parentPath, dirName, lastSlash - dirName);

        uint32_t tempCluster, tempSize;
        uint8_t tempAttributes;
        if (!findFile(parentPath, tempCluster, tempSize, tempAttributes) ||
            !(tempAttributes & 0x10))
            return false;

        directoryCluster = tempCluster;
        dirName = lastSlash + 1;
    }

    uint8_t sectorData[m_bpb.m_bytesPerSector];
    uint32_t currentCluster = directoryCluster;

    while (true) {
        uint32_t sector = (currentCluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;

        for (uint32_t i = 0; i < m_bpb.m_sectorsPerCluster; i++) {
            if (!readSector(sector + i, sectorData)) return false;

            for (size_t j = 0; j < m_bpb.m_bytesPerSector; j += 32) {
                uint8_t* entry = &sectorData[j];
                if (entry[0] == 0x00) break;
                if (entry[0] == 0xE5) continue;
                if (entry[11] == 0x0F) continue;

                char entryName[13] = {0};
                memcpy(entryName, entry, 11);
                for (int k = 10; k >= 0; k--) {
                    if (entryName[k] == ' ')
                        entryName[k] = '\0';
                    else
                        break;
                }

                if (strcmp(entryName, dirName) == 0) {
                    entry[0] = 0xE5;
                    if (!diskWrite(sector + i, sectorData, m_bpb.m_bytesPerSector)) return false;
                    break;
                }
            }
        }

        uint32_t nextCluster = readFatEntry(currentCluster);
        if (nextCluster >= 0x0FFFFFF8) break;
        if (nextCluster == 0x0FFFFFF7) return false;
        currentCluster = nextCluster;
    }

    while (cluster < 0x0FFFFFF8) {
        uint32_t nextCluster = readFatEntry(cluster);
        freeCluster(cluster);
        if (nextCluster >= 0x0FFFFFF8) break;
        if (nextCluster == 0x0FFFFFF7) break;
        cluster = nextCluster;
    }

    return true;
}

bool CFat32FileSystem::renameFile(const char* oldName, const char* newName) {
    if (!validateFileName(newName)) return false;

    uint32_t sourceCluster, sourceSize;
    uint8_t sourceAttributes;
    if (!findFile(oldName, sourceCluster, sourceSize, sourceAttributes)) return false;

    uint32_t sourceDirectoryCluster = m_currentDirectoryCluster;
    const char* sourceFileName = oldName;

    if (oldName[0] == '/') {
        sourceDirectoryCluster = ROOT_CLUSTER;
        sourceFileName++;
    }

    const char* sourceLastSlash = strrchr(sourceFileName, '/');
    if (sourceLastSlash) {
        char sourcePath[MAX_PATH] = {0};
        strncpy(sourcePath, sourceFileName, sourceLastSlash - sourceFileName);

        uint32_t tempCluster, tempSize;
        uint8_t tempAttributes;
        if (!findFile(sourcePath, tempCluster, tempSize, tempAttributes) ||
            !(tempAttributes & 0x10))
            return false;

        sourceDirectoryCluster = tempCluster;
        sourceFileName = sourceLastSlash + 1;
    }

    uint32_t destDirectoryCluster = m_currentDirectoryCluster;
    const char* destFileName = newName;
    bool destIsDirectory = false;

    if (newName[0] == '/') {
        destDirectoryCluster = ROOT_CLUSTER;
        destFileName++;
    }

    uint32_t destCluster, destSize;
    uint8_t destAttributes;
    if (findFile(newName, destCluster, destSize, destAttributes)) {
        if (destAttributes & 0x10) {
            destIsDirectory = true;
            destDirectoryCluster = destCluster;
            destFileName = sourceFileName;
        } else
            return false;
    } else {
        const char* destLastSlash = strrchr(destFileName, '/');
        if (destLastSlash) {
            char destPath[MAX_PATH] = {0};
            strncpy(destPath, destFileName, destLastSlash - destFileName);

            uint32_t tempCluster, tempSize;
            uint8_t tempAttributes;
            if (!findFile(destPath, tempCluster, tempSize, tempAttributes) ||
                !(tempAttributes & 0x10))
                return false;

            destDirectoryCluster = tempCluster;
            destFileName = destLastSlash + 1;
        }
    }

    if (sourceDirectoryCluster == destDirectoryCluster && strcmp(sourceFileName, destFileName) == 0)
        return true;

    if (sourceDirectoryCluster == destDirectoryCluster) {
        uint8_t sectorData[m_bpb.m_bytesPerSector];
        uint32_t currentCluster = sourceDirectoryCluster;

        while (true) {
            uint32_t sector = (currentCluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;

            for (uint32_t i = 0; i < m_bpb.m_sectorsPerCluster; i++) {
                if (!readSector(sector + i, sectorData)) return false;

                for (size_t j = 0; j < m_bpb.m_bytesPerSector; j += 32) {
                    uint8_t* entry = &sectorData[j];
                    if (entry[0] == 0x00) break;
                    if (entry[0] == 0xE5) continue;
                    if (entry[11] == 0x0F) continue;

                    char entryName[13] = {0};
                    memcpy(entryName, entry, 11);
                    for (int k = 10; k >= 0; k--) {
                        if (entryName[k] == ' ')
                            entryName[k] = '\0';
                        else
                            break;
                    }

                    if (strcmp(entryName, sourceFileName) == 0) {
                        memset(entry, ' ', 11);
                        strncpy(reinterpret_cast<char*>(entry), destFileName, strlen(destFileName));

                        if (!diskWrite(sector + i, sectorData, m_bpb.m_bytesPerSector))
                            return false;
                        return true;
                    }
                }
            }

            uint32_t nextCluster = readFatEntry(currentCluster);
            if (nextCluster >= 0x0FFFFFF8) break;
            if (nextCluster == 0x0FFFFFF7) return false;
            currentCluster = nextCluster;
        }
    } else {
        if (!writeDirectoryEntry(destDirectoryCluster, destFileName, sourceAttributes,
                                 sourceCluster, sourceSize))
            return false;

        uint8_t sectorData[m_bpb.m_bytesPerSector];
        uint32_t currentCluster = sourceDirectoryCluster;

        while (true) {
            uint32_t sector = (currentCluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;

            for (uint32_t i = 0; i < m_bpb.m_sectorsPerCluster; i++) {
                if (!readSector(sector + i, sectorData)) return false;

                for (size_t j = 0; j < m_bpb.m_bytesPerSector; j += 32) {
                    uint8_t* entry = &sectorData[j];
                    if (entry[0] == 0x00) break;
                    if (entry[0] == 0xE5) continue;
                    if (entry[11] == 0x0F) continue;

                    char entryName[13] = {0};
                    memcpy(entryName, entry, 11);
                    for (int k = 10; k >= 0; k--) {
                        if (entryName[k] == ' ')
                            entryName[k] = '\0';
                        else
                            break;
                    }

                    if (strcmp(entryName, sourceFileName) == 0) {
                        entry[0] = 0xE5;
                        if (!diskWrite(sector + i, sectorData, m_bpb.m_bytesPerSector))
                            return false;
                        return true;
                    }
                }
            }

            uint32_t nextCluster = readFatEntry(currentCluster);
            if (nextCluster >= 0x0FFFFFF8) break;
            if (nextCluster == 0x0FFFFFF7) return false;
            currentCluster = nextCluster;
        }
    }

    return false;
}

bool CFat32FileSystem::findFile(const char* name, uint32_t& cluster, uint32_t& size,
                                uint8_t& attributes) {
    uint32_t searchCluster = m_currentDirectoryCluster;

    if (name[0] == '/') {
        searchCluster = ROOT_CLUSTER;
        name++;
    }

    if (!*name) {
        cluster = searchCluster;
        size = 0;
        attributes = 0x10;
        return true;
    }

    const char* slash = strchr(name, '/');
    if (slash) {
        char dirname[13] = {0};
        strncpy(dirname, name, slash - name);

        uint32_t dirCluster;
        uint32_t dirSize;
        uint8_t dirAttrs;
        if (!findFileInDirectory(searchCluster, dirname, dirCluster, dirSize, dirAttrs))
            return false;

        if (!(dirAttrs & 0x10)) return false;

        return findFile(slash + 1, cluster, size, attributes);
    }

    return findFileInDirectory(searchCluster, name, cluster, size, attributes);
}

bool CFat32FileSystem::findFileInDirectory(uint32_t dirCluster, const char* name, uint32_t& cluster,
                                           uint32_t& size, uint8_t& attributes) {
    uint8_t sectorData[m_bpb.m_bytesPerSector];
    uint32_t currentCluster = dirCluster;

    while (true) {
        uint32_t sector = (currentCluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;

        for (uint32_t i = 0; i < m_bpb.m_sectorsPerCluster; i++) {
            if (!readSector(sector + i, sectorData)) return false;

            for (size_t j = 0; j < m_bpb.m_bytesPerSector; j += 32) {
                uint8_t* entry = &sectorData[j];
                if (entry[0] == 0x00) break;

                if (entry[0] == 0xE5) continue;

                if (entry[11] == 0x0F) continue;

                char entryName[13] = {0};
                memcpy(entryName, entry, 11);

                for (int k = 10; k >= 0; k--) {
                    if (entryName[k] == ' ')
                        entryName[k] = '\0';
                    else
                        break;
                }

                if (strcmp(entryName, name) == 0) {
                    cluster = (entry[26] | (entry[27] << 8));
                    size = (entry[28] | (entry[29] << 8) | (entry[30] << 16) | (entry[31] << 24));
                    attributes = entry[11];
                    return true;
                }
            }
        }

        uint32_t nextCluster = readFatEntry(currentCluster);
        if (nextCluster >= 0x0FFFFFF8) break;

        if (nextCluster == 0x0FFFFFF7) return false;

        currentCluster = nextCluster;
    }

    return false;
}

bool CFat32FileSystem::updateFileSize(const char* filename, uint32_t newSize) {
    uint32_t directoryCluster = m_currentDirectoryCluster;
    const char* fileName = filename;

    if (filename[0] == '/') {
        directoryCluster = ROOT_CLUSTER;
        fileName++;
    }

    const char* lastSlash = strrchr(fileName, '/');
    if (lastSlash) {
        char dirPath[MAX_PATH] = {0};
        strncpy(dirPath, fileName, lastSlash - fileName);

        uint32_t tempCluster, tempSize;
        uint8_t tempAttributes;
        if (!findFile(dirPath, tempCluster, tempSize, tempAttributes) || !(tempAttributes & 0x10))
            return false;

        directoryCluster = tempCluster;
        fileName = lastSlash + 1;
    }

    uint8_t sectorData[m_bpb.m_bytesPerSector];
    uint32_t currentCluster = directoryCluster;

    while (true) {
        uint32_t sector = (currentCluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;

        for (uint32_t i = 0; i < m_bpb.m_sectorsPerCluster; i++) {
            if (!readSector(sector + i, sectorData)) return false;

            for (size_t j = 0; j < m_bpb.m_bytesPerSector; j += 32) {
                uint8_t* entry = &sectorData[j];
                if (entry[0] == 0x00) break;
                if (entry[0] == 0xE5) continue;
                if (entry[11] == 0x0F) continue;

                char entryName[13] = {0};
                memcpy(entryName, entry, 11);

                for (int k = 10; k >= 0; k--) {
                    if (entryName[k] == ' ')
                        entryName[k] = '\0';
                    else
                        break;
                }

                if (strcmp(entryName, fileName) == 0) {
                    entry[28] = newSize & 0xFF;
                    entry[29] = (newSize >> 8) & 0xFF;
                    entry[30] = (newSize >> 16) & 0xFF;
                    entry[31] = (newSize >> 24) & 0xFF;

                    if (!diskWrite(sector + i, sectorData, m_bpb.m_bytesPerSector)) return false;
                    return true;
                }
            }
        }

        uint32_t nextCluster = readFatEntry(currentCluster);
        if (nextCluster >= 0x0FFFFFF8) break;
        if (nextCluster == 0x0FFFFFF7) return false;
        currentCluster = nextCluster;
    }

    return false;
}

bool CFat32FileSystem::deleteDirectoryRecursive(const char* name) {
    uint32_t dirCluster, dirSize;
    uint8_t dirAttributes;
    if (!findFile(name, dirCluster, dirSize, dirAttributes)) return false;

    if (!(dirAttributes & 0x10)) return false;

    char fullPath[MAX_PATH] = {0};
    if (name[0] == '/')
        strncpy(fullPath, name, MAX_PATH - 1);
    else {
        if (m_currentPath[strlen(m_currentPath) - 1] == '/')
            snprintf(fullPath, MAX_PATH - 1, "%s%s", m_currentPath, name);
        else
            snprintf(fullPath, MAX_PATH - 1, "%s/%s", m_currentPath, name);
    }

    uint8_t sectorData[m_bpb.m_bytesPerSector];
    uint32_t currentCluster = dirCluster;

    while (true) {
        uint32_t sector = (currentCluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;

        for (uint32_t i = 0; i < m_bpb.m_sectorsPerCluster; i++) {
            if (!readSector(sector + i, sectorData)) return false;

            for (size_t j = 0; j < m_bpb.m_bytesPerSector; j += 32) {
                uint8_t* entry = &sectorData[j];
                if (entry[0] == 0x00) break;
                if (entry[0] == 0xE5) continue;
                if (entry[11] == 0x0F) continue;

                char entryName[13] = {0};
                memcpy(entryName, entry, 11);

                for (int k = 10; k >= 0; k--) {
                    if (entryName[k] == ' ')
                        entryName[k] = '\0';
                    else
                        break;
                }

                if (strcmp(entryName, ".") == 0 || strcmp(entryName, "..") == 0) continue;

                char entryPath[MAX_PATH] = {0};
                snprintf(entryPath, MAX_PATH - 1, "%s/%s", fullPath, entryName);

                uint8_t attributes = entry[11];
                if (attributes & 0x10)
                    deleteDirectoryRecursive(entryPath);
                else
                    deleteFile(entryPath);
            }
        }

        uint32_t nextCluster = readFatEntry(currentCluster);
        if (nextCluster >= 0x0FFFFFF8) break;
        if (nextCluster == 0x0FFFFFF7) return false;
        currentCluster = nextCluster;
    }

    uint32_t directoryCluster = m_currentDirectoryCluster;
    const char* dirName = name;

    if (name[0] == '/') {
        directoryCluster = ROOT_CLUSTER;
        dirName++;
    }

    const char* lastSlash = strrchr(dirName, '/');
    if (lastSlash) {
        char parentPath[MAX_PATH] = {0};
        strncpy(parentPath, dirName, lastSlash - dirName);

        uint32_t tempCluster, tempSize;
        uint8_t tempAttributes;
        if (!findFile(parentPath, tempCluster, tempSize, tempAttributes) ||
            !(tempAttributes & 0x10))
            return false;

        directoryCluster = tempCluster;
        dirName = lastSlash + 1;
    }

    uint8_t entryData[m_bpb.m_bytesPerSector];
    currentCluster = directoryCluster;

    while (true) {
        uint32_t sector = (currentCluster - 2) * m_bpb.m_sectorsPerCluster + m_firstDataSector;

        for (uint32_t i = 0; i < m_bpb.m_sectorsPerCluster; i++) {
            if (!readSector(sector + i, entryData)) return false;

            for (size_t j = 0; j < m_bpb.m_bytesPerSector; j += 32) {
                uint8_t* entry = &entryData[j];
                if (entry[0] == 0x00) break;
                if (entry[0] == 0xE5) continue;
                if (entry[11] == 0x0F) continue;

                char entryName[13] = {0};
                memcpy(entryName, entry, 11);
                for (int k = 10; k >= 0; k--) {
                    if (entryName[k] == ' ')
                        entryName[k] = '\0';
                    else
                        break;
                }

                if (strcmp(entryName, dirName) == 0) {
                    entry[0] = 0xE5;
                    if (!diskWrite(sector + i, entryData, m_bpb.m_bytesPerSector)) return false;

                    while (dirCluster < 0x0FFFFFF8) {
                        uint32_t nextCluster = readFatEntry(dirCluster);
                        freeCluster(dirCluster);
                        if (nextCluster >= 0x0FFFFFF8) break;
                        if (nextCluster == 0x0FFFFFF7) break;
                        dirCluster = nextCluster;
                    }

                    return true;
                }
            }
        }

        uint32_t nextCluster = readFatEntry(currentCluster);
        if (nextCluster >= 0x0FFFFFF8) break;
        if (nextCluster == 0x0FFFFFF7) return false;
        currentCluster = nextCluster;
    }

    return false;
}

}  // namespace fs