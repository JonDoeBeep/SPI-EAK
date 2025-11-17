#ifndef SPI_H
#define SPI_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>

class SPI {
public:
    // SPI modes (CPOL | CPHA)
    enum class Mode : uint8_t {
        MODE_0 = 0,  // CPOL=0, CPHA=0
        MODE_1 = 1,  // CPOL=0, CPHA=1
        MODE_2 = 2,  // CPOL=1, CPHA=0
        MODE_3 = 3   // CPOL=1, CPHA=1
    };

    struct Config {
        std::string device;
        uint32_t speed_hz = 1'000'000;
        Mode mode = Mode::MODE_0;
        uint8_t bits_per_word = 8;
        uint16_t delay_usecs = 0;
        bool cs_change = false;
    };

    struct Segment {
        const uint8_t* tx_buffer = nullptr;
        uint8_t* rx_buffer = nullptr;
        size_t length = 0;
        uint32_t speed_override_hz = 0; // 0 -> use current config
        uint16_t delay_override_usecs = 0;
        uint8_t bits_override = 0;      // 0 -> use current config
        bool cs_change = false;
    };

    /**
     * Constructor that acquires and configures the SPI device.
     * Throws std::runtime_error on failure.
     * @param device SPI device path (e.g., "/dev/spidev0.0")
     * @param speed Speed in Hz (e.g., 1000000 for 1MHz)
     * @param mode SPI mode
     * @param bits_per_word Bits per word (typically 8)
     */
    SPI(const std::string& device, uint32_t speed, Mode mode = Mode::MODE_0, uint8_t bits_per_word = 8);
    explicit SPI(const Config& config);
    
    /**
     * Destructor - automatically closes the SPI device.
     */
    ~SPI();

    // --- Make the class non-copyable but movable ---
    SPI(const SPI&) = delete;
    SPI& operator=(const SPI&) = delete;
    SPI(SPI&& other) noexcept;
    SPI& operator=(SPI&& other) noexcept;
    // -----------------------------------------------------------

    /**
     * Transfer data using safe and convenient C++ vectors.
     * @param tx_data A vector of bytes to transmit.
     * @return A vector of bytes received.
     * @throws std::runtime_error on transfer failure.
     */
    std::vector<uint8_t> transfer(const std::vector<uint8_t>& tx_data);
    
    /**
     * Transfer data using C-style raw pointers for performance/flexibility.
     * @param rx_data Pointer to the receive buffer.
     * @param tx_data Pointer to the transmit buffer.
     * @param length Number of bytes to transfer.
     * @throws std::runtime_error on transfer failure.
     */
    void transfer(uint8_t* rx_data, const uint8_t* tx_data, size_t length);

    /**
     * Transfer multiple segments in a single CS assertion.
     * Allows callers to send headers + payloads without round-trips.
     */
    void transfer(const std::vector<Segment>& segments);

    /**
     * Check if the device handle is valid.
     * @return true if the handle is valid, false otherwise (e.g., after being moved from).
     */
    [[nodiscard]] bool isOpen() const;

    void setSpeed(uint32_t hz);
    void setMode(Mode new_mode);
    void setBitsPerWord(uint8_t bits);

    [[nodiscard]] uint32_t getSpeed() const;
    [[nodiscard]] Mode getMode() const;
    [[nodiscard]] uint8_t getBitsPerWord() const;

    void reconfigure(const Config& config);
    [[nodiscard]] const Config& currentConfig() const noexcept { return config_; }

private:
    void close(); // Private helper for RAII
    void configureDevice();

    int fd; // File descriptor for the device
    Config config_;
};

#endif // SPI_H