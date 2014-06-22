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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "flashee-eeprom.h"

using namespace Flashee;

// NB: The FakeFlashDevice is initially random content.
// And the LogicalPageMapperImpl doesn't initialize the storage either.
TEST(LogicalPageMapperTest, PageIsDurty_Small) {
    FakeFlashDevice fake(6, 50);
    fake.eraseAll();
    LogicalPageMapperImpl<> mapper(fake, 5);
    ASSERT_FALSE(mapper.pageIsDirty(0));
}

TEST(LogicalPageMapperTest, PageIsDurty_Large) {
    // this catches a bug
    FakeFlashDevice fake(6, STACK_BUFFER_SIZE*2+50);
    fake.eraseAll();
    LogicalPageMapperImpl<> mapper(fake, 5);
    ASSERT_FALSE(mapper.pageIsDirty(0));
}

TEST(LogicalPageMapperTest, MaxPage) {
    FakeFlashDevice fake(6, 50);
    LogicalPageMapperImpl<> mapper(fake, 5);
    ASSERT_EQ(mapper.maxPage(), 5);
}

TEST(LogicalPageMapperTest, LogicalPageCount) {
    FakeFlashDevice fake(6, 50);
    LogicalPageMapperImpl<> mapper(fake, 5);
    ASSERT_EQ(mapper.logicalPageCount, 5);
}

TEST(LogicalPageMapperTest, LogicalPageMapInitialized) {
    FakeFlashDevice fake(40, 50);
    LogicalPageMapperImpl<> mapper(fake, 20);
    mapper.formatIfNeeded();
    mapper.buildInUseMap();
    for (int i=0; i<mapper.logicalPageCount; i++) {
        ASSERT_LE(mapper.logicalPageMap[i], 39) << "logical page " << i << " maps to physical page " << mapper.logicalPageMap[i];
    }
}

TEST(LogicalPageMapperTest, WriteHeader) {
    FakeFlashDevice fake(40, 50);
    LogicalPageMapperImpl<> mapper(fake, 20);    
    mapper.formatIfNeeded();    
    mapper.writeHeader(5, 0x1234);
    
    uint16_t value;
    ASSERT_TRUE(fake.read(&value, fake.pageAddress(5), sizeof(value)));
    ASSERT_EQ(0x1234, value);
}

TEST(LogicalPageMapperTest, FormatWritesHeaderID) {
    FakeFlashDevice fake(40, 50);           // random content
    LogicalPageMapper<> mapper(fake, 20);   // ensure the last page is erased

    uint16_t value;
    ASSERT_TRUE(fake.read(&value, fake.pageAddress(39), sizeof(value)));
    ASSERT_EQ(0x2FFFu, value);    
}

TEST(LogicalPageMapperTest, LogicalMappingIsPersisted) {    
    FakeFlashDevice fake(40, 50);
    LogicalPageMapper<> mapper(fake, 20);

    const char* msg = "Hello";
    ASSERT_TRUE(mapper.writeString(msg, 75));
    
    char buf[10];       // sanity test - read that it was written.
    ASSERT_TRUE(mapper.readPage(buf, 75, sizeof(buf)));
    ASSERT_STREQ(buf, msg) << "string not correctly written";
    
    // now allocate a new mapper in the same storage
    LogicalPageMapper<> mapper2(fake, 20);
    
    memset(buf, 0, sizeof(buf));
    ASSERT_TRUE(mapper.readPage(buf, 75, sizeof(buf)));
    ASSERT_STREQ(buf, msg) << "expected data to be unchanged in original mapper.";

    memset(buf, 0, sizeof(buf));
    ASSERT_TRUE(mapper2.readPage(buf, 75, sizeof(buf)));
    ASSERT_STREQ(buf, msg) << "expected data to be persisted for use by second mapper";

}