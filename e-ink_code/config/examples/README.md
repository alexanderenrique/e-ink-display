# Device config JSON examples

Deploy these (or your variants) to `https://ota.denton.works/config/devices/` so each device can fetch `{name}.json` (e.g. `dillon.json`) or fall back to `default.json`.

All keys are **optional**; firmware uses defaults when a key is missing.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `refresh_ms` | number | 300000 | Milliseconds between display cycles (min 10000). |
| `modules` | object | all true | Human-readable toggles. Each key is a module name, value is `true`/`false`. Names: `room`, `meow_fact`, `Earthquakes`, `Iss_location`, `useless_fact`. Cycle order: room → Earthquakes → meow_fact → Iss_location → useless_fact. |
| `messages_url` | string | "" | URL for custom messages endpoint. |
| `messages_key` | string | "" | Optional API key for messages. |
| `key` | string | "" | Arbitrary per-device value. |

To add new config options later: extend the schema in `src_hold/device_config.h`, parse in `device_config.cpp`, and add a getter with a default.
