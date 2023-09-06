#include <stdio.h>
#include <stdlib.h>
#include "driver/i2s.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include "AsyncUDP.h"
#include "Arduino.h"
#include "utils.h"

#define pushButton_pin   32
#define LED_pin   14

#define AP_CONNECT_N_TRIES 10
#define AP_CONNECT_TRY_GAP_MS 1000
#define UDP_PAYLOAD_SIZE 256

// Przyjmuje wartość od 0 do 2 - wyznacza indeks esp 
// - 0 dla pierwszego ESP32
// - 1 dla drugiego ESP32
// - 2 dla trzeciego ESP32
// Jedyna różnica w kodzie pomiędzy różnymi ESP to ta zmienna
const int USER_IDX_ME = 0;
// maksymalna liczba aktywnych ESP - służy do resetowania licznika ()
const int MAX_USERS = 3;
// port UDP na którym nasłuchuje każde ESP
// - jest wspólne ponieważ port wysyłania nie ma znaczenia
const uint16_t LISTEN_PORT = 12345;

AsyncUDP udp;
// Tablica adresów IP dla ESP
const IPAddress USER_ADDR[MAX_USERS] = {
  IPAddress(192, 168, 0, 160),
  IPAddress(192, 168, 0, 161),
  IPAddress(192, 168, 0, 162)
};
// Gateway AP jako konfiguracja adresów statycznych dla ESP
const IPAddress GATEWAY(192, 168, 0, 254);
const IPAddress SUBNET(255, 255, 255, 0);
// Stałe parametry połaczenia do AP
const char * SSID = "TP-Link_C798";
const char * PASSWD = "26097855"; 

// Globalna zmienna wyznaczająca obecnie słuchanego ESP
int curr_user_listen_idx = USER_IDX_ME;

// Procedura obsługi przerwania 
// - wyznacz następne ESP do słuchania
// - cyklicznie rozpocznij od 0
// - zapal diodę jeżeli słuchamy siebie
void change_user() {
  curr_user_listen_idx++;
  if (curr_user_listen_idx >= MAX_USERS) {
    curr_user_listen_idx = 0;
  }

  if (curr_user_listen_idx == USER_IDX_ME) {
    digitalWrite(LED_pin, HIGH);
  } else {
    digitalWrite(LED_pin, LOW);
  }
}

// Konfiguracja wstępna
void setup_config() {
  initArduino();
  Serial.begin(115200);

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);

  REG_WRITE(PIN_CTRL, 0b000011110000);
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3); 
  
  pinMode(LED_pin, OUTPUT);
  pinMode(pushButton_pin, INPUT);
}

// Spróbuj podłączyć się do AP
// - ustaw statyczne IP 
// - spróbuj podłączyć się N_TRIES razy, czekając TRY_GAP_MS czasu pomiędzy próbami
// - zwróć TRUE jeżeli się udało
bool connect_to_ap() {

  WiFi.mode(WIFI_STA);
  // Set static IP params -> DHCP query to AP 
  WiFi.config(USER_ADDR[USER_IDX_ME], GATEWAY, SUBNET);
  WiFi.begin(SSID, PASSWD);

  for (int i = 0; WiFi.status() != WL_CONNECTED && i < AP_CONNECT_N_TRIES; i++) {
    delay(AP_CONNECT_TRY_GAP_MS/2);
    digitalWrite(LED_pin, HIGH);
    delay(AP_CONNECT_TRY_GAP_MS/2);
    digitalWrite(LED_pin, LOW);
  }

  return (WiFi.status() == WL_CONNECTED);
}

extern "C" void app_main() {
	
  setup_config();

  bool connected = connect_to_ap();
  // Jeżeli nie udało się połączyć, zakończ program
  if (!connected) {
    return;
  } 

  // na początku słuchamy siebie - włącz diodę
  digitalWrite(LED_pin, (curr_user_listen_idx == USER_IDX_ME ? HIGH : LOW));

  // Ustaw procedurę przerwań
  attachInterrupt(pushButton_pin, change_user, RISING);
  
  //Nasłuch portu w celu otrzymania pakietów 
  if (udp.listen(LISTEN_PORT)) 
  {          
    udp.onPacket([](AsyncUDPPacket packet)
    {
        size_t writesize;
        // Słuchamy tylko obecnie wybranego ESP
        if (packet.remoteIP() == USER_ADDR[curr_user_listen_idx]) {
          i2s_write(I2S_NUM_0, packet.data(),packet.length(), &writesize, 1000);
        }
    });
  }

  uint8_t rxbuf[UDP_PAYLOAD_SIZE];
  while (1)  {
    size_t readsize, writesize;
    esp_err_t rxfb = i2s_read(I2S_NUM_0 , rxbuf, UDP_PAYLOAD_SIZE, &readsize , 1000);
    if (rxfb == ESP_OK)
    { 
      // Wysyłamy do wszystkich poza sobą/
      for (int i = 0; i < MAX_USERS; i++) {
        if (i != USER_IDX_ME) udp.writeTo(rxbuf, readsize, USER_ADDR[i], LISTEN_PORT);
      }
      // Jeżeli jesteśmy "aktywnym" użytkownikiem
      // - puść bezpośrednio dźwięk 
      if (curr_user_listen_idx == USER_IDX_ME) {
        i2s_write(I2S_NUM_0, rxbuf, readsize, &writesize, 1000);
      }
    }
   }
}