#include "Comms.h"

#include <cstring>
#include <type_traits>
#include <utility>

namespace {
constexpr uint32_t kDefaultRxBufferSize = 256;  //!< Generous RX buffer for bursty packets

template <typename... Ts>
using void_t = void;

template <typename T, typename = void>
struct HasDirectBuffers : std::false_type {};

template <typename T>
struct HasDirectBuffers<T, void_t<decltype(std::declval<T>().txBuff),
                                    decltype(std::declval<T>().rxBuff)>>
    : std::true_type {};

template <typename T, typename = void>
struct HasPacketBuffers : std::false_type {};

template <typename T>
struct HasPacketBuffers<
    T, void_t<decltype(std::declval<T>().packet.txBuff),
              decltype(std::declval<T>().packet.rxBuff)>> : std::true_type {};

template <typename Transfer, typename Enable = void>
struct SerialTransferBufferAccessor;

template <typename Transfer>
struct SerialTransferBufferAccessor<Transfer,
                                    typename std::enable_if<
                                        HasDirectBuffers<Transfer>::value>::type> {
  static uint8_t* tx(Transfer& transfer) { return transfer.txBuff; }
  static const uint8_t* rx(const Transfer& transfer) { return transfer.rxBuff; }
};

template <typename Transfer>
struct SerialTransferBufferAccessor<
    Transfer,
    typename std::enable_if<!HasDirectBuffers<Transfer>::value &&
                            HasPacketBuffers<Transfer>::value>::type> {
  static uint8_t* tx(Transfer& transfer) { return transfer.packet.txBuff; }
  static const uint8_t* rx(const Transfer& transfer) {
    return transfer.packet.rxBuff;
  }
};

template <typename Transfer>
struct SerialTransferBufferAccessor<
    Transfer,
    typename std::enable_if<!HasDirectBuffers<Transfer>::value &&
                            !HasPacketBuffers<Transfer>::value>::type> {
  static uint8_t* tx(Transfer&) {
    static_assert(sizeof(Transfer) == 0,
                  "SerialTransfer buffer access unsupported");
    return nullptr;
  }
  static const uint8_t* rx(const Transfer&) { return nullptr; }
};

using TransferBuffers = SerialTransferBufferAccessor<SerialTransfer>;
}  // namespace

Comms::Comms() = default;

bool Comms::begin(HardwareSerial& serial, int rxPin, int txPin, uint32_t baud) {
  serial_ = &serial;
  serial_->begin(baud, SERIAL_8N1, rxPin, txPin);
  serial_->setRxBufferSize(kDefaultRxBufferSize);
  serial_->setTimeout(0);  // fully non-blocking

  transfer_.begin(*serial_);

  started_ = true;
  linkState_ = LinkState::kIdle;
  lastHeartbeatSentMs_ = millis();
  lastHeartbeatSeenMs_ = 0;
  lastTxMs_ = 0;
  lastRxMs_ = 0;
  lastError_ = Error::kNone;
  lastTransferStatus_ = 0;
  stats_ = {};

  return true;
}

void Comms::end() {
  if (!started_) {
    return;
  }
  serial_->end();
  started_ = false;
  linkState_ = LinkState::kIdle;
}

void Comms::setCallbacks(const Callbacks& callbacks) {
  callbacks_ = callbacks;
}

void Comms::setHeartbeatInterval(uint32_t intervalMs) {
  heartbeatIntervalMs_ = intervalMs;
}

void Comms::setHeartbeatTimeout(uint32_t timeoutMs) {
  heartbeatTimeoutMs_ = timeoutMs;
}

bool Comms::send(const Packet& packet) {
  return send(packet.channel, packet.data, packet.size);
}

bool Comms::send(uint8_t channel, const uint8_t* data, size_t length) {
  return sendFrame(FrameType::kData, channel, data, length);
}

void Comms::update() {
  if (!started_) {
    return;
  }

  const uint32_t now = millis();

  if (heartbeatIntervalMs_ > 0 && (now - lastHeartbeatSentMs_) >= heartbeatIntervalMs_) {
    sendHeartbeat(now);
  }

  uint16_t bytesAvailable = transfer_.available();
  while (bytesAvailable > 0) {
    handleIncoming(bytesAvailable);
    bytesAvailable = transfer_.available();
  }

  if (bytesAvailable == 0 && transfer_.status < 0) {
    handleErrorStatus(transfer_.status);
  }

  updateLinkState(now);
}

void Comms::clearError() {
  lastError_ = Error::kNone;
  lastTransferStatus_ = 0;
}

bool Comms::sendFrame(FrameType type, uint8_t channel, const uint8_t* payload, size_t length) {
  if (!started_) {
    return false;
  }

  if (length > kMaxPayloadSize) {
    lastError_ = Error::kPayloadTooLarge;
    stats_.payloadErrors++;
    if (callbacks_.onError) {
      callbacks_.onError(lastError_, 0, callbacks_.context);
    }
    return false;
  }

  if (length > 0 && payload == nullptr) {
    lastError_ = Error::kInvalidPayload;
    stats_.payloadErrors++;
    if (callbacks_.onError) {
      callbacks_.onError(lastError_, 0, callbacks_.context);
    }
    return false;
  }

  uint8_t* txBuffer = TransferBuffers::tx(transfer_);
  txBuffer[0] = static_cast<uint8_t>(type);
  txBuffer[1] = channel;
  txBuffer[2] = static_cast<uint8_t>(length);
  if (length > 0) {
    memcpy(&txBuffer[kFrameOverhead], payload, length);
  }

  transfer_.sendData(static_cast<uint16_t>(length + kFrameOverhead));
  lastTxMs_ = millis();

  if (type == FrameType::kHeartbeat) {
    stats_.heartbeatsTx++;
    lastHeartbeatSentMs_ = lastTxMs_;
  } else {
    stats_.packetsTx++;
  }
  return true;
}

void Comms::handleIncoming(uint16_t size) {
  if (size < kFrameOverhead) {
    stats_.payloadErrors++;
    lastError_ = Error::kSerialTransfer;
    if (callbacks_.onError) {
      callbacks_.onError(lastError_, transfer_.status, callbacks_.context);
    }
    return;
  }

  const uint8_t* rxBuffer = TransferBuffers::rx(transfer_);
  const FrameType type = static_cast<FrameType>(rxBuffer[0]);
  const uint8_t channel = rxBuffer[1];
  const uint8_t payloadSize = rxBuffer[2];

  if (payloadSize > kMaxPayloadSize || payloadSize != size - kFrameOverhead) {
    stats_.payloadErrors++;
    lastError_ = Error::kSerialTransfer;
    if (callbacks_.onError) {
      callbacks_.onError(lastError_, transfer_.status, callbacks_.context);
    }
    return;
  }

  lastRxMs_ = millis();
  recordSuccessfulTransfer();

  if (type == FrameType::kHeartbeat) {
    lastHeartbeatSeenMs_ = lastRxMs_;
    stats_.heartbeatsRx++;
    if (callbacks_.onHeartbeat) {
      callbacks_.onHeartbeat(callbacks_.context);
    }
    return;
  }

  if (type != FrameType::kData) {
    // Unknown frame type -> treat as payload error
    stats_.payloadErrors++;
    lastError_ = Error::kSerialTransfer;
    if (callbacks_.onError) {
      callbacks_.onError(lastError_, transfer_.status, callbacks_.context);
    }
    return;
  }

  lastHeartbeatSeenMs_ = lastRxMs_;

  Packet packet;
  packet.channel = channel;
  packet.size = payloadSize;
  if (packet.size > 0) {
    memcpy(packet.data, &rxBuffer[kFrameOverhead], packet.size);
  }
  stats_.packetsRx++;

  if (callbacks_.onPacket) {
    callbacks_.onPacket(packet, callbacks_.context);
  }
}

void Comms::handleErrorStatus(int16_t status) {
  lastTransferStatus_ = status;
  lastError_ = Error::kSerialTransfer;
  stats_.crcErrors++;
  if (callbacks_.onError) {
    callbacks_.onError(lastError_, status, callbacks_.context);
  }
}

void Comms::recordSuccessfulTransfer() {
  lastTransferStatus_ = 0;
  if (lastError_ == Error::kSerialTransfer) {
    lastError_ = Error::kNone;
  }
}

void Comms::sendHeartbeat(uint32_t now) {
  sendFrame(FrameType::kHeartbeat, 0, nullptr, 0);
  lastHeartbeatSentMs_ = now;
}

void Comms::updateLinkState(uint32_t now) {
  if (!started_) {
    linkState_ = LinkState::kIdle;
    return;
  }

  if (lastError_ == Error::kSerialTransfer && lastTransferStatus_ < 0) {
    linkState_ = LinkState::kSerialError;
    return;
  }

  if (heartbeatTimeoutMs_ == 0) {
    linkState_ = LinkState::kActive;
    return;
  }

  if (lastHeartbeatSeenMs_ == 0) {
    linkState_ = LinkState::kIdle;
    return;
  }

  if ((now - lastHeartbeatSeenMs_) > heartbeatTimeoutMs_) {
    if (linkState_ != LinkState::kTimedOut) {
      lastError_ = Error::kHeartbeatLost;
      if (callbacks_.onError) {
        callbacks_.onError(lastError_, 0, callbacks_.context);
      }
    }
    linkState_ = LinkState::kTimedOut;
    return;
  }

  linkState_ = LinkState::kActive;
}

