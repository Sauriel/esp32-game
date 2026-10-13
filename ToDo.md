# Auftrag: modularer Datenübertragungsmodus für esp32-game

Implementiere in diesem Projekt einen separaten Modus, in dem Dateien über WLAN und einen Browser auf die MicroSD-Karte des Gameduino 2 übertragen werden können. Die vorhandene Spiel-/Controller-Demo muss weiterhin funktionieren. Der Moduswechsel wird durch die KEY-Taste am ESP32 ausgelöst und erfolgt über einen kontrollierten Neustart.

## 1. Bestand prüfen

Lies zuerst die aktuelle AGENTS.md, README, Build-Konfiguration, sdkconfig und den Code unter main/ sowie die beteiligten Komponenten. Arbeite mit den tatsächlich verwendeten Versionen und APIs; ändere nicht unnötig das Framework oder die Toolchain.

Bekannter Stand (gegen das aktuelle Repository prüfen):

- Klassischer ESP32, 4 MB Flash, kein vorausgesetztes PSRAM; Gameduino 2 und PS4-Controller über Bluepad32/BTstack.
- ESP-IDF mit Arduino als Komponente, kein reines Arduino-Sketch-Projekt.
- main.c startet BTstack und Bluepad32; main.cpp enthält setup()/loop().
- Vorhandene Module: display/, game/, input/, timing/.
- SPI: SCK 18, MISO 19, MOSI 23, Gameduino-CS 5, SD-CS 27.
- Der Nutzer hat KEY erfolgreich getestet: GPIO 0, INPUT_PULLUP, gedrückt = LOW. Einfache Flankenerkennung funktioniert bereits; Entprellung fehlt noch.
- Display::begin() verwendet im bekannten Stand GD.begin(0, PIN_CS, PIN_SD_CS). Daraus allein keine bereits gemountete oder beschreibbare SD-Karte ableiten; die Bedeutung der Flags in der konkreten GD2-Version prüfen.

## 2. Verhalten und Moduswahl

Zwei Modi: Game und Transfer.

- Nach Einschalten, Reset oder einem Neustart ohne gültige Modusanforderung startet Game.
- Ein vollständiger entprellter KEY-Tastendruck im Spiel fordert Transfer an. Der Wechsel wird beim Loslassen ausgelöst.
- Ein vollständiger entprellter KEY-Tastendruck in Transfer fordert Game an, ebenfalls beim Loslassen.
- Halten oder Kontaktprellen darf keine mehrfachen Wechsel verursachen. Ein beim Start bereits gedrückter Taster muss zunächst losgelassen werden; dieses erste Loslassen löst keinen Wechsel aus.
- Polling mit millis()/monotoner Zeit, beispielsweise 30 ms Entprellzeit. Kein blockierendes Warten und kein GPIO-Interrupt für diese Aufgabe.
- GPIO 0 ist ein Boot-Strapping-Pin. Nicht sofort bei gedrückter KEY-Taste neu starten und nicht „KEY beim Reset halten“ als Einstieg verwenden. Vor dem tatsächlichen Neustart erneut prüfen, dass KEY stabil losgelassen ist.
- Modusanforderung als einmaliges Ticket in NVS ablegen und vor dem Neustart committen. Beim nächsten Start genau einmal lesen, validieren und entfernen/committen, bevor der Zielmodus startet. So führt ein späterer gewöhnlicher Reset wieder zu Game.
- Bei fehlgeschlagenem Speichern/Löschen keinen unkontrollierten Neustart oder Neustartzyklus auslösen; Fehler anzeigen und protokollieren. NVS nicht bei jedem Frame beschreiben. Keine automatische Löschung der ganzen NVS-Partition zur Fehlerbehandlung.
- Keine zweite Firmware oder zusätzlichen App-Partitionen für die Modi; eine Firmware mit verzweigter Initialisierung.

## 3. Bluetooth wirklich nur im Spiel starten

Die Moduswahl muss vor der ersten Bluetooth-Initialisierung erfolgen, nicht erst in setup(). Prüfe insbesondere CONFIG_AUTOSTART_ARDUINO, initBluepad32()/app_main(), Arduino-Plattform und Aufrufreihenfolge der eingebundenen Komponenten.

- Game: bestehende Bluetooth-/Bluepad32-Initialisierung und Spielschleife erhalten; WLAN/Webserver nicht initialisieren.
- Transfer: BTstack/Bluepad32 und Bluetooth-Controller nicht initialisieren; GameInput::begin(), Game::begin() und Game::update()/render() nicht aufrufen.
- Der Transfer-Zweig muss trotzdem Arduino-Initialisierung und setup()/loop() zuverlässig erreichen. Nicht einfach vor btstack_run_loop_execute() zurückkehren, ohne zu prüfen, wer den Arduino-Task startet.
- Eine kleine C-kompatible Schnittstelle erlaubt main.c den Zugriff auf die bereits festgelegte Moduswahl. C++-Implementierungen korrekt mit extern "C" anbinden. Es darf keine zweite, widersprüchliche Bootentscheidung geben.
- Prüfe für die konkrete ESP-IDF-/BTstack-Kombination, ob im Transfer-Modus vor Controller-Initialisierung esp_bt_controller_mem_release(ESP_BT_MODE_BTDM) sicher verwendbar ist. Fehlercodes prüfen. Keine Bluedroid-spezifische Freigabe als Ersatz für BTstack-Analyse verwenden.
- Bluetooth-Speicherfreigabe ist innerhalb desselben Bootzyklus nicht rückgängig zu machen; deshalb der Neustart. Statisch gebundener BTstack-Speicher wird durch unterlassene Initialisierung nicht automatisch vollständig zu Heap. Dokumentiere tatsächliche Grenzen und gemessene Werte statt pauschaler RAM-Versprechen.

## 4. Module und Verantwortlichkeiten

Nutze kleine, fachlich abgegrenzte Module und die vorhandenen Namespace-/Dateikonventionen. Keine abstrakte Plugin-Architektur, unnötigen Basisklassen oder Datei pro einzelner Funktion. Folgende Aufteilung ist ein Vorschlag; passe sie bei begründetem Bedarf an:

| Dateien unter main/ | Verantwortung |
| --- | --- |
| config/BoardConfig.h | Gemeinsame Pinbelegung und Hardwarekonstanten; keine mehrfach definierten SPI-/CS-Pins. |
| config/WifiCredentials.example.h und ignorierte WifiCredentials.h | Lokale WLAN-Konfiguration mit dokumentierter Beispieldatei. |
| input/ModeButton.h/.cpp | KEY-Polling, Entprellung und einmaliges Ereignis nach vollständigem Tastendruck; kennt weder NVS noch WLAN. |
| app/BootMode.h/.cpp | Modusdatentyp, einmalige Bootentscheidung und NVS-Ticket; kleine C-Schnittstelle für main.c. |
| app/ModeSwitch.h/.cpp | Wechselanforderungen koordinieren, laufende Operationen auslaufen lassen, Neustart ausführen. |
| storage/SdStorage.h/.cpp | Mounten, Lesen, Schreiben, Auflisten, Dateihandles und gemeinsame SPI-Synchronisierung. Kein HTTP/WLAN. |
| network/WifiConnection.h/.cpp | WLAN-Verbindung, Ereignisse, IP, begrenzte Wiederverbindung und Status. Kein SD-Zugriff. |
| transfer/FileServer.h/.cpp | HTTP-Routen und Streaming über SdStorage; meldet Wechselwünsche, führt selbst keinen Neustart aus. |
| transfer/TransferMode.h/.cpp | Transfer-Lebenszyklus, Status und Zusammenarbeit der Dienste. |
| transfer/TransferView.h/.cpp | Darstellung von Verbindung, URL, Transferfortschritt und Fehlern auf dem Gameduino. |
| transfer/web/index.html | Browseroberfläche als im Flash eingebettete Ressource mit lokalem CSS/JS; keine externen Abhängigkeiten. |
| main.cpp | Gemeinsames Setup und Weiterleitung an den gewählten Modus; keine HTTP-Handler, GPIO-Entprellung, NVS-, Dateisystem- oder WLAN-Implementierung. |

Headers so schlank wie möglich halten. Ressourcen besitzen einen klaren Besitzer. Ergebnisse und Fehler explizit weitergeben. main.c bleibt ein kleiner Einstieg für die notwendige Startverzweigung.

## 5. WLAN

- Zunächst Station-Modus: mit einem vorhandenen 2,4-GHz-WLAN verbinden. Kein Access-Point-Fallback und kein WLAN-Einrichtungsportal in dieser ersten Version.
- SSID/Passwort dürfen fest in die Firmware kompiliert werden, stammen aber aus einer ignorierten lokalen Konfigurationsdatei. Beispieldatei ohne echte Zugangsdaten einchecken; keine Passwörter loggen.
- Ohne angelegte Konfiguration muss das Projekt weiter bauen können; Transfer zeigt dann „WLAN nicht konfiguriert“ und KEY bleibt zum Zurückwechseln bedienbar.
- Vorhandene Arduino-WiFi- oder ESP-IDF-WiFi-API konsistent verwenden, nicht beide unabhängig für dieselbe Verbindung initialisieren.
- Kein endloses blockierendes while(!connected). Begrenzte Verbindungsversuche/Backoff, sichtbarer Fehlerstatus, weiterhin bedienbarer KEY-Taster.
- Bei erfolgreicher Verbindung IP und vollständige Browser-URL anzeigen. Netzwerkverlust sauber behandeln.

## 6. SD-Karte und geteilter SPI-Bus

- Verwende einen zur vorhandenen GD2-/Arduino-/ESP-IDF-Konfiguration passenden beschreibbaren FAT-Dateisystemzugriff, vorzugsweise ohne zweiten konkurrierenden SPI-Treiber.
- Gameduino und SD teilen den SPI-Bus, besitzen aber getrennte CS-Pins. Den Bus nicht neben GD2 unabhängig erneut initialisieren und keine parallelen unsynchronisierten Transaktionen zulassen.
- GD2 kann eine eigene SD-/FatFs-Implementierung mitbringen. Prüfe sie, bevor eine weitere SD-Bibliothek hinzugefügt wird.
- Im Transfer-Modus laufen HTTP-Handler und Displayaktualisierung möglicherweise in verschiedenen Tasks. Alle Zugriffe auf diesen Bus müssen dieselbe Synchronisierung beachten, einschließlich kompletter GD2-Kommandotransaktionen. Auch GD.finish() berücksichtigen. Lange Uploads nur stückweise sperren, damit die Oberfläche bedienbar bleibt.
- SD-Operationen begrenzen/serialisieren; keine unkontrolliert gleichzeitigen Dateihandles oder Schreibzugriffe.
- Karte niemals automatisch formatieren. Fehlende/unlesbare Karte anzeigen, Spielstart weiterhin ermöglichen.

## 7. Kleiner HTTP-Dateiserver

Nutze esp_http_server oder einen gleichwertig kleinen, bereits passenden Server. Keine große Async-Webserver-Abhängigkeit ohne konkreten Grund. Orientiere dich an Espressifs file_serving-Beispiel, passe aber dessen Speicherlimits, Dateisystem und Synchronisierung an dieses Projekt an.

Erste Version:

- Dateien und Ordner auflisten und navigieren.
- Dateien hochladen und herunterladen.
- Ein Button „Spiel starten“ im Browser, zusätzlich zu KEY.
- Kein Löschen, Umbenennen, Editor, Firmware-Upload oder komplexer Dateimanager erforderlich.
- Uploads streamen, etwa mit einem festen 4096-Byte-Puffer. Keine vollständigen Dateien im RAM; keine riesigen Strings für Verzeichnislisten oder Antworten. HTTP-Sessions und gleichzeitige Uploads begrenzen.
- Einfaches Binär-POST/PUT aus dem Browser verwenden, falls dadurch Multipart-Parsing entfällt. Streaming-Download und bounded/chunked Auflistung vorsehen.
- Pfade nach URL-Dekodierung validieren: keine Traversal-Segmente, NUL-Bytes, Ausbrüche aus dem SD-Wurzelverzeichnis oder unbegrenzten Pfad-/Dateinamenslängen. Dateinamen in HTML/JSON korrekt escapen.
- Zunächst keine vorhandenen Dateien überschreiben; eindeutigen Konfliktstatus zurückgeben. Damit bleiben bestehende Assets bei Fehlern erhalten.
- In eine temporäre Datei im Zielverzeichnis schreiben, empfangene Länge und sämtliche Schreib-/Schließergebnisse prüfen, erst danach zum Zielnamen umbenennen. Umbenennungsfehler behandeln. Unvollständige temporäre Dateien nicht als gültige Assets auflisten; bei Abbruch aufräumen, nach Stromausfall erkennbare Reste sicher behandeln. Keine garantierte Stromausfallsicherheit von FAT behaupten.
- Timeouts, voller Datenträger, Schreibfehler und Verbindungsabbruch dürfen weder hängen noch ein defektes Ziel als erfolgreichen Upload melden.
- Kleine Oberfläche mit Uploadauswahl, Dateiliste und Fortschritt; alle Ressourcen im Firmware-Flash, damit auch eine leere SD-Karte bedient werden kann.

## 8. Sicherer Wechsel während Übertragung

KEY und Browser lösen dieselbe Wechselanforderung aus. HTTP-Handler melden sie threadsicher an den Koordinator; sie starten nicht mitten in der Antwort oder Dateioperation neu.

- Bei Wechselanforderung keine neuen Transfers zulassen.
- Einen laufenden Transfer bis Abschluss oder begrenztem Timeout auslaufen lassen; danach gegebenenfalls kontrolliert abbrechen und temporäre Datei entfernen.
- Status „Wechsel zum Spiel …“ anzeigen; Dateihandles schließen und Schreibdaten flushen. Browserantwort zunächst abschließen, dann kontrolliert Dienste herunterfahren und neu starten.
- Die genaue Shutdown-Reihenfolge passend zum Server wählen; insbesondere nicht httpd_stop() aus dessen eigenem Request-Handler ausführen.
- Auch bei WLAN-/SD-Fehlern muss ein Wechsel über KEY möglich sein.

## 9. Umsetzung und Prüfung

Arbeite in überprüfbaren Schritten: Bootverzweigung und KEY-Wechsel, SD-Schreibzugriff, WLAN, Dateiserver, Oberfläche und Shutdown. Passe CMake, Ressourceneinbettung, .gitignore und nötige sdkconfig-Abhängigkeiten gezielt an.

Führe den vorhandenen Build aus. Prüfe Firmwaregröße gegen die tatsächliche App-Partition, nicht nur gegen die insgesamt vorhandenen 4 MB Flash. Keine unbegründeten Toolchain-Upgrades oder Umstellung auf ein anderes Bluetooth-Framework.

Ergänze gezielte Tests für Entprellung, einmaliges Modusticket und Pfadvalidierung, soweit die vorhandene Testumgebung das sinnvoll erlaubt. Keine Tests, die nur Implementierungsdetails nachbilden.

Hardware-Abnahme (bei fehlendem Gerät als klare manuelle Prüfliste dokumentieren; nicht als ausgeführt ausgeben):

1. Normalstart: Demo, Controller und Timing funktionieren wie zuvor; WLAN bleibt aus.
2. KEY drücken/halten/loslassen: genau ein Wechsel; längeres Halten und Prellen erzeugen keine Wiederholungen.
3. Transfer-Start ohne Bluetooth-Initialisierung; mit WLAN IP/URL sichtbar.
4. Upload einer Datei, die größer als verfügbarer RAM ist; Download auf dem PC byteweise oder per Hash mit Original vergleichen.
5. Fehlendes WLAN, falsche Zugangsdaten, fehlende Karte, volle Karte und Uploadabbruch: verständliche Fehler, KEY funktioniert weiter.
6. Gleicher Dateiname wird abgelehnt; Traversal wird abgelehnt; unvollständige Uploads erscheinen nicht als Assets.
7. KEY/Browser-Rückkehr während eines Uploads: geordneter Abschluss oder Abbruch, anschließend funktionierende Demo.
8. Wiederholte Wechsel in beide Richtungen und gewöhnlicher Reset: keine Neustartschleife; Reset ohne Ticket startet Game.
9. Displayaktualisierung während SD-Transfer ohne SPI-Konflikte.

Logge nach relevanten Initialisierungsphasen freien internen Heap, größten freien Block und Minimum-Heap, soweit die Plattform diese Werte bereitstellt. Vorher/nachher im Spiel und Transfer messen; statischen RAM und Flash aus dem Buildbericht getrennt nennen. Keine erfundenen Messwerte.

Zum Abschluss kurz berichten: Architektur, geänderte Dateien, Build-/Testergebnisse, Anleitung für WLAN-Konfiguration und Bedienung sowie offene Hardwareprüfungen. README entsprechend aktualisieren.

## Offizielle Referenzen

Die Dokumentation passend zur tatsächlich eingesetzten ESP-IDF-Version verwenden; Links dienen als Einstieg:

- GPIO 0 / Bootmodus: https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html
- Bluetooth-Controller und Speicherfreigabe: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/bluetooth/controller_vhci.html
- Dateiserver-Beispiel: https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server/file_serving

Implementiere die beschriebenen Funktionen im bestehenden Projekt. Bewahre dessen Spielverhalten und Hardwarebelegung. Falls der tatsächliche Komponentenstart oder SD-Treiber vom bekannten Stand abweicht, löse das anhand des Codes und erläutere die notwendige Anpassung, statt Annahmen ungeprüft zu übernehmen.
