#pragma once

#include <Arduino.h>

namespace ControlConfig {

// Zwei normale WPA/WPA2-WLANs (kein Captive Portal / WPA-Enterprise).
constexpr char WIFI_SSID_1[] = "HIER_SSID_1_EINTRAGEN";
constexpr char WIFI_PASSWORD_1[] = "HIER_PASSWORT_1_EINTRAGEN";
constexpr char WIFI_SSID_2[] = "HIER_SSID_2_EINTRAGEN";
constexpr char WIFI_PASSWORD_2[] = "HIER_PASSWORT_2_EINTRAGEN";

constexpr char HOSTNAME[] = "controlx4";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

// OTA bleibt aus, solange hier noch der Platzhalter steht.
constexpr bool ENABLE_OTA = true;
constexpr char OTA_PASSWORD[] = "HIER_OTA_PASSWORT_AENDERN";

}  // namespace ControlConfig
