spark-flashee-eeprom
====================

Eeprom emulation using external flash on the [Spark Core](http://spark.io).
Includes unit tests cross compiled to regular gcc, and on-device integration tests.

Elevator Pitch
--------------

This library allows client code to write to the external flash as if it were
EEPROM. It takes care of performing erases when required to ensure
data is written correctly. 3 storage schemes are provided that offer
different trade-offs in storage efficiency for increased number of write
cycles over the lifetime of the device.

Key features:

- Provides persistent storage using the external 1.5MB user flash on the spark
- EEPROM-like access - (byte erasable) no need to worry about erasing pages to ensure data integrity.
- 3 different types of eeprom emulations providing speed/erase cycle tradeoff.
- Wear levelling and page allocation on demand for increased endurance
- Circular buffers for logs, temporary data etc.
- Stream access to the storage for convenient read and write of multiple values.

Getting Started
===============

To use the library in application code, include the header file and import the namespace:

```c++
    #include "flashee-eeprom/flashee-eeprom.h"
    using namespace Flashee;
```

To create the flash object:

```c++
    FlashDevice* flash = Devices::createAddressErase();
```

This reserves all pages in the external flash for a flash device that provides byte-level erases. This offers
the best erase cycle count at the cost of an 8x overhead.

Another eeprom emulation scheme is basic wear levelling:

```c++
    FlashDevice* flash = Devices::createWearLevelErase();
```

This uses wear levelling to spread the erases out over the flash region. Each write to the flash - or specifically
each write that changes a 0 to a 1, will cause a page erase.

To store data:

```c++
    flash->writeString("Hello World!", 0);
```

This writes the string to the flash memory starting at address 0.

To retrieve data:

```c++
    char buf[13];
    flash->read(buf, 0, 13);
```

So far, this is just like the regular sFLASH access functions that are available in the spark core API.
Where this library is different is that you can freely
overwrite the data, just by issuing another write command:

```c++
    flash->writeString("I think I changed my mind!", 0);
```

If you're not storing strings but simply data buffers, binary writes are supported:

```c++
    flash->write(&my_struct, 123, sizeof(my_struct));
```

The key difference between flash and eeprom is that with flash memory you cannot normally erase a single byte, but have to
erase a whole page. This library takes care of that, and presents an interface to the flash device that makes it
behave like eeprom, which does support rewrites of data without having to perform a page erase.


Circular Buffers
================

The library provides a [circular buffer](http://en.wikipedia.org/wiki/Circular_buffer) implementation that
allows data to be written to a storage device and subsequently read out again.

To create a circular buffer, specify the start and end address (as usual, these should be on page boundaries):

```c++
    CircularBuffer* buffer = Devices::createCircularBuffer(4096*256, 4096*384);
```

This will create a circular buffer from page 256 through to page 384 (0.5MB).

Writing to the buffer:
```c++
    MyStruct data = ...;
    bool success = buffer->write(&data, sizeof(data));
```

If there is not room in the buffer for the complete block, the call fails and returns `false`.

Reading from the buffer:

```c++
    MyStruct data = ...;
    bool success = buffer->read(&data, sizeof(data));
```
Data is read from the buffer in the same sequence it was written.


To determine how much data can be read from or written to the buffer:

```c++
    page_size_t can_read = buffer->available();
    page_size_t maximum = buffer->capacity();
    page_size_t can_store = buffer->free();
```

Note that the buffer is just a large block of bytes. It is up to the caller to be sure that the way the data is
retrieved is compatible with how it was stored.

Normally, the buffer is all or nothing - if the requested number of bytes cannot be read or written, then no bytes
are read or written. There are 'soft' variants of the read/write methods that allow less than the specified number of
bytes to be read/written.

Note that although the circular buffer storage is in flash, the data is essentially non-persistent. On reset, a new
buffer is created which ignores the data in flash.


Streaming
=========
The streaming classes provide a higher-level access to the storage. For example:

```c++
    FlashDevice* device = Devices::createWearLevelErase();
    FlashWriter writer(device);
    writer.writeString("Hello World");
    writer.writeInt(42);
```

And this data can then be read back later:

    FlashDevice* device = Devices::createWearLevelErase();
    FlashReader reader(device);
    char buf[50];
    reader.readeString(buf);
    int answer = reader.readInt();



Coding tips
===========

* The [main API](firmware/flashee-eeprom.h) to the library is provided by the `Devices` class, which is a factory for obtaining various flash-access
devices, and the `FlashDevice` abstract base class, which defines the operations of a flash device.

* The spark external eeprom has 384 pages, each 4096 bytes in size and is managed as the device provided via the `Devices::userFlash()` method.
Rather than hard-code these page count and page size constants, you can make the code more flexible for future changes by using `Devices::userFlash().pageSize()` and
 `Devices::userFlash().pageCount()`.

* when writing data to a device, try to write in as few blocks as possible (particularly if the data is overwriting previously eritten data).
 This will reduce the number of erases performed by the library, particularly for the wear levelling scheme. When using
 the Address Erase strategy, then byte by byte writes are fine.

* At present, the maximum contiguous area that the Wear Levelling or Address Erase scheme can occupy is 1MB (256 pages).
This is to keep runtime memory overhead to a minimum. This restriction may later be relaxed.

* It's possible to create several different devices and have them all active at once, so long as they are in separate regions. For example

```c++
    FlashDevice* eeprom = Devices::createAddressErase(0, 256*4096);
    CircularBuffer* logBuffer = Devices::createCircularBuffer(256*4096, 256*4096);
```

This creates a byte erasable eeprom device in the first 1MB, and a circular buffer in the final 0.5MB.


Testing
=======
This Flashee-Eeprom library contains unit tests and integration tests.
Unit tests are compiled using GCC and run on the host platform.
Integration tests are executed on the embedded device.

Unit Tests
----------
The unit tests exercise the types of flash eeprom emulation, as well as stressing individual classes and methods.
The tests run against mock or fake implementations of a flash device.

To run the unit tests:

    1. clone this repo to your desktop machine
    2. (if running Windows, be sure to install MinGW installed also.)
    3. cd to flashee-eeprom/firmware/test
    4. run `make test`

After the tests are built, they are automatically run. You should then see output similar to this:

    [----------] 4 tests from FlashDeviceRegionTest
    [ RUN      ] FlashDeviceRegionTest.CanCreateSubregion
    [       OK ] FlashDeviceRegionTest.CanCreateSubregion (0 ms)
    [ RUN      ] FlashDeviceRegionTest.SubregionOutOfRangeFails
    [       OK ] FlashDeviceRegionTest.SubregionOutOfRangeFails (0 ms)
    [ RUN      ] FlashDeviceRegionTest.NestedSubregionOutOfRangeFails
    [       OK ] FlashDeviceRegionTest.NestedSubregionOutOfRangeFails (0 ms)
    [ RUN      ] FlashDeviceRegionTest.NestedSubregionInRangeOk
    [       OK ] FlashDeviceRegionTest.NestedSubregionInRangeOk (1 ms)
    [----------] 4 tests from FlashDeviceRegionTest (4 ms total)

    [----------] 2 tests from FakeFlashDeviceTest
    [ RUN      ] FakeFlashDeviceTest.CorrectSize
    [       OK ] FakeFlashDeviceTest.CorrectSize (0 ms)
    [ RUN      ] FakeFlashDeviceTest.CorrectSize2
    [       OK ] FakeFlashDeviceTest.CorrectSize2 (0 ms)
    [----------] 2 tests from FakeFlashDeviceTest (1 ms total)

    [----------] Global test environment tear-down
    [==========] 102 tests from 8 test cases ran. (1714 ms total)
    [  PASSED  ] 102 tests.


Integration Tests
-----------------
The integration tests combine the real external flash device in the spark core with the eeprom emulation layers.
The aim of the integration test is to test that the library functions as a whole.

The integration test is available from the online IDE, under `examples/integration-test.cpp`.

To run the integration test:

    1. build and flash integration-test-cpp to the core
    2. use a serial monitor to connect to the core locally via the USB serial interface
    3. press 't' to start the tests.

If all goes well, you should see output in the serial monitor after a few seconds. The whole suite takes about a minute
to run and produces output similar to this:

    Running tests
    Test CanWriteAfterErase passed.
    Test ErasePageNoEffectOnOtherPages passed.
    Test ErasePageResetsData passed.
    Test EraseWriteAllowsBitsToBeSet passed.
    Test EraseWritePreservesRestOfPage passed.
    Test HasNonZeroPageCount passed.
    Test HasNonZeroPageSize passed.
    Test PageAddress passed.
    Test RepeatedEraseWritePreservesRestOfPage passed.
    Test SparkFlashCanWriteEvenAddressBytes passed.
    Test SparkFlashCanWriteEvenAddressBytesOddLength passed.
    Test SparkFlashCanWriteOddBytes passed.
    Test SuccessiveWrittenValuesAreAnded passed.
    Test WriteDistinctValueToPages passed.
    Test summary: 15 passed, 0 failed, and 0 skipped, out of 15 test(s).


Implementation Details
======================

Development
-----------
I developed the library initially as a standalone library compiled on regular
g++ on my desktop. Compared to embedded development, this allowed a
faster development cycle and easier debugging. For testing, the flash
memory was faked using a `FakeFlashDevice` class that emulated a flash
device in memory (ANDed writes, page erases and read/write only on
even address/even length.)


Emulation Layers
----------------
The library provides a random read/write access device, similar to
EEPROM from the external flash on the spark. It's implemented as one or more layers on
top of the flash API. These layers take care of page erases and provide multiple
writes to the same logical address. It does this through two key techniques:

 * redundant storage - multiple writes to the same logical address are
implemented as writes to distinct physical addresses in flash.

 * wear levelling: a mapping from logical pages to physical pages is
maintained. When a logical page is erased, it is assigned to a new free
physical page (using an even random distribution). This ensures updates
to a single page are spread out over the area of flash allocated, so the
wear is spread out across many pages rather than just one.


For EEPROM-like storage, there are several implementations that
provide the same API (so they can be used interchangably), with each
implementation providing a different trade-off between storage efficiency
and the maximum erase wear for any given page.

### Direct flash

This is simply direct access to the flash memory. Automatic erase before
write is not supported. On construction, the range of pages in flash to be
used is specified. This doesn't provide EEPROM semantics, but rather
provide the base storage for the implementations that follow.

The following implementations provide EEPROM-like semantics,
supporting free read/write to addresses. Despite the complexity of some
of the schemes, the client simply sees a linear address range that they
can read and write to, and doesn't need to concernt themselves so much
with the internal workings. To provide EEPROM-like semantics, the
implementations take care of performing erases when they are
necessary. This is typically when a destructive write is performed - writing
a 1 bit to a location where there was previously a 0.

Direct with erase copy
^^^^^^^^^^^^^^^^^^^^^^
This scheme stores the data directly in flash at the address
specified. When a page erase is needed to reset 0 bits back to 1, the page
is copied to a reserved page in flash. The original page is then erased
and the data copied back.

This makes the most efficient use of space in terms of storage used, but
at the cost of considerable wear on the reserved page, which is erased for every
single erasure on any page, since every write that requires an erase will erase
the reserved page.

This implementation is recommended for cases when the data changes
less than 10^5 times during the lifetime of the system (the maximum wear
for a page in external flash being 10^5.)

### Wear Levelled storage

This uses a specified region of flash where logical pages are mapped to
their actual page location in flash. This allows the actual location of a logical
address to be changed, such as when erasing a page. When a logical page is
erased, it is assigned to a new physical page in flash. This distributes the
wear over all free and changing pages. This happens transparently - to
the user they simply see the same API - `FlashDevice`.

This scheme reserves 2 bytes per page for housekeeping. There is also 1 page used for
housekeeping, at at a minimum there must be at least 1 free page. Initially all pages are free
until data is written to the device. When a page is erased, it is mapped to a different
physical page, reducing the wear per page.

For example, if you have reserved region in flash, and usage is such that
a single page takes most of the changes, then allocating 10 free pages
will ensure those changes are spread out over 10+1 pages (the erased
page itself is freed so becomes part of the free pool.). The number of
erases for any given page can then as high as 10^6.  The EEPROM in
the arduino allows 10^5 erases, this is already surpassing eeprom wear
levels.

Redundant Storage
^^^^^^^^^^^^^^^^^
This scheme allows multiple destructive writes to the same logical
address without requiring an erase in the underlying flash. It achieves this by
representing each logical byte as a 8-byte slot. Data is written until the slot
is full, and then an erase is required. This scheme supports 7 destructive
writes before the page is erased. Note that when a page is erased, all
addresses are reset back to the 7 write capacity.

This is naturally the least efficient in terms of storage, but also offers the
least wear, reducing the number of erases required by approximately an
order of magnitude. When combined with the wear levelled storage,
destructive writes in the order of 10^8 can be achieved over the lifetime
of the device.

### Combining Layers

All the implementations of the eeprom emulation expose the same interface, and the higher level
storage schemes (Wear Levelled storage/Redundant storage) also
expect an implementation of that interface as their base storage. This
allows the implementations to be stacked, effectively combining them.

There two utility devices: `PageSpanFlashDevice` and `FlashDeviceRegion`
that make working with devices easier. `PageSpanFlashDevice` allows the
various read and write methods to accept a buffer that is larger than a
page or allow reads and writes to span page boundaries.
FlashDeviceRegion provides a flash device that maps to a contiguous
subregion in some other flash device. This is used to partition a given
device into distinct areas.

These utility devices are coded to the FlashDevice interface.
Consequently, they can be used both with the physical flash device, as
well as any of the higher level layers. Typically, the PageSpanFlashDevice
is placed on the top-level device that the client uses, so the client is free
to read arbitrary blocks. The FlashDeviceRegion is used to divide up the
physical flash memory into distinct areas.

