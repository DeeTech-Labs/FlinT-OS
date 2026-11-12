#include "modules/bme280/bme280.h"
#include "modules/bmp280/bmp280.h"
#include "modules/veml7700/veml7700.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                   \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
namespace {
struct Client {
    unsigned address{};
    bool used{};
};
std::array<Client, 4> clients;
std::array<std::array<uint8_t, 256>, 128> regs;
unsigned removed{}, transfers{};
int failRegister = -1;
void trim(unsigned address, int h4 = 325, int h5 = 50) {
    auto &r = regs[address];
    const int values[]{
        27504, 26435, -1000, 36477, -10685, 3024, 2855, 140, -7, 15500, -14600, 6000};
    for (unsigned i = 0; i < 12; ++i) {
        auto v = uint16_t(values[i]);
        r[0x88 + i * 2] = uint8_t(v);
        r[0x89 + i * 2] = uint8_t(v >> 8);
    }
    r[0xa1] = 75;
    r[0xe1] = 0x6a;
    r[0xe2] = 1;
    r[0xe3] = 0;
    const auto a = unsigned(h4) & 0xfff, b = unsigned(h5) & 0xfff;
    r[0xe4] = uint8_t(a >> 4);
    r[0xe5] = uint8_t((a & 15) | ((b & 15) << 4));
    r[0xe6] = uint8_t(b >> 4);
    r[0xe7] = 30;
}
void sample(unsigned address, unsigned humidity = 30000) {
    auto &r = regs[address];
    const unsigned raw[]{415148, 519888};
    for (unsigned i = 0; i < 2; ++i) {
        r[0xf7 + i * 3] = uint8_t(raw[i] >> 12);
        r[0xf8 + i * 3] = uint8_t(raw[i] >> 4);
        r[0xf9 + i * 3] = uint8_t(raw[i] << 4);
    }
    r[0xfd] = uint8_t(humidity >> 8);
    r[0xfe] = uint8_t(humidity);
}
} // namespace
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *, i2c_master_bus_handle_t *bus) {
    *bus = reinterpret_cast<void *>(1);
    return ESP_OK;
}
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t) {
    std::abort();
}
esp_err_t i2c_master_bus_add_device(
    i2c_master_bus_handle_t, const i2c_device_config_t *c, i2c_master_dev_handle_t *d) {
    for (auto &client : clients)
        if (!client.used) {
            client = {unsigned(c->device_address), true};
            *d = &client;
            return ESP_OK;
        }
    std::abort();
}
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t d) {
    static_cast<Client *>(d)->used = false;
    ++removed;
    return ESP_OK;
}
esp_err_t i2c_master_transmit(
    i2c_master_dev_handle_t d, const uint8_t *data, size_t size, int timeout) {
    ++transfers;
    CHECK(timeout == 5);
    if (data[0] == failRegister)
        return ESP_ERR_TIMEOUT;
    auto &r = regs[static_cast<Client *>(d)->address];
    std::copy_n(data + 1, size - 1, r.begin() + data[0]);
    return ESP_OK;
}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t d, const uint8_t *reg, size_t tx,
    uint8_t *data, size_t count, int timeout) {
    ++transfers;
    CHECK(tx == 1 && timeout == 5);
    if (*reg == failRegister)
        return ESP_ERR_TIMEOUT;
    const auto &r = regs[static_cast<Client *>(d)->address];
    std::copy_n(r.begin() + *reg, count, data);
    return ESP_OK;
}
int main() {
    using namespace flint::modules;
    auto bus = deviceI2cBus();
    regs[0x76][0xd0] = 0x58;
    trim(0x76);
    sample(0x76);
    {
        Bmp280 sensor;
        EnvironmentalSample value;
        const auto before = transfers;
        CHECK(sensor.beginAsync(bus, 0x76) == ESP_OK && transfers == before);
        CHECK(sensor.initializing() && !sensor.ready());
        for (unsigned step = 0; step < 4; ++step) {
            const auto prior = transfers;
            CHECK(sensor.maintainInit() == ESP_OK && transfers == prior + 1);
        }
        CHECK(sensor.ready() && !sensor.initializing() && sensor.nextWake() == UINT64_MAX);
        CHECK(sensor.maintainInit() == ESP_ERR_INVALID_STATE);
        CHECK(sensor.read(value, 0) == ESP_ERR_INVALID_STATE);
        CHECK(sensor.start(100) == ESP_OK && sensor.nextWake() == 112 && regs[0x76][0xf4] == 0x25);
        CHECK(sensor.start(101) == ESP_ERR_INVALID_STATE);
        CHECK(sensor.read(value, 111) == ESP_ERR_NOT_FOUND);
        CHECK(sensor.read(value, 112) == ESP_OK);
        CHECK(std::abs(value.temperatureC - 25.08248) < 0.001);
        CHECK(std::abs(value.pressurePa - 100653.27) < 0.1 && !value.humidityAvailable);
        CHECK(sensor.nextWake() == UINT64_MAX);
        CHECK(sensor.start(200) == ESP_OK);
        regs[0x76][0xf3] = 8;
        CHECK(sensor.read(value, 212) == ESP_ERR_TIMEOUT && sensor.nextWake() == UINT64_MAX &&
            regs[0x76][0xf4] == 0);
        regs[0x76][0xf3] = 0;
    }
    regs[0x76][0xd0] = 0x60;
    {
        Bmp280 wrong;
        CHECK(wrong.begin(bus) == ESP_ERR_NOT_SUPPORTED && !wrong.ready());
    }
    {
        Bme280 sensor;
        EnvironmentalSample value;
        CHECK(sensor.begin(bus) == ESP_OK && sensor.start(0) == ESP_OK && regs[0x76][0xf2] == 1);
        CHECK(sensor.read(value, 12) == ESP_OK && value.humidityAvailable);
        CHECK(std::abs(value.humidityPercent - 50.7267767) < 0.001);
    }
    trim(0x76, -30, -50);
    sample(0x76, 7000);
    {
        Bme280 sensor;
        EnvironmentalSample value;
        CHECK(sensor.begin(bus) == ESP_OK && sensor.start(0) == ESP_OK &&
            sensor.read(value, 12) == ESP_OK);
        CHECK(std::abs(value.humidityPercent - 50.9221009) < 0.001);
        failRegister = 0xf7;
        CHECK(sensor.start(20) == ESP_OK && sensor.read(value, 32) == ESP_ERR_TIMEOUT);
        CHECK(value.pressurePa == 0 && !value.humidityAvailable);
        failRegister = -1;
    }
    regs[0x76][0x8e] = regs[0x76][0x8f] = 0;
    {
        Bme280 bad;
        CHECK(bad.begin(bus) == ESP_ERR_INVALID_RESPONSE && !bad.ready());
        trim(0x76);
        CHECK(bad.beginAsync(bus, 0x76) == ESP_OK);
        failRegister = 0xd0;
        CHECK(bad.maintainInit() == ESP_ERR_TIMEOUT && !bad.ready() && !bad.initializing());
        failRegister = -1;
        CHECK(bad.beginAsync(bus, 0x76) == ESP_OK);
        for (unsigned step = 0; step < 6; ++step)
            CHECK(bad.maintainInit() == ESP_OK);
        CHECK(bad.ready());
    }
    {
        Veml7700 light;
        double lux{};
        CHECK(light.begin(bus) == ESP_OK && regs[0x10][0] == 1 && regs[0x10][1] == 0x10);
        CHECK(light.start(10) == ESP_OK && light.nextWake() == 220 && regs[0x10][0] == 0);
        CHECK(light.read(lux, 219) == ESP_ERR_NOT_FOUND);
        regs[0x10][4] = 0xe8;
        regs[0x10][5] = 3;
        CHECK(light.read(lux, 220) == ESP_OK && std::abs(lux - 460.8) < 0.001);
        CHECK(light.nextWake() == UINT64_MAX && regs[0x10][0] == 1);
        CHECK(light.start(300) == ESP_OK);
        failRegister = 4;
        CHECK(light.read(lux, 510) == ESP_ERR_TIMEOUT && lux == 0 && regs[0x10][0] == 1);
        failRegister = -1;
        CHECK(light.start(600) == ESP_OK);
        regs[0x10][4] = regs[0x10][5] = 255;
        CHECK(light.read(lux, 810) == ESP_ERR_INVALID_RESPONSE);
    }
    CHECK(removed == 8);
    for (const auto &client : clients)
        CHECK(!client.used);
    std::puts("Bosch calibration/reference vectors, signed humidity trims, forced deadlines and "
              "light shutdown: passed");
}
