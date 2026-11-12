#include "input/cst816.h"
#include "driver/gpio.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

namespace {
int operation{}, failAt{}, buses{}, devices{}, handlers{}, wakes{};
uint8_t chip = 0xb5;
std::array<uint8_t, 5> contact{};
void (*irq)(void *){};
void *irqContext{};
esp_err_t result() {
    return ++operation == failAt ? ESP_ERR_TIMEOUT : ESP_OK;
}
void wake(void *value) {
    assert(value == &wakes);
    ++wakes;
}
} // namespace
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *, i2c_master_bus_handle_t *out) {
    auto e = result();
    if (!e) {
        *out = &buses;
        ++buses;
    }
    return e;
}
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t) {
    --buses;
    return ESP_OK;
}
esp_err_t i2c_master_bus_add_device(
    i2c_master_bus_handle_t, const i2c_device_config_t *config, i2c_master_dev_handle_t *out) {
    assert(config->device_address == 0x15);
    auto e = result();
    if (!e) {
        *out = &devices;
        ++devices;
    }
    return e;
}
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t) {
    --devices;
    return ESP_OK;
}
esp_err_t i2c_master_transmit(
    i2c_master_dev_handle_t, const uint8_t *data, size_t size, int timeout) {
    assert(size == 2 && data[0] == 0xfa && data[1] == 0x60 && timeout == 5);
    return result();
}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t, const uint8_t *data, size_t size,
    uint8_t *out, size_t count, int timeout) {
    assert(size == 1 && timeout == 5);
    auto e = result();
    if (e)
        return e;
    if (data[0] == 0xa7) {
        assert(count == 1);
        *out = chip;
    } else {
        assert(data[0] == 2 && count == 5);
        std::memcpy(out, contact.data(), count);
    }
    return ESP_OK;
}
esp_err_t gpio_config(const gpio_config_t *) {
    return result();
}
esp_err_t gpio_set_level(gpio_num_t, int) {
    return result();
}
esp_err_t gpio_intr_disable(gpio_num_t) {
    return ESP_OK;
}
esp_err_t gpio_intr_enable(gpio_num_t) {
    return result();
}
esp_err_t gpio_set_intr_type(gpio_num_t, int) {
    return result();
}
esp_err_t gpio_install_isr_service(int) {
    return result();
}
esp_err_t gpio_isr_handler_add(gpio_num_t, void (*fn)(void *), void *arg) {
    auto e = result();
    if (!e) {
        irq = fn;
        irqContext = arg;
        ++handlers;
    }
    return e;
}
esp_err_t gpio_isr_handler_remove(gpio_num_t) {
    --handlers;
    irq = nullptr;
    return ESP_OK;
}

int main() {
    const flint::input::Cst816Config config{.sda = 11, .scl = 10, .reset = 13, .interrupt = 14};
    flint::input::Cst816 driver;
    flint::input::TouchSample sample;
    assert(driver.read(sample) == ESP_ERR_INVALID_STATE);
    assert(driver.begin(config, wake, &wakes) == ESP_OK);
    const int beginOperations = operation;
    irq(irqContext);
    assert(wakes == 1);
    contact = {1, 0x80, 239, 1, 23};
    assert(driver.read(sample) == ESP_OK && sample.down && sample.x == 239 && sample.y == 279);
    contact[1] = 0x40;
    assert(driver.read(sample) == ESP_OK && !sample.down);
    contact[0] = 0;
    contact[1] = 0;
    assert(driver.read(sample) == ESP_OK && !sample.down);
    contact[0] = 2;
    assert(driver.read(sample) == ESP_ERR_INVALID_RESPONSE && !sample.down);
    contact[0] = 1;
    contact[1] = 0xc0;
    assert(driver.read(sample) == ESP_ERR_INVALID_RESPONSE);
    contact[0] = 0;
    assert(driver.read(sample) == ESP_OK && !sample.down);
    driver.stop();
    driver.stop();
    assert(!buses && !devices && !handlers);
    for (int n = 1; n <= beginOperations; ++n) {
        operation = 0;
        failAt = n;
        assert(driver.begin(config, wake, &wakes) == ESP_ERR_TIMEOUT);
        assert(!driver.ready() && !buses && !devices && !handlers);
    }
    failAt = 0;
    chip = 0;
    assert(driver.begin(config, wake, &wakes) == ESP_ERR_NOT_SUPPORTED);
    assert(!buses && !devices && !handlers);
    chip = 0xb5;
    auto shared = reinterpret_cast<void *>(uintptr_t(1));
    assert(driver.begin(config, wake, &wakes, shared) == ESP_OK && !buses);
    driver.stop();
    assert(!buses && !devices && !handlers);
    auto invalid = config;
    invalid.scl = invalid.sda;
    assert(driver.begin(invalid, wake, &wakes) == ESP_ERR_INVALID_ARG);
    puts(
        "CST816 raw reports, IRQ dispatch, bounded I2C and partial initialization cleanup: passed");
}
