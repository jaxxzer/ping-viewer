#include "bootloader.h"
#include "packet.h"
#include "port.h"
#include "time.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define BL_TIMEOUT_DEFAULT_US 75000
#define BL_TIMEOUT_WRITE_US 500000
#define BL_TIMEOUT_READ_US 500000

static packet_parser_t bl_parser;

void bl_write_packet(const packet_t packet) { port_write(packet, packet_get_length(packet)); }

packet_t bl_wait_packet(uint8_t id, uint32_t timeout_us) {
  bl_parser.parseState = WAIT_START;

  uint32_t tstop = time_us() + timeout_us;

  while (time_us() < tstop) {
    uint8_t b;
    if (port_read(&b, 1) > 0) {
      packet_parse_state_e parseResult = packet_parse_byte(&bl_parser, b);
      if (parseResult == NEW_MESSAGE) {
        if (packet_get_id(bl_parser.rxBuffer) == id) {
          return bl_parser.rxBuffer;
        } else {
          printf("bootloader error: got unexpected id 0x%02x while waiting for 0x%02x",
                 packet_get_id(bl_parser.rxBuffer), id);
          return NULL;
        }
      } else if (parseResult == ERROR) {
        printf("bootloader error: parse error while waiting for 0x%02x!\n", id);
        return NULL;
      }
    }
  }
  printf("bootloader error: timed out waiting for 0x%02x!\n", id);
  return NULL;
}

// false on nack or error
bool bl_read_device_id(uint16_t *device_id) {
  packet_cmd_read_dev_id_t readDevId = packet_cmd_read_dev_id_init;
  packet_update_footer(readDevId.data);
  bl_write_packet(readDevId.data);
  packet_t ret = bl_wait_packet(RSP_DEV_ID, BL_TIMEOUT_DEFAULT_US);
  if (ret) {
    packet_rsp_dev_id_t *resp = (packet_rsp_dev_id_t *)ret;
    *device_id                = resp->message.deviceId;
    return true;
  }
  return false;
}

bool bl_read_version(packet_rsp_version_t *version) {
  packet_cmd_read_version_t readVersion = packet_cmd_read_version_init;
  packet_update_footer(readVersion.data);
  bl_write_packet(readVersion.data);
  packet_t ret = bl_wait_packet(RSP_VERSION, BL_TIMEOUT_DEFAULT_US);
  if (ret) {
    *version = *(packet_rsp_version_t *)ret;
    return true;
  }
  return false;
}

// false on nack or error
bool bl_read_program_memory(uint8_t **data, uint32_t address) {
  packet_cmd_read_pgm_mem_t pkt = packet_cmd_read_pgm_mem_init;
  pkt.message.address           = address;
  packet_update_footer(pkt.data);
  bl_write_packet(pkt.data);
  packet_t ret = bl_wait_packet(RSP_PGM_MEM, BL_TIMEOUT_READ_US);
  if (ret) {
    packet_rsp_pgm_mem_t *resp = (packet_rsp_pgm_mem_t *)ret;
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
bool bl_write_program_memory(const uint8_t *data, uint32_t address) {
  packet_cmd_write_pgm_mem_t pkt = packet_cmd_write_pgm_mem_init;
  memcpy(pkt.message.rowData, data, PACKET_ROW_LENGTH);
  pkt.message.address = address;
  packet_update_footer(pkt.data);
  bl_write_packet(pkt.data);
  return bl_wait_packet(RSP_ACK, BL_TIMEOUT_WRITE_US);
}

// false on nack or error
bool bl_write_configuration_memory(const uint8_t *data) {
  packet_cmd_write_cfg_mem_t pkt = packet_cmd_write_cfg_mem_init;
  memcpy(pkt.message.cfgData, data, 24);
  packet_update_footer(pkt.data);
  bl_write_packet(pkt.data);
  return bl_wait_packet(RSP_ACK, BL_TIMEOUT_DEFAULT_US);
}

bool bl_reset() {
  packet_cmd_reset_processor_t pkt = packet_cmd_reset_processor_init;
  packet_update_footer(pkt.data);
  bl_write_packet(pkt.data);
  return bl_wait_packet(RSP_ACK, BL_TIMEOUT_DEFAULT_US);
}
