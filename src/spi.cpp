#include "spi.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <cstring>
#include <cerrno>
#include <utility>
#include <vector>

SPI::SPI(const std::string& device, uint32_t speed, Mode mode, uint8_t bits)
    : SPI(Config{device, speed, mode, bits}) {}

SPI::SPI(const Config& config)
    : fd(-1)
    , config_(config)
{
    fd = ::open(config_.device.c_str(), O_RDWR);
    if (fd < 0) {
        throw std::runtime_error("Failed to open SPI device '" + config_.device + "': " + std::string(strerror(errno)));
    }

    try {
        configureDevice();
        config_dirty_ = false;
    } catch (...) {
        ::close(fd);
        fd = -1;
        throw;
    }
}

SPI::~SPI() {
    close();
}

// Move constructor: "steals" the file descriptor from the other object
SPI::SPI(SPI&& other) noexcept
    : fd(other.fd)
    , config_(std::move(other.config_))
    , config_dirty_(other.config_dirty_)
{
    // Invalidate the other object so its destructor does nothing
    other.fd = -1;
    other.config_dirty_ = false;
}

// Move assignment operator
SPI& SPI::operator=(SPI&& other) noexcept {
    if (this != &other) { // Prevent self-assignment
        close(); // Close our own resource first

        // Steal resources from the other object
        fd = other.fd;
        config_ = std::move(other.config_);
        config_dirty_ = other.config_dirty_;

        // Invalidate the other object
        other.fd = -1;
        other.config_dirty_ = false;
    }
    return *this;
}

void SPI::close() {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

bool SPI::isOpen() const {
    return fd >= 0;
}

// C++ vector-based transfer (wrapper)
std::vector<uint8_t> SPI::transfer(const std::vector<uint8_t>& tx_data) {
    std::vector<uint8_t> rx_data(tx_data.size());
    transfer(rx_data.data(), tx_data.data(), tx_data.size());
    return rx_data;
}

// C-style raw pointer transfer (core logic)
void SPI::transfer(uint8_t* rx_data, const uint8_t* tx_data, size_t length) {
    if (fd < 0) {
        throw std::logic_error("SPI device is not open (was it moved from?)");
    }
    if (!tx_data || !rx_data) {
        throw std::invalid_argument("Invalid buffer pointer provided to SPI transfer");
    }

    ensureConfigured();

    struct spi_ioc_transfer transfer_desc;
    memset(&transfer_desc, 0, sizeof(transfer_desc));
    
    transfer_desc.tx_buf = reinterpret_cast<__u64>(tx_data);
    transfer_desc.rx_buf = reinterpret_cast<__u64>(rx_data);
    transfer_desc.len = length;
    transfer_desc.speed_hz = config_.speed_hz;
    transfer_desc.bits_per_word = config_.bits_per_word;
    transfer_desc.delay_usecs = config_.delay_usecs;
    transfer_desc.cs_change = config_.cs_change;
    
    if (ioctl(fd, SPI_IOC_MESSAGE(1), &transfer_desc) < 0) {
        throw std::runtime_error("SPI transfer failed: " + std::string(strerror(errno)));
    }
}

void SPI::transfer(const std::vector<Segment>& segments) {
    if (fd < 0) {
        throw std::logic_error("SPI device is not open (was it moved from?)");
    }
    if (segments.empty()) {
        return;
    }

    ensureConfigured();

    std::vector<spi_ioc_transfer> ops(segments.size());
    for (size_t i = 0; i < segments.size(); ++i) {
        const auto& seg = segments[i];
        if (seg.length == 0) {
            throw std::invalid_argument("Segment length must be non-zero");
        }
        if (!seg.tx_buffer && !seg.rx_buffer) {
            throw std::invalid_argument("At least one buffer pointer must be provided for SPI segment");
        }

        auto& op = ops[i];
        memset(&op, 0, sizeof(op));
        op.tx_buf = reinterpret_cast<__u64>(seg.tx_buffer);
        op.rx_buf = reinterpret_cast<__u64>(seg.rx_buffer);
        op.len = seg.length;
        op.speed_hz = seg.speed_override_hz ? seg.speed_override_hz : config_.speed_hz;
        op.bits_per_word = seg.bits_override ? seg.bits_override : config_.bits_per_word;
        op.delay_usecs = seg.delay_override_usecs ? seg.delay_override_usecs : config_.delay_usecs;
        op.cs_change = seg.cs_change ? 1 : config_.cs_change;
    }

    if (ioctl(fd, SPI_IOC_MESSAGE(ops.size()), ops.data()) < 0) {
        throw std::runtime_error("SPI multi-segment transfer failed: " + std::string(strerror(errno)));
    }
}

void SPI::setSpeed(uint32_t hz) {
    config_.speed_hz = hz;
    config_dirty_ = true;
}

void SPI::setMode(Mode new_mode) {
    config_.mode = new_mode;
    config_dirty_ = true;
}

void SPI::setBitsPerWord(uint8_t bits) {
    config_.bits_per_word = bits;
    config_dirty_ = true;
}

uint32_t SPI::getSpeed() const {
    return config_.speed_hz;
}

SPI::Mode SPI::getMode() const {
    return config_.mode;
}

uint8_t SPI::getBitsPerWord() const {
    return config_.bits_per_word;
}

void SPI::reconfigure(const Config& config) {
    config_ = config;
    configureDevice();
    config_dirty_ = false;
}

void SPI::applyConfig() {
    ensureConfigured();
}

void SPI::configureDevice() {
    if (fd < 0) {
        throw std::logic_error("Cannot configure SPI device before opening");
    }

    uint8_t raw_mode = static_cast<uint8_t>(config_.mode);
    if (ioctl(fd, SPI_IOC_WR_MODE, &raw_mode) < 0) {
        throw std::runtime_error("Failed to set SPI mode: " + std::string(strerror(errno)));
    }
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &config_.bits_per_word) < 0) {
        throw std::runtime_error("Failed to set bits per word: " + std::string(strerror(errno)));
    }
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &config_.speed_hz) < 0) {
        throw std::runtime_error("Failed to set max speed: " + std::string(strerror(errno)));
    }
    config_dirty_ = false;
}

void SPI::ensureConfigured() {
    if (fd < 0) {
        throw std::logic_error("SPI device is not open (was it moved from?)");
    }
    if (!config_dirty_) {
        return;
    }
    configureDevice();
}