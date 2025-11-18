#ifndef LINK_LAYER_H
#define LINK_LAYER_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace spi_eak {

class FrameCodec {
public:
    struct Parameters {
        uint8_t start_byte = 0x7E;
        uint8_t stop_byte = 0x7F;
        uint8_t escape_byte = 0x7D;
        bool enable_crc16 = true;
    };

    enum class EncodeError {
        None,
        InvalidStartStop,
        InvalidEscape
    };

    struct Result {
        bool ok = true;
        EncodeError error = EncodeError::None;
        std::vector<uint8_t> frame;
    };

    static Result encode(const std::vector<uint8_t>& payload,
                         const Parameters& params);

    static Result encode(const std::vector<uint8_t>& payload) {
        return encode(payload, Parameters{});
    }
};

class FrameDecoder {
public:
    struct Options {
        FrameCodec::Parameters params;
        std::size_t max_frame_bytes = 2048;
    };

    FrameDecoder();
    explicit FrameDecoder(const Options& options);

    struct Result {
        enum class DropReason {
            None,
            TooShortForCrc,
            CrcMismatch,
            FrameTooLarge
        };

        bool frame_ready = false;
        bool frame_dropped = false;
        DropReason drop_reason = DropReason::None;
    };

    /**
     * Push a single byte from the SPI stream into the decoder.
     * Returns Result indicating whether a frame completed or was dropped.
     */
    Result push(uint8_t byte, std::vector<uint8_t>& out_frame);

    void reset();

private:
    Options options_;
    bool in_frame_ = false;
    bool escape_next_ = false;
    std::vector<uint8_t> buffer_;
};

} // namespace spi_eak

#endif // LINK_LAYER_H
