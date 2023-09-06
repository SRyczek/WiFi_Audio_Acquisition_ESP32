#include "driver/i2s.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include "AsyncUDP.h"
#include "Arduino.h"

i2s_config_t i2s_config = {
    .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX  | I2S_MODE_RX),  
    .sample_rate = 16000,
    .bits_per_sample =  I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,                                            
    .tx_desc_auto_clear = true
                            
};

i2s_pin_config_t pin_config = {
    .bck_io_num = 26,
    .ws_io_num = 25,
    .data_out_num = 22,
    .data_in_num = 19                                                       
};




static inline void wifi_AP_begin(){
    WiFi.enableAP(true);
      Serial.println(WiFi.softAP("someSSID", "somePassword", 1, 0, 8)); //ssid,pw,ch,hid,conn
      IPAddress myIP = WiFi.softAPIP();
      Serial.print("AP IP address: ");
      Serial.println(myIP);
}

