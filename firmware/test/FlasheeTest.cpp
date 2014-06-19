#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "flashee-eeprom.h"

using namespace Flashee;

TEST(FlasheeTest, CanCreateCircularBuffer) {
    CircularBuffer* buf = flashee::createCircularBuffer(0, 4096*10);
    ASSERT_TRUE(buf!=NULL);
    delete buf;
}

TEST(FlasheeTest, CanCreateAddressEraseFull) {
    FlashDevice* dev = flashee::createAddressErase();
    ASSERT_TRUE(dev!=NULL);
    delete dev;
}

TEST(FlasheeTest, CanCreateAddressEraseSegment) {
    FlashDevice* dev = flashee::createAddressErase(4096*20, 4096*100);
    ASSERT_TRUE(dev!=NULL);
    delete dev;
}

TEST(FlasheeTest, CreateAddressEraseNonPageBoundaryFails) {
    FlashDevice* dev = flashee::createAddressErase(4096*20+50, 4096*100);
    EXPECT_TRUE(dev==NULL);
    delete dev;
}

TEST(FlasheeTest, CanCreateMultiPageEraseFull) {
    FlashDevice* dev = flashee::createMultiPageErase();
    EXPECT_TRUE(dev!=NULL);
    delete dev;
}

TEST(FlasheeTest, CanCreateMultiPageEraseSegment) {
    FlashDevice* dev = flashee::createMultiPageErase(4096*20, 4096*40);
    EXPECT_TRUE(dev!=NULL);
    delete dev;
}

TEST(FlasheeTest, CreateMultiPageEraseSegmentNonPageAddressFails) {
    FlashDevice* dev = flashee::createMultiPageErase(4096*20+20, 4096*40);
    EXPECT_TRUE(dev==NULL);
    delete dev;
}

TEST(FlasheeTest, CreateSinglePageEraseFull) {
    FlashDevice* dev = flashee::createSinglePageErase(0, flashee::userFlash().length());
    EXPECT_TRUE(dev!=NULL);
    delete dev;
}

TEST(FlasheeTest, CreateSinglePageEraseSegment) {
    FlashDevice* dev = flashee::createSinglePageErase(20*4096, 100*4096);
    EXPECT_TRUE(dev!=NULL);
    delete dev;
}

TEST(FlasheeTest, CreateSinglePageEraseNonPageAddressFails) {
    FlashDevice* dev = flashee::createSinglePageErase(20*4096+20, 100*4096);
    EXPECT_TRUE(dev==NULL);
    delete dev;
}
