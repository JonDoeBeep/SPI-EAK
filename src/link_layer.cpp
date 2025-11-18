#include "link_layer.h"

#include <stdexcept>

namespace spi_eak {

namespace {
uint16_t crc16_ccitt(const uint8_t* data, std::size_t length) {
    uint16_t crc = 0xFFFF;
    for (std::size_t idx = 0; idx < length; ++idx) {
        uint8_t byte = data[idx];
        crc ^= static_cast<uint16_t>(byte) << 8;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x8000) {
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

uint16_t crc16_ccitt(const std::vector<uint8_t>& data) {
    return crc16_ccitt(data.data(), data.size());
}

void appendEscaped(std::vector<uint8_t>& frame,
                   uint8_t value,
                   uint8_t escape_byte,
                   uint8_t start_byte,
                   uint8_t stop_byte) {
    if (value == escape_byte || value == start_byte || value == stop_byte) {
        frame.push_back(escape_byte);
        frame.push_back(static_cast<uint8_t>(value ^ 0x20));
    } else {
        frame.push_back(value);
    }
}
}

FrameCodec::Result FrameCodec::encode(const std::vector<uint8_t>& payload,
                                      const Parameters& params) {
    Result result;
    if (params.start_byte == params.stop_byte) {
        result.ok = false;
        result.error = EncodeError::InvalidStartStop;
        return result;
    }
    if (params.escape_byte == params.start_byte || params.escape_byte == params.stop_byte) {
        result.ok = false;
        result.error = EncodeError::InvalidEscape;
        return result;
    }

    result.frame.clear();
    result.frame.reserve(payload.size() + 6); // rough guess for escapes + crc
    result.frame.push_back(params.start_byte);

    for (uint8_t byte : payload) {
        appendEscaped(result.frame, byte, params.escape_byte, params.start_byte, params.stop_byte);
    }

    if (params.enable_crc16) {
        uint16_t crc = crc16_ccitt(payload);
        appendEscaped(result.frame, static_cast<uint8_t>((crc >> 8) & 0xFF),
                      params.escape_byte, params.start_byte, params.stop_byte);
        appendEscaped(result.frame, static_cast<uint8_t>(crc & 0xFF),
                      params.escape_byte, params.start_byte, params.stop_byte);
    }

    result.frame.push_back(params.stop_byte);
    return result;
}

FrameDecoder::FrameDecoder()
    : options_(Options{}) {}

FrameDecoder::FrameDecoder(const Options& options)
    : options_(options) {}

FrameDecoder::Result FrameDecoder::push(uint8_t byte, std::vector<uint8_t>& out_frame) {
    Result result;

    if (byte == options_.params.start_byte) {
        buffer_.clear();
        in_frame_ = true;
        escape_next_ = false;
        return result;
    }

    if (!in_frame_) {
        return result;
    }

    if (byte == options_.params.stop_byte) {
        const uint8_t* payload_ptr = buffer_.data();
        size_t payload_size = buffer_.size();

        if (options_.params.enable_crc16) {
            if (buffer_.size() < 2) {
                reset();
                result.frame_dropped = true;
                result.drop_reason = Result::DropReason::TooShortForCrc;
                return result;
            }
            payload_size -= 2;
            uint16_t received_crc = static_cast<uint16_t>(buffer_[payload_size]) << 8;
            received_crc |= buffer_[payload_size + 1];
            if (crc16_ccitt(payload_ptr, payload_size) != received_crc) {
                reset();
                result.frame_dropped = true;
                result.drop_reason = Result::DropReason::CrcMismatch;
                return result;
            }
        }

        out_frame.assign(payload_ptr, payload_ptr + payload_size);
        reset();
        result.frame_ready = true;
        return result;
    }

    if (escape_next_) {
        if (options_.max_frame_bytes && buffer_.size() >= options_.max_frame_bytes) {
            reset();
            result.frame_dropped = true;
            result.drop_reason = Result::DropReason::FrameTooLarge;
            escape_next_ = false;
            return result;
        }
        buffer_.push_back(static_cast<uint8_t>(byte ^ 0x20));
        escape_next_ = false;
        return result;
    }

    if (byte == options_.params.escape_byte) {
        escape_next_ = true;
        return result;
    }

    if (options_.max_frame_bytes && buffer_.size() >= options_.max_frame_bytes) {
        reset();
        result.frame_dropped = true;
        result.drop_reason = Result::DropReason::FrameTooLarge;
        return result;
    }
    buffer_.push_back(byte);
    return result;
}

void FrameDecoder::reset() {
    in_frame_ = false;
    escape_next_ = false;
    buffer_.clear();
}

} // namespace spi_eak
