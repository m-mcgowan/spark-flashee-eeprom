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

class FatTest : public ::testing::Test {

};


TEST(CreateFSTest, DeviceNotFormattedReturnsErrorCode) {
    ASSERT_TRUE(Devices::userFlash().eraseAll());
    FATFS fs;
    FRESULT result = Devices::createFATRegion(0*4096, 128*4096, &fs, FORMAT_CMD_NONE);
    ASSERT_EQ(FR_NO_FILESYSTEM, result) << "Expected no filesystem on uninitialized flash";
}

TEST(CreateFSTest, DeviceNotFormattedIsFormattedByDefault) {
    ASSERT_TRUE(Devices::userFlash().eraseAll());
    FATFS fs;
    FRESULT result = Devices::createFATRegion(0*4096, 128*4096, &fs);
    ASSERT_EQ(FR_OK, result) << "Expected no filesystem on uninitialized flash";
    
    DIR d; FILINFO i;    
    ASSERT_EQ(FR_OK, f_opendir(&d, ""));
    ASSERT_EQ(FR_OK, f_readdir(&d, &i));
    ASSERT_STREQ("", i.fname);           // no files    
}

TEST(CreateFSTest, DeviceNotFormattedIfAlreadyFormatted) {
    FATFS fs;
    ASSERT_TRUE(Devices::userFlash().eraseAll());
    ASSERT_EQ(FR_OK, Devices::createFATRegion(0*4096, 128*4096, &fs));
    
    FIL fil;    // create a file
    const char* filename = "abc.txt";
    ASSERT_EQ(FR_OK, f_open(&fil, filename, FA_CREATE_NEW));
    ASSERT_EQ(FR_OK, f_close(&fil));
    ASSERT_EQ(FR_OK, f_stat(filename, NULL)) << "expected file to exist after create";
    
    FATFS fs2;
    ASSERT_EQ(FR_OK, Devices::createFATRegion(0*4096, 128*4096, &fs2));
    ASSERT_EQ(FR_OK, f_stat(filename, NULL)) << "expected file to exist after recreating flash region";    
}

TEST(CreateFSTest, DeviceClearedWhenFormatRequested) {
    Devices::userFlash().eraseAll();
    FATFS fs;
    ASSERT_EQ(FR_OK, Devices::createFATRegion(0*4096, 128*4096, &fs));
    
    FIL fil;    // create a file
    const char* filename = "abc.txt";
    ASSERT_EQ(FR_OK, f_open(&fil, filename, FA_CREATE_NEW));
    ASSERT_EQ(FR_OK, f_close(&fil));
    ASSERT_EQ(FR_OK, f_stat(filename, NULL)) << "expected file to exist after create";
    
    FATFS fs2;
    ASSERT_EQ(FR_OK, Devices::createFATRegion(0*4096, 128*4096, &fs2, FORMAT_CMD_FORMAT));
    ASSERT_EQ(FR_NO_FILE, f_stat(filename, NULL)) << "expected file to not exist after recreating and formatting the flash region";    
}


class FSTest : public ::testing::Test {
    FATFS fs; FIL fp; UINT dw;

public:
    FSTest() {        
    }
    
    void SetUp() {
        // format 
        ASSERT_TRUE(Devices::userFlash().eraseAll());
        createFS();
    }
    
    void createFS() {
        memset(&fs, 0, sizeof(fs));
        ASSERT_EQ(FR_OK, Devices::createFATRegion(0*4096, 128*4096, &fs));
    }
    
    void assertCreateFile(const TCHAR* name, const char* c) {
        ASSERT_EQ(FR_OK, f_open(&fp, name, FA_CREATE_NEW|FA_WRITE));            
        ASSERT_EQ(FR_OK, f_write(&fp,c, strlen(c)+1, &dw));        
        ASSERT_EQ(FR_OK, f_close(&fp));

        assertFileExists(name, c);
    }
    
    void assertFileExists(const TCHAR* name, const char* c) {
        SCOPED_TRACE(testing::Message() << "file " << name);
        ASSERT_EQ(FR_OK, f_open(&fp, name, FA_OPEN_EXISTING|FA_READ)) << "expected file to exist";
        char buf[50];
        ASSERT_EQ(FR_OK, f_read(&fp, buf, 50, &dw)) << "could not read from file";
        ASSERT_STREQ(c, buf);
        ASSERT_EQ(FR_OK, f_close(&fp));
    }

    void assertFileNotExists(const TCHAR* name, const char* c) {
        ASSERT_EQ(FR_NO_FILE, f_open(&fp, name, FA_OPEN_EXISTING|FA_READ)) << "expecting file to exist";
    }


};

TEST_F(FSTest, CanCreateFile) {
    assertCreateFile("abcd.txt", "hello world!");
}

TEST_F(FSTest, CreatedFileIsPersisted) {
    assertCreateFile("abcd.txt", "hello world!");    
    assertCreateFile("abc.txt", "foo bar");    

    assertFileExists("abcd.txt", "hello world!");
    assertFileExists("abc.txt", "foo bar");    

    createFS();
    assertFileExists("abcd.txt", "hello world!");
    assertFileExists("abc.txt", "foo bar");    
}

TEST_F(FSTest, CreatedSingleFileIsPersisted) {
    assertCreateFile("abcd.txt", "hello world!");    
    createFS();
    assertFileExists("abcd.txt", "hello world!");
}

