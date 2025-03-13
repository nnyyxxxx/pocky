#pragma once

#include <cstdint>

namespace drivers {

constexpr uint16_t ATA_PRIMARY_IO = 0x1F0;
constexpr uint16_t ATA_SECONDARY_IO = 0x170;

constexpr uint16_t ATA_DATA = 0;
constexpr uint16_t ATA_ERROR = 1;
constexpr uint16_t ATA_FEATURES = 1;
constexpr uint16_t ATA_SECTOR_COUNT = 2;
constexpr uint16_t ATA_LBA_LOW = 3;
constexpr uint16_t ATA_LBA_MID = 4;
constexpr uint16_t ATA_LBA_HIGH = 5;
constexpr uint16_t ATA_DRIVE_HEAD = 6;
constexpr uint16_t ATA_STATUS = 7;
constexpr uint16_t ATA_COMMAND = 7;

constexpr uint8_t ATA_CMD_READ_SECTORS = 0x20;
constexpr uint8_t ATA_CMD_WRITE_SECTORS = 0x30;
constexpr uint8_t ATA_CMD_IDENTIFY = 0xEC;

constexpr uint8_t ATA_STATUS_ERR = 0x01;
constexpr uint8_t ATA_STATUS_DRQ = 0x08;
constexpr uint8_t ATA_STATUS_SRV = 0x10;
constexpr uint8_t ATA_STATUS_DF = 0x20;
constexpr uint8_t ATA_STATUS_RDY = 0x40;
constexpr uint8_t ATA_STATUS_BSY = 0x80;

class CATADriver {
public:
    static CATADriver& instance();

    bool initialize();
    bool read_sectors(uint32_t lba, uint8_t sectors, uint8_t* buffer);
    bool write_sectors(uint32_t lba, uint8_t sectors, const uint8_t* buffer);

private:
    CATADriver() = default;
    ~CATADriver() = default;
    CATADriver(const CATADriver&) = delete;
    CATADriver& operator=(const CATADriver&) = delete;

    bool wait_not_busy();
    bool wait_ready();
    void select_drive(uint8_t slave);
    bool identify();

    bool m_initialized = false;
    bool m_is_slave = false;
    uint16_t m_io_base = ATA_PRIMARY_IO;
    uint32_t m_sectors = 0;
    char m_model[41] = {0};
    char m_serial[21] = {0};
};

}  // namespace drivers