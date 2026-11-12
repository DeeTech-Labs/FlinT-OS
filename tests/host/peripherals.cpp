#include "modules/pcf85063/pcf85063.h"
#include "modules/qmi8658/qmi8658.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

#define CHECK(value)                                                                               \
    do {                                                                                           \
        if (!(value)) {                                                                            \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #value);                               \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
namespace {
struct FakeDevice {
    unsigned address{};
    bool used{};
};
std::array<FakeDevice, 4> clients;
std::array<std::array<uint8_t, 256>, 128> registers;
unsigned buses{}, removed{}, timeouts{}, writes{};
int failRegister = -1;
} // namespace
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *config, i2c_master_bus_handle_t *bus) {
    CHECK(config->sda_io_num == 11 && config->scl_io_num == 10);
    ++buses;
    *bus = reinterpret_cast<void *>(1);
    return ESP_OK;
}
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t) {
    std::abort();
}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus, const i2c_device_config_t *config,
    i2c_master_dev_handle_t *device) {
    CHECK(bus && config->scl_speed_hz == 400000);
    for (auto &client : clients)
        if (!client.used) {
            client = {unsigned(config->device_address), true};
            *device = &client;
            return ESP_OK;
        }
    std::abort();
}
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t device) {
    auto &client = *static_cast<FakeDevice *>(device);
    CHECK(client.used);
    client.used = false;
    ++removed;
    return ESP_OK;
}
esp_err_t i2c_master_transmit(
    i2c_master_dev_handle_t device, const uint8_t *data, size_t size, int timeout) {
    CHECK(timeout == 5 && size >= 2 && size <= 17);
    ++writes;
    if (data[0] == failRegister) {
        ++timeouts;
        return ESP_ERR_TIMEOUT;
    }
    auto &memory = registers[static_cast<FakeDevice *>(device)->address];
    std::copy_n(data + 1, size - 1, memory.data() + data[0]);
    return ESP_OK;
}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t device, const uint8_t *reg, size_t tx,
    uint8_t *data, size_t size, int timeout) {
    CHECK(tx == 1 && timeout == 5);
    if (*reg == failRegister) {
        ++timeouts;
        return ESP_ERR_TIMEOUT;
    }
    auto &memory = registers[static_cast<FakeDevice *>(device)->address];
    std::copy_n(memory.data() + *reg, size, data);
    return ESP_OK;
}
int main() {
    using namespace flint::modules;
    auto bus = deviceI2cBus();
    CHECK(bus == deviceI2cBus() && buses == 1);
    auto &clock = registers[0x51];
    const std::array<uint8_t, 7> date{0x59, 0x58, 0x23, 0x29, 0x04, 0x02, 0x24};
    std::copy(date.begin(), date.end(), clock.begin() + 4);
    {
        Pcf85063 rtc;
        CHECK(rtc.begin(bus) == ESP_OK);
        std::tm time{};
        CHECK(rtc.read(time) == ESP_OK);
        CHECK(time.tm_year == 124 && time.tm_mon == 1 && time.tm_mday == 29 && time.tm_hour == 23);
        clock[4] |= 0x80;
        CHECK(rtc.read(time) == ESP_ERR_INVALID_RESPONSE);
        clock[4] &= 0x7f;
        clock[5] = 0x6a;
        CHECK(rtc.read(time) == ESP_ERR_INVALID_RESPONSE);
        clock[5] = 0;
        clock[10] = 0x23;
        CHECK(rtc.read(time) == ESP_ERR_INVALID_RESPONSE);
        clock[10] = 0x24;
        CHECK(rtc.read(time) == ESP_OK);
        time.tm_year = 123;
        const auto before = writes;
        CHECK(rtc.set(time) == ESP_ERR_INVALID_ARG && before == writes);
        time.tm_year = 124;
        time.tm_sec = 5;
        CHECK(rtc.set(time) == ESP_OK && clock[4] == 5 && !(clock[0] & 0x20));
        failRegister = 4;
        CHECK(rtc.set(time) == ESP_ERR_TIMEOUT);
        failRegister = -1;
        CHECK(clock[0] & 0x20);
        CHECK(rtc.read(time) == ESP_ERR_INVALID_STATE);
        time = {};
        time.tm_year = 125;
        time.tm_mon = 0;
        time.tm_mday = 1;
        CHECK(rtc.set(time) == ESP_OK);
        CHECK(rtc.read(time) == ESP_OK);
    }
    CHECK(removed == 1);
    {
        Qmi8658 wrong;
        CHECK(wrong.begin(bus) == ESP_ERR_NOT_SUPPORTED && !wrong.ready());
    }
    CHECK(removed == 2);
    auto &motion = registers[0x6b];
    motion[0] = 5;
    {
        Qmi8658 imu;
        MotionSample sample;
        CHECK(imu.begin(bus) == ESP_OK);
        CHECK(motion[2] == 0x41 && motion[8] == 0);
        CHECK(imu.read(sample) == ESP_ERR_INVALID_STATE);
        CHECK(imu.enable(true) == ESP_OK && motion[8] == 0x83);
        CHECK(imu.read(sample) == ESP_ERR_NOT_FOUND);
        motion[0x2e] = 3;
        const uint8_t bytes[]{0, 0x40, 0, 0xc0, 0xff, 0xff, 1, 0, 0, 0x80, 0xff, 0x7f};
        std::copy_n(bytes, sizeof(bytes), motion.begin() + 0x35);
        CHECK(imu.read(sample) == ESP_OK);
        CHECK((sample.acceleration == std::array<int16_t, 3>{16384, -16384, -1}));
        CHECK((sample.angularVelocity == std::array<int16_t, 3>{1, -32768, 32767}));
        failRegister = 0x35;
        CHECK(imu.read(sample) == ESP_ERR_TIMEOUT);
        failRegister = -1;
        CHECK((sample.acceleration == std::array<int16_t, 3>{}));
        CHECK(imu.enable(false) == ESP_OK && motion[2] == 0x41 && motion[8] == 0);
    }
    CHECK(removed == 3 && timeouts == 2 && buses == 1);
    for (const auto &client : clients)
        CHECK(!client.used);
    std::puts("RTC integrity, failed writes, IMU locking/raw data, shared I2C and RAII: passed");
}
