#include "devices/uart8250.h"

namespace tvmm {

namespace {

constexpr std::uint8_t kIerValidBits = 0x0f;
constexpr std::uint8_t kMcrValidBits = 0x1f;
constexpr std::uint8_t kFcrEnableFifo = 0x01;
constexpr std::uint8_t kFcrClearRx = 0x02;
constexpr std::uint8_t kMsrDefault = 0xb0;

}  // namespace

std::uint8_t Uart8250::Read(std::uint8_t offset) {
  const bool dlab = lcr_ & kLcrDlab;
  std::uint8_t value = 0;
  switch (offset & 7) {
    case kData:
      if (dlab) {
        value = dll_;
      } else if (!rx_.empty()) {
        value = rx_.front();
        rx_.pop_front();
      }
      break;
    case kIer:
      value = dlab ? dlm_ : ier_;
      break;
    case kIirFcr:
      if ((ier_ & kIerRdi) && !rx_.empty()) {
        value = kIirRdi;
      } else if ((ier_ & kIerThri) && thre_pending_) {
        value = kIirThri;
        thre_pending_ = false;
      } else {
        value = kIirNoInt;
      }
      if (fifo_enabled_) value |= kIirFifoBits;
      break;
    case kLcr:
      value = lcr_;
      break;
    case kMcr:
      value = mcr_;
      break;
    case kLsr:
      value = kLsrThre | kLsrTemt | (rx_.empty() ? 0 : kLsrDataReady);
      break;
    case kMsr:
      if (mcr_ & kMcrLoop) {
        value = static_cast<std::uint8_t>(((mcr_ & 0x02) << 3) | ((mcr_ & 0x01) << 5) | ((mcr_ & 0x04) << 4) |
                                          ((mcr_ & 0x08) << 4));
      } else {
        value = kMsrDefault;
      }
      break;
    case kScr:
      value = scr_;
      break;
  }
  UpdateLine();
  return value;
}

void Uart8250::Write(std::uint8_t offset, std::uint8_t value) {
  const bool dlab = lcr_ & kLcrDlab;
  switch (offset & 7) {
    case kData:
      if (dlab) {
        dll_ = value;
      } else {
        if (mcr_ & kMcrLoop) {
          rx_.push_back(value);
        } else if (sink_) {
          sink_(value);
        }
        thre_pending_ = true;
      }
      break;
    case kIer:
      if (dlab) {
        dlm_ = value;
      } else {
        const std::uint8_t next = value & kIerValidBits;
        if ((next & kIerThri) && !(ier_ & kIerThri)) thre_pending_ = true;
        ier_ = next;
      }
      break;
    case kIirFcr:
      fifo_enabled_ = value & kFcrEnableFifo;
      if (value & kFcrClearRx) rx_.clear();
      break;
    case kLcr:
      lcr_ = value;
      break;
    case kMcr:
      mcr_ = value & kMcrValidBits;
      break;
    case kScr:
      scr_ = value;
      break;
    default:
      break;
  }
  UpdateLine();
}

void Uart8250::EnqueueInput(std::uint8_t byte) {
  rx_.push_back(byte);
  UpdateLine();
}

bool Uart8250::interrupt_line() const {
  return ((ier_ & kIerRdi) && !rx_.empty()) || ((ier_ & kIerThri) && thre_pending_);
}

bool Uart8250::TakeInterruptEdge() {
  const bool edge = edge_;
  edge_ = false;
  return edge;
}

void Uart8250::UpdateLine() {
  const bool now = interrupt_line();
  if (now && !line_) edge_ = true;
  line_ = now;
}

}  // namespace tvmm
