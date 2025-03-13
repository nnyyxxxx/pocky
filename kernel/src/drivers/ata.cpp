#include "ata.hpp"

#include "io.hpp"
#include "printf.hpp"

namespace drivers {

CATADriver& CATADriver::instance() {
    static CATADriver instance;
    return instance;
}

bool CATADriver::initialize() {
    if (m_initialized) return true;

    if (!identify()) {
        printf("ATA: Drive not found\n");
        return false;
    }

    m_initialized = true;
    printf("ATA: Drive initialized\n");
    return true;
}

bool CATADriver::wait_not_busy() {
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(m_io_base + ATA_STATUS);
        if (!(status & ATA_STATUS_BSY)) return true;
    }
    return false;
}

bool CATADriver::wait_ready() {
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(m_io_base + ATA_STATUS);
        if (status & ATA_STATUS_RDY) return true;
    }
    return false;
}

void CATADriver::select_drive(uint8_t slave) {
    outb(m_io_base + ATA_DRIVE_HEAD, 0xE0 | (slave << 4));
    for (int i = 0; i < 10; i++)
        inb(m_io_base + ATA_STATUS);
}

bool CATADriver::identify() {
    select_drive(m_is_slave);

    outb(m_io_base + ATA_SECTOR_COUNT, 0);
    outb(m_io_base + ATA_LBA_LOW, 0);
    outb(m_io_base + ATA_LBA_MID, 0);
    outb(m_io_base + ATA_LBA_HIGH, 0);

    outb(m_io_base + ATA_COMMAND, ATA_CMD_IDENTIFY);

    uint8_t status = inb(m_io_base + ATA_STATUS);
    if (status == 0) return false;

    while (true) {
        status = inb(m_io_base + ATA_STATUS);
        if (status & ATA_STATUS_ERR) return false;
        if (!(status & ATA_STATUS_BSY) && (status & ATA_STATUS_DRQ)) break;
    }

    uint16_t data[256];
    for (int i = 0; i < 256; i++)
        data[i] = inw(m_io_base + ATA_DATA);

    m_sectors = data[60] | (data[61] << 16);

    for (int i = 0; i < 20; i++) {
        m_serial[i] = (char)((data[10 + i] >> 8) | (data[10 + i] << 8));
        m_serial[i] = m_serial[i] == ' ' ? '_' : m_serial[i];
    }

    for (int i = 0; i < 40; i++) {
        m_model[i] = (char)((data[27 + i] >> 8) | (data[27 + i] << 8));
        m_model[i] = m_model[i] == ' ' ? '_' : m_model[i];
    }

    printf("ATA: Model: %s, Serial: %s, Sectors: %u\n", m_model, m_serial, m_sectors);
    return true;
}

bool CATADriver::read_sectors(uint32_t lba, uint8_t sectors, uint8_t* buffer) {
    if (!m_initialized) return false;
    if (!wait_not_busy()) return false;

    select_drive(m_is_slave);

    outb(m_io_base + ATA_SECTOR_COUNT, sectors);
    outb(m_io_base + ATA_LBA_LOW, lba & 0xFF);
    outb(m_io_base + ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(m_io_base + ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(m_io_base + ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(m_io_base + ATA_COMMAND, ATA_CMD_READ_SECTORS);

    for (int sector = 0; sector < sectors; sector++) {
        if (!wait_not_busy()) return false;
        if (!wait_ready()) return false;

        for (int i = 0; i < 256; i++) {
            uint16_t data = inw(m_io_base + ATA_DATA);
            buffer[i * 2] = data & 0xFF;
            buffer[i * 2 + 1] = (data >> 8) & 0xFF;
        }

        buffer += 512;
    }

    return true;
}

bool CATADriver::write_sectors(uint32_t lba, uint8_t sectors, const uint8_t* buffer) {
    if (!m_initialized) return false;
    if (!wait_not_busy()) return false;

    select_drive(m_is_slave);

    outb(m_io_base + ATA_SECTOR_COUNT, sectors);
    outb(m_io_base + ATA_LBA_LOW, lba & 0xFF);
    outb(m_io_base + ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(m_io_base + ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(m_io_base + ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(m_io_base + ATA_COMMAND, ATA_CMD_WRITE_SECTORS);

    for (int sector = 0; sector < sectors; sector++) {
        if (!wait_not_busy()) return false;
        if (!wait_ready()) return false;

        for (int i = 0; i < 256; i++) {
            uint16_t data = buffer[i * 2] | (buffer[i * 2 + 1] << 8);
            outw(m_io_base + ATA_DATA, data);
        }

        buffer += 512;
    }

    return true;
}

}  // namespace drivers