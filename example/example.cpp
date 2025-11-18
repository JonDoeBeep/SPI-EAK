#include "link_layer.h"
#include "spi.h"

#include <iomanip>
#include <iostream>
#include <vector>

using namespace spi_eak;

int main() {
    try {
        // Initialize SPI on device 0, chip select 0
        SPI spi("/dev/spidev0.0", 5'000'000, SPI::Mode::MODE_0, 8);

        FrameCodec::Parameters frame_params;
        FrameDecoder::Options decoder_opts;
        decoder_opts.params = frame_params;
        decoder_opts.max_frame_bytes = 2048;
        FrameDecoder decoder(decoder_opts);

        // Example payload
        std::vector<uint8_t> payload = {0x42, 0x01, 0x10, 0x00, 0x7E};
        std::vector<uint8_t> encoded;
        auto encode_result = FrameCodec::encode(payload, frame_params, encoded);
        if (!encode_result.ok) {
            std::cerr << "Failed to encode frame: "
                  << (encode_result.error == FrameCodec::EncodeError::InvalidStartStop
                      ? "start/stop conflict"
                      : "escape conflicts with sentinel")
                  << std::endl;
            return 1;
        }

        // Perform full duplex transfer (tx and rx sizes must match on SPI)
        std::vector<uint8_t> rx_frame = spi.transfer(encoded);

        std::vector<uint8_t> decoded;
        bool frame_complete = false;
        for (uint8_t byte : rx_frame) {
            auto result = decoder.push(byte, decoded);
            if (result.frame_dropped) {
                std::cout << "Frame dropped due to ";
                switch (result.drop_reason) {
                    case FrameDecoder::Result::DropReason::TooShortForCrc:
                        std::cout << "too short for CRC";
                        break;
                    case FrameDecoder::Result::DropReason::CrcMismatch:
                        std::cout << "CRC mismatch";
                        break;
                    case FrameDecoder::Result::DropReason::FrameTooLarge:
                        std::cout << "exceeded max frame bytes";
                        break;
                    default:
                        std::cout << "unspecified reason";
                        break;
                }
                std::cout << std::endl;
            }
            frame_complete = result.frame_ready || frame_complete;
        }

        std::cout << "Sent frame (" << encoded.size() << " bytes):";
        for (auto byte : encoded) {
            std::cout << " 0x" << std::hex << std::setw(2) << std::setfill('0')
                      << static_cast<int>(byte);
        }
        std::cout << std::dec << std::endl;

        if (frame_complete) {
            std::cout << "Received payload (" << decoded.size() << " bytes):";
            for (auto byte : decoded) {
                std::cout << " 0x" << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<int>(byte);
            }
            std::cout << std::dec << std::endl;
        } else {
            std::cout << "No complete frame received in loopback window" << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cerr << "SPI session failed: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
