#!/bin/sh

set -eu

CONFIG_FILE=${CONTROLX4_CONFIG:-"$HOME/.config/controlx4/controlx4.conf"}

if [ ! -r "$CONFIG_FILE" ]; then
  echo "Fehler: Konfiguration fehlt: $CONFIG_FILE" >&2
  exit 2
fi

# Die Datei muss CONTROLX4_URL enthalten.
. "$CONFIG_FILE"

: "${CONTROLX4_URL:?CONTROLX4_URL fehlt in der Konfiguration}"

request() {
  curl --fail --silent --show-error \
    --connect-timeout 3 --max-time 8 \
    "$@"
  printf '\n'
}

usage() {
  echo "Verwendung:" >&2
  echo "  $0 status" >&2
  echo "  $0 relay <1-4> <on|off>" >&2
  echo "  $0 all-off" >&2
  exit 2
}

case "${1:-}" in
  status)
    [ "$#" -eq 1 ] || usage
    request "$CONTROLX4_URL/api/status"
    ;;

  relay)
    [ "$#" -eq 3 ] || usage
    case "$2" in
      1|2|3|4) ;;
      *) usage ;;
    esac
    case "$3" in
      on|off) ;;
      *) usage ;;
    esac
    request --request POST "$CONTROLX4_URL/api/relay?id=$2&state=$3"
    ;;

  all-off)
    [ "$#" -eq 1 ] || usage
    relay=1
    while [ "$relay" -le 4 ]; do
      request --request POST "$CONTROLX4_URL/api/relay?id=$relay&state=off"
      relay=$((relay + 1))
    done
    ;;

  *)
    usage
    ;;
esac
