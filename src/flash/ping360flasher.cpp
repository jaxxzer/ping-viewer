#include "ping360flasher.h"

#include <QByteArray>
#include <QSerialPortInfo>

Ping360Flasher::Ping360Flasher()
    : Flasher()
{


  // if (!link()->isOpen()) {
  //   qCCritical(PING_PROTOCOL_SENSOR) << "Connection fail !" << conConf << link()->errorString();
  //   emit connectionClose();
  //   return;
  // }
}

void Ping360Flasher::flash()
{
    QSerialPortInfo pInfo(_link.serialPort());
    _port.setPort(pInfo);
    _port.setBaudRate(_baudRate);
    _port.open(QIODevice::ReadWrite);


    printf("\nfetch device id...\n");
    uint16_t id;
    if (bl_read_device_id(&id)) {
    printf(" > device id: 0x%04x <\n", id);
    } else {
    printf("error fetching device id\n");
    // return 1;
    }


    printf("\nfetch version...\n");
    Ping360BootloaderPacket::packet_rsp_version_t version;
    if (bl_read_version(&version)) {
        if (version.message.version_major != expectedVersionMajor ||
            version.message.version_minor != expectedVersionMinor ||
            version.message.version_patch != expectedVersionPatch) {
        printf("error, bootloader version is v%d.%d.%d, expected v%d.%d.%d\n", version.message.version_major,
                version.message.version_minor, version.message.version_patch, expectedVersionMajor, expectedVersionMinor,
                expectedVersionPatch);
        // return 1;
        }

        printf(" > device type 0x%02x : hardware revision %c : bootloader v%d.%d.%d <\n", version.message.device_type,
            version.message.device_revision, version.message.version_major, version.message.version_minor,
            version.message.version_patch);

    } else {
        printf("error fetching version\n");
        // return 1;
    }

}


#define BL_TIMEOUT_DEFAULT_US 75000
#define BL_TIMEOUT_WRITE_US 500000
#define BL_TIMEOUT_READ_US 500000


void Ping360Flasher::bl_write_packet(const packet_t packet)
{
    port_write(packet, bl_parser.packet_get_length(packet));
}

Ping360BootloaderPacket::packet_t Ping360Flasher::bl_wait_packet(uint8_t id, uint32_t timeout_us) {
  bl_parser.reset();

  uint32_t tstop = time_us() + timeout_us;

  while (time_us() < tstop) {
    uint8_t b;
    if (port_read(&b, 1) > 0) {
      Ping360BootloaderPacket::packet_parse_state_e parseResult = bl_parser.packet_parse_byte(b);
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
  printf("bootloader error: timed out waiting for 0x%02x!\n", id);
  return NULL;
}

// false on nack or error
bool Ping360Flasher::bl_read_device_id(uint16_t *device_id) {
  Ping360BootloaderPacket::packet_cmd_read_dev_id_t readDevId = Ping360BootloaderPacket::packet_cmd_read_dev_id_init;
  Ping360BootloaderPacket::packet_update_footer(readDevId.data);
  bl_write_packet(readDevId.data);
  Ping360BootloaderPacket::packet_t ret = bl_wait_packet(Ping360BootloaderPacket::RSP_DEV_ID, BL_TIMEOUT_DEFAULT_US);
  if (ret) {
    Ping360BootloaderPacket::packet_rsp_dev_id_t *resp = (Ping360BootloaderPacket::packet_rsp_dev_id_t *)ret;
    *device_id                = resp->message.deviceId;
    return true;
  }
  return false;
}

bool Ping360Flasher::bl_read_version(Ping360BootloaderPacket::packet_rsp_version_t *version) {
  Ping360BootloaderPacket::packet_cmd_read_version_t readVersion = Ping360BootloaderPacket::packet_cmd_read_version_init;
  Ping360BootloaderPacket::packet_update_footer(readVersion.data);
  bl_write_packet(readVersion.data);
  Ping360BootloaderPacket::packet_t ret = bl_wait_packet(Ping360BootloaderPacket::RSP_VERSION, BL_TIMEOUT_DEFAULT_US);
  if (ret) {
    *version = *(Ping360BootloaderPacket::packet_rsp_version_t *)ret;
    return true;
  }
  return false;
}

// false on nack or error
bool Ping360Flasher::bl_read_program_memory(uint8_t **data, uint32_t address) {
  Ping360BootloaderPacket::packet_cmd_read_pgm_mem_t pkt = Ping360BootloaderPacket::packet_cmd_read_pgm_mem_init;
  pkt.message.address           = address;
  Ping360BootloaderPacket::packet_update_footer(pkt.data);
  bl_write_packet(pkt.data);
  Ping360BootloaderPacket::packet_t ret = bl_wait_packet(Ping360BootloaderPacket::RSP_PGM_MEM, BL_TIMEOUT_READ_US);
  if (ret) {
    Ping360BootloaderPacket::packet_rsp_pgm_mem_t *resp = (Ping360BootloaderPacket::packet_rsp_pgm_mem_t *)ret;
    uint8_t *rowData           = resp->message.rowData;

    // rotate data to fix endianness (read is opposite endianness of write)
    for (uint16_t i = 0; i < 512; i++) {
      uint16_t idx     = i * 3;
      uint8_t tmp      = rowData[idx];
      rowData[idx]     = rowData[idx + 2];
      rowData[idx + 2] = tmp;
    }

    *data = rowData;

    return true;
  }
  return false;
}

// false on nack or error
bool Ping360Flasher::bl_write_program_memory(const uint8_t *data, uint32_t address) {
  Ping360BootloaderPacket::packet_cmd_write_pgm_mem_t pkt = Ping360BootloaderPacket::packet_cmd_write_pgm_mem_init;
  memcpy(pkt.message.rowData, data, Ping360BootloaderPacket::PACKET_ROW_LENGTH);
  pkt.message.address = address;
  Ping360BootloaderPacket::packet_update_footer(pkt.data);
  bl_write_packet(pkt.data);
  return bl_wait_packet(Ping360BootloaderPacket::RSP_ACK, BL_TIMEOUT_WRITE_US);
}

// false on nack or error
bool Ping360Flasher::bl_write_configuration_memory(const uint8_t *data) {
  Ping360BootloaderPacket::packet_cmd_write_cfg_mem_t pkt = Ping360BootloaderPacket::packet_cmd_write_cfg_mem_init;
  memcpy(pkt.message.cfgData, data, 24);
  Ping360BootloaderPacket::packet_update_footer(pkt.data);
  bl_write_packet(pkt.data);
  return bl_wait_packet(Ping360BootloaderPacket::RSP_ACK, BL_TIMEOUT_DEFAULT_US);
}

bool Ping360Flasher::bl_reset() {
  Ping360BootloaderPacket::packet_cmd_reset_processor_t pkt = Ping360BootloaderPacket::packet_cmd_reset_processor_init;
  Ping360BootloaderPacket::packet_update_footer(pkt.data);
  bl_write_packet(pkt.data);
  return bl_wait_packet(Ping360BootloaderPacket::RSP_ACK, BL_TIMEOUT_DEFAULT_US);
}

int Ping360Flasher::port_write(const uint8_t* buffer, int nBytes) {
  // _link.self()->sendData(QByteArray(reinterpret_cast<const char*>(buffer), nBytes));
  int bytes = _port.write(reinterpret_cast<const char*>(buffer), nBytes);
  _port.flush();
  return bytes;
}

int Ping360Flasher::port_read(uint8_t* data, int nBytes) {
  return _port.read(reinterpret_cast<char*>(data), nBytes);
}