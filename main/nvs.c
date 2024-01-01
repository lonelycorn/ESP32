/*
  nvs.c - An embedded CNC Controller with rs274/ngc (g-code) support

  Peristent storage of settings in flash

  Part of grblHAL

  Copyright (c) 2018-2023 Terje Io

  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "esp_partition.h"
#include "esp_log.h"

#include "spi_flash_mmap.h" // SPI_FLASH_SEC_SIZE

#include "nvs.h"
#include "grbl/hal.h"

#if !NVSDATA_BUFFER_ENABLE
#error NVSDATA_BUFFER_ENABLE must be enabled to use flash for settings storage
#endif

static const DRAM_ATTR char ESP_SPACE_CHAR = ' ';
static const DRAM_ATTR char ESP_DEL_CHAR = 0x7F;
static const DRAM_ATTR char ESP_CR = ASCII_CR;
static const DRAM_ATTR char ESP_LF = ASCII_LF;
static const DRAM_ATTR char ESP_QUESTION_MARK = '?';
static const esp_partition_t *grblNVS = NULL;


// Strip top bit set characters, control characters except CR and LF and question mark
static IRAM_ATTR bool nvs_enqueue_realtime_command (char c)
{
    return (c < ESP_SPACE_CHAR && !(c == ESP_CR || c == ESP_LF)) || c == ESP_QUESTION_MARK || c >= ESP_DEL_CHAR;
}

bool nvsRead (uint8_t *dest)
{
    bool ok;

    if(!(ok = grblNVS && esp_partition_read(grblNVS, 0, (void *)dest, hal.nvs.size) == ESP_OK))
        grblNVS = NULL;

    return ok;
}

bool nvsWrite (uint8_t *source)
{
    enqueue_realtime_command_ptr realtime_command_handler;

    // Save and redirect real time command handler here to avoid panic in uart isr
    // due to constants in standard handler residing in flash.
    realtime_command_handler = hal.stream.set_enqueue_rt_handler(nvs_enqueue_realtime_command);

    bool ok = grblNVS &&
               esp_partition_erase_range(grblNVS, 0, SPI_FLASH_SEC_SIZE) == ESP_OK &&
                esp_partition_write(grblNVS, 0, (void *)source, hal.nvs.size) == ESP_OK;

    // Restore real time command handler
    hal.stream.set_enqueue_rt_handler(realtime_command_handler);

    return ok;
}

bool nvsInit (void)
{
    grblNVS = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "grbl");

    return grblNVS != NULL;
}
