#pragma once

#include "packet.h"

#include <inttypes.h>
#include <stdbool.h>

void bl_write_packet(const packet_t packet);
packet_t bl_wait_packet(uint8_t id, uint32_t timeout_us);

bool bl_read_device_id(uint16_t *device_id);
bool bl_read_version(packet_rsp_version_t *version);
bool bl_read_program_memory(uint8_t **data, uint32_t address);

bool bl_write_program_memory(const uint8_t *data, uint32_t address);
bool bl_write_configuration_memory(const uint8_t *data);

bool bl_reset();
