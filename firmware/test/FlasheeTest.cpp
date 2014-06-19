#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "flashee-eeprom.h"


TEST(FlasheeTest, CanCreateCircularBuffer) {
    CircularBuffer* buf = Flashee::createCircularBuffer(0, 4096*10);
    ASSERT_TRUE(buf!=NULL);
    delete buf;
}

TEST(FlasheeTest, CanCreateAddressEraseFull) {
    FlashDevice* dev = Flashee::createAddressErase();
    ASSERT_TRUE(dev!=NULL);
    delete dev;
}

TEST(FlasheeTest, CanCreateAddressEraseSegment) {
    FlashDevice* dev = Flashee::createAddressErase(4096*20, 4096*100);
    ASSERT_TRUE(dev!=NULL);
    delete dev;
}

TEST(FlasheeTest, CreateAddressEraseNonPageBoundaryFails) {
    FlashDevice* dev = Flashee::createAddressErase(4096*20+50, 4096*100);
    EXPECT_TRUE(dev==NULL);
    delete dev;
}

TEST(FlasheeTest, CanCreateMultiPageEraseFull) {
    FlashDevice* dev = Flashee::createMultiPageErase();
    EXPECT_TRUE(dev!=NULL);
    delete dev;
}

TEST(FlasheeTest, CanCreateMultiPageEraseSegment) {
    FlashDevice* dev = Flashee::createMultiPageErase(4096*20, 4096*40);
    EXPECT_TRUE(dev!=NULL);
    delete dev;
}

TEST(FlasheeTest, CreateMultiPageEraseSegmentNonPageAddressFails) {
    FlashDevice* dev = Flashee::createMultiPageErase(4096*20+20, 4096*40);
    EXPECT_TRUE(dev==NULL);
    delete dev;
}

TEST(FlasheeTest, CreateSinglePageEraseFull) {
    FlashDevice* dev = Flashee::createSinglePageErase(0, Flashee::userFlash().length());
    EXPECT_TRUE(dev!=NULL);
    delete dev;
}

TEST(FlasheeTest, CreateSinglePageEraseSegment) {
    FlashDevice* dev = Flashee::createSinglePageErase(20*4096, 100*4096);
    EXPECT_TRUE(dev!=NULL);
    delete dev;
}

TEST(FlasheeTest, CreateSinglePageEraseNonPageAddressFails) {
    FlashDevice* dev = Flashee::createSinglePageErase(20*4096+20, 100*4096);
    EXPECT_TRUE(dev==NULL);
    delete dev;
}
