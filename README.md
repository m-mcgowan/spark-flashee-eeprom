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

- provides persistent storage using the external 1.5MB user flash on the spark
- EEPROM-like access - no need to worry about erasing pages to ensure data integrity.
- 3 different types of eeprom emulations providing speed/erase cycle tradeoff.
- wear levelling and page allocation on demand

Getting Started
===============

To create the flash object:

    FlashDevice* flash = Flashee::createAddressErase();

This allocates all pages in the external flash for writing using a scheme that emulates erases on individual addresses.

To store data:

    flash->writeString("Hello World!", 0);

This writes the string to the flash memory starting at address 0.

To retrieve data:

    char buf[13];
    flash->read(buf, 0, 13);

So far, this is just like the regular sFLASH access functions that are available in the spark core API.
Where this library is different is that you can freely
overwrite the data, just by issuing another write command:

    flash->writeString("I think I changed my mind!", 0);

If you're not storing strings but simply data buffers, binary writes are supported:

    flash->write(&my_struct, 123, sizeof(my_struct));


The key difference between flash and eeprom is that with flash memory you cannot erase a single byte, but have to
erase a whole page. This library takes care of that, and presents an interface to the flash device that makes it
behave like eeprom.

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

# clone this repo to your desktop machine
# (if running Windows, be sure to install MinGW installed also.)
# cd to flashee-eeprom/firmware/test
# run `make test`

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
    [==========] 66 tests from 8 test cases ran. (1714 ms total)
    [  PASSED  ] 66 tests.


Integration Tests
-----------------
The integration tests combine the real external flash device in the spark core with the eeprom emulation layers.
The aim of the integration test is to test that the library functions as a whole.

The integration test is available from the online IDE, under `examples/integration-test.cpp`.

To run the integration test:
# build and flash integration-test-cpp to the core
# use a serial monitor to connect to the core locally via the USB serial interface
# press 't' to start the tests.

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
    Test ok passed.
    Test summary: 15 passed, 0 failed, and 0 skipped, out of 15 test(s).


Development
===========
I developed the library initially as a standalone library compiled on regular
g++ on my desktop. Compared to embedded development, this allowed a
faster development cycle and easier debugging. For testing, the flash
memory was faked using a `FakeFlashDevice` class that emulated a flash
device in memory (ANDed writes, page erases and read/write only on
even address/even length.)



Emulation Layers
================

The library provides a random read/write access device, similar to
EEPROM from the external flash on the spark. It's implemented as one or more layers on
top of the flash API. These layers take care of page erases and provide multiple
writes to the same logical address. It does this through two key techniques:

 - redundant storage - multiple writes to the same logical address are
implemented as writes to distinct physical addresses in flash.

 - wear levelling: a mapping from logical pages to physical pages is
maintained. When a logical page is erased, it is assigned to a new free
physical page (using an even random distribution). This ensures updates
to a single page are spread out over the area of flash allocated, so the
wear is spread out across many pages rather than just one.

For EEPROM-like storage, there are several implementations that
provide the same API (so they can be used interchangably), with each
implementation providing a different trade-off between storage efficiency
and the maximum erase wear for any given page.

Direct flash
------------
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
----------------------
This scheme stores the data directly in flash at the location you
specify. When a page erase is needed to reset 0 bits back to 1, the page
is copied - either to a memory buffer (if there is sufficient free memory
available) or to a reserved page in flash. The original page is then erased
and the data copied back.

This makes the most efficient use of space in terms of storage used, but
at the cost of considerable wear on pages that are updated frequently,
and even more wear on the reserved page if there is not enough memory
to allocate a 4K buffer, since  every write that requries an erase will erase
the reserved page.

This implementation is recommended for cases when the data changes
less than 10^5 times during the lifetime of the system (the maximum wear
for a page in external flash.)

Wear Levelled storage
---------------------
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
-----------------
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

Combining Layers
----------------
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





