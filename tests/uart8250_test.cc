#include "devices/uart8250.h"

#include <gtest/gtest.h>

#include <string>

namespace tvmm {
namespace {

class UartTest : public ::testing::Test {
 protected:
  std::string out_;
  Uart8250 uart_{[this](std::uint8_t b) { out_ += static_cast<char>(b); }};
};

TEST_F(UartTest, TransmitsToSink) {
  for (char c : std::string("hi\n")) uart_.Write(Uart8250::kData, static_cast<std::uint8_t>(c));
  EXPECT_EQ(out_, "hi\n");
}

TEST_F(UartTest, LineStatusAlwaysReadyToTransmit) {
  EXPECT_EQ(uart_.Read(Uart8250::kLsr), Uart8250::kLsrThre | Uart8250::kLsrTemt);
}

TEST_F(UartTest, ScratchRegisterHoldsValue) {
  uart_.Write(Uart8250::kScr, 0x5a);
  EXPECT_EQ(uart_.Read(Uart8250::kScr), 0x5a);
}

TEST_F(UartTest, DivisorLatchAccessBitRedirectsDataAndIer) {
  uart_.Write(Uart8250::kLcr, Uart8250::kLcrDlab | 0x03);
  uart_.Write(Uart8250::kData, 0x01);
  uart_.Write(Uart8250::kIer, 0x00);
  EXPECT_EQ(uart_.divisor(), 1);
  EXPECT_EQ(uart_.Read(Uart8250::kData), 0x01);
  uart_.Write(Uart8250::kLcr, 0x03);
  EXPECT_EQ(uart_.Read(Uart8250::kIer), 0);
  EXPECT_TRUE(out_.empty());
}

TEST_F(UartTest, IerKeepsOnlyValidBits) {
  uart_.Write(Uart8250::kIer, 0xff);
  EXPECT_EQ(uart_.Read(Uart8250::kIer), 0x0f);
}

TEST_F(UartTest, LoopbackPassesLinuxProbe) {
  uart_.Write(Uart8250::kMcr, Uart8250::kMcrLoop | 0x0a);
  EXPECT_EQ(uart_.Read(Uart8250::kMsr) & 0xf0, 0x90);
  uart_.Write(Uart8250::kData, 'x');
  EXPECT_TRUE(out_.empty());
  EXPECT_TRUE(uart_.Read(Uart8250::kLsr) & Uart8250::kLsrDataReady);
  EXPECT_EQ(uart_.Read(Uart8250::kData), 'x');
  EXPECT_FALSE(uart_.Read(Uart8250::kLsr) & Uart8250::kLsrDataReady);
}

TEST_F(UartTest, ModemStatusWithoutLoopback) {
  uart_.Write(Uart8250::kMcr, 0x0b);
  EXPECT_EQ(uart_.Read(Uart8250::kMsr), 0xb0);
}

TEST_F(UartTest, IirReportsFifoBitsOnlyWhenEnabled) {
  EXPECT_EQ(uart_.Read(Uart8250::kIirFcr), Uart8250::kIirNoInt);
  uart_.Write(Uart8250::kIirFcr, 0x01);
  EXPECT_EQ(uart_.Read(Uart8250::kIirFcr), Uart8250::kIirFifoBits | Uart8250::kIirNoInt);
}

TEST_F(UartTest, EnablingThriRaisesOneEdgeClearedByIirRead) {
  EXPECT_FALSE(uart_.TakeInterruptEdge());
  uart_.Write(Uart8250::kIer, Uart8250::kIerThri);
  EXPECT_TRUE(uart_.interrupt_line());
  EXPECT_TRUE(uart_.TakeInterruptEdge());
  EXPECT_FALSE(uart_.TakeInterruptEdge());
  EXPECT_EQ(uart_.Read(Uart8250::kIirFcr), Uart8250::kIirThri);
  EXPECT_FALSE(uart_.interrupt_line());
  EXPECT_EQ(uart_.Read(Uart8250::kIirFcr), Uart8250::kIirNoInt);
}

TEST_F(UartTest, TransmitWithThriRaisesNewEdge) {
  uart_.Write(Uart8250::kIer, Uart8250::kIerThri);
  ASSERT_TRUE(uart_.TakeInterruptEdge());
  uart_.Read(Uart8250::kIirFcr);
  uart_.Write(Uart8250::kData, 'a');
  EXPECT_TRUE(uart_.TakeInterruptEdge());
  uart_.Write(Uart8250::kData, 'b');
  EXPECT_FALSE(uart_.TakeInterruptEdge());
  EXPECT_EQ(out_, "ab");
}

TEST_F(UartTest, LinuxThreProbeDoesNotSeeBrokenUart) {
  uart_.Write(Uart8250::kIer, Uart8250::kIerThri);
  const auto iir1 = uart_.Read(Uart8250::kIirFcr);
  uart_.Write(Uart8250::kIer, 0);
  uart_.Write(Uart8250::kIer, Uart8250::kIerThri);
  const auto iir = uart_.Read(Uart8250::kIirFcr);
  uart_.Write(Uart8250::kIer, 0);
  EXPECT_FALSE(iir1 & Uart8250::kIirNoInt);
  EXPECT_FALSE(iir & Uart8250::kIirNoInt);
}

TEST_F(UartTest, ReceiveInterruptHasPriority) {
  uart_.Write(Uart8250::kIer, Uart8250::kIerThri | Uart8250::kIerRdi);
  uart_.EnqueueInput('z');
  EXPECT_EQ(uart_.Read(Uart8250::kIirFcr), Uart8250::kIirRdi);
  EXPECT_EQ(uart_.Read(Uart8250::kData), 'z');
  EXPECT_EQ(uart_.Read(Uart8250::kIirFcr), Uart8250::kIirThri);
}

TEST_F(UartTest, FifoClearDropsPendingInput) {
  uart_.EnqueueInput('q');
  uart_.Write(Uart8250::kIirFcr, 0x03);
  EXPECT_FALSE(uart_.Read(Uart8250::kLsr) & Uart8250::kLsrDataReady);
}

}  // namespace
}  // namespace tvmm
