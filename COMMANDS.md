# Jura Coffee Machine Commands Reference

This document lists known serial commands for controlling Jura coffee machines via the UART protocol.

## Command Format

Commands are sent using the `cmd2jura()` function in ESPHome YAML:

```yaml
button:
  - platform: template
    name: 'Make Espresso'
    on_press:
      - lambda: |-
          auto result = id(jura_coffee).cmd2jura("FA:04");
```

## Machine Control Commands

These commands control the machine's power state:

| Command | Description | Verified Models |
|---------|-------------|-----------------|
| `AN:01` | Turn machine ON | E6, E8, J6, F7 |
| `AN:02` | Turn machine OFF | E6, E8, J6, F7 |

## Beverage Commands (FA:XX)

**Important**: FA commands simulate button presses and are **model-specific**. The same FA command may trigger different drinks on different models. Always test commands on your specific machine.

### Known Working Commands

| Command | E6 | E8 | J6 | F7 | Notes |
|---------|----|----|----|----|-------|
| `FA:04` | Espresso | Espresso | Espresso | Espresso | Single espresso shot |
| `FA:05` | ? | Ristretto | ? | Ristretto | Short, concentrated espresso |
| `FA:06` | Hot Water | Hot Water | Hot Water | Hot Water | Dispense hot water |
| `FA:07` | ? | Cappuccino | ? | Cappuccino | Espresso with milk foam |
| `FA:09` | Coffee | Coffee | Coffee | Coffee | Standard coffee |

**Legend:**
- ✓ = Verified working
- ? = Unknown/Untested
- Model name = Expected to work based on machine capabilities

### Beverages Without Known Commands

The following beverages are available on the machine's display but we don't yet have confirmed FA commands:

#### E6 Model
- Americano
- Macchiato
- Caffe Barista
- Milk Foam
- Double variants (2x Espresso, 2x Coffee)

#### E8 Model
- Flat White (regular and extra shot)
- Latte Macchiato (regular and extra shot)
- Cortado
- Lungo Barista
- Cappuccino Extra Shot
- Macchiato
- Double variants

#### J6 Model
- Double Ristretto
- Latte Macchiato
- Espresso Macchiato
- Flat White
- Hot Milk

#### F7 Model
- Double Ristretto
- Latte Macchiato
- Espresso Macchiato
- Milk Foam

## Discovery Process

To discover FA commands for your specific model:

1. Enable the **diagnostic sensors** (`counters_changed` and `ic_bits`) in your configuration
2. Make a beverage using the machine's physical buttons
3. Watch which counters increment in the `counters_changed` sensor
4. Try different FA commands (FA:01 through FA:0F) and observe results
5. Document your findings and contribute back to the project!

## Contributing

If you discover working FA commands for your Jura model, please:
1. Open an issue or pull request
2. Include your model name and firmware version (if known)
3. List the FA command and the resulting beverage
4. Note any special behavior or requirements

## References

- [Jura Protocol Documentation (Jutta-Proto)](https://github.com/Jutta-Proto/protocol-cpp)
- [Jura UART Map](Jura_uart_map.md) - Counter/register documentation
- Command format varies by model - consult your machine's documentation

## Safety Notes

- Test commands carefully - some FA commands may trigger maintenance cycles
- Not all FA commands are beverages - some may control internal functions
- Always supervise the machine when testing new commands
- Use at your own risk
