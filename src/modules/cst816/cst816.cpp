#include "cst816.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>

namespace flint::modules {
namespace {
constexpr int TransferTimeoutMs = 5;
void delayMs(unsigned ms) {
    vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(ms)));
}
void checkCleanup(esp_err_t error) {
    if (error != ESP_OK)
        ESP_LOGE("cst816", "cleanup: %s", esp_err_to_name(error));
}
} // namespace

Cst816::~Cst816() {
    stop();
}

void IRAM_ATTR Cst816::interrupt(void *context) {
    auto *self = static_cast<Cst816 *>(context);
    self->wake_(self->context_);
}

void Cst816::stop() {
    ready_ = false;
    if (handlerInstalled_) {
        checkCleanup(gpio_intr_disable(gpio_num_t(interrupt_)));
        checkCleanup(gpio_isr_handler_remove(gpio_num_t(interrupt_)));
        handlerInstalled_ = false;
    }
    // GPIO ISR service is process-wide; do not uninstall other modules' handlers.
    if (device_) {
        checkCleanup(i2c_master_bus_rm_device(device_));
        device_ = nullptr;
    }
    if (busOwned_) {
        checkCleanup(i2c_del_master_bus(bus_));
        busOwned_ = false;
    }
    bus_ = nullptr;
}

esp_err_t Cst816::begin(const Cst816Config &config, void (*wakeFromIsr)(void *), void *context,
    i2c_master_bus_handle_t sharedBus) {
    if (ready_)
        return ESP_ERR_INVALID_STATE;
    if (!wakeFromIsr || !GPIO_IS_VALID_OUTPUT_GPIO(config.reset) ||
        !GPIO_IS_VALID_GPIO(config.interrupt) || !GPIO_IS_VALID_OUTPUT_GPIO(config.sda) ||
        !GPIO_IS_VALID_OUTPUT_GPIO(config.scl) || config.sda == config.scl ||
        config.reset == config.interrupt || config.reset == config.sda ||
        config.reset == config.scl || config.interrupt == config.sda ||
        config.interrupt == config.scl || config.address > 0x7f || config.frequency < 10000 ||
        config.frequency > 400000)
        return ESP_ERR_INVALID_ARG;
    const auto fail = [this](esp_err_t error) {
        stop();
        return error;
    };
    interrupt_ = config.interrupt;
    wake_ = wakeFromIsr;
    context_ = context;
    bus_ = sharedBus;
    if (!bus_) {
        i2c_master_bus_config_t bus{};
        bus.i2c_port = i2c_port_num_t(config.port);
        bus.sda_io_num = gpio_num_t(config.sda);
        bus.scl_io_num = gpio_num_t(config.scl);
        bus.clk_source = I2C_CLK_SRC_DEFAULT;
        bus.glitch_ignore_cnt = 7;
        // The board must provide external pull-ups, including when sharing the bus.
        auto error = i2c_new_master_bus(&bus, &bus_);
        if (error != ESP_OK)
            return fail(error);
        busOwned_ = true;
    }
    i2c_device_config_t device{};
    device.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device.device_address = config.address;
    device.scl_speed_hz = config.frequency;
    auto error = i2c_master_bus_add_device(bus_, &device, &device_);
    if (error != ESP_OK)
        return fail(error);

    gpio_config_t reset{};
    reset.pin_bit_mask = 1ULL << config.reset;
    reset.mode = GPIO_MODE_OUTPUT;
    error = gpio_config(&reset);
    if (error != ESP_OK)
        return fail(error);
    error = gpio_set_level(gpio_num_t(config.reset), 0);
    if (error != ESP_OK)
        return fail(error);
    delayMs(10);
    error = gpio_set_level(gpio_num_t(config.reset), 1);
    if (error != ESP_OK)
        return fail(error);
    delayMs(200);

    uint8_t id{}, idRegister = 0xa7;
    error = i2c_master_transmit_receive(device_, &idRegister, 1, &id, 1, TransferTimeoutMs);
    if (error != ESP_OK)
        return fail(error);
    if (id != 0xb4 && id != 0xb5 && id != 0xb6)
        return fail(ESP_ERR_NOT_SUPPORTED);
    // Periodic contact reports + contact-state changes, rather than gesture-only IRQs.
    const uint8_t irqMode[] = {0xfa, 0x60};
    error = i2c_master_transmit(device_, irqMode, sizeof(irqMode), TransferTimeoutMs);
    if (error != ESP_OK)
        return fail(error);

    gpio_config_t irq{};
    irq.pin_bit_mask = 1ULL << config.interrupt;
    irq.mode = GPIO_MODE_INPUT;
    irq.intr_type = GPIO_INTR_DISABLE;
    error = gpio_config(&irq);
    if (error != ESP_OK)
        return fail(error);
    error = gpio_install_isr_service(0);
    if (error != ESP_OK && error != ESP_ERR_INVALID_STATE)
        return fail(error);
    error = gpio_isr_handler_add(gpio_num_t(interrupt_), interrupt, this);
    if (error != ESP_OK)
        return fail(error);
    handlerInstalled_ = true;
    error = gpio_set_intr_type(gpio_num_t(interrupt_), GPIO_INTR_NEGEDGE);
    if (error != ESP_OK)
        return fail(error);
    error = gpio_intr_enable(gpio_num_t(interrupt_));
    if (error != ESP_OK)
        return fail(error);
    ready_ = true;
    return ESP_OK;
}

esp_err_t Cst816::read(TouchSample &sample) {
    sample = {};
    if (!ready_)
        return ESP_ERR_INVALID_STATE;
    uint8_t firstRegister = 0x02, data[5]{};
    const auto error = i2c_master_transmit_receive(
        device_, &firstRegister, 1, data, sizeof(data), TransferTimeoutMs);
    if (error != ESP_OK)
        return error;
    const auto event = data[1] >> 6;
    if (data[0] > 1 || (data[0] == 1 && event == 3))
        return ESP_ERR_INVALID_RESPONSE;
    sample.down = data[0] == 1 && event != 1;
    sample.x = uint16_t((data[1] & 0x0f) << 8) | data[2];
    sample.y = uint16_t((data[3] & 0x0f) << 8) | data[4];
    return ESP_OK;
}
} // namespace flint::modules
