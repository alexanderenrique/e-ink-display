# Bin Lookup Server

A lightweight Python HTTP server that pre-processes user and bin data from the NEMO API and provides a simple endpoint for ESP32 shelf labels to query bin owner information.

## Why This Server?

The NEMO API has ~2000 users, which is too much data for the ESP32 to download and store. This server runs on a more capable machine (your server computer) and handles:

1. Fetching all users from NEMO_USER_URL
2. Fetching all bins from NEMO_BIN_URL (recurring_consumable_charges endpoint)
3. Building lookup tables mapping bins to users (using the `customer` field)
4. Serving a lightweight API endpoint for ESP32 devices

## API Structure

The `recurring_consumable_charges` endpoint returns bins with this structure:
```json
{
    "id": 317,
    "name": "Bin E01",
    "quantity": 1,
    "last_charge": "2026-02-01T00:00:16.028498-08:00",
    "customer": 447,  // This is the user ID (maps to users API)
    "consumable": 64,
    "project": 869,
    ...
}
```

The server maps the `customer` field to user information from the users API.

## Setup

1. **Install dependencies:**
   ```bash
   pip install -r requirements.txt
   ```

2. **Configure environment variables** (in `.env` file):
   ```env
   NEMO_API_KEY=your_token_here
   NEMO_USER_URL=https://nemo.stanford.edu/api/users/
   NEMO_BIN_URL=https://nemo.stanford.edu/api/recurring_consumable_charges/
   SERVER_HOST=0.0.0.0  # Optional, default: 0.0.0.0
   SERVER_PORT=8080      # Optional, default: 8080
   CACHE_REFRESH_INTERVAL=3600  # Optional, default: 3600 seconds (1 hour)
   API_TIMEOUT=300      # Optional, default: 300 seconds (5 min) or None for no timeout
   ```

3. **Run the server:**
   ```bash
   python scripts/bin_lookup_server.py
   ```

   Or make it executable and run directly:
   ```bash
   chmod +x scripts/bin_lookup_server.py
   ./scripts/bin_lookup_server.py
   ```

## API Endpoints

### GET `/bin/<bin_id>`

Returns bin information including owner details.

**Example Request:**
```bash
curl http://localhost:8080/bin/123
```

**Example Response:**
```json
{
  "bin_id": "317",
  "bin_name": "Bin E01",
  "owner": {
    "name": "John Doe",
    "username": "jdoe",
    "email": "jdoe@stanford.edu"
  }
}
```

**Error Response (404):**
```json
{
  "error": "Bin not found: 999"
}
```

### GET `/refresh`

Manually refresh the user and bin cache.

**Example Request:**
```bash
curl http://localhost:8080/refresh
```

**Example Response:**
```json
{
  "status": "Cache refreshed",
  "users": 1987,
  "bins": 342
}
```

### GET `/health`

Health check endpoint showing cache status.

**Example Request:**
```bash
curl http://localhost:8080/health
```

**Example Response:**
```json
{
  "status": "ok",
  "users": 1987,
  "bins": 342,
  "last_refresh": 1707234567.89
}
```

## ESP32 Configuration

Configure the ESP32 shelf app via BLE with:

```json
{
  "mode": "shelf",
  "binId": "123",
  "serverUrl": "http://192.168.1.100:8080",
  "refreshInterval": 5
}
```

Where:
- `binId`: The bin ID to look up
- `serverUrl`: The base URL of this server (replace with your server's IP/domain)
- `refreshInterval`: Refresh interval in minutes (optional, default: 5)

## Architecture

```
ESP32 Device          Python Server          NEMO API
     |                      |                     |
     |-- GET /bin/123 ----->|                     |
     |                      |-- GET /api/users -->|
     |                      |<-- 2000 users ------|
     |                      |-- GET /api/bins ---->|
     |                      |<-- 342 bins --------|
     |                      | (build lookup)      |
     |<-- JSON response -----|                     |
     |                      |                     |
```

The server caches user and bin data, refreshing automatically every hour (configurable). ESP32 devices make simple GET requests with just the bin ID and receive lightweight JSON responses.

## Notes

- The server handles pagination automatically if the NEMO API uses paginated responses
- Cache is refreshed automatically based on `CACHE_REFRESH_INTERVAL`
- The server can handle both numeric bin IDs and string bin names
- The `recurring_consumable_charges` API uses `customer` field (not `user`) to reference the user ID
- User lookup supports nested customer objects, integer IDs, and string IDs
- The API response includes additional fields like `quantity`, `last_charge`, `consumable_id`, and `project_id` which are included in the response
