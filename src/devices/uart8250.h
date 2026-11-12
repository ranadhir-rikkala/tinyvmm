#pragma once

#include <cstdint>
#include <deque>
#include <functional>

namespace tvmm {

class Uart8250 {
 public:
  using Sink = std::function<void(std::uint8_t)>;

  enum Reg : std::uint8_t {
    kData = 0,
    kIer = 1,
    kIirFcr = 2,
    kLcr = 3,
    kMcr = 4,
    kLsr = 5,
    kMsr = 6,
    kScr = 7,
  };

  static constexpr std::uint8_t kIerRdi = 0x01;
  static constexpr std::uint8_t kIerThri = 0x02;
  static constexpr std::uint8_t kIirNoInt = 0x01;
  static constexpr std::uint8_t kIirThri = 0x02;
  static constexpr std::uint8_t kIirRdi = 0x04;
  static constexpr std::uint8_t kIirFifoBits = 0xc0;
  static constexpr std::uint8_t kLcrDlab = 0x80;
  static constexpr std::uint8_t kMcrLoop = 0x10;
  static constexpr std::uint8_t kLsrDataReady = 0x01;
  static constexpr std::uint8_t kLsrThre = 0x20;
  static constexpr std::uint8_t kLsrTemt = 0x40;

  explicit Uart8250(Sink sink) : sink_(std::move(sink)) {}

  std::uint8_t Read(std::uint8_t offset);
  void Write(std::uint8_t offset, std::uint8_t value);
  void EnqueueInput(std::uint8_t byte);

  bool interrupt_line() const;
  bool TakeInterruptEdge();

  std::uint16_t divisor() const { return static_cast<std::uint16_t>(dlm_ << 8 | dll_); }

 private:
  void UpdateLine();

  Sink sink_;
  std::deque<std::uint8_t> rx_;
  std::uint8_t ier_ = 0;
  std::uint8_t lcr_ = 0x03;
  std::uint8_t mcr_ = 0x08;
  std::uint8_t scr_ = 0;
  std::uint8_t dll_ = 0x0c;
  std::uint8_t dlm_ = 0;
  bool fifo_enabled_ = false;
  bool thre_pending_ = false;
  bool line_ = false;
  bool edge_ = false;
};

}  // namespace tvmm
