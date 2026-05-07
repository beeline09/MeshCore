#pragma once
#include <cstdint>

#define WIFI_MAX_NETWORKS     5
#define WIFI_TCP_PORT_DEFAULT 5000

#define COMMS_MODE_BLE   0
#define COMMS_MODE_WIFI  1
#define COMMS_MODE_USB   2

#define IP_MODE_DHCP     0
#define IP_MODE_STATIC   1

struct WifiNetwork {
  char ssid[33];
  char password[64];
};

struct WifiPrefs {
  uint8_t     comms_mode;                       // COMMS_MODE_BLE / _WIFI / _USB
  uint8_t     network_count;
  WifiNetwork networks[WIFI_MAX_NETWORKS];
  uint8_t     ip_mode;                          // IP_MODE_DHCP / IP_MODE_STATIC
  char        static_ip[16];
  char        static_gw[16];
  char        static_mask[16];
  uint16_t    tcp_port;                         // 0 = use WIFI_TCP_PORT_DEFAULT
};
