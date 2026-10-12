#!/usr/bin/env bash
set -euo pipefail

# ESP32 Gameduino 2 + Bluepad32: externe Komponenten wiederherstellen.
# Quelle: offizielle ESP-IDF + Arduino + Bluepad32 Projektvorlage.
# Fester Stand: Arduino 3.2.1, Bluepad32 4.2.0, BTstack 1.6.2.
TEMPLATE_URL="https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template.git"
TEMPLATE_COMMIT="d07a9385f46f7215f51fc3eb5e40c5a484cfe102"

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
COMPONENTS="$ROOT/components"
DEPENDENCIES=(arduino bluepad32 btstack cmd_nvs cmd_system)

for command in git mktemp cp; do
    if ! command -v "$command" >/dev/null 2>&1; then
        printf 'Fehlendes Programm: %s\n' "$command" >&2
        exit 1
    fi
done

missing=()
for name in "${DEPENDENCIES[@]}"; do
    if [[ -d "$COMPONENTS/$name" ]]; then
        printf 'Vorhanden (unverändert): components/%s\n' "$name"
    elif [[ -e "$COMPONENTS/$name" ]]; then
        printf 'Pfad ist vorhanden, aber kein Verzeichnis: %s\n' "$COMPONENTS/$name" >&2
        exit 1
    else
        missing+=("$name")
    fi
done

if (( ${#missing[@]} == 0 )); then
    echo 'Alle externen Komponenten sind bereits vorhanden.'
    exit 0
fi

tmp="$(mktemp -d)"
trap 'rm -rf -- "$tmp"' EXIT

echo 'Lade die festgelegte Bluepad32-Projektvorlage ...'
git clone --quiet "$TEMPLATE_URL" "$tmp/template"
git -C "$tmp/template" checkout --quiet --detach "$TEMPLATE_COMMIT"

# Arduino liegt in der Vorlage als Git-Submodul vor.
if [[ " ${missing[*]} " == *" arduino "* ]]; then
    git -C "$tmp/template" submodule update --init --depth 1 -- components/arduino
fi

# Erst prüfen, ob alle benötigten Quellverzeichnisse existieren.
for name in "${missing[@]}"; do
    if [[ ! -d "$tmp/template/components/$name" ]]; then
        printf 'Abhängigkeit in Vorlage nicht gefunden: %s\n' "$name" >&2
        exit 1
    fi
done

mkdir -p "$COMPONENTS"
for name in "${missing[@]}"; do
    printf 'Installiere components/%s ...\n' "$name"
    cp -a -- "$tmp/template/components/$name" "$COMPONENTS/$name"
    # Das Arduino-Git-Submodul enthält einen Verweis auf das temporäre Repo.
    rm -f -- "$COMPONENTS/$name/.git"
done

echo 'Abhängigkeiten eingerichtet. Bestehende Komponenten wurden nicht verändert.'
echo 'Nächster Schritt: pio run -e esp32dev'
