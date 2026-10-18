# Documentation images

| File | Source and meaning |
| --- | --- |
| [ha-native-controls.png](ha-native-controls.png) | Unmodified screenshot supplied by the project owner, who authorised its inclusion. Earlier HA device controls before saved-recipe synchronisation; values are examples. |
| [original-connector-reference.png](original-connector-reference.png) | Unchanged connector image from the original project README; [original URL](https://github.com/user-attachments/assets/6f2bb48f-e853-409c-b768-2b08b87c70d2). Check connector orientation and machine compatibility. |
| [uart-voltage-divider.png](uart-voltage-divider.png) | Generated schematic matching the reference GPIO4 TX / GPIO3 RX wiring. |
| [ha-monitoring-illustration.png](ha-monitoring-illustration.png) | Generated UI illustration combining E8 test readings, rather than a live or simultaneous capture. Not an installable HA card. |

The two generated illustrations can be reproduced with
[tools/render_doc_visuals.py](../../tools/render_doc_visuals.py), using Pillow
and Segoe UI or DejaVu Sans fonts. They do not contain Wi-Fi or API credentials.
