#include "packet.h"

#include <inttypes.h>
#include <stdbool.h>

const packet_cmd_read_dev_id_t packet_cmd_read_dev_id_init = {
    .message.header.start = PACKET_FRAMING_START,
    .message.header.id    = CMD_READ_DEV_ID,
    .message.footer.end   = PACKET_FRAMING_END,
};

const packet_cmd_reset_processor_t packet_cmd_reset_processor_init = {
    .message.header.start = PACKET_FRAMING_START,
    .message.header.id    = CMD_RESET_PROCESSOR,
    .message.footer.end   = PACKET_FRAMING_END,
};

const packet_cmd_jump_start_t packet_cmd_jump_start_init = {
    .message.header.start = PACKET_FRAMING_START,
    .message.header.id    = CMD_JUMP_START,
    .message.footer.end   = PACKET_FRAMING_END,
};

const packet_cmd_restart_bootloader_t packet_cmd_restart_bootloader_init = {
    .message.header.start = PACKET_FRAMING_START,
    .message.header.id    = CMD_RESTART_BOOTLOADER,
    .message.footer.end   = PACKET_FRAMING_END,
};

const packet_cmd_read_version_t packet_cmd_read_version_init = {
    .message.header.start = PACKET_FRAMING_START,
    .message.header.id    = CMD_READ_VERSION,
    .message.footer.end   = PACKET_FRAMING_END,
};

const packet_cmd_read_pgm_mem_t packet_cmd_read_pgm_mem_init = {
    .message.header.start  = PACKET_FRAMING_START,
    .message.header.id     = CMD_READ_PGM_MEM,
    .message.header.length = 4, // length
    .message.footer.end    = PACKET_FRAMING_END,
};

const packet_cmd_write_pgm_mem_t packet_cmd_write_pgm_mem_init = {
    .message.header.start  = PACKET_FRAMING_START,
    .message.header.id     = CMD_WRITE_PGM_MEM,
    .message.header.length = 1540, // length
    .message.footer.end    = PACKET_FRAMING_END,
};

const packet_cmd_write_cfg_mem_t packet_cmd_write_cfg_mem_init = {
    .message.header.start  = PACKET_FRAMING_START,
    .message.header.id     = CMD_WRITE_CFG_MEM,
    .message.header.length = 24, // length
    .message.footer.end    = PACKET_FRAMING_END,
};

uint16_t packet_get_payload_length(packet_t packet) { return *(uint16_t *)(packet + 2); }

uint16_t packet_get_length(packet_t packet) { return packet_get_payload_length(packet) + 7; }

uint8_t packet_calculate_checksum(packet_t packet) {
  uint8_t checksum = packet[1] ^ packet[2];
  checksum         = checksum ^ packet[3];
  for (int i = 0; i < packet_get_payload_length(packet); i++) {
    checksum ^= packet[4 + i];
  }
  return checksum;
}

packet_id_e packet_get_id(packet_t packet) { return packet[1]; }

uint8_t packet_calculate_complement(packet_t packet) { return ~packet_get_id(packet) + 1; }

void packet_update_footer(packet_t packet) {
  uint8_t *checksum_p = packet + 4 + packet_get_payload_length(packet);
  *checksum_p         = packet_calculate_checksum(packet);
  *(checksum_p + 1)   = packet_calculate_complement(packet);
}

packet_parse_state_e packet_parse_byte(struct packet_parser *parser, const uint8_t byte) {
  switch (parser->parseState) {
  case ERROR:
  case NEW_MESSAGE:
  case WAIT_START:
    parser->packetLength = 0;
    parser->rxTail       = 0;
    if (byte != PACKET_FRAMING_START) {
      parser->parseState = ERROR;
    } else {
      parser->rxBuffer[parser->rxTail++] = byte;
      parser->parseState                 = WAIT_ID;
    }
    break;
  case WAIT_ID:
    if (byte < ID_LOW || byte >= ID_HIGH) {
      parser->parseState = ERROR;
    } else {
      parser->rxBuffer[parser->rxTail++] = byte;
      parser->parseState                 = WAIT_LENGTH_L;
    }
    break;
  case WAIT_LENGTH_L:
    parser->rxBuffer[parser->rxTail++] = byte;
    parser->parseState                 = WAIT_LENGTH_H;
    break;
  case WAIT_LENGTH_H:
    parser->rxBuffer[parser->rxTail++] = byte;
    parser->packetLength               = packet_get_payload_length(parser->rxBuffer);
    if (parser->packetLength > PACKET_MAX_LENGTH) {
      parser->parseState = ERROR;
    } else if (parser->packetLength == 0) {
      parser->parseState = WAIT_CHECKSUM;
    } else {
      parser->parseState = WAIT_DATA;
    }
    break;
  case WAIT_DATA:
    parser->rxBuffer[parser->rxTail++] = byte;
    parser->packetLength--;
    if (!parser->packetLength) {
      parser->parseState = WAIT_CHECKSUM;
    }
    break;
  case WAIT_CHECKSUM:
    if (byte != packet_calculate_checksum(parser->rxBuffer)) {
      parser->parseState = ERROR;
    } else {
      parser->rxBuffer[parser->rxTail++] = byte;
      parser->parseState                 = WAIT_COMPLEMENT;
    }
    break;
  case WAIT_COMPLEMENT:
    if (byte != packet_calculate_complement(parser->rxBuffer)) {
      parser->parseState = ERROR;
    } else {
      parser->rxBuffer[parser->rxTail++] = byte;
      parser->parseState                 = WAIT_END;
    }
    break;
  case WAIT_END:
    if (byte != PACKET_FRAMING_END) {
      parser->parseState = ERROR;
    } else {
      parser->rxBuffer[parser->rxTail++] = byte;
      parser->parseState                 = NEW_MESSAGE;
    }
    break;
  default:
    parser->parseState = ERROR;
    break;
  }
  return parser->parseState;
}
