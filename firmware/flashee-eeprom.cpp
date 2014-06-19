/**
 * Copyright 2014  Matthew McGowan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef SPARK 
#include "application.h"
#else
#include <stdint.h>
#endif
#include "flashee-eeprom.h"

namespace Flashee {

FlashDevice::~FlashDevice() { }

#ifdef SPARK
    static SparkExternalFlashDevice directFlash;
#else
    static FakeFlashDevice directFlash(384, 4096);
#endif

FlashDeviceRegion flashee::userRegion(directFlash, 0x80000, 0x200000);


/**
 * Compares data in the buffer with the data in flash.
 * @param data
 * @param address
 * @param length
 * @return
 */
#if 0
bool FlashDevice::comparePage(const void* data, flash_addr_t address, page_size_t length) {
    uint8_t buf[STACK_BUFFER_SIZE];
    page_size_t offset = 0;
    while (offset<length) {
        page_size_t toRead = min(sizeof(buf), length-offset);
        if (!readPage(buf, address+offset, toRead))
            break;
        if (!memcmp(buf, as_bytes(data)+offset, toRead))
            break;
        offset += toRead;
    }
    return offset==length;
}
#endif

}