# Manual AN:01 shutdown-delay experiment

This experiment is available on the **dev** branch. `AN:01` is an unverified
legacy UART command, not a confirmed AT keep-awake, standby or power-on command.
The purpose is to observe whether it affects the E8's switch-off delay, display
or service-port 5 V supply. A reply alone cannot establish those effects.

## Native component YAML

In the existing Git source entry change `ref: main` to `ref: dev`, retaining
`components: [jura_native]`. Add the button from
[examples/an01-test.yaml](../examples/an01-test.yaml) to your device YAML:

```yaml
button:
  - platform: template
    name: "Jura Test AN01"
    entity_category: diagnostic
    on_press:
      - lambda: |-
          if (!id(jura_coffee).test_an01())
            ESP_LOGW("jura_test", "AN:01 test was not queued; see Jura Command Result");
```

Use your component ID if it differs from `jura_coffee`. Append the entry to an
existing `button:` block rather than creating a duplicate. Compile/upload after
changing the source and adding the button; the current main implementation does
not have this method. No native brew/power-off permission is needed for this
explicit manual diagnostic.

`test_an01()` sends the fixed, unencrypted `AN:01` command through the component's
existing serial transaction queue. It does not write arbitrary raw bytes around
the AT transport. It requires a connected AT session but deliberately permits
the blocked TF status associated with SwitchOff Delay. It rejects an active
operation, pending brew/settings controls or another pending AN:01 test, and
rechecks connection/operation state before dispatch. There is no automatic retry
or periodic keep-awake automation.

## Observe the result

First observe an ordinary shutdown without pressing the button, recording when
the display goes dark, SwitchOff Delay appears and the ESP loses power. On the
next comparable shutdown press the button **once** during the powered delay,
while the ESP is still reachable and its AT session is connected.

Watch **Jura Command Result**, UART timeout count, TF/status messages and physical
display behaviour. Record whether the machine returns to ready, remains dark
but continues powering the ESP, or powers off at the usual time. Compare elapsed
times from entry into the delay, rather than the button press alone. Allow the
full usual delay to elapse; a brief response is not evidence of indefinite power.

Both `an:` and `ok:` replies are recorded verbatim as responses with unverified
effect. A reply is not reported as a successful wake. A timeout is also
inconclusive: the machine may have ignored the command, stopped listening or
removed power before replying. The ESP cannot report after its supply disappears.

The test never bypasses brew readiness checks or queues a drink. It cannot
promise to preserve the 5 V rail while keeping the rest of the machine asleep.

## Legacy component

The legacy `jura` component already supports this command using its existing
queue. In a legacy-only configuration the corresponding lambda is:

```cpp
id(jura_coffee).queue_command("AN:01");
```

That is not the native component's diagnostic API. Do not add a second Jura
component or use `uart.write` on the same bus to run this experiment.
