/* SPI Master example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/portmacro.h"
#include "esp_system.h"

// lx add
#include "esp_log.h"
#include "my_st7789.h"
#include "mystruct.h"
#include "mpu6050.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "time.h"

// #include "ws2812.h"
#include "mytask.h"
#include "wifi.h"
#include "my_lvgl.h"
#include "lvgl.h"
#include "gui_guider.h"
#include "lvgl_helpers.h"
#include "my_led.h"
#include "my_touchpad.h"
#include "esp_system.h"


// 调试用宏定义
#define DEBUG 0


// 全局变量的声明和定义
extern const char* TAG;
lv_ui guider_ui;
extern SemaphoreHandle_t mutexLvgl;  // 访问Lvgl变量时用到的互斥信号量，防止多个地方来回用导致冲突


// 6050测试
void mpu6050_task(void * pvParameters){
    int test_6050_value;;
    uint8_t buff_temp[10];
    // 转换参数 my add
    spi_device_handle_t *spi = (spi_device_handle_t *)pvParameters;

    while(1){
        // test 6050
        // show x acc vaule
        test_6050_value = read_accel_x();
        sprintf((char *)buff_temp, "%-6d", test_6050_value);
        my_ips_show_string(*spi, 0, 170, buff_temp, MAGENTA, BLACK, 24, 0);
        // show y acc vaule
        test_6050_value = read_accel_y();
        sprintf((char *)buff_temp, "%-6d", test_6050_value);
        my_ips_show_string(*spi, 80, 170, buff_temp, MAGENTA, BLACK, 24, 0);
        // show z acc vaule
        test_6050_value = read_accel_z();
        sprintf((char *)buff_temp, "%-6d", test_6050_value);
        my_ips_show_string(*spi, 160, 170, buff_temp, MAGENTA, BLACK, 24, 0);
        // show x gyr vaule
        test_6050_value = read_gyro_x();
        sprintf((char *)buff_temp, "%-6d", test_6050_value);
        my_ips_show_string(*spi, 0, 200, buff_temp, MAGENTA, BLACK, 24, 0);
        // show y gyr vaule
        test_6050_value = read_gyro_y();
        sprintf((char *)buff_temp, "%-6d", test_6050_value);
        my_ips_show_string(*spi, 80, 200, buff_temp, MAGENTA, BLACK, 24, 0);
        // show z gyr vaule
        test_6050_value = read_gyro_z();
        sprintf((char *)buff_temp, "%-6d", test_6050_value);
        my_ips_show_string(*spi, 160, 200, buff_temp, MAGENTA, BLACK, 24, 0);
        // test 6050 end

        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}


static void my_task(lv_task_t * task)
{
    static uint8_t i = 0;
    xSemaphoreTake(mutexLvgl, portMAX_DELAY);  // 防止争夺资源
    lv_tileview_set_tile_act(guider_ui.screen_tileview_daily_temp, i++, 0, LV_ANIM_ON);
    xSemaphoreGive(mutexLvgl);
    if(i == 3){
        i = 0;
    }
}


// 初始化系统结构体
Esp32_State myesp32;

void app_main(void)
{
    // 显示一些系统信息
    ESP_LOGE("软件", "IDF版本号: %s\r\n", esp_get_idf_version());
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGE("硬件", "cpu 核心数: %d", chip_info.cores);
    ESP_LOGE("硬件", "cpu 版本号: %d", chip_info.revision);
    ESP_LOGE("硬件: ", "特征: %d", chip_info.features);  // 总的显示
    ESP_LOGE("硬件: ", "embedded flash: %s\r\n", chip_info.features & CHIP_FEATURE_EMB_FLASH ? "Yes" : "No");
    ESP_LOGE("硬件: ", "WIFI: %s\r\n", chip_info.features & CHIP_FEATURE_WIFI_BGN ? "Yes" : "No");
    ESP_LOGE("硬件: ", "Bluetooth Low Energy: %s\r\n", chip_info.features & CHIP_FEATURE_BLE ? "Yes" : "No");
    ESP_LOGE("硬件: ", "Bluetooth: %s\r\n", chip_info.features & CHIP_FEATURE_BT ? "Yes" : "No");
    ESP_LOGE("硬件: ", "IEEE 802.15.4: %s\r\n", chip_info.features & CHIP_FEATURE_IEEE802154 ? "Yes" : "No");
    ESP_LOGE("硬件: ", "embedded psram: %s\r\n", chip_info.features & CHIP_FEATURE_EMB_PSRAM ? "Yes" : "No");
    ESP_LOGE("硬件: ", "Flash 容量: %dMB", spi_flash_get_chip_size() / (1024 * 1024));  // 

    // 进度条变量初始化
    uint8_t barlength_count = 1;

    // // 初始化系统结构体
    // Esp32_State myesp32;
    // memset(&myesp32, 0, sizeof(Esp32_State));
    Init_Myesp32(&myesp32);

    // --- 新的lvgl ---
    my_lvgl_init();
    setup_ui(&guider_ui);
    
    // init led
    lv_label_set_text(guider_ui.scr_init_label_state, "gpio initing");
    led_init_my();
    lv_label_set_text(guider_ui.scr_init_label_state, "gpio inited");
    // 这里设置进度条的长度
    lv_bar_set_value(guider_ui.scr_init_bar_init_state, barlength_count++ * 1.0 / BARLENGTH * BARLENGTHMAX, LV_ANIM_ON);

    // init touchPad
    lv_label_set_text(guider_ui.scr_init_label_state, "touchPad initing");
    touchpad_init();
    lv_label_set_text(guider_ui.scr_init_label_state, "touchPad inited");
    // 这里设置进度条的长度
    lv_bar_set_value(guider_ui.scr_init_bar_init_state, barlength_count++ * 1.0 / BARLENGTH * BARLENGTHMAX, LV_ANIM_ON);

    // init 6050
    lv_label_set_text(guider_ui.scr_init_label_state, "MPU6050 initing");
    i2c_master_init();
    while(!init_mpu6050());
    lv_label_set_text(guider_ui.scr_init_label_state, "MPU6050 inited");
    // 这里设置进度条的长度
    lv_bar_set_value(guider_ui.scr_init_bar_init_state, barlength_count++ * 1.0 / BARLENGTH * BARLENGTHMAX, LV_ANIM_ON);
    

    // Initialize NVS
    lv_label_set_text(guider_ui.scr_init_label_state, "NVS initing");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    lv_label_set_text(guider_ui.scr_init_label_state, "NVS inited");
    lv_bar_set_value(guider_ui.scr_init_bar_init_state, barlength_count++ * 1.0 / BARLENGTH * BARLENGTHMAX, LV_ANIM_ON);

    // 这里判断是否需要重新配置WiFi（先每次都配置，然后再改）配置完保存配置，然后WiFi初始化的时候从nvs读配置参数

    // init wifi
    lv_label_set_text(guider_ui.scr_init_label_state, "wifi initing");
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    lv_label_set_text(guider_ui.scr_init_label_state, "wifi inited");
    lv_bar_set_value(guider_ui.scr_init_bar_init_state, barlength_count++ * 1.0 / BARLENGTH * BARLENGTHMAX, LV_ANIM_ON);
    vTaskDelay(pdMS_TO_TICKS(500));

    
    // 创建各个任务,然后创建新的屏幕和屏幕上的元素，并开启画面切换
    lv_label_set_text(guider_ui.scr_init_label_state, "task initing");
    setup_scr_screen(&guider_ui);
    create_task(&myesp32);
    
    xSemaphoreTake(mutexLvgl, portMAX_DELAY);  // 防止争夺资源
    lv_label_set_text(guider_ui.scr_init_label_state, "task inited");
    lv_bar_set_value(guider_ui.scr_init_bar_init_state, barlength_count++ * 1.0 / BARLENGTH * BARLENGTHMAX, LV_ANIM_ON);
    lv_scr_load_anim(guider_ui.screen, LV_SCR_LOAD_ANIM_FADE_ON, 1000, 0, true);
    xSemaphoreGive(mutexLvgl);

    // 创建LVGL的任务
    xSemaphoreTake(mutexLvgl, portMAX_DELAY);  // 防止争夺资源
    lv_task_t * task = lv_task_create(my_task, 5000, LV_TASK_PRIO_MID, NULL);
    lv_task_ready(task);
    xSemaphoreGive(mutexLvgl);

    while(1){
        vTaskDelay(pdMS_TO_TICKS(10*60*1000));
    }
}