# ControlX4 mit Cron auf Kubuntu steuern

Diese Anleitung richtet die zeitgesteuerte HTTP-Steuerung auf dem Raspberry Pi
400 ein. Vor einem Anschluss an einen Jalousiemotor den Abschnitt
"Jalousiemotoren" beachten.

## 1. ControlX4 im Netzwerk finden

Nach dem Start schreibt ControlX4 seine DHCP-Adresse auf die serielle
Schnittstelle. Im gleichen lokalen Netzwerk sollte ausserdem dieser Name
funktionieren:

```text
http://controlx4.local/
```

Falls `.local` im Schulnetz nicht funktioniert, muss statt des Namens die
serielle IP-Adresse verwendet werden. Ideal ist eine DHCP-Reservierung durch
den Netzwerkadministrator, damit diese IP gleich bleibt. Webseite und API
benoetigen keine Anmeldung.

Ein Schul-WLAN kann Endgeraete voneinander isolieren. Dann kann der Raspberry
Pi den ESP32 trotz erfolgreicher WLAN-Verbindung nicht erreichen. Der folgende
Test muss funktionieren:

```sh
ping -c 3 controlx4.local
```

## 2. Voraussetzungen auf Kubuntu pruefen

```sh
command -v curl
systemctl status cron --no-pager
```

Fehlt `curl` oder der Cron-Dienst, kann beides ueber die Kubuntu-Paketverwaltung
installiert werden. Fuer Namen mit `.local` werden normalerweise
`avahi-daemon` und `libnss-mdns` benoetigt.

## 3. Steuerskript installieren

Im Terminal:

```sh
mkdir -p "$HOME/bin" "$HOME/.config/controlx4" "$HOME/.local/state/controlx4"
cp tools/controlx4.sh "$HOME/bin/controlx4.sh"
chmod 700 "$HOME/bin/controlx4.sh"
nano "$HOME/.config/controlx4/controlx4.conf"
```

In die geoeffnete Datei kommt nur diese Zeile:

```sh
CONTROLX4_URL='http://controlx4.local'
```

Danach speichern und die Datei schuetzen:

```sh
chmod 600 "$HOME/.config/controlx4/controlx4.conf"
```

Wenn `.local` nicht funktioniert, beispielsweise so die IP verwenden:

```sh
CONTROLX4_URL='http://10.20.30.40'
```

## 4. Befehle von Hand testen

Status lesen:

```sh
"$HOME/bin/controlx4.sh" status
```

Relais 1 ein- und ausschalten:

```sh
"$HOME/bin/controlx4.sh" relay 1 on
"$HOME/bin/controlx4.sh" relay 1 off
```

Alle vier Relais ausschalten:

```sh
"$HOME/bin/controlx4.sh" all-off
```

Eine erfolgreiche Antwort sieht ungefaehr so aus:

```json
{"relays":[false,false,false,false],"wifiConnected":true}
```

## 5. Cron einrichten

Die persoenliche Cron-Tabelle oeffnen:

```sh
crontab -e
```

Eine Cron-Zeile besteht aus:

```text
Minute Stunde Monatstag Monat Wochentag Befehl
```

Dieses reine Testbeispiel schaltet Relais 1 montags bis freitags um 07:00 ein
und um 07:01 wieder aus:

```cron
0 7 * * 1-5 /home/kuran/bin/controlx4.sh relay 1 on >> /home/kuran/.local/state/controlx4/cron.log 2>&1
1 7 * * 1-5 /home/kuran/bin/controlx4.sh relay 1 off >> /home/kuran/.local/state/controlx4/cron.log 2>&1
```

Alle Relais jeden Abend um 18:00 ausschalten:

```cron
0 18 * * * /home/kuran/bin/controlx4.sh all-off >> /home/kuran/.local/state/controlx4/cron.log 2>&1
```

Installierte Eintraege anzeigen:

```sh
crontab -l
```

Protokoll ansehen:

```sh
tail -n 50 "$HOME/.local/state/controlx4/cron.log"
```

Cron verwendet keine grafische Sitzung und nur eine kleine Standardumgebung.
Darum stehen in den Beispielen absolute Pfade.

## Jalousiemotoren

Die obigen Cron-Zeilen sind noch keine fertige Jalousiesteuerung. Bei einem
Motor mit getrennten Leitungen fuer AUF und AB darf niemals beides gleichzeitig
eingeschaltet werden. Fuer eine sichere Umsetzung werden mindestens benoetigt:

- eindeutige Zuordnung jedes Relais zu Motor und Richtung;
- elektrische Verriegelung, die AUF und AB auch bei einem Softwarefehler trennt;
- zusaetzliche Softwareverriegelung mit Umschaltpause;
- automatische Abschaltung nach der gemessenen maximalen Laufzeit;
- definierter AUS-Zustand bei Neustart und WLAN-Ausfall.

Vor dem Einbau werden daher noch die Anzahl der Motoren, die vier
Relaisfunktionen und die benoetigte Fahrzeit erfasst. Arbeiten an 230 V und der
Einbau in die Decke gehoeren in die Hand einer Elektrofachkraft.
