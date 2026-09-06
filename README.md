# ControlX4 3.1

Websteuerung fuer das ESP32 Relay X4 V1.1 mit ESP32-WROOM-32E (N4).

ControlX4 versucht zuerst WLAN 1 und nach 15 Sekunden ohne Verbindung WLAN 2.
Die Bedienseite ist im verbundenen lokalen Netzwerk unter
`http://controlx4.local/` oder ueber die seriell ausgegebene DHCP-Adresse
erreichbar. Nach zwei fehlgeschlagenen Startversuchen versucht der ESP32 die
Verbindung mit WLAN 2 automatisch erneut aufzubauen.

## Vor dem Bauen konfigurieren

Zuerst die oeffentliche Vorlage in die lokale, von Git ignorierte Konfiguration
kopieren:

```sh
cp include/control.example.h include/control.h
```

Danach muessen in `include/control.h` diese Platzhalter ersetzt werden:

```cpp
constexpr char WIFI_SSID_1[] = "MeinErstesWLAN";
constexpr char WIFI_PASSWORD_1[] = "PasswortErstesWLAN";
constexpr char WIFI_SSID_2[] = "MeinZweitesWLAN";
constexpr char WIFI_PASSWORD_2[] = "PasswortZweitesWLAN";
constexpr char OTA_PASSWORD[] = "EigenesOtaPasswort";
```

Das OTA-Passwort muss mindestens acht Zeichen haben. `control.h` enthaelt
Geheimnisse und wird deshalb von Git ignoriert. Webseite und HTTP-API haben
bewusst keine eigene Anmeldung.

Die einfache Konfiguration funktioniert fuer normale offene oder
WPA/WPA2-PSK-Netze. Schulnetze mit Browser-Anmeldeseite, WPA-Enterprise,
Benutzerzertifikat oder Client-Isolation benoetigen Unterstuetzung durch den
Netzwerkadministrator.

## Pinbelegung

| Funktion | GPIO |
| --- | ---: |
| Relais 1 | 32 |
| Relais 2 | 33 |
| Relais 3 | 25 |
| Relais 4 | 26 |
| Programmierbare LED | 23 |
| Programmiertaste | 0 |

Die Relaisausgaenge sind als `active LOW` konfiguriert. Beim Einschalten und
vor einem OTA-Update werden alle Relais ausgeschaltet.

Relais 1 und 2 sowie Relais 3 und 4 sind jeweils gegenseitig verriegelt. Sobald
ein Relais eines Paares eingeschaltet ist, kann das andere erst eingeschaltet
werden, nachdem das aktive Relais ausgeschaltet wurde. Die Firmware prueft
diese Bedingung auch bei direkten HTTP-API-Aufrufen; ein unzulaessiger
Einschaltbefehl wird mit HTTP-Status `409 Conflict` abgelehnt.

Die programmierbare LED zeigt den WLAN-Zustand:

| LED | Bedeutung |
| --- | --- |
| Langsames Blinken | Verbindung mit WLAN 1 wird geprueft |
| Schnelles Blinken | Verbindung mit WLAN 2 wird geprueft |
| Dauerlicht | WLAN verbunden |
| Alle 1 s ein kurzer Lichtimpuls | Beide Startversuche fehlgeschlagen |

## USB bauen und uebertragen

Am CP2102-6-in-1-Adapter gilt: DIP 1 ON, DIP 2 OFF und S1 auf `232-TTL`.

```sh
pio run
pio run --target upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

Vor dem USB-Upload GPIO0 mit GND verbinden und EN kurz druecken. Nach dem
Upload GPIO0-GND entfernen und EN erneut druecken.

## OTA-Update ueber WLAN

Der erste Stand mit WLAN und OTA muss einmal per USB geladen werden. Danach
kann im Projektverzeichnis ohne offenen ESP32-Aufbau aktualisiert werden:

```sh
read -rsp "OTA-Passwort: " CONTROLX4_OTA_PASSWORD
export CONTROLX4_OTA_PASSWORD
echo
pio run -e controlx4-ota --target upload
unset CONTROLX4_OTA_PASSWORD
```

Das eingegebene Passwort muss mit `OTA_PASSWORD` aus `control.h`
uebereinstimmen. Wenn mDNS im Netzwerk gesperrt ist, die seriell ausgegebene IP
angeben:

```sh
pio run -e controlx4-ota --target upload --upload-port 10.20.30.40
```

OTA ist eine bequeme Wartungsmoeglichkeit, aber kein Ersatz fuer einen
zugaenglichen stromlosen Reset- und Programmierweg.

## Raspberry Pi und Cron

Die ausfuehrliche Anleitung steht in `CRON_ANLEITUNG.md`. Das Hilfsskript liegt
unter `tools/controlx4.sh`.

> Achtung: Die Firmware besitzt eine softwareseitige Verriegelung fuer die
> Relaispaare 1/2 und 3/4. Eine Jalousiesteuerung sollte zusaetzlich elektrisch
> verriegelt werden und benoetigt eine zur Anwendung passende Laufzeitbegrenzung.
> Arbeiten an 230 V duerfen nur spannungsfrei und durch entsprechend
> qualifizierte Personen durchgefuehrt werden.
