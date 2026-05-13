"""Persist device_id registry (friendly names, optional MAC) for fun clients."""

from __future__ import annotations

import json
import logging
import os
import threading
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

log = logging.getLogger(__name__)

_lock = threading.Lock()
_store_path: Path | None = None
_roster: dict[str, Any] = {"devices": {}}


def roster_path() -> Path:
    global _store_path
    if _store_path is None:
        raw = os.getenv("FUN_DEVICE_STORE", "data/devices.json")
        _store_path = Path(raw)
    return _store_path


def init_store() -> None:
    """Load roster from disk if present; ensure parent directory exists."""
    path = roster_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.is_file():
        return
    try:
        raw = path.read_text(encoding="utf-8")
        data = json.loads(raw)
        if isinstance(data, dict) and "devices" in data and isinstance(data["devices"], dict):
            with _lock:
                _roster["devices"].update(data["devices"])
            log.info("Loaded %d device(s) from %s", len(data["devices"]), path)
    except (OSError, json.JSONDecodeError) as e:
        log.warning("Could not load device roster (%s): %s", path, e)


def _persist_unlocked() -> None:
    path = roster_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(_roster, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def register_device(friendly_name: str, hardware_mac: str | None) -> str:
    """Allocate a new device_id, persist row, return uuid string."""
    fid = str(uuid.uuid4())
    now = datetime.now(timezone.utc).isoformat()
    entry: dict[str, Any] = {
        "friendly_name": friendly_name.strip() or "unnamed",
        "hardware_mac": hardware_mac.strip() if hardware_mac else None,
        "registered_at": now,
        "last_seen_at": now,
    }
    with _lock:
        _roster.setdefault("devices", {})[fid] = entry
        _persist_unlocked()
    log.info(
        "Registered device id=%s friendly_name=%r mac=%r",
        fid,
        entry["friendly_name"],
        entry["hardware_mac"],
    )
    return fid


def note_seen(device_id: str | None, friendly_name: str | None) -> None:
    """Update last_seen and optional friendly_name for an existing device."""
    if not device_id or not device_id.strip():
        return
    did = device_id.strip()
    name = (friendly_name or "").strip()
    with _lock:
        devices: dict[str, Any] = _roster.setdefault("devices", {})
        if did not in devices:
            # Unknown id (e.g. phone-mint UUID not yet in file); create minimal row
            devices[did] = {
                "friendly_name": name or "unnamed",
                "hardware_mac": None,
                "registered_at": datetime.now(timezone.utc).isoformat(),
                "last_seen_at": None,
            }
        row = devices[did]
        row["last_seen_at"] = datetime.now(timezone.utc).isoformat()
        if name:
            row["friendly_name"] = name
        _persist_unlocked()


def roster_entry(device_id: str) -> dict[str, Any] | None:
    with _lock:
        return dict(_roster.get("devices", {}).get(device_id, {}))
