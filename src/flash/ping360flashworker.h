#pragma once

#include "flasher.h"
#include "link.h"
#include "ping360bootloaderpacket.h"

#include <QDateTime>
#include <QLoggingCategory>
#include <QSerialPort>
#include <QThread>

Q_DECLARE_LOGGING_CATEGORY(PING360FLASHWORKER)

/**
 * @brief Flasher implementation for Ping360 sensor device
 *
 */
class Ping360FlashWorker : public QThread {
    Q_OBJECT
public:
    /**
     * @brief Construct a new Ping360FlashWorker object
     *
     * @param parent
     */
    Ping360FlashWorker();

    /**
     * @brief Destroy the Ping360FlashWorker object
     *
     */
    ~Ping360FlashWorker() = default;

    void setFirmwarePath(QString firmwareFilePath) { _firmwareFilePath = firmwareFilePath; }
    void setLink(LinkConfiguration link) { _link = link; }
    void setVerify(bool verify) { _verify = verify; }

    void run() override final;

signals:
    void messageChanged(QString message);
    void stateChanged(Flasher::States state);
    void flashProgressChanged(float flashPercent);

private:
    QString _firmwareFilePath;
    LinkConfiguration _link;
    bool _verify = true;

    QSerialPort* _port;
    static const int _baudRate = 115200;

    typedef Ping360BootloaderPacket::packet_t packet_t;
    void bl_write_packet(const packet_t packet);
    packet_t bl_wait_packet(uint8_t id, uint32_t timeout_us);

    bool bl_read_device_id(uint16_t* device_id);
    bool bl_read_version(Ping360BootloaderPacket::packet_rsp_version_t* version);
    bool bl_read_program_memory(uint8_t** data, uint32_t address);

    bool bl_write_program_memory(const uint8_t* data, uint32_t address);
    bool bl_write_configuration_memory(const uint8_t* data);

    bool bl_reset();
    void error(QString message);
    const uint16_t expectedDeviceId = 0x062f;
    const uint8_t expectedVersionMajor = 2;
    const uint8_t expectedVersionMinor = 1;
    const uint8_t expectedVersionPatch = 2;

    Ping360BootloaderPacket bl_parser;

    qint64 time_us() { return QDateTime::currentMSecsSinceEpoch() * 1000; };
    int port_write(const uint8_t* buffer, int nBytes);
    int port_read(uint8_t* data, int nBytes);
    // AbstractLink* link() const { return _link.data() ? _link->self() : nullptr; };
};
