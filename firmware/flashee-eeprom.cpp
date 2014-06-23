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

#include "flashee-eeprom.h"
#include "diskio.h"
#include "FlashIO.h"
#include "ff.h"

namespace Flashee {    
    
FlashDevice::~FlashDevice() { }

#ifdef SPARK
    static SparkExternalFlashDevice directFlash;
#else
    static FakeFlashDevice directFlash(512, 4096);
#endif

FlashDeviceRegion Devices::userRegion(directFlash, 0x80000, 0x200000);


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

FRESULT Devices::createFATRegion(flash_addr_t startAddress, flash_addr_t endAddress, 
    FATFS* pfs, FormatCmd formatCmd) {
    FlashDevice* device = createMultiPageEraseImpl(startAddress, endAddress, 2);
    if (device==NULL)
        return FR_INVALID_PARAMETER;
    
    return f_setFlashDevice(device, pfs, formatCmd);
}

static FlashDevice* flash = NULL;


const page_size_t sector_size = 512;

bool is_formatted() {
    uint8_t sig[2];
    flash->read(sig, 510, 2);
    return (sig[0]==0x55 && sig[1]==0xAA);
}

FRESULT low_level_format() {
    flash->eraseAll();
    FRESULT result = f_mkfs("", 1, sector_size);
    if (result==FR_OK && !is_formatted())
        result = FR_DISK_ERR;        
    return result;
}

bool needs_low_level_format() {
    uint8_t sig[2];
    flash->read(sig, 510, 2);
    return ((sig[0]!=0x55 && sig[1]!=0xAA) && (sig[0]&sig[1])!=0xFF);   
}

FRESULT f_setFlashDevice(FlashDevice* device, FATFS* pfs, FormatCmd cmd) {
    delete flash;
    flash = device;
    if (!flash)
        return FR_OK;
    
    FRESULT result = f_mount(pfs, "", 0);
    if (result==FR_OK) {   
        bool formatRequired = cmd==Flashee::FORMAT || (cmd==Flashee::FORMAT_IF_NEEDED && !is_formatted());
        if (formatRequired) {
            result = low_level_format();
        }    
    }
    if (result==FR_OK) {
        FIL fil;
        result = f_open(&fil, "@@@@123~.tmp", FA_OPEN_EXISTING);        
        if (result==FR_NO_FILE)     // expected not to exist, if not formatted, will return FR_NO_FILESYSTEM
            result = FR_OK;
    }
    return result;
}

}   // namespace Flashee

using namespace Flashee;

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber (0..) */
)
{
    if (pdrv)
        return STA_NOINIT;
            
    // determine if boot sector is present, if not, then erase area    
    if (needs_low_level_format()) {
        low_level_format();
    }
    return 0;
}



/*-----------------------------------------------------------------------*/
/* Get Disk Status                                                       */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber (0..) */
)
{
    if (pdrv)
        return STA_NOINIT;

	return 0; //needs_low_level_format() ? STA_NOINIT : 0;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address (LBA) */
	UINT count		/* Number of sectors to read (1..128) */
)
{
	if (pdrv)
        return RES_PARERR;
        
    return flash->read(buff, sector*sector_size, count*sector_size) ?
        RES_OK : RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber (0..) */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address (LBA) */
	UINT count			/* Number of sectors to write (1..128) */
)
{
	if (pdrv)
        return RES_PARERR;
        
    return flash->write(buff, sector*sector_size, count*sector_size) ?
        RES_OK : RES_PARERR;
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
    DWORD* dw = (DWORD*)buff;
    
	switch (cmd) {
        case CTRL_SYNC: return RES_OK;
        case GET_SECTOR_COUNT:
            *dw = flash->length()/sector_size;
            return RES_OK;
        case GET_SECTOR_SIZE:
            *dw = sector_size;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *dw = flash->pageSize()/sector_size;
            return RES_OK;
    }
	return RES_PARERR;
}
#endif

extern "C" {

DWORD get_fattime() {
#ifdef SPARK_CORE
    
#else
    return 0;
#endif    
}

}


