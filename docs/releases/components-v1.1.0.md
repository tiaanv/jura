# Components 1.1.0

Optional direct UDP communication between **jura_coolcontrol** and **jura_native**,
released from main on 2026-10-31. The existing Home Assistant bridge and standalone
machine configurations remain supported without YAML migration.

## Changes

- Forward cooler status directly between ESPHome devices using built-in UDP
  packet transport, without Home Assistant or MQTT in the forwarding path.
- Use one internal numeric packet sensor for atomic status and optional temperature/
  level data. No Home Assistant message counter is needed in this mode.
- Send changed snapshots immediately and repeat the current snapshot on a five-second
  heartbeat in the examples. Repeated identical packets refresh receiver freshness.
- Detect cooler UART loss independently of network heartbeats, and withdraw stale
  network status. Preserve older status-only frames, cached machine replies and
  the persistent manual healthy-state override.
- Add paired configuration examples, transport diagnostics and an optional unicast
  workaround for networks with delayed broadcast delivery. Broadcast remains the
  default and does not require a reserved IP.

## Updating and configuring

Release pinning is optional. Use `ref: components-v1.1.0` for this fixed release,
`ref: main` for stable development, or `ref: dev` for development testing. Refresh
external component sources as needed, rebuild and upload to apply changes.

To adopt UDP, update **both devices** to components 1.1.0 or later and follow the
[direct bridge guide](../direct-coolcontrol.md) and its paired examples. Replace
the old machine-side raw_message/message_counter/online bridge inputs with the
packet input; the two input modes cannot be combined. The cooler counter and
old HA imports can be removed when nothing else uses them. Other HA entities
remain optional for normal monitoring.

Both devices need ESPHome **2026.9.1 or later**, matching private transport keys,
a provider name matching the cooler's actual esphome.name, and local UDP
connectivity. Use matching component versions for the direct bridge; 1.0.0 has
no packet bridge support. Users retaining the HA bridge can upgrade either
component without adopting UDP or changing the existing bridge YAML.

For unicast, give the receiving machine device a stable address, preferably a
DHCP reservation, and configure that address in the cooler's UDP block. The cooler
can continue using ordinary DHCP. This is an optional network-specific workaround.

## Validation and remaining limitations

Release checks cover direct packet round trips, repeated-value freshness, UART
and network expiry, malformed packets, recovery and override, plus existing
controller/native/HA bridge/cooler/legacy and ESP8266 flash-reader host suites.
ESPHome 2026.9.1 schema validation includes the direct provider/consumer and
invalid or mixed input configurations; direct configuration C++ generation is
also checked. These are not complete firmware build or hardware tests.

An E8 EF533M V02.09 owner running ESPHome 2026.10.1 confirmed direct forwarding
and fast status changes using unicast after observing delayed broadcast updates.
This does not establish a general broadcast fault. Complete failure/recovery and
reboot-order hardware checks remain outstanding; see the
[testing record](../testing.md) and guide checklist. Existing EF1092 HA bridge
confirmation does not imply direct UDP testing on that model.

The card remains separately versioned and released; no card files or version are
changed by this release. Read the [release policy](../releases.md),
[disclaimer](../disclaimer.md) and [contributor notices](../notices.md).
