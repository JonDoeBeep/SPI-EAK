#ifndef LINK_LAYER_H
#define LINK_LAYER_H

#include <cstdint>
#include <vector>

class FrameCodec {
public:
    struct Parameters {
        uint8_t start_byte = 0x7E;
        uint8_t stop_byte = 0x7F;
        uint8_t escape_byte = 0x7D;
        bool enable_crc16 = true;
    };

    static std::vector<uint8_t> encode(const std::vector<uint8_t>& payload,
                                       const Parameters& params = Parameters{});
};

class FrameDecoder {
public:
    explicit FrameDecoder(const FrameCodec::Parameters& params = FrameCodec::Parameters{});

    /**
     * Push a single byte from the SPI stream into the decoder.
     * Returns true if a full frame has been reconstructed and stored in out_frame.
     */
    bool push(uint8_t byte, std::vector<uint8_t>& out_frame);

    void reset();

private:
    FrameCodec::Parameters params_;
    bool in_frame_ = false;
    bool escape_next_ = false;
    std::vector<uint8_t> buffer_;
};

#endif // LINK_LAYER_H
