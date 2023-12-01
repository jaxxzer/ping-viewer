#pragma once

#include "flasher.h"
#include "link.h"
#include "packet.h"

#include <QDateTime>
#include <QLoggingCategory>
#include <QSerialPort>

Q_DECLARE_LOGGING_CATEGORY(PING360FLASH)

/**
 * @brief Flasher implementation for Ping360 sensor device
 *
 */
class Ping360Flasher : public Flasher {
    Q_OBJECT
public:
    /**
     * @brief Construct a new Ping360Flasher object
     *
     * @param parent
     */
    Ping360Flasher();

    /**
     * @brief Destroy the Ping360Flasher object
     *
     */
    ~Ping360Flasher() = default;

    /**
     * @brief Start the flash procedure
     *
     */
    void flash() override;

private:
    QSerialPort _port;
    int _baudRate = 115200;
    const QList<int> _validBaudRates = {115200};

    typedef Ping360BootloaderPacket::packet_t packet_t;
    void bl_write_packet(const packet_t packet);
    packet_t bl_wait_packet(uint8_t id, uint32_t timeout_us);

    bool bl_read_device_id(uint16_t *device_id);
    bool bl_read_version(Ping360BootloaderPacket::packet_rsp_version_t *version);
    bool bl_read_program_memory(uint8_t **data, uint32_t address);

    bool bl_write_program_memory(const uint8_t *data, uint32_t address);
    bool bl_write_configuration_memory(const uint8_t *data);

    bool bl_reset();

    const uint16_t expectedDeviceId    = 0x062f;
    const uint8_t expectedVersionMajor = 2;
    const uint8_t expectedVersionMinor = 1;
    const uint8_t expectedVersionPatch = 2;

    Ping360BootloaderPacket bl_parser;

    qint64 time_us() { return QDateTime::currentMSecsSinceEpoch() * 1000; };
    int port_write(const uint8_t *buffer, int nBytes);
    int port_read(uint8_t* data, int nBytes);
    // AbstractLink* link() const { return _link.data() ? _link->self() : nullptr; };

};
