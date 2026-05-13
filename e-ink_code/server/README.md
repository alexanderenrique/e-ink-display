# Fun aggregator server

Small **FastAPI** service (`fun_aggregator/`) that caches fun-fact / ISS / earthquake data and serves it to the e-ink firmware over HTTP. The ESP32 calls endpoints under `/v1/fun/…` with an optional shared secret header `X-Fun-Key` (must match `FUN_API_KEY` on the server when configured).

This folder is the right place to run the API—on a **Raspberry Pi at home**, on a **VPS**, or behind **your existing website** via a reverse proxy—without requiring a public IP on the Pi itself.

---

## Raspberry Pi (recommended home setup)

**Assumptions:** Raspberry Pi OS with Python 3.10+ and network access. The service listens on **port 8080** by default.

### 1. System packages

```bash
sudo apt update
sudo apt install -y python3-venv
```

### 2. Install app under `/opt/fun-aggregator`

Pick a user that will run the service (below uses `pi`; change `User=` in the unit file if you use another account).

```bash
sudo mkdir -p /opt/fun-aggregator
sudo chown "$USER":"$USER" /opt/fun-aggregator
```

Copy the `fun_aggregator` tree from this repo (from the **repository root** on your dev machine; adjust `pi@your-pi`):

```bash
rsync -a --exclude '.venv' server/fun_aggregator/ pi@your-pi:/opt/fun-aggregator/
```

On the Pi:

```bash
cd /opt/fun-aggregator
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
```

To run unit tests locally: `.venv/bin/pip install -r requirements-dev.txt` then `cd /path/to/fun_aggregator && .venv/bin/python -m pytest`.

### 3. Environment file (strongly recommended)

For any machine reachable beyond your LAN, set a long random `FUN_API_KEY` and require it at startup:

```bash
sudo install -m 600 /dev/null /etc/fun-aggregator.env
sudo nano /etc/fun-aggregator.env
```

Example:

```env
FUN_API_KEY=<long random secret>
FUN_REQUIRE_API_KEY=1
REFRESH_SECONDS=900
FUN_DEVICE_STORE=/var/lib/fun-aggregator/devices.json
# Optional: third-party fact APIs (see README "Upstream fact APIs")
# FUN_CAT_UPSTREAM_URL=https://catfact.ninja/fact
# FACT_REFRESH_SECONDS=400
# Optional: targeted slides (see "Special messages" below)
# FUN_ADMIN_API_KEY=<separate secret for POST /v1/admin/special>
# FUN_SPECIAL_STORE=/var/lib/fun-aggregator/special_messages.json
```

Create the roster directory if you use a path under `/var/lib`:

```bash
sudo mkdir -p /var/lib/fun-aggregator
sudo chown pi:pi /var/lib/fun-aggregator
```

Optional tuning (defaults are fine for a few displays):

- `FUN_RATE_LIMIT_SCREEN` — default `60/minute`
- `FUN_RATE_LIMIT_BATCH` — default `40/minute`

**Upstream fact APIs (optional).** If set, the server periodically fetches JSON from public fact APIs and merges lines into `data/cat_facts.json`, `data/useless_facts.json`, and/or `data/fun_facts.json` (FIFO cap, default **30** lines per pool). Devices still only call *your* `/v1/fun/…` endpoints.

| Variable | Meaning |
|----------|---------|
| `FUN_CAT_UPSTREAM_URL`, `FUN_USELESS_UPSTREAM_URL`, `FUN_FUN_UPSTREAM_URL` | GET URL per category (`fun` writes `fun_facts.json`). |
| `FUN_CAT_JSON_PATH`, `FUN_USELESS_JSON_PATH`, `FUN_FUN_JSON_PATH` | Optional dot-separated path to the fact string (e.g. `data`). If unset, common keys `fact`, `text`, `data` are tried. |
| `FUN_FACT_SOURCES_JSON` | Alternative: JSON array of `{"pool": "cat_facts", "url": "...", "json_path": "..."}`. |
| `FACT_REFRESH_SECONDS` | Seconds between scheduler ticks (default **400**). With round-robin, each pool is fetched about every `FACT_REFRESH_SECONDS × (number of pools)`. |
| `FACT_ROUND_ROBIN` | Default **on** (`1`): one pool per tick. Set `0` to fetch every configured URL each tick. |
| `FACT_POOL_MAX_LINES` | Max stored lines per pool (default **30**); oldest removed when a new unique line is added. |
| `FACT_FETCHES_PER_SOURCE_PER_CYCLE` | Sequential GETs per pool per tick (default **1**). |
| `FACT_INTER_SOURCE_DELAY_SECONDS` | Delay between those GETs (default **1.5**). |
| `FACT_UPSTREAM_USER_AGENT`, `FACT_UPSTREAM_TIMEOUT_SECONDS`, `FACT_MAX_FACT_CHARS` | Optional HTTP tuning. |

See comments at the top of `fun_aggregator/deploy/fun-aggregator.service` for a full list.

### 4. systemd service

```bash
sudo cp /opt/fun-aggregator/deploy/fun-aggregator.service /etc/systemd/system/
# Edit User= and paths if needed:
# sudo nano /etc/systemd/system/fun-aggregator.service
sudo systemctl daemon-reload
sudo systemctl enable --now fun-aggregator
sudo systemctl status fun-aggregator
```

Smoke test on the Pi:

```bash
curl -s http://127.0.0.1:8080/healthz
```

### 5. Point the firmware at the Pi

In `firmware/core/hardware_config.h`, set `FUN_FACTS_BASE_URL` to the URL the ESP can actually reach, e.g. `http://192.168.1.10:8080` on your LAN. If `FUN_API_KEY` is set on the server, set `FUN_FACTS_API_KEY` in the firmware to the same value.

For **HTTPS** to a hostname with a public CA, the firmware can use `https://…` and a pinned/root CA as documented in that header file.

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
- **`503` on `/v1/fun/screen`:** upstream sources not warmed yet; wait for the background refresh or check Pi outbound internet (firewall/DNS).

For endpoint and header details, see the module docstring at the top of `fun_aggregator/main.py`.
