"""Per-device FIFO queues and named groups for targeted special slides."""

from __future__ import annotations

import json
import logging
import os
import threading
import uuid
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any

log = logging.getLogger(__name__)

_lock = threading.Lock()
_store_path: Path | None = None
_state: dict[str, Any] = {"queues": {}, "groups": {}}


def store_path() -> Path:
    global _store_path
    if _store_path is None:
        raw = os.getenv("FUN_SPECIAL_STORE", "data/special_messages.json")
        _store_path = Path(raw)
    return _store_path


def init_store() -> None:
    path = store_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.is_file():
        return
    try:
        raw = path.read_text(encoding="utf-8")
        data = json.loads(raw)
        if not isinstance(data, dict):
            return
        queues = data.get("queues")
        groups = data.get("groups")
        with _lock:
            if isinstance(queues, dict):
                _state["queues"] = {str(k): v for k, v in queues.items() if isinstance(v, list)}
            if isinstance(groups, dict):
                _state["groups"] = {
                    str(k): [str(x) for x in v if isinstance(x, str)] for k, v in groups.items() if isinstance(v, list)
                }
        log.info("Loaded special-message store (%d queue(s), %d group(s)) from %s", len(_state["queues"]), len(_state["groups"]), path)
    except (OSError, json.JSONDecodeError) as e:
        log.warning("Could not load special messages (%s): %s", path, e)


def _persist_unlocked() -> None:
    path = store_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(_state, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _utc_now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()


def _calendar_day_tz() -> timezone:
    """Fixed offset used to decide which 'calendar date' applies (default UTC−7 ≈ Pacific during DST)."""
    raw = os.getenv("FUN_SPECIAL_CALENDAR_DAY_UTC_OFFSET_HOURS", "-7").strip()
    try:
        hours = float(raw)
    except ValueError:
        log.warning(
            "Invalid FUN_SPECIAL_CALENDAR_DAY_UTC_OFFSET_HOURS=%r; using default -7",
            raw,
        )
        hours = -7.0
    minutes = int(round(hours * 60))
    return timezone(timedelta(minutes=minutes))


def default_expires_calendar_day_cutoff(now: datetime | None = None) -> datetime:
    """UTC-aware instant when the configured local calendar day ends (exclusive next local midnight).

    Default local strip is UTC−7 (not full America/Los_Angeles DST rules; use env to match PST −8 etc.).
    """
    n = now if now is not None else datetime.now(timezone.utc)
    if n.tzinfo is None:
        n = n.replace(tzinfo=timezone.utc)
    cal_tz = _calendar_day_tz()
    local = n.astimezone(cal_tz)
    today_start_local = datetime(local.year, local.month, local.day, tzinfo=cal_tz)
    next_midnight_local = today_start_local + timedelta(days=1)
    return next_midnight_local.astimezone(timezone.utc)


def parse_expires_datetime(exp_raw: Any) -> datetime | None:
    if not exp_raw or not isinstance(exp_raw, str):
        return None
    e = exp_raw.strip()
    if not e:
        return None
    try:
        dt = datetime.fromisoformat(e.replace("Z", "+00:00"))
        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=timezone.utc)
        return dt.astimezone(timezone.utc)
    except ValueError:
        return None


def compose_slide_text(
    *,
    text: str | None = None,
    header: str | None = None,
    body: str | None = None,
) -> str | None:
    """
    Build FunSlide ``text`` for the default layout (red first line, black body).

    Prefer explicit ``header`` / ``body``; fall back to ``text`` (may already contain ``\\n``).
    """
    h = (header or "").strip()
    b = (body or "").strip()
    t = (text or "").strip()
    if h or b:
        if not h:
            return b or None
        if not b:
            return h
        return f"{h}\n{b}"
    return t or None


def _merge_groups(groups_update: dict[str, Any]) -> None:
    if not isinstance(groups_update, dict) or not groups_update:
        return
    tgt = _state.setdefault("groups", {})
    for gid, members in groups_update.items():
        if not isinstance(gid, str) or gid.strip() == "":
            continue
        key = gid.strip()
        if members is None or (isinstance(members, list) and len(members) == 0):
            tgt.pop(key, None)
            continue
        if not isinstance(members, list):
            continue
        tgt[key] = [str(x).strip() for x in members if isinstance(x, str) and str(x).strip()]


def enqueue(
    text: str,
    layout: str | None,
    device_ids: list[str],
    group_ids: list[str],
    *,
    optional_groups_update: dict[str, Any] | None = None,
    expires_at: str | None = None,
) -> dict[str, Any]:
    """
    Optionally merge ``optional_groups_update`` into ``groups``, then enqueue
    one copy of the message for every resolved device_id (FIFO per device).
    """
    trimmed = text.strip()
    if not trimmed:
        return {"ok": False, "error": "text is empty", "enqueued_for": [], "unknown_groups": []}

    with _lock:
        if optional_groups_update:
            _merge_groups(optional_groups_update)

        targets: set[str] = set()
        for raw in device_ids:
            if isinstance(raw, str) and raw.strip():
                targets.add(raw.strip())

        grp = _state.setdefault("groups", {})
        unknown_groups: list[str] = []
        for gid in group_ids:
            if not isinstance(gid, str) or not gid.strip():
                continue
            key = gid.strip()
            if key not in grp:
                unknown_groups.append(key)
                continue
            for did in grp.get(key, []):
                if did:
                    targets.add(did)

        if not targets:
            _persist_unlocked()
            err = "no target device_ids or group_ids resolved"
            if unknown_groups:
                err = "unknown group_ids: " + ", ".join(unknown_groups)
            return {
                "ok": False,
                "error": err,
                "enqueued_for": [],
                "unknown_groups": unknown_groups,
            }

        queues: dict[str, Any] = _state.setdefault("queues", {})
        now = _utc_now_iso()
        slide_layout = (layout or "default").strip() or "default"
        enqueued: list[str] = []

        entry: dict[str, Any] = {
            "id": str(uuid.uuid4()),
            "text": trimmed,
            "layout": slide_layout,
            "created_at": now,
        }
        if expires_at and isinstance(expires_at, str) and expires_at.strip():
            entry["expires_at"] = expires_at.strip()
        else:
            entry["expires_at"] = default_expires_calendar_day_cutoff(None).isoformat()

        for did in sorted(targets):
            q = queues.setdefault(did, [])
            item = dict(entry)
            item["id"] = str(uuid.uuid4())
            q.append(item)
            enqueued.append(did)

        _persist_unlocked()

        return {
            "ok": True,
            "enqueued_for": enqueued,
            "unknown_groups": unknown_groups,
        }


def pop_next_slide(device_id: str | None) -> dict[str, Any] | None:
    """Pop oldest eligible message for this device; return FunSlide fields or None."""
    if not device_id or not device_id.strip():
        return None
    did = device_id.strip()

    now = datetime.now(timezone.utc)

    def _expired(exp_raw: Any) -> bool:
        dt = parse_expires_datetime(exp_raw)
        if dt is None:
            return False
        return dt <= now

    with _lock:
        queues: dict[str, Any] = _state.setdefault("queues", {})
        q = queues.get(did)
        if not isinstance(q, list):
            return None

        mutated = False
        while q:
            item = q[0]
            if not isinstance(item, dict):
                q.pop(0)
                mutated = True
                continue
            if _expired(item.get("expires_at")):
                q.pop(0)
                mutated = True
                continue
            expiry_dt = parse_expires_datetime(item.get("expires_at"))
            q.pop(0)
            mutated = True
            txt = item.get("text")
            lay = item.get("layout") or "default"
            if not isinstance(txt, str) or not txt.strip():
                continue
            slide: dict[str, Any] = {
                "layout": str(lay),
                "text": txt.strip(),
            }
            if expiry_dt is not None:
                slide["display_hold_until_epoch"] = int(expiry_dt.timestamp())
            if mutated:
                _persist_unlocked()
            return slide
        if mutated:
            _persist_unlocked()
        return None
