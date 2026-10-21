#include "DEV_Config.h"

static const char *TAG = "DEV_Config";
static spi_device_handle_t spi;

UBYTE DEV_Module_Init(void)
{
    // Configurar salidas
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << EPD_DC_PIN) | (1ULL << EPD_RST_PIN) | 
                        (1ULL << EPD_CS_PIN) | (1ULL << EPD_PWR_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Encender alimentación de la e-Paper (Active LOW)
    gpio_set_level(EPD_PWR_PIN, 0);

    // Configurar BUSY como entrada
    io_conf.pin_bit_mask = (1ULL << EPD_BUSY_PIN);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // Configurar Bus SPI2
    spi_bus_config_t buscfg = {
        .mosi_io_num = EPD_MOSI_PIN,
        .miso_io_num = -1,
        .sclk_io_num = EPD_SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = EPD_CS_PIN,
        .queue_size = 7,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &spi));

    ESP_LOGI(TAG, "Hardware SPI e inicializacion GPIO correcta");
    return 0;
}

void DEV_Module_Exit(void)
{
    gpio_set_level(EPD_PWR_PIN, 1); // Apagar alimentación
    spi_bus_remove_device(spi);
    spi_bus_free(SPI2_HOST);
}

void DEV_SPI_WriteByte(UBYTE value)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &value;
    spi_device_polling_transmit(spi, &t);
}

void DEV_SPI_Write_nByte(UBYTE *pData, uint32_t Len)
{
    if (Len == 0) return;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = Len * 8;
    t.tx_buffer = pData;
    spi_device_polling_transmit(spi, &t);
}
