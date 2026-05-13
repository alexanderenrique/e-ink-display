# Fun aggregator server

Small **FastAPI** service (`fun_aggregator/`) that caches fun-fact / ISS / earthquake data and serves it to the e-ink firmware over HTTP. The ESP32 calls endpoints under `/v1/fun/…` with an optional shared secret header `X-Fun-Key` (must match `FUN_API_KEY` on the server when configured).

This folder is the right place to run the API—on a **Raspberry Pi at home**, on a **VPS**, or behind **your existing website** via a reverse proxy—without requiring a public IP on the Pi itself.

---

## Raspberry Pi (step-by-step)

Use this checklist to run the aggregator on **Raspberry Pi OS** (or another Debian-based Pi image). The HTTP server listens on **port 8080** by default. The **systemd** unit uses **`WorkingDirectory=/opt/fun-aggregator`**, so pool files and default `data/*.json` paths live next to the app unless you override env vars.

### 1. Prerequisites

- Raspberry Pi on your network, with **Python 3.10+** (`python3 --version`).
- Decide which Linux user runs the service. These steps use **`pi`**; if you use another account, set that user in **`deploy/fun-aggregator.service`** (`User=` and file ownership under `/opt` and `/var/lib`).

### 2. System packages

```bash
sudo apt update
sudo apt install -y python3-venv
```

### 3. Install app files under `/opt/fun-aggregator`

On the **Pi**:

```bash
sudo mkdir -p /opt/fun-aggregator
sudo chown "$USER":"$USER" /opt/fun-aggregator
```

From your **development machine** (repository **root**; adjust `pi@raspberrypi.local`):

```bash
rsync -a --exclude '.venv' server/fun_aggregator/ pi@raspberrypi.local:/opt/fun-aggregator/
```

Alternatively, clone the repo on the Pi and copy or symlink so that `/opt/fun-aggregator` contains the contents of `server/fun_aggregator/` (`main.py`, `requirements.txt`, `deploy/`, `data/`, etc.).

### 4. Python virtualenv and dependencies

On the **Pi**:

```bash
cd /opt/fun-aggregator
python3 -m venv .venv
.venv/bin/pip install --upgrade pip
.venv/bin/pip install -r requirements.txt
```

(Optional) Dev dependencies and unit tests:

```bash
.venv/bin/pip install -r requirements-dev.txt
.venv/bin/python -m pytest
```

### 5. Environment file `/etc/fun-aggregator.env`

`Systemd` loads **`EnvironmentFile=-/etc/fun-aggregator.env`** (see `deploy/fun-aggregator.service`). Create it on the Pi:

```bash
sudo install -m 600 /dev/null /etc/fun-aggregator.env
sudo nano /etc/fun-aggregator.env
```

**Production-style example** (require API key; persist roster and specials outside the app tree):

```env
# Required for FUN_REQUIRE_API_KEY=1
FUN_API_KEY=<long random secret>
FUN_REQUIRE_API_KEY=1

# Earthquake + ISS background refresh (seconds)
REFRESH_SECONDS=900

# Fact harvest tick (seconds); cat + useless upstreams work out of the box (meowfacts + uselessfacts.jsph)
FACT_REFRESH_SECONDS=400

# Persist device roster and special-message queues (optional but recommended)
FUN_DEVICE_STORE=/var/lib/fun-aggregator/devices.json
FUN_SPECIAL_STORE=/var/lib/fun-aggregator/special_messages.json

# Optional: admin-only enqueue for targeted slides (must differ from FUN_API_KEY)
# FUN_ADMIN_API_KEY=<separate long secret>
```

**Generate `FUN_API_KEY` (and a separate admin key if you use special messages).** Use a long, unpredictable value (aim for **≥32 random bytes** of entropy). On your laptop or the Pi you can run either:

```bash
openssl rand -hex 32
```

```bash
python3 -c "import secrets; print(secrets.token_urlsafe(32))"
```

Copy the output into **`FUN_API_KEY=`** in `/etc/fun-aggregator.env`. If you enable **`FUN_ADMIN_API_KEY`**, run the command again and paste a **different** value—never reuse the device/fun key for admin routes.

**Firmware: where to put the same secret before you flash.** The ESP32 sends **`X-Fun-Key`** on fun endpoints. That string is **compiled into the firmware** here (path from the **`e-ink_code`** project root):

`firmware/core/hardware_config.h`

- **`FUN_FACTS_BASE_URL`** — aggregator base URL (no trailing slash), e.g. `http://192.168.1.10:8080`.
- **`FUN_FACTS_API_KEY`** — must **exactly match** the server’s **`FUN_API_KEY`**. Use `""` (empty) only if the Pi does **not** set `FUN_API_KEY` (typical LAN-only testing).

Example:

```cpp
#define FUN_FACTS_BASE_URL "http://192.168.1.10:8080"
#define FUN_FACTS_API_KEY "paste_the_same_secret_as_FUN_API_KEY_here"
```

Prefer secrets that are **hex** or **URL-safe** so you do not have to escape quotes or backslashes inside the C macro. After changing these defines, **rebuild and upload** the firmware (e.g. PlatformIO **Upload** on the `e-ink_code` project) so the chip runs the new key.

**LAN-only lab example** (no API key; not for exposure beyond your home):

```env
REFRESH_SECONDS=900
FACT_REFRESH_SECONDS=400
```

With **no** `FUN_FACT_SOURCES_JSON`, the server still harvests **cat** and **useless** facts using built-in URLs (see table below). **Earthquake** and **ISS** data come from USGS and Where The ISS At inside `refresh.py`; no extra env is required.

Create `/var/lib/fun-aggregator` if you pointed stores there:

```bash
sudo mkdir -p /var/lib/fun-aggregator
sudo chown pi:pi /var/lib/fun-aggregator
```

Replace **`pi`** with your service user if different.

**Upstream fact APIs (optional overrides).** Harvested lines are appended to `data/cat_facts.json`, `data/useless_facts.json`, and optionally `data/fun_facts.json` under **`WorkingDirectory`** (FIFO cap, default **30** lines per pool). The ESP32 only talks to **your** `/v1/fun/…` endpoints.

| Variable | Meaning |
|----------|---------|
| `FUN_CAT_UPSTREAM_URL`, `FUN_USELESS_UPSTREAM_URL`, `FUN_FUN_UPSTREAM_URL` | Override GET URLs. If unset and `FUN_FACT_SOURCES_JSON` is not used, **cat** defaults to `https://meowfacts.herokuapp.com/` and **useless** to `https://uselessfacts.jsph.pl/api/v2/facts/random?language=en`. |
| `FUN_CAT_JSON_PATH`, `FUN_USELESS_JSON_PATH`, `FUN_FUN_JSON_PATH` | Optional dot-separated path to the fact string. If unset, keys such as `fact`, `text`, and `data` (including `data` as a string array) are handled. |
| `FUN_FACT_SOURCES_JSON` | Alternative: JSON array of `{"pool": "cat_facts", "url": "...", "json_path": "..."}`. When set, it **replaces** the env mapping above for harvest configuration. |
| `FACT_REFRESH_SECONDS` | Seconds between harvest ticks (default **400**). |
| `FACT_ROUND_ROBIN` | Default **on** (`1`): one pool per tick. Set `0` to hit every configured URL each tick. |
| `FACT_POOL_MAX_LINES` | Max lines per pool (default **30**). |
| `FACT_FETCHES_PER_SOURCE_PER_CYCLE`, `FACT_INTER_SOURCE_DELAY_SECONDS` | Burst and delay between sequential GETs per pool. |
| `FACT_UPSTREAM_USER_AGENT`, `FACT_UPSTREAM_TIMEOUT_SECONDS`, `FACT_MAX_FACT_CHARS` | Optional HTTP tuning. |
| `FUN_RATE_LIMIT_SCREEN`, `FUN_RATE_LIMIT_BATCH` | SlowAPI limits (defaults `60/minute` and `40/minute`). |

Additional variables are documented in `fun_aggregator/deploy/fun-aggregator.service` comments and in `main.py`’s module docstring.

### 6. Verify outbound APIs (before or after systemd)

The script **`smoke_fact_upstreams.py`** uses the same URL configuration as the running app and performs live HTTP checks (fact harvest sources plus USGS and ISS feeds used for earthquake/ISS slides):

```bash
cd /opt/fun-aggregator
set -a
[ -f /etc/fun-aggregator.env ] && . /etc/fun-aggregator.env
set +a
.venv/bin/python smoke_fact_upstreams.py
```

Run this after step 5 so custom `FUN_*` URLs are picked up. Exit code **0** means every probe returned usable JSON—you can run it again any time you change env or network.

### 7. Install and start systemd

```bash
sudo cp /opt/fun-aggregator/deploy/fun-aggregator.service /etc/systemd/system/
```

Edit the unit if needed (service user, paths):

```bash
sudo nano /etc/systemd/system/fun-aggregator.service
```

Enable and start:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now fun-aggregator
sudo systemctl status fun-aggregator
```

Logs:

```bash
journalctl -u fun-aggregator -e -f
```

Firewall (only if you use `ufw` and clients reach the Pi on **8080**):

```bash
sudo ufw allow 8080/tcp
sudo ufw reload
```

### 8. Smoke-test the running service

On the **Pi**:

```bash
curl -s http://127.0.0.1:8080/healthz
```

If **`FUN_API_KEY`** is set:

```bash
curl -s -H "X-Fun-Key: YOUR_KEY" "http://127.0.0.1:8080/v1/fun/screen?m=1"
```

After the first **fact harvest** cycles, cat/useless slides should work from pools; **`503`** on a mode usually means empty pool or USGS/ISS not yet refreshed—wait a short time or check outbound internet (`smoke_fact_upstreams.py`).

### 9. Point the ESP32 at the Pi

Set **`FUN_FACTS_BASE_URL`** and **`FUN_FACTS_API_KEY`** in `firmware/core/hardware_config.h` (see **step 5** for generating the key and matching it to **`FUN_API_KEY`**). Rebuild and flash after any change.

For **HTTPS** with a well-known CA, configure the firmware as described in that header (root CA / pinning). Self-signed certs on the Pi are awkward on ESP32 unless you embed a matching trust anchor.

### 10. Updating the app after code changes

On your dev machine, **rsync** again (step 3), then on the **Pi**:

```bash
cd /opt/fun-aggregator
.venv/bin/pip install -r requirements.txt
sudo systemctl restart fun-aggregator
```

---

## Special messages (targeted slides)

You can queue a custom slide per device UUID (FIFO). Each fun-app wake (when **`apis.special_messages`** is enabled on the ESP32, the default) issues **`GET /v1/fun/special`** with the same **`X-Fun-Key`** and **`X-Device-Id`** as other fun endpoints; if a message is waiting, the server returns **`FunSlide` JSON** and **removes** that message from the queue; otherwise it returns **`204 No Content`**. The firmware does **not** call the admin route.

**Configuration**

- **`FUN_ADMIN_API_KEY`** — required to use **`POST /v1/admin/special`**. Send it as header **`X-Fun-Admin-Key`**. This must be **different** from **`FUN_API_KEY`** so a leaked device key cannot enqueue messages for arbitrary UUIDs. If unset, admin enqueue returns **503**.
- **`FUN_SPECIAL_STORE`** — JSON file for queues and named groups (default `data/special_messages.json` under the app working directory). See `fun_aggregator/special_messages.py` for the on-disk shape.

Look up recipient UUIDs in **`FUN_DEVICE_STORE`** (the device roster, often `devices.json`) or from the friend’s BLE provisioning.

**Enqueue for one device**

```bash
curl -sS -X POST 'http://your-host:8080/v1/admin/special' \
  -H 'Content-Type: application/json' \
  -H 'X-Fun-Admin-Key: YOUR_ADMIN_SECRET' \
  -d '{"text":"Happy birthday!","device_ids":["00000000-0000-0000-0000-000000000000"]}'
```

Replace the host/port, **`YOUR_ADMIN_SECRET`**, and the placeholder UUID with real values.

**Enqueue for a named group** (optional **`groups`** map merges members into the store before **`group_ids`** is resolved in the same request):

```bash
curl -sS -X POST 'http://your-host:8080/v1/admin/special' \
  -H 'Content-Type: application/json' \
  -H 'X-Fun-Admin-Key: YOUR_ADMIN_SECRET' \
  -d '{"text":"Hello book club!","group_ids":["book_club"],"groups":{"book_club":["<uuid-1>","<uuid-2>"]}}'
```

Optional JSON fields: **`layout`** (string, defaults to `default`), **`expires_at`** (ISO-8601 string; expired entries are dropped when consumed).

---

## You do not need a public IP on the Raspberry Pi

Keeping the Pi **private** (behind NAT, no port forwarding) is reasonable. The display only needs to **outbound** reach whatever hostname you give it. Common patterns:

| Approach | Idea |
|----------|------|
| **LAN only** | Pi has a private IP; ESP32 and Pi on the same Wi‑Fi. No internet exposure. Easiest for a single home network. |
| **Cloudflare Tunnel (`cloudflared`)** | Pi (or any host) **outbound** connects to Cloudflare; you get a `https://something.yourdomain.com` URL with no inbound ports on the Pi. |
| **Tailscale / WireGuard** | Pi and phone/laptop share a virtual LAN; ESP32 still needs a route—often you combine with a small VPS or run the API on a node the ESP can reach, or use LAN + Tailscale for *admin* only. |
| **API on the cloud, Pi optional** | Run the **same** `fun_aggregator` app on a cheap VPS or container host; firmware uses `https://api.yourdomain.com` with `FUN_API_KEY`. The Pi is not required for the API (only if you want local hosting). |
| **Your existing website as front door** | Run the Python app on a **VPS** or **PaaS**, or keep it on the Pi but expose it only via **Tunnel** to your VPS/nginx **reverse proxy** (subdomain or path). Visitors never connect directly to the Pi’s public IP—because there isn’t one. |

### Using a VPS or PaaS (“cloud”)

The app is a normal **uvicorn** process:

```bash
cd fun_aggregator
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
FUN_API_KEY=… FUN_REQUIRE_API_KEY=1 .venv/bin/uvicorn main:app --host 0.0.0.0 --port 8080
```

Put **nginx**, **Caddy**, or your host’s load balancer in front for TLS on **443**, and proxy to `127.0.0.1:8080`.

**Persistence:** device registration is stored in `FUN_DEVICE_STORE` (JSON on disk). On ephemeral platforms (some free containers), disk resets on redeploy—use a **mounted volume** or accept that the roster resets unless you add external storage.

**Examples of where this fits well:** any small Linux VM (Linode, DigitalOcean, Hetzner, AWS Lightsail), or container platforms that allow a volume and a stable URL.

### ESP32 and HTTPS

If the public URL uses HTTPS with a standard CA certificate, configure the firmware accordingly. Self-signed certs on the Pi are painful on ESP32 unless you embed the right trust anchor—prefer Let’s Encrypt on the public hostname or HTTP on trusted LAN only.

---

## Troubleshooting

- **`401` on fun endpoints:** `X-Fun-Key` missing or wrong; align firmware `FUN_FACTS_API_KEY` with server `FUN_API_KEY`.
- **Service exits immediately:** with `FUN_REQUIRE_API_KEY=1`, ensure `FUN_API_KEY` is set in `/etc/fun-aggregator.env`.
- **`503` on `/v1/fun/screen`:** USGS/ISS not warmed yet, or fact pools empty—wait for refresh/harvest, check Pi outbound DNS/firewall, and run `smoke_fact_upstreams.py` from `/opt/fun-aggregator`.
- **Upstream / harvest issues:** `journalctl -u fun-aggregator -e` and `.venv/bin/python smoke_fact_upstreams.py` (load `/etc/fun-aggregator.env` first, as in the Raspberry Pi checklist step 6).

For endpoint and header details, see the module docstring at the top of `fun_aggregator/main.py`.
