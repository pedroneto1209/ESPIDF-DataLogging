#include <stdio.h>
#include <stdlib.h>
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <time.h>
#include "freertos/ringbuf.h"

#define RING_SIZE 256
#define PIN_NUM_MISO 2
#define PIN_NUM_MOSI 15
#define PIN_NUM_CLK  14
#define PIN_NUM_CS   13

typedef uint32_t ring_pos_t;
volatile ring_pos_t ring_head;
volatile ring_pos_t ring_tail;
volatile packet_t ring_data[RING_SIZE];

static const char *TAG = "esplog";

struct tm timez;

bool run = true;

typedef struct
{
    int lat;
    int lon;
    int16_t alt;
    uint32_t timestamp;
} packet_t;

void app_main(void) {
    ESP_LOGI(TAG, "SD init");

    FILE* fp;  

    sdmmc_card_t* card;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = PIN_NUM_MISO;
    slot_config.gpio_mosi = PIN_NUM_MOSI;
    slot_config.gpio_sck  = PIN_NUM_CLK;
    slot_config.gpio_cs   = PIN_NUM_CS;
    
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024           //bigger values will be beter to low data
    };


    esp_err_t ret;
    do
    {
    ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    } while (ret != ESP_OK);

    sdmmc_card_print_info(stdout, card);

    time_t tt = time(NULL);
    timez = *gmtime(&tt);
    char data_formatada[64];
    strftime(data_formatada, 64, "%d/%m/%Y %H:%M:%S", &timez);

    ESP_LOGI(TAG, "Opening file");
    fp = fopen(data_formatada, "w");
    if (fp == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }

    packet_t *pck;
    RingbufHandle_t buf_handle;
    size_t item_size;
    buf_handle = xRingbufferCreate(256, RINGBUF_TYPE_NOSPLIT);
    if (buf_handle == NULL) {
        printf("Failed to create ring buffer\n");
    }
    
    while (run)
    {
        /*
        get pck
        */
        UBaseType_t res =  xRingbufferSend(buf_handle, pck, sizeof(pck), pdMS_TO_TICKS(1000));
        
        if(xRingbufferGetCurFreeSize(buf_handle) < 256)
        {   
            packet_t *item = (packet_t *)xRingbufferReceive(buf_handle, &item_size, pdMS_TO_TICKS(1000));
            fwrite((void *)&item, sizeof(packet_t), 1, fp);
        }
    }

    esp_vfs_fat_sdmmc_unmount();
    ESP_LOGI(TAG, "Card unmounted");
}