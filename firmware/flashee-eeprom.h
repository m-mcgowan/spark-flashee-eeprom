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

#ifndef _FLASHEE_EEPROM_H_
#define _FLASHEE_EEPROM_H_

#ifdef SPARK 
#include "application.h"
#else
#include <stdint.h>
#endif
#include "flashee-eeprom.h"


#include "string.h"
#include "stdlib.h"

namespace Flashee {

typedef uint32_t flash_addr_t;
typedef uint32_t page_size_t;
typedef uint32_t page_count_t;

#ifndef min
// allow standalone use where Arduino/Spark min macro may not be available.

template <class T> T min(T a, T b) {
    return a < b ? a : b;
}
#endif

inline uint8_t* as_bytes(void* buf) {
    return (uint8_t*) buf;
}

inline const uint8_t* as_bytes(const void* buf) {
    return (const uint8_t*) buf;
}

/**
 * This must be a multiple of 8.
 */
const uint16_t STACK_BUFFER_SIZE = 128u;

/**
 * Function that performs the data transformation when relocating a page in flash.
 */
typedef void (*TransferHandler)(page_size_t pageOffset, void* data, uint8_t* buf, page_size_t bufSize);

/**
 * Provides an interface to a flash device.
 * Reads/Writes can span multiple pages, and be any arbitrary length or offset.
 */
class FlashDevice {
public:
    virtual ~FlashDevice();

    /**
     * @return The size of each page in this flash device.
     */
    virtual page_size_t pageSize() const = 0;

    /**
     * @return The number of pages in this flash device.
     */
    virtual page_count_t pageCount() const = 0;

    flash_addr_t length() const {
        return pageAddress(pageCount());
    }

    inline bool write(const void* data, flash_addr_t address, page_size_t length) {
        return writeErasePage(data, address, length);
    }

    inline bool read(void* data, flash_addr_t address, page_size_t length) {
        return readPage(data, address, length);
    }

    inline bool writeString(const char* s, flash_addr_t address) {
        return write(s, address, strlen(s));
    }

    /**
     * Converts a page index [0,N) into the corresponding read/write address.
     * @param page  The page to convert to an address.
     * @return
     */
    flash_addr_t pageAddress(page_count_t page) const {
        return flash_addr_t(page) * pageSize();
    }

    /**
     * Determines if the given address represents the start of a page.
     */
    bool isPageAddress(flash_addr_t address) const {
        return (address % pageSize()) == 0;
    }

    bool writeEraseByte(uint8_t data, flash_addr_t address) {
        return writeErasePage(&data, address, 1);
    }

    uint8_t readByte(flash_addr_t address) const {
        uint8_t data = 0xFF;
        readPage(&data, address, 1);
        return data;
    }

    virtual bool erasePage(flash_addr_t address) = 0;

    /**
     * Writes directly to the flash. Depending upon the state of the flash, the
     * write may provide the data required or it may not.
     * @param data
     * @param address
     * @param length
     * @return
     */
    virtual bool writePage(const void* data, flash_addr_t address, page_size_t length) = 0;


    virtual bool readPage(void* data, flash_addr_t address, page_size_t length) const = 0;

    /**
     * Writes data to the flash memory, performing an erase beforehand if necessary
     * to ensure the data is written correctly.
     * @param data
     * @param address
     * @param length
     * @return
     */
    virtual bool writeErasePage(const void* data, flash_addr_t address, page_size_t length) = 0;

    /**
     * Internally re-reorganizes the page's storage by passing the page contents via
     * a buffer through a handler function and then writing the buffer back to
     * memory.
     *
     * @param address
     * @param handler
     * @param data
     * @param buf
     * @param bufSize
     * @return
     */
    virtual bool copyPage(flash_addr_t address, TransferHandler handler, void* data, uint8_t* buf, page_size_t bufSize) = 0;

};

/**
 * A flash device implementation that uses a block of dynamically allocated RAM
 * as the emulated backing store for the flash device.
 * Writes are logically ANDed with the existing data to emulate NAND flash behaviour. 
 */
class FakeFlashDevice : public FlashDevice {
    page_count_t pageCount_;
    page_size_t pageSize_;
    uint8_t* data_;

    inline bool isValidRegion(flash_addr_t address, page_size_t length) const {
        return address + length <= pageSize() * pageCount();
    }

public:

    FakeFlashDevice(page_count_t pageCount, page_size_t pageSize) :
    pageCount_(pageCount), pageSize_(pageSize) {
        flash_addr_t size = length();
        data_ = new uint8_t[size];
    }

    virtual ~FakeFlashDevice() {
        delete[] data_;
    }

    void eraseAll() {
        memset(data_, -1, length());
    }

    /**
     * @return The size of each page in this flash device.
     */
    virtual page_size_t pageSize() const {
        return pageSize_;
    }

    /**
     * @return The number of pages in this flash device.
     */
    virtual page_count_t pageCount() const {
        return pageCount_;
    }

    virtual bool erasePage(flash_addr_t address) {
        bool success = false;
        if (isPageAddress(address)) {
            memset(data_ + address, 0xFF, pageSize());
            success = true;
        }
        return success;
    }

    /**
     * Only writes an even number of bytes to an even address.
     * @param data
     * @param address
     * @param length
     * @return
     */
    virtual bool writePage(const void* _data, flash_addr_t address, page_size_t length) {
        bool success = false;
        const uint8_t* data = as_bytes(_data);
        if (isValidRegion(address, length)) {
            for (; length-- > 0;) {
                this->data_[address++] &= *data++;
            }
            success = true;
        }
        return success;
    }

    virtual bool readPage(void* data, flash_addr_t address, page_size_t length) const {
        bool success = false;
        if (isValidRegion(address, length)) {
            memcpy(data, this->data_ + address, length);
            success = true;
        }
        return success;
    }

    virtual bool writeErasePage(const void* _data, flash_addr_t address, page_size_t length) {
        bool success = false;
        const uint8_t* data = as_bytes(_data);
        if (!(address & 1) && !(length & 1) && isValidRegion(address, length)) {
            for (; length-- > 0;) {
                this->data_[address++] = *data++;
            }
            success = true;
        }
        return success;
    }

    virtual bool copyPage(flash_addr_t address, TransferHandler handler, void* data, uint8_t* buf, page_size_t bufSize) {
        return false;
    }
};

/**
 * Describes a region of a page that should not be copied by copyPage.
 */
struct FlashExcludeRegion {
    page_size_t start;
    page_size_t end;

    bool isExcluded(page_size_t value) const {
        return value >= start && value<end;
    }
};

/**
 * An abstract FlashDevice that provides the building blocks for creating
 * a view based on the address space of another flash device.
 */
class TranslatingFlashDevice : public FlashDevice {
protected:
    FlashDevice& flash;

    TranslatingFlashDevice(FlashDevice& storage) : flash(storage) {
    }

    /**
     * Translates an address from the publicly-visible address region
     * to the address region of the delegate flash device.
     */
    virtual flash_addr_t translateAddress(flash_addr_t address) const {
        return address;
    }

    /**
     * Copies data to the page, erasing the portion of a page specified.
     * @param pageOffset    The current page offset of the data in the buffer
     * @param data          A pointer to a FlashExcludeRegion.
     * @param buf           The data from the page
     * @param bufLen        The number of bytes of data in the buffer.
     */
    static void eraseExcludedHandler(page_size_t pageOffset, void* data, uint8_t* buf, page_size_t bufLen) {
        FlashExcludeRegion* region = (FlashExcludeRegion*) data;
        // todo - this can be made more efficient by intersecting the region [pageOffset, pageOffset+length] with the exclude region.
        for (page_size_t i = 0; i < bufLen; i++) {
            if (region->isExcluded(pageOffset + i)) {
                as_bytes(buf)[i] = 0xFF;
            }
        }
    }

    /**
     * A no-op copy handler - the buffer contents are not modified but simply copied as is.
     */
    static void copyPageHandler(page_size_t pageOffset, void* data, uint8_t* buf, page_size_t bufLen) {
    }


    /**
     * Handles writing of data to a given page performing a copy page operation if necessary to
     * erase the region being written to.
     *
     * @param data          The buffer of data to write
     * @param address       The logical address to write to. This is translated to the
     *      address space of the backing flash device.
     * @param length        The number of bytes to write to flash
     * @param buf           Temporary storage to be used during the write.
     * @param bufSize       The length of the temporary storage.
     * @return {@code true} if the data was successfully written. {@code false} otherwise.
     *
     * This function calls itself recursively if the page has to be to be erased in order to
     * successfully write the data.
     */
    bool writeErasePageBuf(const void* const _data, flash_addr_t address, page_size_t length, uint8_t* buf, page_size_t bufSize) {
        page_size_t offset = 0;
        flash_addr_t flashAddress = translateAddress(address);
        const uint8_t * const data = as_bytes(_data);
        while (offset < length) {
            const page_size_t toRead = min(bufSize, length - offset);
            const flash_addr_t dest = flashAddress + offset;
            if (!flash.writePage(data + offset, dest, toRead))
                break;

            if (!flash.readPage(buf, dest, toRead))
                break;

            if (memcmp(buf, data + offset, toRead)) {
                page_size_t pageOffset = address % pageSize();
                FlashExcludeRegion region = {pageOffset + offset, pageOffset + offset + length};
                if (!copyPage(address, eraseExcludedHandler, &region, buf, bufSize))
                    break;

                // now copy the data block to the freshly initialised page
                return writeErasePageBuf(data + offset, address + offset, length - offset, buf, bufSize);
            }

            offset += toRead;
        }
        return offset == length;
    }

    /**
     * Copies data from one page to another, via a TransferHandler.
     * @param src_page      The page to copy from.
     * @param dest_page
     * @param page_offset
     * @param count
     * @param handler
     * @param data
     * @param buf
     * @param bufSize
     * @return 
     */
    bool copyPageImpl(page_count_t src_page, page_count_t dest_page, page_size_t page_offset, page_size_t count,
            TransferHandler handler, void* data, uint8_t* buf, page_size_t bufSize) {
        flash_addr_t oldBase = flash.pageAddress(src_page) + page_offset;
        flash_addr_t newBase = flash.pageAddress(dest_page) + page_offset;
        page_size_t offset = 0;
        while (offset < count) {
            page_size_t toRead = min(bufSize, count - offset);
            if (!flash.readPage(buf, oldBase + offset, toRead))
                break;
            handler(offset, data, buf, toRead);
            if (!flash.writePage(buf, newBase + offset, toRead))
                break;
            offset += toRead;
        }
        return offset == count;
    }
};

/**
 * A flash device implementation that forwards calls to a delegate flash device
 * after checking the address range is within bounds. 
 */
class ForwardingFlashDevice : public TranslatingFlashDevice {
public:

    ForwardingFlashDevice(FlashDevice& storage) : TranslatingFlashDevice(storage) {
    }

    /**
     * @return The size of each page in this flash device.
     */
    virtual page_size_t pageSize() const {
        return flash.pageSize();
    }

    /**
     * @return The number of pages in this flash device.
     */
    virtual page_count_t pageCount() const {
        return flash.pageCount();
    }

    virtual bool isValidRange(flash_addr_t address, flash_addr_t length) const {
        return address + length <= flash.pageSize() * flash.pageCount();
    }

    virtual bool erasePage(flash_addr_t address) {
        return isValidRange(address, pageSize()) ? flash.erasePage(address) : false;
    }

    /**
     * Writes directly to the flash. Depending upon the state of the flash, the
     * write may provide the data required or it may not.
     * @param data
     * @param address
     * @param length
     * @return
     */
    virtual bool writePage(const void* data, flash_addr_t address, page_size_t length) {
        return isValidRange(address, length) ? flash.writePage(data, address, length) : false;
    }

    virtual bool readPage(void* data, flash_addr_t address, page_size_t length) const {
        return isValidRange(address, length) ? flash.readPage(data, address, length) : false;
    }

    /**
     * Writes data to the flash memory, performing an erase beforehand if necessary
     * to ensure the data is written correctly.
     * @param data
     * @param address
     * @param length
     * @return
     */
    virtual bool writeErasePage(const void* data, flash_addr_t address, page_size_t length) {
        return isValidRange(address, length) ? flash.writeErasePage(data, address, length) : false;
    }

    virtual bool copyPage(flash_addr_t address, TransferHandler handler, void* data, uint8_t* buf, page_size_t bufSize) {
        return flash.copyPage(address, handler, data, buf, bufSize);
    }

};

/**
 * A delegating FlashDevice that allows reads and writes to 
 * span page boundaries.
 * This allows clients to read and write data to arbitrary address ranges without
 * having to confine the datablock to a single page.
 */
class PageSpanFlashDevice : public ForwardingFlashDevice {

    static bool writeChunkHandler(PageSpanFlashDevice* span, const uint8_t* data, flash_addr_t address, page_size_t length) {
        return span->flash.writePage(data, address, length);
    }

    static bool readChunkHandler(const PageSpanFlashDevice* span, uint8_t* data, flash_addr_t address, page_size_t length) {
        return span->flash.readPage(data, address, length);
    }

    static bool writeEraseChunkHandler(PageSpanFlashDevice* span, const uint8_t* data, flash_addr_t address, page_size_t length) {
        return span->flash.writeErasePage(data, address, length);
    }

    /**
     * Splits the requested address span into chunks that do not cross page boundaries.
     * @param data
     * @param address
     * @param length
     * @param handler
     * @return
     */
    template<typename Data, typename Class> bool static chunk(Class* obj, Data* data, flash_addr_t address, page_size_t length,
            bool (*handler)(Class*, Data*, flash_addr_t, page_size_t)) {
        page_size_t size = obj->pageSize();
        page_size_t offset = address % size;
        while (length > 0) {
            page_size_t toWrite = min(size - offset, length);
            if (!handler(obj, data, address, toWrite))
                return false;
            data += toWrite;
            address += toWrite;
            length -= toWrite;
            offset = 0;
        }
        return true;
    }

public:

    PageSpanFlashDevice(FlashDevice& storage) : ForwardingFlashDevice(storage) {
    }

    virtual ~PageSpanFlashDevice() {
    }

    /**
     * Allows more than a single page to be written.
     * @param data
     * @param address
     * @param length
     * @return
     */
    virtual bool writePage(const void* data, flash_addr_t address, page_size_t length) {
        // rather than repeating the chunking logic for each method that needs it, it is
        // separated out and the chunked offsets and lengths passed back to the callback.
        return chunk(this, as_bytes(data), address, length, writeChunkHandler);
    }

    virtual bool readPage(void* data, flash_addr_t address, page_size_t length) const {
        return chunk(this, as_bytes(data), address, length, readChunkHandler);
    }

    /**
     * Writes data to the flash memory, performing an erase beforehand if necessary
     * to ensure the data is written correctly.
     * @param data
     * @param address
     * @param length
     * @return
     */
    virtual bool writeErasePage(const void* data, flash_addr_t address, page_size_t length) {
        return chunk(this, as_bytes(data), address, length, writeEraseChunkHandler);
    }
};

/**
 * Provides access to a subrange of an existing flash device, described by
 * a start address and an end address. The start address is inclusive, the end address is exclusive.
 * Both addresses must be on a page boundary. 
 */
class FlashDeviceRegion : public ForwardingFlashDevice {
private:
    flash_addr_t base_;
    flash_addr_t end_;

    typedef ForwardingFlashDevice super;

    flash_addr_t translateAddress(flash_addr_t address) const {
        return base_ + address;
    }

public:

    FlashDeviceRegion(FlashDevice& storage)
    : ForwardingFlashDevice(storage), base_(0), end_(storage.length()) {
    }

    FlashDeviceRegion(FlashDevice& storage, flash_addr_t start, flash_addr_t end)
    : ForwardingFlashDevice(storage), base_(start), end_(end) {
    }

    virtual ~FlashDeviceRegion() {
    }

    virtual bool isValidRegion(flash_addr_t address, page_size_t length) const {
        return address + length <= end_;
    }

    /**
     * @return The number of pages in this flash device.
     */
    virtual page_count_t pageCount() const {
        return (end_ - base_) / super::pageSize();
    }

    virtual bool erasePage(flash_addr_t address) {
        flash_addr_t dest = translateAddress(address);
        return super::erasePage(dest);
    }

    /**
     * Writes directly to the flash. Depending upon the state of the flash, the
     * write may provide the data required or it may not.
     * @param data
     * @param address
     * @param length
     * @return
     */
    virtual bool writePage(const void* data, flash_addr_t address, page_size_t length) {
        flash_addr_t dest = translateAddress(address);
        return super::writePage(data, dest, length);
    }

    virtual bool readPage(void* data, flash_addr_t address, page_size_t length) const {
        flash_addr_t dest = translateAddress(address);
        return super::readPage(data, dest, length);
    }

    /**
     * Writes data to the flash memory, performing an erase beforehand if necessary
     * to ensure the data is written correctly.
     * @param data
     * @param address
     * @param length
     * @return
     */
    virtual bool writeErasePage(const void* data, flash_addr_t address, page_size_t length) {
        flash_addr_t dest = translateAddress(address);
        return super::writeErasePage(data, dest, length);
    }

    virtual bool copyPage(flash_addr_t address, TransferHandler handler, void* data, uint8_t* buf, page_size_t bufSize) {
        flash_addr_t dest = translateAddress(address);
        return super::copyPage(dest, handler, data, buf, bufSize);
    }

    FlashDeviceRegion* createSubregion(flash_addr_t start, flash_addr_t end) const {
        flash_addr_t size = end_ - base_;
        if (start > end || !isPageAddress(start) || !isPageAddress(end) || end > size)
            return NULL;
        return new FlashDeviceRegion(flash, base_ + start, base_ + end);
    }

};

/**
 * When non-zero, the page mapper will pre-allocate all pages so that only the
 * number of free pages specified will be free. When false, pages are allocated
 * on demand as they are written, potentially increasing the number of free pages.
 */
#define PAGE_MAPPER_PRE_ALLOCATE_PAGES  0

/**
 * Implementation class for the logical page mapper.
 * This exists so that we can easily test the internal parts of the page mapper,
 * without having to resort to friend classes or including gtest in production
 * code.
 */
template <class page_index_t = uint8_t>
class LogicalPageMapperImpl {
public:

    typedef uint16_t header_t;

    /**
     * Each logical page starts with a 2-byte header.
     * The format of the header is:
     * 0xC000 - 2 bit flags. When both set, or both reset, the page is not in use.
     * 0x3FFF is the logical page corresponding to this
     * physical page.
     */
    static const unsigned int headerSize = 2u;
    static const header_t FORMAT_HEADER_SIGNATURE = 0x2FFFu;

    FlashDevice& flash;

    /**
     * The number of logical pages that will be allocated out of the physical storage.
     */
    const page_count_t logicalPageCount;

    /**
     * Each bit N%8 at index N/8 is set if physical page N is in use.
     */
    mutable uint8_t* inUse;

    /**
     * Maps each logical page (the array index) to the corresponding physical page.
     */
    mutable page_index_t* logicalPageMap;

    LogicalPageMapperImpl(FlashDevice& storage, page_count_t count)
    : flash(storage), logicalPageCount(count) {
        inUse = new uint8_t[(flash.pageCount() + 7) / 8];
        logicalPageMap = new page_index_t[logicalPageCount];
    }

    ~LogicalPageMapperImpl() {
        delete[] inUse;
        delete[] logicalPageMap;
    }

    /**
     * Initialises the physical storage if not already done.
     * @return
     */
    bool formatIfNeeded() {
        page_count_t max = maxPage();
        header_t header = readHeader(max);
        bool erased = false;
        if (header != FORMAT_HEADER_SIGNATURE) {
            erased = true;
            for (page_count_t i = max; i-- > 0;) {
                erasePageIfNecessary(i);
            }
            writeHeader(max, FORMAT_HEADER_SIGNATURE);
        }
        return erased;
    }

    /**
     * Determines if the page has been written to - specifically if bits have
     * been set to 0 (assumes NAND flash.)
     * @param page The page index to check if the page has any 0 bits.
     * @return {@code true} if this page requires erasing to reset.
     */
    bool pageIsDirty(page_count_t page) const {
        flash_addr_t addr = flash.pageAddress(page);
        flash_addr_t end = addr + flash.pageSize();
        uint8_t buf[STACK_BUFFER_SIZE];
        while (addr < end) {
            page_size_t toRead = page_size_t(min(flash_addr_t(128), end - addr));
            flash.readPage(buf, addr, toRead);
            for (page_size_t i = 0; i < toRead; i++) {
                if (buf[i] != 0xFF)
                    return true;
            }
            addr += toRead;
        }
        return false;
    }

    /**
     * Ensures a page is in pristine state and flags this page as not in use.
     * @param page The page index to erase.
     */
    void erasePageIfNecessary(page_count_t page) {
        if (pageIsDirty(page)) {
            flash.erasePage(flash.pageAddress(page));
        }
        setPageInUse(page, false);
    }

    /**
     * Fetches the maximum (exclusive) physical page that can be used for
     * backing logical pages. The housekeeping region starts here.
     */
    page_count_t maxPage() const {
        return flash.pageCount() - 1;
    }

    void assignLogicalPage(page_index_t logicalPage, page_index_t physicalPage) {
        logicalPageMap[logicalPage] = physicalPage;
    }

    void buildInUseMap() {
        page_count_t unallocated = maxPage();
        for (page_count_t i = 0; i < logicalPageCount; i++) {
            logicalPageMap[i] = page_index_t(unallocated);
        }

        for (page_count_t i = maxPage(); i-- > 0;) {
            uint16_t header = readHeader(i);
            bool inUse = isHeaderInUse(header);
            setPageInUse(i, inUse);
            if (inUse) {
                assignLogicalPage(logicalPageUse(header), i);
            }
        }

#if PAGE_MAPPER_PRE_ALLOCATE_PAGES
        for (page_count_t i = 0; i < logicalPageCount; i++) {
            if (logicalPageMap[i] == unallocated) {
                allocateLogicalPage(i);
            }
        }
#endif
    }

    static page_count_t randomPage() {
#ifdef SPARK
        return millis();
#else
        return rand();
#endif
    }

    /**
     * Allocates a new physical page for the given logical page.
     * @param page
     * @return The physical page allocated.
     */
    page_count_t allocateLogicalPage(page_index_t page) const {
        page_count_t free = nextFreePage(randomPage() % maxPage());
        setPageInUse(free, true);
        if (readHeader(free) != 0xFFFF) // if the header is clean the rest will be.
            flash.erasePage(flash.pageAddress(free));
        logicalPageMap[page] = free;
        writeHeader(free, page | 0x7F); // bit 15 clear means in use.
        return free;
    }

    /**
     * Finds the next free page starting from some random location.
     * @param offset
     * @return
     */
    page_count_t nextFreePage(page_count_t offset) const {
        page_count_t max = maxPage();
        for (page_count_t i = 0; i < max; i++) {
            page_count_t page = (i + offset) % max;
            if (!isPageInUse(page)) {
                return page;
            }
        }
        return page_count_t(-1);
    }

    /**
     * Retrieves the logical page index stored in this physical page header.
     * @param header
     * @return
     */
    page_index_t logicalPageUse(uint16_t header) const {
        return header & 0x3FFF;
    }

    bool isHeaderInUse(uint16_t header) const {
        uint8_t inUseFlags = header >> 14;
        // bits 0b11 mean not in use, when first in use set to 0b01
        // when not in use but before erasing, bits are set to 0b00
        return inUseFlags == 1;
    }

    uint16_t readHeader(page_count_t page) const {
        uint16_t header = 0;
        flash.readPage(&header, flash.pageAddress(page), headerSize);
        return header;
    }

    void writeHeader(page_count_t page, uint16_t header) const {
        flash.writePage(&header, flash.pageAddress(page), headerSize);
    }

    inline uint8_t& inUseFlags(page_count_t page) const {
        return inUse[page >> 3];
    }

    inline uint8_t pageFlagMask(page_count_t page) const {
        return 1 << (page & 7);
    }

    void setPageInUse(page_count_t page, bool inUse) const {
        if (inUse)
            inUseFlags(page) |= pageFlagMask(page);
        else
            inUseFlags(page) &= ~pageFlagMask(page);
    }

    bool isPageInUse(page_count_t page) const {
        return inUseFlags(page) & pageFlagMask(page);
    }

    page_count_t fetchAllocatePage(page_index_t page) const {
        page_count_t flashPage = logicalPageMap[page];
        if (flashPage == maxPage()) {
            flashPage = allocateLogicalPage(page);
        }
        return flashPage;
    }

    inline page_count_t physicalPageFor(page_count_t logicalPage) const {
        return logicalPageMap[logicalPage];
    }

    inline page_count_t pageFromAddress(flash_addr_t address, page_size_t pageSize) const {
        return address / pageSize;
    }

    flash_addr_t physicalAddress(flash_addr_t address, page_size_t size) const {
        page_index_t page = address / size;
        page_size_t offset = address % size;
        page_count_t flashPage = fetchAllocatePage(page);
        flash_addr_t flashAddr = flash.pageAddress(flashPage) + offset + headerSize;
        return flashAddr;
    }

    // FlashDevice implementation starts here.

    page_size_t pageSize() const {
        return flash.pageSize() - headerSize;
    }

    page_count_t pageCount() const {
        return logicalPageCount;
    }

    /**
     * Erase a logical page.
     * The corresponding physical page is marked as unused.
     * @param address
     * @return
     */
    bool erasePage(flash_addr_t address) {
        page_index_t page = pageFromAddress(address, pageSize());
        bool success = false;
        if (page >= 0 && page < logicalPageCount) {
            page_count_t physicalPage = physicalPageFor(page);
            page_count_t max = maxPage();
            success = physicalPage == max;
            if (!success) {
                logicalPageMap[page] = max; // mark as no allocation
                if (flash.erasePage(flash.pageAddress(physicalPage))) {
                    setPageInUse(physicalPage, false);
                    allocateLogicalPage(page);
                    success = true;
                }
            }
        }
        return success;
    }

    inline bool writePage(const void* data, flash_addr_t address, page_size_t length) {
        // assume for now that it's within a single block. will deal with multiple blocks later
        return flash.writePage(data, physicalAddress(address, pageSize()), length);
    }

    inline bool readPage(void* data, flash_addr_t address, page_size_t length) const {
        return flash.readPage(data, physicalAddress(address, pageSize()), length);
    }

    /**
     * Implements a page copy by logical page reassignment. The physical page is
     * copied from it's current location to a free page. After copy, the old page
     * is freed and the new page made the backing page for the original logical page.
     * @param address
     * @param handler
     * @param data
     * @param buf
     * @param bufSize
     * @return
     */
    bool copyPage(flash_addr_t address, TransferHandler handler, void* data, uint8_t* buf, page_size_t bufSize) {
        page_count_t logicalPage = address / pageSize();
        page_count_t oldPage = this->physicalPageFor(logicalPage);
        page_count_t newPage = this->allocateLogicalPage(logicalPage);
        page_size_t offset = 0;
        page_size_t size = pageSize();
        flash_addr_t oldBase = flash.pageAddress(oldPage) + headerSize;
        flash_addr_t newBase = flash.pageAddress(newPage) + headerSize;

        while (offset < size) {
            page_size_t toRead = min(bufSize, size - offset);
            if (!flash.readPage(buf, oldBase + offset, toRead))
                break;
            handler(offset, data, buf, toRead);
            if (!flash.writePage(buf, newBase + offset, toRead))
                break;
            offset += toRead;
        }

        this->setPageInUse(oldPage, false);
        return offset == size;
    }

};

/**
 * Exposes a block of flash memory, based on mapping a smaller logical region
 * (what the client sees) to a larger physical region of memory. As logical pages
 * are erased they are mapped to different physical pages, providing primitive
 * wear levelling.
 * Maps X logical pages to Y physical pages in a flash device. X<Y
 * Uses one page as swap storage so there is at least one free physical page.
 */
template <class page_index_t = uint8_t>
class LogicalPageMapper : public TranslatingFlashDevice {
    typedef LogicalPageMapperImpl<page_index_t> Impl;

    Impl impl;

protected:

    flash_addr_t translateAddress(flash_addr_t address) const {
        return impl.physicalAddress(address, pageSize());
    }

    bool writeErasePageBuf(const void* const data, flash_addr_t address, page_size_t length, uint8_t* buf, page_size_t bufSize) {
        return TranslatingFlashDevice::writeErasePageBuf(data, address, length, buf, bufSize);
    }

public:

    /**
     *
     * @param logicalPageCount  The number of logical pages to maintain.
     */
    LogicalPageMapper(FlashDevice& storage, page_count_t logicalPageCount)
    : TranslatingFlashDevice(storage), impl(storage, logicalPageCount) {
        impl.formatIfNeeded();
        impl.buildInUseMap();
    }

    virtual page_size_t pageSize() const {
        return impl.pageSize();
    }

    virtual page_count_t pageCount() const {
        return impl.pageCount();
    }

    /**
     * Erase a logical page.
     * The corresponding physical page is marked as unused.
     * @param address
     * @return
     */
    virtual bool erasePage(flash_addr_t address) {
        return impl.erasePage(address);
    }

    virtual bool writePage(const void* data, flash_addr_t address, page_size_t length) {
        return impl.writePage(data, address, length);
    }

    virtual bool readPage(void* data, flash_addr_t address, page_size_t length) const {
        return impl.readPage(data, address, length);
    }

    /**
     * Implements a page copy by logical page reassignment. The physical page is
     * copied from it's current location to a free page. After copy, the old page
     * is freed and the new page made the backing page for the original logical page.
     * @param address
     * @param handler
     * @param data
     * @param buf
     * @param bufSize
     * @return
     */
    virtual bool copyPage(flash_addr_t address, TransferHandler handler, void* data, uint8_t* buf, page_size_t bufSize) {
        return impl.copyPage(address, handler, data, buf, bufSize);
    }

    /**
     * Attempts to write the data to the flash memory and compares the written data.
     * If the data could not be written, the page is copied to a new page and
     * the operation retried.
     * data.
     * @param _data
     * @param address
     * @param length
     * @return
     */
    bool writeErasePage(const void* data, flash_addr_t address, page_size_t length) {
        uint8_t buf[STACK_BUFFER_SIZE];
        return writeErasePageBuf(data, address, length, buf, STACK_BUFFER_SIZE);
    }

};

/**
 * Utility class for reading, writing and compacting redundant slots.
 */
class MultiWriteSlotAccess {
public:
    static const unsigned int SLOT_SIZE = 8;
    static const unsigned int SLOT_SIZE_SHIFT = 3;
    static const unsigned int DATA_BYTES_PER_SLOT = 7;

    /**
     * Reads the value at the given slot. This inspects the bitmask to determine
     * the location that the current value is at.
     * @param slot
     * @return
     */
    static uint8_t readSlot(uint8_t* slot) {
        uint8_t bitmap = *slot;
        // NB: this also works for the special case when the bitmap is 0xFF,
        // meaning, uninitialised. The resulting index will be 0, which returns the bitmap, 0xFF
        int index = 0; // find the last used index.
        while (!(bitmap & 1)) { // bit is 0, index used
            bitmap >>= 1;
            index++;
        }
        return slot[index];
    }

    /**
     * Writes a byte value to the slot. The byte is written in the current slot.
     * A slot is 8 bytes wide. The first byte is a bitmap, and subsequent 7 bytes
     * are values. bit N in the bitmap is cleared when slot[N] has been written to.
     * @param data
     * @param slot
     * @param inPlace If a forced in place update of the value should be done.
     * The value is ANDed with the current value regardless, even if this produces a different data value.
     * This is mainly for testing when we need to to preserve flash memory semantics in the writePage() method.
     * @return {@code true} if the data was successfully written, {@link false} otherwise.
     * Note that when {@code inPlace} is true, the function always succeeds.
     */
    static bool writeSlot(uint8_t data, uint8_t* slot, bool inPlace = false) {
        uint8_t bitmap = *slot;
        bool success = false;
        if (bitmap == 0xFF) { // uninitialized
            if (data != 0xFF) { // data needs to be written
                slot[1] = data;
                slot[0] = 0xFE; // flag first location used
            }
            success = true;
        } else {
            int index = 0; // find the last used index.
            uint8_t mask = 1;
            while (!(bitmap & mask)) { // bit is 0, index used
                index++;
                mask <<= 1;
            }

            success = ((slot[index] &= data) == data || inPlace);
            if (!success) { // if cannot update in place
                if (index < 7) { // not on the last index
                    index++;
                    bitmap = bitmap << 1;
                    slot[0] = bitmap; // mark next slot as used
                    slot[index] = data;
                    success = true;
                }
            }
            // else no more room in this slot for further writes
        }
        return success;
    }

    /**
     * Compacts the data in a slot.
     * @param slot
     */
    static void compactSlot(uint8_t* slot) {
        uint8_t value = readSlot(slot);
        memset(slot, 0xFF, SLOT_SIZE);
        writeSlot(value, slot);
    }


};

/**
 * Stores data redundantly in flash so that multiple writes to the same logical
 * address do not require erasing the page.
 * Each logical address corresponds to a 8-byte slot. The first byte is a bitmap
 * that indicates which of the subsequent bytes contain values.
 * The slot stores data with 7x redundancy so that the page is erased only after a
 * given logical address is destructively written 7 times. (A destructive write is
 * where the new value is not a subset of the set bits in the previous value.)
 */
class MultiWriteFlashStore : public FlashDevice, private MultiWriteSlotAccess {
    FlashDevice& flash;

    /**
     * Convert the address at this level to the corresponding address in the
     * lower level storage.
     *
     * @return
     */
    inline flash_addr_t toPhysicalAddress(flash_addr_t address) const {
        page_size_t size = pageSize();
        page_count_t page = address / size;
        page_size_t offset = address % size;
        flash_addr_t flashAddress = flash.pageAddress(page) + (offset << SLOT_SIZE_SHIFT);
        return flashAddress;
    }

    /**
     * Handles the relocation of a physical page. Each slot is compressed to a single value
     * apart from those slots in the exclude region, which are simply initialized,
     * since they will be filled in afterwards.
     * @param pageOffset
     * @param data
     * @param buf
     * @param bufLen
     */
    static void compactPageExcludeRegionHandler(page_size_t pageOffset, void* data, uint8_t* buf, const page_size_t bufLen) {
        FlashExcludeRegion* region = (FlashExcludeRegion*) data;
        for (page_size_t i = 0; i < (bufLen & ~(SLOT_SIZE - 1)); i += SLOT_SIZE) {
            if (region->isExcluded(pageOffset + i))
                memset(as_bytes(buf) + i, 0xFF, SLOT_SIZE); // slot is uninitialized - will be written later.
            else
                compactSlot(as_bytes(buf) + i);
        }
    }

    /**
     * Normalizes each slot, passing the current value to a TransferHandler function.
     * @param pageOffset
     * @param data          Expected the address of a delegate transfer function.
     * @param buf
     * @param bufLen
     */
    static void copyPageHandler(page_size_t pageOffset, void* data, uint8_t* buf, const page_size_t bufLen) {
        void** pData = (void**) data;
        TransferHandler delegateHandler = TransferHandler(pData[0]);
        void* delegateData = pData[1];
        page_size_t byteOffset = pageOffset>>SLOT_SIZE_SHIFT;
        for (page_size_t i = 0; i < (bufLen & ~(SLOT_SIZE - 1)); i += SLOT_SIZE, byteOffset++) {
            uint8_t value = readSlot(as_bytes(buf) + i);
            memset(as_bytes(buf) + i, 0xFF, SLOT_SIZE);
            delegateHandler(byteOffset, delegateData, &value, 1);
            writeSlot(value, as_bytes(buf) + i);
        }
    }

public:

    MultiWriteFlashStore(FlashDevice& storage) : flash(storage) {
    }

    /**
     * The size of each page is reduced by a factor of 8 since each byte
     * is stored in a slot of size 8.
     */
    virtual page_size_t pageSize() const {
        return flash.pageSize() >> SLOT_SIZE_SHIFT;
    }

    /**
     * There is a one to one correspondence of pages between this layer and the lower
     * flash layer, so the number of pages is identical.
     */

    virtual page_count_t pageCount() const {
        return flash.pageCount();
    }

    virtual bool writeErasePage(const void* data, flash_addr_t address, page_size_t length) {
        uint8_t buf[STACK_BUFFER_SIZE];
        return writeErasePageBuf(data, address, length, buf, STACK_BUFFER_SIZE);
    }

    bool writeErasePageBuf(const void* _data, flash_addr_t address, page_size_t length, uint8_t* buf, page_size_t bufSize) {
        page_size_t offset = 0;
        const uint8_t* data = as_bytes(_data);
        while (offset < length) {
            page_size_t toReadLogical = min(page_size_t(bufSize >> SLOT_SIZE_SHIFT), length - offset);
            page_size_t toReadBase = toReadLogical << SLOT_SIZE_SHIFT;
            flash_addr_t dest = toPhysicalAddress(address + offset);
            if (!flash.readPage(buf, dest, toReadBase))
                break;

            for (page_size_t i = 0; i < toReadBase; i += SLOT_SIZE, data++) {
                if (!writeSlot(*data, buf + i)) { // cannot write the slot
                    if (!flash.writePage(buf, dest, i)) // but write what has been copied to the buffer so far
                        break;

                    offset += (i >> SLOT_SIZE_SHIFT); // account for data successfully copied so far
                    // pass in params for the region to exclude from the copy
                    // data+i points to the first data byte not to copy, through to
                    // data+i(length-offset))
                    page_size_t addressOffset = address % pageSize();
                    FlashExcludeRegion region = {addressOffset + offset, addressOffset + offset + length};
                    if (!flash.copyPage(address, &compactPageExcludeRegionHandler, &region, buf, bufSize))
                        break;

                    // now copy the data block to the freshly initialized page
                    return writeErasePageBuf(data, address + offset, length - offset, buf, bufSize);
                }
            }

            if (!flash.writePage(buf, dest, toReadBase))
                break;

            offset += toReadLogical;
        }
        return offset == length;
    }

    /**
     * Erases the given page. This is done by erasing the corresponding page
     * in the logical layer below.
     * @param address
     * @return
     */
    bool erasePage(flash_addr_t address) {
        return flash.erasePage(toPhysicalAddress(address));
    }

    bool writePage(const void* _data, flash_addr_t address, page_size_t length) {
        // todo factor out common code in writePage/writeErasePage.
        page_size_t offset = 0;
        const uint8_t* data = as_bytes(_data);
        uint8_t buf[STACK_BUFFER_SIZE];
        while (offset < length) {
            page_size_t toRead = min(page_size_t(sizeof (buf) >> SLOT_SIZE_SHIFT), length - offset);
            flash_addr_t dest = toPhysicalAddress(address + offset);
            if (!flash.readPage(buf, dest, toRead << SLOT_SIZE_SHIFT))
                break;

            for (page_size_t i = 0; i < toRead; i++, data++) {
                writeSlot(*data, buf + (i << SLOT_SIZE_SHIFT), true);
            }

            if (!flash.writePage(buf, dest, toRead << SLOT_SIZE_SHIFT))
                break;

            offset += toRead;
        }
        return offset == length;
    }

    virtual bool readPage(void* _data, flash_addr_t address, page_size_t length) const {
        page_size_t offset = 0;
        uint8_t* data = as_bytes(_data);
        uint8_t buf[STACK_BUFFER_SIZE];
        while (offset < length) {
            page_size_t toRead = min(page_size_t(sizeof (buf) >> SLOT_SIZE_SHIFT), length - offset);
            flash_addr_t dest = toPhysicalAddress(address + offset);
            if (!flash.readPage(buf, dest, toRead << SLOT_SIZE_SHIFT))
                break;

            for (page_size_t i = 0; i < toRead; i++, data++) {
                *data = readSlot(buf + (i << SLOT_SIZE_SHIFT));
            }

            offset += toRead;
        }
        return offset == length;
    }

    bool copyPage(flash_addr_t address, TransferHandler handler, void* data, uint8_t* buf, page_size_t bufSize) {
        return false;
    }
};

/**
 * Uses a single page in the flash memory to serve as a copy buffer when a page
 * needs to be refreshed.
 */
class SinglePageWear : public ForwardingFlashDevice {
    typedef ForwardingFlashDevice super;

public:

    SinglePageWear(FlashDevice& storage) : ForwardingFlashDevice(storage) {
    }

    bool isValidRegion(flash_addr_t address, page_size_t size) const {
        return address + size <= length();
    }

    /**
     * @return The number of pages in this flash device.
     */
    virtual page_count_t pageCount() const {
        return super::pageCount() - 1;
    }

    /**
     * Writes data to the flash memory, performing an erase beforehand if necessary
     * to ensure the data is written correctly.
     * @param data
     * @param address
     * @param length
     * @return
     */
    virtual bool writeErasePage(const void* data, flash_addr_t address, page_size_t length) {
        uint8_t buf[STACK_BUFFER_SIZE];
        return writeErasePageBuf(data, address, length, buf, STACK_BUFFER_SIZE);
    }

    /**
     * Internally re-reorganizes the page's storage by passing the page contents via
     * a buffer through a handler function and then writing the buffer back to
     * memory.
     * 
     * This implementation copies the page to the reserved page (the last page
     * in the underlying storage) and then copies back to the original page.
     *
     * @param address
     * @param handler
     * @param data
     * @param buf
     * @param bufSize
     * @return
     */
    virtual bool copyPage(flash_addr_t address, TransferHandler handler,
            void* data, uint8_t* buf, page_size_t bufSize) {
        page_count_t oldPage = address / pageSize();
        page_count_t newPage = this->pageCount();
        page_size_t size = pageSize();
        bool success = flash.erasePage(newPage*size);
        success = success && copyPageImpl(oldPage, newPage, 0, size, handler, data, buf, bufSize);
        success = success && flash.erasePage(oldPage*size);
        success = success && copyPageImpl(newPage, oldPage, 0, size, copyPageHandler, data, buf, bufSize);
        return success;
    }

};

/**
 * A circular buffer over flash memory. When the writer attempts to overwrite
 * the page that the reader is on, writing fails by returning 0.
 */
class CircularBuffer {
    FlashDevice& flash;
    flash_addr_t write_pointer;
    mutable flash_addr_t read_pointer;
    const flash_addr_t capacity;
    mutable flash_addr_t size;
    
public:

    CircularBuffer(FlashDevice& storage)
    : flash(storage), write_pointer(0), read_pointer(0), 
            capacity(flash.pageAddress(flash.pageCount())), size(0) {
    }

    /**
     * Reads up to {@code length} bytes from the buffer. If data is available
     * then at least one byte will be returned (unless length is 0.)
     * @param buf
     * @param len
     * @return The number of bytes written to the buffer. This will be >0 if there
     * is data available, and <= length specified. If this returns 0, there is no
     * data available in the buffer.
     */
    page_size_t read(void* buf, page_size_t length) const {
        if (length == 0 || size==0)
            return 0;

        page_size_t toRead = 0;
        int32_t diff = write_pointer - read_pointer;
        if (diff > 0) {
            toRead = min(page_size_t(diff), length);
        } else if (diff <= 0) {
            // read up to the end of flash
            toRead = min(capacity - read_pointer, flash_addr_t(length));
        }
        page_size_t result = toRead;
        page_size_t blockSize = flash.pageSize();
        while (toRead > 0) {
            // write each page             
            page_size_t offset = read_pointer % blockSize;
            page_size_t blockRead = min(toRead, blockSize-offset);            
            flash.readPage(buf, read_pointer, blockRead);
            read_pointer += blockRead;
            toRead -= blockRead;
            buf = ((uint8_t*)buf)+blockRead;
        }
        if (read_pointer == capacity)
            read_pointer = 0;
        size -= result;
        return result;
    }

    /**
     * Attempts to write data to the buffer.
     * @param buf       Pointer to the data to write
     * @param length    The maximum number of bytes of data to write.
     * @return The number of bytes actually written. If this is 0, then the buffer
     *  is full.
     */
    page_size_t write(void* buf, page_size_t length) {
        if (length == 0 || size==capacity)
            return 0;
        
        int32_t diff = write_pointer - read_pointer;
        page_size_t toWrite = 0;
        if (diff >= 0) {
            toWrite = min(capacity - write_pointer, flash_addr_t(length));
        } else { // write pointer behind read pointer - don't write into the read page
            flash_addr_t max = read_pointer - (read_pointer % flash.pageSize());
            toWrite = min(max - write_pointer, flash_addr_t(length));
        }
        page_size_t blockSize = flash.pageSize();
        page_size_t result = toWrite;
        while (toWrite > 0) {
            // write each page             
            page_size_t offset = write_pointer % blockSize;
            page_size_t blockWrite = min(toWrite, blockSize-offset);
            if (!offset)
                flash.erasePage(write_pointer);            
            flash.writePage(buf, write_pointer, blockWrite);
            write_pointer += blockWrite;
            buf = ((uint8_t*)buf)+blockWrite;
            toWrite -= blockWrite;
        }
        if (write_pointer == capacity)
            write_pointer = 0;
        size += result;
        return result;
    }

    /**
     * Retrieves the maximum number of bytes that can be read from the buffer.
     * @return The maximum number of bytes that can be read from the buffer.
     */
    page_size_t available() const {
        return size;
    }
};

#ifdef SPARK
#include "sst25vf_spi.h"

class SparkExternalFlashDevice : public FlashDevice {

    /**
     * @return The size of each page in this flash device.
     */
    virtual page_size_t pageSize() const {
        return sFLASH_PAGESIZE;
    }

    /**
     * @return The number of pages in this flash device.
     */
    virtual page_count_t pageCount() const {
        return 512;
    }

    virtual bool erasePage(flash_addr_t address) {
        bool success = false;
        if (address < pageAddress(pageCount()) && (address % pageSize()) == 0) {
            sFLASH_EraseSector(address);
            success = true;
        }
        return success;
    }

    /**
     * Writes directly to the flash. Depending upon the state of the flash, the
     * write may provide the data required or it may not.
     * @param data
     * @param address
     * @param length
     * @return
     */
    virtual bool writePage(const void* data, flash_addr_t address, page_size_t length) {
        // TODO: SPI interface shouldn't need variable data to write?
        sFLASH_WriteBuffer((const uint8_t*) data, address, length);
        return true;
    }

    virtual bool readPage(void* data, flash_addr_t address, page_size_t length) const {
        sFLASH_ReadBuffer((uint8_t*) data, address, length);
        return true;
    }

    /**
     * Writes data to the flash memory, performing an erase beforehand if necessary
     * to ensure the data is written correctly.
     * @param data
     * @param address
     * @param length
     * @return
     */
    virtual bool writeErasePage(const void* data, flash_addr_t address, page_size_t length) {
        return false;
    }

    /**
     * Internally re-reorganizes the page's storage by passing the page contents via
     * a buffer through a handler function and then writing the buffer back to
     * memory.
     *
     * @param address
     * @param handler
     * @param data
     * @param buf
     * @param bufSize
     * @return
     */
    virtual bool copyPage(flash_addr_t address, TransferHandler handler, void* data, uint8_t* buf, page_size_t bufSize) {
        return false;
    }

};
#endif // SPARK

class flashee {
private:
    static FlashDeviceRegion userRegion;

    inline static FlashDevice* createUserFlashRegion(flash_addr_t startAddress, flash_addr_t endAddress) {
        return userRegion.createSubregion(startAddress, endAddress);
    }

    inline static FlashDevice* createLogicalPageMapper(FlashDevice* flash, page_count_t pageCount) {
        page_count_t count = flash->pageCount();
        return count <= 256 && pageCount < count && pageCount > 1 ? new LogicalPageMapper<>(*flash, pageCount) : NULL;
    }

    inline static FlashDevice* createMultiWrite(FlashDevice* flash) {
        return new MultiWriteFlashStore(*flash);
    }

    inline static FlashDevice* createMultiPageEraseImpl(flash_addr_t startAddress, flash_addr_t endAddress, page_count_t freePageCount) {
        if (endAddress == flash_addr_t(-1))
            endAddress = startAddress + userRegion.pageAddress(256);
        if (freePageCount < 2 || freePageCount >= ((endAddress - startAddress) / userRegion.pageSize()))
            return NULL;
        FlashDevice* userFlash = createUserFlashRegion(startAddress, endAddress);
        if (userFlash==NULL)
            return NULL;
        FlashDevice* mapper = createLogicalPageMapper(userFlash, userFlash->pageCount() - freePageCount);
        return mapper;
    }

public:

    /**
     * Provides access to the user accessible region.
     * The hides the actual location in external flash - so the
     * the first writable address is 0x000000.
     * @return A reference to the user accessible flash.
     */
    static FlashDevice& userFlash() {
        return userRegion;
    }

    /**
     * Creates a flash device where each destructive write causes the page to
     * be erased and and internal reserved page to be erased.
     * This should not be used only if the number of destructive writes is known to be
     * less than 10^6 for the lifetime of the unit.
     */
    static FlashDevice* createSinglePageErase(flash_addr_t startAddress, flash_addr_t endAddress) {
        FlashDevice* userFlash = createUserFlashRegion(startAddress, endAddress);
        if (userFlash!=NULL) {
            SinglePageWear* wear = new SinglePageWear(*userFlash);
            return new PageSpanFlashDevice(*wear);
        }
        return NULL;
    }

    /**
     * Creates a flash device where destructive writes cause a page erase, and
     * the page erases are levelled out over the available free pages.
     * @param startAddress
     * @param endAddress
     * @param freePageCount     The number of pages to allocate in the given region.
     *
     * @return
     */
    static FlashDevice* createMultiPageErase(flash_addr_t startAddress = 0, flash_addr_t endAddress = flash_addr_t(-1), page_count_t freePageCount = 2) {
        FlashDevice* mapper = createMultiPageEraseImpl(startAddress, endAddress, freePageCount);
        return mapper == NULL ? NULL : new PageSpanFlashDevice(*mapper);
    }

    /**
     * Creates a flash device where destructive writes do not require a page erase,
     * and when a page erase is required, it is wear-levelled out over the available
     * free pages.
     * @param startAddress
     * @param endAddress
     * @param pageCount
     * @return
     */
    static FlashDevice* createAddressErase(flash_addr_t startAddress = 0, flash_addr_t endAddress = flash_addr_t(-1), page_count_t freePageCount = 2) {
        FlashDevice* mapper = createMultiPageEraseImpl(startAddress, endAddress, freePageCount);
        if (mapper == NULL)
            return NULL;
        FlashDevice* multi = createMultiWrite(mapper);
        return new PageSpanFlashDevice(*multi);
    }

    /**
     * Creates a circular buffer that uses the pages
     */
    static CircularBuffer* createCircularBuffer(flash_addr_t startAddress, flash_addr_t endAddress) {              
        FlashDevice* device = createUserFlashRegion(startAddress, endAddress);
        return device ? new CircularBuffer(*device) : NULL;
    }
};

} // namespace

#endif