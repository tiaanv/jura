# Direct Cool Control bridge (dev trial)

The optional direct bridge sends cooler snapshots between ESPHome devices over
[ESPHome UDP packet transport](https://esphome.io/components/packet_transport/).
Home Assistant and MQTT are not in the forwarding path. Both devices still use
Wi-Fi and must be able to exchange local UDP traffic. This mode needs ESPHome
2026.9.1 or later. An E8 EF533M V02.09 owner has confirmed fast status updates
using unicast; the remaining failure/recovery checks below still need hardware testing.

## Configure the two devices

Use [the cooler example](../examples/coolcontrol-direct.yaml) and
[the coffee-machine example](../examples/e8-coolcontrol-direct.yaml) as additions
to your existing device configurations. Keep your working UART pins, API
security, OTA settings, machine permissions and any required startup probe.
Both examples use `ref: dev` for this trial.

1. On the cooler, add `bridge_packet` to `jura_coolcontrol` and publish that sensor
   through `packet_transport`. The example marks it `internal: true`, so it is not
   exposed to Home Assistant. Remove `message_counter` when no HA bridge uses it.
   `raw_message` is also optional; retain it only if useful for diagnostics.
2. On the machine, receive that packet sensor and reference it in
   `jura_native.coolcontrol.packet`. Replace the old `raw_message`,
   `message_counter` and `online` references in that block. The two input modes
   cannot be combined. Remove the three old `platform: homeassistant` imports
   if nothing else uses them.
3. Replace the example encryption key on both devices with the same private
   value. The provider name must match the cooler's `esphome.name`, and
   `remote_id` must match its packet sensor ID. Keep the transport sensors
   internal and unfiltered; they are protocol data, not measured quantities.
4. Flash both devices. The cooler example repeats its current packet every five
   seconds, in addition to immediate changes. The receiver expires the source
   after fifteen seconds without a valid update. Keep the heartbeat interval
   comfortably below that timeout. Direct packets are atomic, so the HA-mode
   `coalesce_delay` does not apply.

The examples use encrypted LAN broadcasts and enable rolling codes at the sender.
A router or access point with client isolation, separate VLANs or blocked UDP
broadcasts will prevent delivery. Broadcast remains the default and does not
require reserved IP addresses. No port forwarding is required.

### Optional workaround for delayed broadcasts

Some networks may delay broadcast delivery. On one tested Wi-Fi network, paired
logs showed an approximately 34–37 second timestamp gap between matching outgoing
and received packets. The owner reported that changing to unicast made status
updates very fast with no noticeable delay. This points to that network's
broadcast delivery path; it does not establish a general problem with broadcast
on all networks or identify the exact access-point behaviour.

If the cooler changes status promptly but the machine receives older statuses
for several seconds, try sending directly to the coffee-machine ESP's IP. In the
**cooler device's existing** UDP block, add `addresses`:

```yaml
udp:
  id: cooler_udp
  addresses:
    - 192.168.1.50  # Replace with your coffee-machine ESP's IPv4 address.
```

Leave the machine's UDP listener, provider name, encryption key and packet sensor
IDs unchanged. Rebuild and flash the cooler after this change. The receiver does
not need to transmit packets back for the supplied rolling-code configuration.

The numeric destination must remain stable: reserve the coffee-machine ESP's
address in your DHCP server, or use a correctly configured static address. Only
the receiver needs a stable IP for this one-way setup; the cooler can keep normal
DHCP. If the receiver's IP changes, update `addresses` and reflash the cooler.
This is an optional workaround, not a requirement for broadcast installations.

For troubleshooting, compare logs on both devices and observe the machine's
actual response. Unchanged packet messages in the Jura DEBUG log are rate-limited
to roughly once per five seconds; a missing DEBUG line alone does not prove a
missed packet. Restore ordinary DEBUG/INFO logging after testing, since global
VERY_VERBOSE logging can affect timing.

Both examples set `api.reboot_timeout: 0s` so loss of Home Assistant does not cause
an API-client watchdog reboot. Wi-Fi itself remains necessary. Normal temperature,
level, status and cooler-online entities can remain available to Home Assistant.
Existing HA entities removed from YAML may remain as unavailable entries until
removed from HA; they no longer receive new counter updates.

## Behaviour and diagnostics

The machine logs `Cool Control direct packet bridge enabled` at setup, followed
by `Cool Control direct packet: @mo...` (or the actual cooler status). Repeated
packets refresh liveness even when the value never changes; a retained sensor
value alone does not. Receipt bookkeeping is local and creates no HA counter.

A running cooler ESP reports `source UART offline` once recognized serial traffic
has stopped for its configured `communication_timeout` (ten seconds by default).
A missing network heartbeat instead produces `source updates stale; withdrawing
with @me1`. Malformed packets fail unavailable. The machine's cached response,
announcement, request priority and persistent healthy override work as before.
Older bare statuses and extended statuses with unknown telemetry remain supported.

During a drink, the coffee machine still receives its requested status from the
local cache; it does not wait for a new UDP message. Disconnected sources are not
made healthy by a network heartbeat. The healthy override remains an explicit
user choice.

## Hardware checks

- Confirm no message-counter entity is being updated in HA and observe ordinary
  cooler readings there.
- Verify `@HM` receives the expected cached `@mo` or `@mn` and test milk preparation.
- Test unchanged status for longer than a minute: it must stay fresh without a
  changing counter value.
- Disconnect the cooler ESP from Wi-Fi or turn it off: the machine bridge should
  withdraw within the receiver timeout, then recover when packets return.
- Separately stop cooler UART traffic while its ESP stays powered: the sender
  should report UART offline rather than continuing a healthy snapshot.
- Test both device reboot orders. HA being stopped should not stop forwarding
  while both ESP devices and their Wi-Fi network remain working.

## Packet format for maintainers

One exact float-representable 24-bit integer carries a whole snapshot: bits
20–23 are version magic `0xA`, bits 16–19 are status, and bits 0–15 are the
optional level/temperature bytes. Status codes are 0 (UART offline), 1 (`@mo`),
2 (`@mn`), 3 (`@me`), 4 (`@me1`), 5 (extended `@mo`) and 6 (extended `@mn`).
Bare/offline statuses require a zero payload. Extended levels accept 0–100 or
`FF` for unknown. Invalid, fractional or non-finite values are rejected.
