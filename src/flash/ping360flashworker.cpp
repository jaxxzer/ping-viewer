#include "ping360flashworker.h"
#include "logger.h"
#include "pic-hex.h"

#include <QByteArray>
#include <QSerialPortInfo>
#include <QThread>

PING_LOGGING_CATEGORY(PING360FLASHWORKER, "ping360.flash.worker")

Ping360FlashWorker::Ping360FlashWorker()
    : QThread()
{
    // if (!link()->isOpen()) {
    //   qCCritical(PING_PROTOCOL_SENSOR) << "Connection fail !" << conConf << link()->errorString();
    //   emit connectionClose();
    //   return;
    // }
}

void Ping360FlashWorker::run()
{
    printf("hellow world");
    _port = new QSerialPort();
    QSerialPortInfo pInfo(_link.serialPort());
    _port->setPort(pInfo);
    // _port->setBaudRate(_baudRate);
    _port->setBaudRate(115200);
    _port->open(QIODevice::ReadWrite);
    // uint8_t a[] = {99, 234, 44, 234};
    // port_write(a, 4);

    printf("\nfetch device id...\n");
    uint16_t id;
    if (bl_read_device_id(&id)) {
        printf(" > device id: 0x%04x <\n", id);
    } else {
        printf("error fetching device id\n");
        // return 1;
    }

    // printf("\nfetch device id...\n");
    // if (bl_read_device_id(&id)) {
    // printf(" > device id: 0x%04x <\n", id);
    // } else {
    // printf("error fetching device id\n");
    // // return 1;
    // }

    qCInfo(PING360FLASHWORKER) << "fetch version...";
    Ping360BootloaderPacket::packet_rsp_version_t version;
    if (bl_read_version(&version)) {
        if (version.message.version_major != expectedVersionMajor
            || version.message.version_minor != expectedVersionMinor
            || version.message.version_patch != expectedVersionPatch) {
            printf("error, bootloader version is v%d.%d.%d, expected v%d.%d.%d\n", version.message.version_major,
                version.message.version_minor, version.message.version_patch, expectedVersionMajor,
                expectedVersionMinor, expectedVersionPatch);
            // return 1;
        }

        printf(" > device type 0x%02x : hardware revision %c : bootloader v%d.%d.%d <\n", version.message.device_type,
            version.message.device_revision, version.message.version_major, version.message.version_minor,
            version.message.version_patch);

    } else {
        printf("error fetching version\n");
        // return 1;
    }

    printf("\nloading application from %s...", _firmwareFilePath);

    PicHex hex = PicHex(_firmwareFilePath.toLocal8Bit().data());

    uint8_t zeros[Ping360BootloaderPacket::PACKET_ROW_LENGTH];
    memset(zeros, 0xff, sizeof(zeros));
    const uint32_t bootAddress = 0x1000;
    printf("\nwipe boot address 0x%08x...", bootAddress);
    if (bl_write_program_memory(zeros, bootAddress)) {
        printf("ok\n");
    } else {
        printf("error\n");
        // return 1;
    }

    // printf("\nwriting application...\n");
    qCInfo(PING360FLASHWORKER) << "writing application...";
    for (int i = 0; i < 86; i++) {
        if (i >= 1 && i <= 3) {
            continue; // protected boot code
        }
        if (i == 4) {
            continue; // we write this page last, to prevent booting after failed programming
        }

        // printf("write 0x%08x: ", i * 0x400);
        if (bl_write_program_memory(
                hex.pic_hex_application_data + i * Ping360BootloaderPacket::PACKET_ROW_LENGTH, i * 0x400)) {
            qCInfo(PING360FLASHWORKER) << QString("write 0x%1: ok").arg(i * 0x400, 8, 16, QChar('0'));

        } else {
            qCCritical(PING360FLASHWORKER) << QString("write 0x%1: error").arg(i * 0x400, 8, 16, QChar('0'));

            // return 1;
        }
        emit flashProgressChanged(50 * i / 86.0f);
    }

    if (bl_write_program_memory(
            hex.pic_hex_application_data + 4 * Ping360BootloaderPacket::PACKET_ROW_LENGTH, bootAddress)) {
        qCInfo(PING360FLASHWORKER) << QString("write boot address 0x%1...").arg(bootAddress, 8, 16, QChar('0')) << "ok";
    } else {
        qCCritical(PING360FLASHWORKER) << QString("write boot address 0x%1...").arg(bootAddress, 8, 16, QChar('0'))
                                       << "error";
        // return 1;
    }

    printf("\nverifying application...\n");
    uint8_t* verify;
    for (int i = 0; i < 86; i++) {
        if (i >= 1 && i <= 3) {
            continue; // protected boot code
        }
        uint32_t offset = i * 0x400;
        bool verify_ok = true;
        printf("verify 0x%08x: ", offset);

        if (bl_read_program_memory(&verify, offset)) {
            for (int j = 0; j < Ping360BootloaderPacket::PACKET_ROW_LENGTH; j++) {
                if (verify[j] != hex.pic_hex_application_data[i * Ping360BootloaderPacket::PACKET_ROW_LENGTH + j]) {
                    printf("X\nerror: program data differs at 0x%08x: 0x%02x != 0x%02x\n",
                        i * Ping360BootloaderPacket::PACKET_ROW_LENGTH + j, verify[j],
                        hex.pic_hex_application_data[i * Ping360BootloaderPacket::PACKET_ROW_LENGTH + j]);
                    // return 1;
                }
            }
            printf("ok\n");
        } else {
            printf("error\n");
            // return 1;
        }
        emit flashProgressChanged(50 + 50 * i / 86.0f);
    }

    if (bl_write_configuration_memory(hex.pic_hex_configuration_data)) {
        qCInfo(PING360FLASHWORKER) << "writing configuration...ok";
    } else {
        qCCritical(PING360FLASHWORKER) << "writing configuration...error";
        // return 1;
    }

    if (bl_reset()) {
        qCInfo(PING360FLASHWORKER) << "starting application...ok";
    } else {
        qCCritical(PING360FLASHWORKER) << "starting application...error";
        // return 1;
    }
}

#define BL_TIMEOUT_DEFAULT_US 750000
#define BL_TIMEOUT_WRITE_US 500000
#define BL_TIMEOUT_READ_US 5000000

void Ping360FlashWorker::bl_write_packet(const packet_t packet)
{
    // for (int i = 0; i < bl_parser.packet_get_length(packet); i++) {
    //     qCCritical(PING360FLASHWORKER) << i << packet[i];
    // }
    port_write(packet, bl_parser.packet_get_length(packet));
}

Ping360BootloaderPacket::packet_t Ping360FlashWorker::bl_wait_packet(uint8_t id, uint32_t timeout_us)
{
    bl_parser.reset();

    uint64_t tstop = time_us() + timeout_us;
    // qCCritical(PING360FLASHWORKER) << "waiting for packet" << time_us() << tstop;

    uint8_t b;
    while (time_us() < tstop) {
        // QThread::msleep(10);
        _port->waitForReadyRead(10);
        for (int i = 0; i < _port->bytesAvailable(); i++) {
            if (port_read(&b, 1) > 0) {
                Ping360BootloaderPacket::packet_parse_state_e parseResult = bl_parser.packet_parse_byte(b);
                // qCInfo(PING360FLASHWORKER) << parseResult;
                if (parseResult == Ping360BootloaderPacket::NEW_MESSAGE) {
                    if (Ping360BootloaderPacket::packet_get_id(bl_parser.parser.rxBuffer) == id) {
                        return bl_parser.parser.rxBuffer;
                    } else {
                        printf("bootloader error: got unexpected id 0x%02x while waiting for 0x%02x",
                            Ping360BootloaderPacket::packet_get_id(bl_parser.parser.rxBuffer), id);
                        return NULL;
                    }
                } else if (parseResult == Ping360BootloaderPacket::ERROR) {
                    printf("bootloader error: parse error while waiting for 0x%02x!\n", id);
                    return NULL;
                }
            }
        }
    }
    printf("bootloader error: timed out waiting for 0x%02x!\n", id);
    return NULL;
}

// false on nack or error
bool Ping360FlashWorker::bl_read_device_id(uint16_t* device_id)
{
    Ping360BootloaderPacket::packet_cmd_read_dev_id_t readDevId = Ping360BootloaderPacket::packet_cmd_read_dev_id_init;
    Ping360BootloaderPacket::packet_update_footer(readDevId.data);
    bl_write_packet(readDevId.data);
    Ping360BootloaderPacket::packet_t ret = bl_wait_packet(Ping360BootloaderPacket::RSP_DEV_ID, BL_TIMEOUT_DEFAULT_US);
    if (ret) {
        Ping360BootloaderPacket::packet_rsp_dev_id_t* resp = (Ping360BootloaderPacket::packet_rsp_dev_id_t*)ret;
        *device_id = resp->message.deviceId;
        return true;
    }
    return false;
}

bool Ping360FlashWorker::bl_read_version(Ping360BootloaderPacket::packet_rsp_version_t* version)
{
    Ping360BootloaderPacket::packet_cmd_read_version_t readVersion
        = Ping360BootloaderPacket::packet_cmd_read_version_init;
    Ping360BootloaderPacket::packet_update_footer(readVersion.data);
    bl_write_packet(readVersion.data);
    Ping360BootloaderPacket::packet_t ret = bl_wait_packet(Ping360BootloaderPacket::RSP_VERSION, BL_TIMEOUT_DEFAULT_US);
    if (ret) {
        *version = *(Ping360BootloaderPacket::packet_rsp_version_t*)ret;
        return true;
    }
    return false;
}

// false on nack or error
bool Ping360FlashWorker::bl_read_program_memory(uint8_t** data, uint32_t address)
{
    Ping360BootloaderPacket::packet_cmd_read_pgm_mem_t pkt = Ping360BootloaderPacket::packet_cmd_read_pgm_mem_init;
    pkt.message.address = address;
    Ping360BootloaderPacket::packet_update_footer(pkt.data);
    bl_write_packet(pkt.data);
    Ping360BootloaderPacket::packet_t ret = bl_wait_packet(Ping360BootloaderPacket::RSP_PGM_MEM, BL_TIMEOUT_READ_US);
    if (ret) {
        Ping360BootloaderPacket::packet_rsp_pgm_mem_t* resp = (Ping360BootloaderPacket::packet_rsp_pgm_mem_t*)ret;
        uint8_t* rowData = resp->message.rowData;

        // rotate data to fix endianness (read is opposite endianness of write)
        for (uint16_t i = 0; i < 512; i++) {
            uint16_t idx = i * 3;
            uint8_t tmp = rowData[idx];
            rowData[idx] = rowData[idx + 2];
            rowData[idx + 2] = tmp;
        }

        *data = rowData;

        return true;
    }
    return false;
}

// false on nack or error
bool Ping360FlashWorker::bl_write_program_memory(const uint8_t* data, uint32_t address)
{
    Ping360BootloaderPacket::packet_cmd_write_pgm_mem_t pkt = Ping360BootloaderPacket::packet_cmd_write_pgm_mem_init;
    memcpy(pkt.message.rowData, data, Ping360BootloaderPacket::PACKET_ROW_LENGTH);
    pkt.message.address = address;
    Ping360BootloaderPacket::packet_update_footer(pkt.data);
    bl_write_packet(pkt.data);
    return bl_wait_packet(Ping360BootloaderPacket::RSP_ACK, BL_TIMEOUT_WRITE_US);
}

// false on nack or error
bool Ping360FlashWorker::bl_write_configuration_memory(const uint8_t* data)
{
    Ping360BootloaderPacket::packet_cmd_write_cfg_mem_t pkt = Ping360BootloaderPacket::packet_cmd_write_cfg_mem_init;
    memcpy(pkt.message.cfgData, data, 24);
    Ping360BootloaderPacket::packet_update_footer(pkt.data);
    bl_write_packet(pkt.data);
    return bl_wait_packet(Ping360BootloaderPacket::RSP_ACK, BL_TIMEOUT_DEFAULT_US);
}

bool Ping360FlashWorker::bl_reset()
{
    Ping360BootloaderPacket::packet_cmd_reset_processor_t pkt
        = Ping360BootloaderPacket::packet_cmd_reset_processor_init;
    Ping360BootloaderPacket::packet_update_footer(pkt.data);
    bl_write_packet(pkt.data);
    return bl_wait_packet(Ping360BootloaderPacket::RSP_ACK, BL_TIMEOUT_DEFAULT_US);
}

int Ping360FlashWorker::port_write(const uint8_t* buffer, int nBytes)
{
    // _link.self()->sendData(QByteArray(reinterpret_cast<const char*>(buffer), nBytes));
    int bytes = _port->write(reinterpret_cast<const char*>(buffer), nBytes);
    _port->flush();
    // if (!_port->waitForBytesWritten(1000)) {
    //   qCCritical(PING360FLASHWORKER) << "write timeout";
    // }
    // qCCritical(PING360FLASHWORKER) << "wrote" << bytes;
    return bytes;
}

int Ping360FlashWorker::port_read(uint8_t* data, int nBytes)
{
    int bytes = _port->read(reinterpret_cast<char*>(data), nBytes);
    // if (bytes > 0) qCCritical(PING360FLASHWORKER) << "read" << bytes << "/" << nBytes;
    return bytes;
}