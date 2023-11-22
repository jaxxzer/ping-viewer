#pragma once

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#define PACKET_MAX_LENGTH 1600
#define PACKET_ROW_LENGTH 1536

typedef enum {
  PACKET_FRAMING_START = 0x5a,
  PACKET_FRAMING_END   = 0xa5,
} packet_framing_e;

typedef enum {
  ID_LOW      = 0x30,
  RSP_NACK    = 0x30,
  RSP_ACK     = 0x31,
  RSP_PGM_MEM = 0x32,
  RSP_DEV_ID  = 0x33,
  RSP_VERSION = 0x34,

  CMD_READ_DEV_ID        = 0x41,
  CMD_READ_PGM_MEM       = 0x42,
  CMD_WRITE_PGM_MEM      = 0x43,
  CMD_WRITE_CFG_MEM      = 0x44,
  CMD_RESET_PROCESSOR    = 0x45,
  CMD_JUMP_START         = 0x46,
  CMD_RESTART_BOOTLOADER = 0x47,
  CMD_READ_VERSION       = 0x48,
  ID_HIGH
} packet_id_e;

typedef uint8_t *packet_t;

typedef struct {
  uint8_t start;
  uint8_t id;
  uint16_t length;
} __attribute__((packed)) packet_header_t;

typedef struct {
  uint8_t checksum;
  uint8_t complement;
  uint8_t end;
} __attribute__((packed)) packet_footer_t;

typedef union {
  struct {
    packet_header_t header;
    packet_footer_t footer;
  } __attribute__((packed)) message;
  uint8_t data[7];
} packet_empty_t;

// empty packets (no payload)
typedef packet_empty_t packet_cmd_read_dev_id_t;
typedef packet_empty_t packet_cmd_reset_processor_t;
typedef packet_empty_t packet_cmd_jump_start_t;
typedef packet_empty_t packet_cmd_restart_bootloader_t;
typedef packet_empty_t packet_cmd_read_version_t;
typedef packet_empty_t packet_rsp_nack_t;
typedef packet_empty_t packet_rsp_ack_t;

const packet_cmd_read_dev_id_t packet_cmd_read_dev_id_init;
const packet_cmd_reset_processor_t packet_cmd_reset_processor_init;
const packet_cmd_jump_start_t packet_cmd_jump_start_init;
const packet_cmd_restart_bootloader_t packet_cmd_restart_bootloader_init;
const packet_cmd_read_version_t packet_cmd_read_version_init;

typedef union {
  struct {
    packet_header_t header;
    uint32_t address;
    packet_footer_t footer;
  } __attribute__((packed)) message;
  uint8_t data[4 + 7];
} packet_cmd_read_pgm_mem_t;

const packet_cmd_read_pgm_mem_t packet_cmd_read_pgm_mem_init;

typedef union {
  struct {
    packet_header_t header;
    uint32_t address;
    uint8_t rowData[PACKET_ROW_LENGTH];
    packet_footer_t footer;
  } __attribute__((packed)) message;
  uint8_t data[4 + PACKET_ROW_LENGTH + 7];
} packet_cmd_write_pgm_mem_t;

const packet_cmd_write_pgm_mem_t packet_cmd_write_pgm_mem_init;

typedef union {
  struct {
    packet_header_t header;
    uint8_t cfgData[24];
    packet_footer_t footer;
  } __attribute__((packed)) message;
  uint8_t data[24 + 7];
} packet_cmd_write_cfg_mem_t;

const packet_cmd_write_cfg_mem_t packet_cmd_write_cfg_mem_init;

typedef union {
  struct {
    packet_header_t header;
    uint8_t rowData[PACKET_ROW_LENGTH];
    packet_footer_t footer;
  } __attribute__((packed)) message;
  uint8_t data[PACKET_ROW_LENGTH + 7];
} packet_rsp_pgm_mem_t;

typedef union {
  struct {
    packet_header_t header;
    uint16_t deviceId;
    packet_footer_t footer;
  } __attribute__((packed)) message;
  uint8_t data[2 + 7];
} packet_rsp_dev_id_t;

typedef union {
  struct {
    packet_header_t header;
    uint8_t device_type;
    uint8_t device_revision;
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t version_patch;
    packet_footer_t footer;
  } __attribute__((packed)) message;
  uint8_t data[5 + 7];
} packet_rsp_version_t;

uint16_t packet_get_payload_length(packet_t packet);
uint16_t packet_get_length(packet_t packet);

uint8_t packet_calculate_checksum(packet_t packet);
packet_id_e packet_get_id(packet_t packet);

uint8_t packet_calculate_complement(packet_t packet);

void packet_update_footer(packet_t packet);

typedef enum {
  ERROR,
  WAIT_START,
  WAIT_ID,
  WAIT_LENGTH_L,
  WAIT_LENGTH_H,
  WAIT_DATA,
  WAIT_CHECKSUM,
  WAIT_COMPLEMENT,
  WAIT_END,
  NEW_MESSAGE,
} packet_parse_state_e;

typedef struct packet_parser {
  packet_parse_state_e parseState;
  uint16_t rxTail;
  uint16_t packetLength;
  uint8_t rxBuffer[PACKET_MAX_LENGTH];
} packet_parser_t;

packet_parse_state_e packet_parse_byte(struct packet_parser *parser, const uint8_t byte);
