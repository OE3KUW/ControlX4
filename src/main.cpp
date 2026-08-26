#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "control.h"

namespace {

constexpr bool RELAY_ACTIVE_LOW = true;
constexpr uint8_t RELAY_PINS[] = {32, 33, 25, 26};
constexpr size_t RELAY_COUNT = sizeof(RELAY_PINS) / sizeof(RELAY_PINS[0]);
constexpr char FIRMWARE_VERSION[] = "3.0 OTA";
constexpr uint8_t STATUS_LED_PIN = 23;
constexpr bool STATUS_LED_ACTIVE_HIGH = true;
constexpr uint32_t LED_SLOW_HALF_PERIOD_MS = 500;
constexpr uint32_t LED_FAST_HALF_PERIOD_MS = 125;
constexpr uint32_t LED_FAILURE_PERIOD_MS = 1000;
constexpr uint32_t LED_FAILURE_ON_MS = 100;
constexpr char OTA_PASSWORD_PLACEHOLDER[] = "HIER_OTA_PASSWORT_AENDERN";

enum class LedMode {
  FirstNetwork,
  SecondNetwork,
  Connected,
  Failed
};

WebServer server(80);
bool relayStates[RELAY_COUNT] = {false, false, false, false};
bool otaActive = false;
bool stationWasConnected = false;
LedMode ledMode = LedMode::Failed;
uint32_t ledModeStartedAt = 0;

const char PAGE_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ControlX4</title>
  <style>
    :root { color-scheme: light; font-family: system-ui, sans-serif; }
    * { box-sizing: border-box; }
    body { margin: 0; background: #f4f6f8; color: #17212b; }
    main { width: min(100% - 32px, 620px); margin: 40px auto; }
    header { display: flex; align-items: baseline; justify-content: space-between; gap: 16px; }
    h1 { margin: 0 0 8px; font-size: 2rem; letter-spacing: 0; }
    .version { display: block; color: #68737d; font-size: .85rem; }
    #connection { color: #5f6b76; font-size: .9rem; }
    .relays { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 12px; margin-top: 24px; }
    .relay { background: #fff; border: 1px solid #d9e0e6; border-radius: 8px; padding: 18px; }
    .relay h2 { margin: 0 0 4px; font-size: 1.05rem; letter-spacing: 0; }
    .pin { color: #68737d; font-size: .85rem; }
    button { width: 100%; min-height: 48px; margin-top: 18px; border: 0; border-radius: 6px; background: #dfe5ea; color: #17212b; font: inherit; font-weight: 700; cursor: pointer; }
    button.on { background: #16825d; color: #fff; }
    button:disabled { cursor: wait; opacity: .65; }
    @media (max-width: 460px) { main { margin-top: 24px; } .relays { grid-template-columns: 1fr; } }
  </style>
</head>
<body>
  <main>
    <header><div><h1>ControlX4</h1><span id="version" class="version">Version ...</span></div><span id="connection">Verbinde...</span></header>
    <section class="relays" aria-label="Relaissteuerung">
      <article class="relay"><h2>Relais 1</h2><span class="pin">GPIO 32</span><button data-id="1">AUS</button></article>
      <article class="relay"><h2>Relais 2</h2><span class="pin">GPIO 33</span><button data-id="2">AUS</button></article>
      <article class="relay"><h2>Relais 3</h2><span class="pin">GPIO 25</span><button data-id="3">AUS</button></article>
      <article class="relay"><h2>Relais 4</h2><span class="pin">GPIO 26</span><button data-id="4">AUS</button></article>
    </section>
  </main>
  <script>
    const connection = document.querySelector('#connection');
    const version = document.querySelector('#version');
    const buttons = [...document.querySelectorAll('button[data-id]')];

    function render(status) {
      const states = status.relays;
      buttons.forEach((button, index) => {
        const on = Boolean(states[index]);
        button.textContent = on ? 'EIN' : 'AUS';
        button.classList.toggle('on', on);
        button.dataset.on = on ? '1' : '0';
        button.setAttribute('aria-pressed', String(on));
      });
      version.textContent = `Version ${status.version}`;
      connection.textContent = 'Verbunden';
    }

    async function loadStatus() {
      try {
        const response = await fetch('/api/status', { cache: 'no-store' });
        if (!response.ok) throw new Error();
        render(await response.json());
      } catch (_) {
        connection.textContent = 'Keine Verbindung';
      }
    }

    buttons.forEach(button => button.addEventListener('click', async () => {
      const nextState = button.dataset.on === '1' ? 'off' : 'on';
      button.disabled = true;
      try {
        const response = await fetch(`/api/relay?id=${button.dataset.id}&state=${nextState}`, { method: 'POST' });
        if (!response.ok) throw new Error();
        render(await response.json());
      } catch (_) {
        connection.textContent = 'Befehl fehlgeschlagen';
      } finally {
        button.disabled = false;
      }
    }));

    loadStatus();
  </script>
</body>
</html>
)HTML";

bool hasConfiguredOtaPassword() {
  return strlen(ControlConfig::OTA_PASSWORD) >= 8 &&
         strcmp(ControlConfig::OTA_PASSWORD, OTA_PASSWORD_PLACEHOLDER) != 0;
}

uint8_t relayLevel(bool on) {
  if (RELAY_ACTIVE_LOW) {
    return on ? LOW : HIGH;
  }
  return on ? HIGH : LOW;
}

void writeStatusLed(bool on) {
  digitalWrite(STATUS_LED_PIN, on == STATUS_LED_ACTIVE_HIGH ? HIGH : LOW);
}

void updateStatusLed() {
  const uint32_t elapsed = millis() - ledModeStartedAt;
  bool on = false;

  switch (ledMode) {
    case LedMode::FirstNetwork:
      on = (elapsed / LED_SLOW_HALF_PERIOD_MS) % 2 == 0;
      break;
    case LedMode::SecondNetwork:
      on = (elapsed / LED_FAST_HALF_PERIOD_MS) % 2 == 0;
      break;
    case LedMode::Connected:
      on = true;
      break;
    case LedMode::Failed:
      on = elapsed % LED_FAILURE_PERIOD_MS < LED_FAILURE_ON_MS;
      break;
  }

  writeStatusLed(on);
}

void setLedMode(LedMode mode) {
  ledMode = mode;
  ledModeStartedAt = millis();
  updateStatusLed();
}

void configureStatusLed() {
  writeStatusLed(false);
  pinMode(STATUS_LED_PIN, OUTPUT);
  setLedMode(LedMode::Failed);
}

void setRelay(size_t index, bool on) {
  digitalWrite(RELAY_PINS[index], relayLevel(on));
  relayStates[index] = on;
}

String statusJson() {
  String json = F("{\"relays\":[");
  for (size_t index = 0; index < RELAY_COUNT; ++index) {
    if (index > 0) {
      json += ',';
    }
    json += relayStates[index] ? F("true") : F("false");
  }
  json += F("],\"wifiConnected\":");
  json += WiFi.status() == WL_CONNECTED ? F("true") : F("false");
  json += F(",\"version\":\"");
  json += FIRMWARE_VERSION;
  json += '"';
  json += F("}");
  return json;
}

void sendJson(int statusCode, const String& json) {
  server.sendHeader(F("Cache-Control"), F("no-store"));
  server.send(statusCode, F("application/json; charset=utf-8"), json);
}

void handleRelayCommand() {
  const String relayArgument = server.arg("id");
  const String stateArgument = server.arg("state");

  Serial.printf("[WEB] Empfangen: POST %s, id=%s, state=%s\n",
                server.uri().c_str(), relayArgument.c_str(), stateArgument.c_str());

  const int relayNumber = relayArgument.toInt();
  const bool validRelay = relayNumber >= 1 && relayNumber <= static_cast<int>(RELAY_COUNT);
  const bool validState = stateArgument == "on" || stateArgument == "off";

  if (!validRelay || !validState) {
    Serial.println(F("[WEB] Nicht ausgefuehrt: ungueltiger Befehl"));
    sendJson(400, F("{\"error\":\"Ungueltiger Befehl\"}"));
    return;
  }

  const size_t relayIndex = static_cast<size_t>(relayNumber - 1);
  const bool turnOn = stateArgument == "on";
  setRelay(relayIndex, turnOn);

  Serial.printf("[RELAIS] Ausgefuehrt: Relais %d (GPIO %u) -> %s\n",
                relayNumber, RELAY_PINS[relayIndex], turnOn ? "EIN" : "AUS");
  sendJson(200, statusJson());
}

void configureRelays() {
  // Den AUS-Pegel vor pinMode setzen, damit beim Start kein kurzer Impuls entsteht.
  for (size_t index = 0; index < RELAY_COUNT; ++index) {
    digitalWrite(RELAY_PINS[index], relayLevel(false));
    pinMode(RELAY_PINS[index], OUTPUT);
    setRelay(index, false);
  }
}

bool tryWifi(const char* ssid, const char* password, uint8_t number, LedMode mode) {
  setLedMode(mode);
  WiFi.begin(ssid, password);
  Serial.printf("WLAN-Versuch %u/2: %s\n", number, ssid);

  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < ControlConfig::WIFI_CONNECT_TIMEOUT_MS) {
    updateStatusLed();
    delay(10);
  }

  if (WiFi.status() == WL_CONNECTED) {
    stationWasConnected = true;
    setLedMode(LedMode::Connected);
    Serial.printf("WLAN verbunden, IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Bedienseite: http://%s.local/\n", ControlConfig::HOSTNAME);
    Serial.printf("Signalstaerke: %d dBm\n", WiFi.RSSI());
    return true;
  }

  Serial.printf("WLAN-Versuch %u nach 15 Sekunden abgebrochen\n", number);
  WiFi.disconnect(false, false);
  delay(100);
  return false;
}

void configureNetwork() {
  WiFi.mode(WIFI_OFF);
  WiFi.setHostname(ControlConfig::HOSTNAME);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);

  if (tryWifi(ControlConfig::WIFI_SSID_1, ControlConfig::WIFI_PASSWORD_1,
              1, LedMode::FirstNetwork) ||
      tryWifi(ControlConfig::WIFI_SSID_2, ControlConfig::WIFI_PASSWORD_2,
              2, LedMode::SecondNetwork)) {
    WiFi.setAutoReconnect(true);
    return;
  }

  setLedMode(LedMode::Failed);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ControlConfig::WIFI_SSID_2, ControlConfig::WIFI_PASSWORD_2);
  Serial.println(F("Beide WLANs nicht erreichbar; Wiederverbindung mit WLAN 2 laeuft"));
}

void configureOtaAndMdns() {
  if (ControlConfig::ENABLE_OTA && hasConfiguredOtaPassword()) {
    ArduinoOTA.setHostname(ControlConfig::HOSTNAME);
    ArduinoOTA.setPassword(ControlConfig::OTA_PASSWORD);
    ArduinoOTA.onStart([]() {
      Serial.println(F("[OTA] Update gestartet, Relais werden ausgeschaltet"));
      for (size_t index = 0; index < RELAY_COUNT; ++index) {
        setRelay(index, false);
      }
    });
    ArduinoOTA.onEnd([]() {
      Serial.println(F("\n[OTA] Update erfolgreich"));
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      const unsigned int percent = total > 0 ? (progress * 100U) / total : 0;
      Serial.printf("\r[OTA] Fortschritt: %u%%", percent);
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("\n[OTA] Fehler %u\n", error);
    });
    ArduinoOTA.begin();
    otaActive = true;
    MDNS.addService("http", "tcp", 80);
    Serial.printf("OTA bereit: %s.local:3232\n", ControlConfig::HOSTNAME);
    return;
  }

  if (MDNS.begin(ControlConfig::HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
  }

  if (ControlConfig::ENABLE_OTA) {
    Serial.println(F("OTA deaktiviert: eigenes OTA_PASSWORD in control.h eintragen"));
  }
}

void configureWebServer() {
  server.on("/", HTTP_GET, []() {
    Serial.println(F("[WEB] Empfangen: GET /"));
    server.sendHeader(F("Cache-Control"), F("no-store"));
    server.send_P(200, PSTR("text/html; charset=utf-8"), PAGE_HTML);
    Serial.println(F("[WEB] Ausgefuehrt: Bedienseite gesendet"));
  });

  server.on("/api/status", HTTP_GET, []() {
    Serial.println(F("[WEB] Empfangen: GET /api/status"));
    sendJson(200, statusJson());
    Serial.println(F("[WEB] Ausgefuehrt: Relaisstatus gesendet"));
  });

  server.on("/api/relay", HTTP_POST, handleRelayCommand);

  server.onNotFound([]() {
    Serial.printf("[WEB] Empfangen: %s %s\n",
                  server.method() == HTTP_GET ? "GET" : "ANDERE", server.uri().c_str());
    Serial.println(F("[WEB] Nicht ausgefuehrt: Pfad nicht gefunden"));
    sendJson(404, F("{\"error\":\"Nicht gefunden\"}"));
  });

  server.begin();
  Serial.println(F("HTTP-Server gestartet"));
}

void maintainNetwork() {
  const bool stationConnected = WiFi.status() == WL_CONNECTED;
  if (stationConnected && !stationWasConnected) {
    stationWasConnected = true;
    setLedMode(LedMode::Connected);
    Serial.printf("WLAN wieder verbunden, IP: %s\n", WiFi.localIP().toString().c_str());
  } else if (!stationConnected && stationWasConnected) {
    stationWasConnected = false;
    setLedMode(LedMode::Failed);
    Serial.println(F("WLAN-Verbindung verloren, Wiederverbindung laeuft"));
  }
  updateStatusLed();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.printf("ControlX4 Version %s startet\n", FIRMWARE_VERSION);
  configureStatusLed();
  configureRelays();
  Serial.println(F("Alle Relais sind AUS"));

  configureNetwork();
  configureOtaAndMdns();
  configureWebServer();
}

void loop() {
  server.handleClient();
  if (otaActive) {
    ArduinoOTA.handle();
  }
  maintainNetwork();
  delay(2);
}
