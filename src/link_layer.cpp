#include "link_layer.h"

#include <stdexcept>

namespace {
uint16_t crc16_ccitt(const std::vector<uint8_t>& data) {
    uint16_t crc = 0xFFFF;
    for (uint8_t byte : data) {
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

std::vector<uint8_t> FrameCodec::encode(const std::vector<uint8_t>& payload,
                                         const Parameters& params) {
    if (params.start_byte == params.stop_byte) {
        throw std::invalid_argument("Start and stop bytes must differ");
    }
    if (params.escape_byte == params.start_byte || params.escape_byte == params.stop_byte) {
        throw std::invalid_argument("Escape byte cannot match start/stop bytes");
    }

    std::vector<uint8_t> frame;
    frame.reserve(payload.size() + 6); // rough guess for escapes + crc
    frame.push_back(params.start_byte);

    for (uint8_t byte : payload) {
        appendEscaped(frame, byte, params.escape_byte, params.start_byte, params.stop_byte);
    }

    if (params.enable_crc16) {
        uint16_t crc = crc16_ccitt(payload);
        appendEscaped(frame, static_cast<uint8_t>((crc >> 8) & 0xFF),
                      params.escape_byte, params.start_byte, params.stop_byte);
        appendEscaped(frame, static_cast<uint8_t>(crc & 0xFF),
                      params.escape_byte, params.start_byte, params.stop_byte);
    }

    frame.push_back(params.stop_byte);
    return frame;
}

FrameDecoder::FrameDecoder()
    : params_(FrameCodec::Parameters{}) {}

FrameDecoder::FrameDecoder(const FrameCodec::Parameters& params)
    : params_(params) {}

bool FrameDecoder::push(uint8_t byte, std::vector<uint8_t>& out_frame) {
    if (byte == params_.start_byte) {
        buffer_.clear();
        in_frame_ = true;
        escape_next_ = false;
        return false;
    }

    if (!in_frame_) {
        return false;
    }

    if (byte == params_.stop_byte) {
        if (params_.enable_crc16) {
            if (buffer_.size() < 2) {
                reset();
                throw std::runtime_error("Frame shorter than CRC size");
            }
            const size_t payload_size = buffer_.size() - 2;
            std::vector<uint8_t> payload(buffer_.begin(), buffer_.begin() + payload_size);
            uint16_t received_crc = static_cast<uint16_t>(buffer_[payload_size]) << 8;
            received_crc |= buffer_[payload_size + 1];
            if (crc16_ccitt(payload) != received_crc) {
                reset();
                throw std::runtime_error("CRC mismatch in frame");
            }
            out_frame = std::move(payload);
        } else {
            out_frame = buffer_;
        }
        reset();
        return true;
    }

    if (escape_next_) {
        buffer_.push_back(static_cast<uint8_t>(byte ^ 0x20));
        escape_next_ = false;
        return false;
    }

    if (byte == params_.escape_byte) {
        escape_next_ = true;
        return false;
    }

    buffer_.push_back(byte);
    return false;
}

void FrameDecoder::reset() {
    in_frame_ = false;
    escape_next_ = false;
    buffer_.clear();
}
