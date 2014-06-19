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
- 3 implementations providing speed/erase cycle tradeoff.


Getting Started
===============

To create the flash object:

    FlashDevice* flash = Flashee::createAddressErase();

This allocates all pages in the external flash for writing using a scheme that emulates erases on individual addresses.

To store data:

    flash->writeString("Hello World!", 0);

This writes the string to the flash memory starting at address 0.

To retrieve data,

    char buf[13];
    flash->read(buf, 0, 13);


So far, this is just like the regular sFLASH access functions. Where this library is different is that you can freely
overwrite the data, just by issuing another write command:

    flash->writeString("I think I changed my mind!", 0);


If you're not storing strings but simply data buffers, binary writes are supported:

    flash->write(&my_struct, 123, sizeof(my_struct));




Operating Principle
===================

The library provides a random read/write access device, similar to
EEPROM from the external flash on the spark. It's implemented as a layer on
top of the flash API that takes care of page erases and provides multiple
writes to the same logical address. It does this through two key
techniques:

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

Direct with erase copy:
This scheme stores the data directly in directly flash at the location you
specify. When a page erase is needed to reset 0 bits back to 1, the page
is copied - either to a memory buffer (if there is sufficient free memory
available) or to a reserved page in flash. The original page is then erased
and the data copied back.

Efficiency/Wear
This makes the most efficient use of space in terms of storage used, but
at the cost of considerable wear on pages that are updated frequently,
and even more wear on the reserved page if there is not enough memory
to allocate a 4K buffer, since  every write that requries an erase will erase
the reserved page.

This implementation is recommended for cases when the data changes
less than 10^5 times during the lifetime of the system (the maximum wear
for a page in external flash.)

wear levelled storage:
Uses a specified region of flash where logical pages are mapped to
pages in flash. In the region, specified number of pages are kept free -
typically about 10% of the total allocation size. When a logical page is
erased, it is assigned to a new physical page in flash. This distributes the
wear over all free and changing pages. This happens transparently - to
the user they simply see the same interface (EEPROM-like storage.)

Efficiency/Wear
This scheme reserves 2 bytes per page for housekeeping and then up to
as many free pages as were specified in the constructor. By allocating
more free pages, the wear can be spread out over many more pages,
reducing the wear per page, but of course at the expense of storage
efficiency.

For example, if you have reserved region in flash, and usage is such that
a single page takes most of the changes, then allocating 10 free pages
will ensure those changes are spread out over 10+1 pages (the erased
page itself is freed so becomes part of the free pool.). The number of
erases for any given page can then as high as 10^6.  The EEPROM in
the arduino allows 10^5 erases, this is already surpassing eeprom wear
levels.

Redundant storage
This scheme allows multiple destructive writes to the same logical
address without requiring an erase in the underlying flash. It does this by
representing each logical byte as a 8-byte slot. Data is written until the slot
is full, and then an erase is required. This scheme supports 7 destructive
writes before the page is erased. Note that when a page is erased, all
addresses are reset back to the 7 write capacity.

This is naturally the least efficient in terms of storage, but also offers the
least wear, reducing the number of erases required by approximately an
order of magnitude. When combined with the wear levelled storage,
destructive writes in the order of 10^8 can be achieved over the lifetime
of the device.

Combining
All the implementations expose the same interface, and the higher level
storage schemes (Wear Levelled storage/Redundant storage) also
expect an implementation of that interface as their base storage. This
allows the implementations to be stacked, effectively combining them.

There two utilitiy devices: PageSpanFlashDevice and FlashDeviceRegion
that make working with devices easier. PageSpanFlashDevice allows the
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


Development

I developed the library initially as a standalone library compiled on regular
g++ on my desktop. Compared to embedded development, this allowed a
faster development cycle and easier debugging. For testing, the flash
memory was faked using a FakeFlashDevice class that emulated a flash
device in memory (ANDed writes, page erases and read/write only on
even address/even length.)

The library is tested with Google Test and Google Mock.



Testing
=======
This Flashee-Eeprom library contains unit tests and integration tests.

Unit tests are compiled using GCC and run on the host platform.
Integration tests are executed on the embedded device.


Building Unit Tests
-------------------

The @firmware/test@ folder contains the unit test code, which is compiled using GCC and run on your desktop OS.

On windows, using MinGW is recommended.



