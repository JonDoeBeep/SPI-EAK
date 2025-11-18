# SPI-EAK

A C++ library for full duplex SPI communication.

## Building

To build the library and example:

```bash
make
```

To clean build artifacts:

```bash
make clean
```

## Usage

```cpp
#include "link_layer.h"
#include "spi.h"

int main() {
    try {
        spi_eak::SPI spi("/dev/spidev0.0", 5'000'000, spi_eak::SPI::Mode::MODE_0, 8);

        spi_eak::FrameCodec::Parameters params; // Start=0x7E, Stop=0x7F, Escape=0x7D, CRC16 enabled
        std::vector<uint8_t> payload = {/* metadata, commands, etc. */};

        std::vector<uint8_t> framed;
        auto encode_result = spi_eak::FrameCodec::encode(payload, params, framed);
        if (!encode_result.ok) {
            // handle invalid framing configuration
            return 1;
        }

        // SPI is full-duplex, so RX is the same size as TX
        std::vector<uint8_t> rx_frame = spi.transfer(framed);

        spi_eak::FrameDecoder::Options decoder_opts;
        decoder_opts.params = params;
        decoder_opts.max_frame_bytes = 2048; // default safeguard
        spi_eak::FrameDecoder decoder(decoder_opts);

        std::vector<uint8_t> decoded;
        for (uint8_t byte : rx_frame) {
            auto result = decoder.push(byte, decoded);
            if (result.frame_dropped) {
                // handle corruption (log, metrics, retry, etc.)
            }
            if (result.frame_ready) {
                // decoded now holds a full variable-length payload
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "SPI failure: " << ex.what() << std::endl;
        return 1;
    }
}
```

This project targets Linux hosts (e.g., Raspberry Pi), relying on the `spidev` userspace driver. Calls to `setSpeed`, `setMode`, and `setBitsPerWord` are batched and automatically flushed before the next transfer (or immediately via `applyConfig()`), so multiple configuration edits cost only one set of ioctl calls.

### Variable-length framing

`FrameCodec` and `FrameDecoder` wrap arbitrary payloads with:

- Configurable start/stop bytes (defaults 0x7E/0x7F) for clear packet boundaries.
- SLIP-style escaping so payloads may contain sentinel bytes.
- Optional CRC-16 (enabled by default) to catch corruption before upper layers read metadata/commands.
- Configurable maximum frame length (default 2 KiB) so a misbehaving peer cannot consume unbounded memory.

The helpers are deterministic and allocation-free on the decode side once the buffer reserves the expected payload size, making them suitable for robotics control loops.

## SPI Modes

- `MODE_0`: CPOL=0, CPHA=0
- `MODE_1`: CPOL=0, CPHA=1
- `MODE_2`: CPOL=1, CPHA=0
- `MODE_3`: CPOL=1, CPHA=1
