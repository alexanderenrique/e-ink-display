"""Periodic fetch of third-party fact APIs into data/*_facts.json with FIFO caps."""

from __future__ import annotations

import asyncio
import json
import logging
import os
import re
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from email.utils import parsedate_to_datetime
from pathlib import Path
from typing import Any

import httpx

log = logging.getLogger(__name__)

_DATA = Path(__file__).resolve().parent / "data"

_DEFAULT_UA = "fun-aggregator/1.0 (fact harvest; +https://github.com/)"

_ROUND_ROBIN_INDEX = 0


@dataclass
class FactHarvestState:
    last_tick_at: datetime | None = None
    last_error: str | None = None
    last_added: int = 0
    sources_configured: int = 0
    round_robin: bool = True


state = FactHarvestState()


@dataclass
class FactSource:
    pool_id: str
    url: str
    json_path: str | None = None


def _truthy_env(name: str, default: bool) -> bool:
    raw = os.getenv(name)
    if raw is None or raw.strip() == "":
        return default
    return raw.strip().lower() in ("1", "true", "yes", "on")


def fact_interval_from_env() -> float:
    return float(os.getenv("FACT_REFRESH_SECONDS", "400"))


def _pool_max_lines() -> int:
    return max(1, int(os.getenv("FACT_POOL_MAX_LINES", "30")))


def _fetches_per_tick() -> int:
    return max(1, int(os.getenv("FACT_FETCHES_PER_SOURCE_PER_CYCLE", "1")))


def _inter_fetch_delay() -> float:
    return max(0.0, float(os.getenv("FACT_INTER_SOURCE_DELAY_SECONDS", "1.5")))


def _max_fact_chars() -> int:
    return max(40, int(os.getenv("FACT_MAX_FACT_CHARS", "300")))


def _request_timeout() -> float:
    return float(os.getenv("FACT_UPSTREAM_TIMEOUT_SECONDS", "30"))


def _user_agent() -> str:
    return os.getenv("FACT_UPSTREAM_USER_AGENT", _DEFAULT_UA).strip() or _DEFAULT_UA


def load_fact_sources() -> list[FactSource]:
    raw = os.getenv("FUN_FACT_SOURCES_JSON", "").strip()
    if raw:
        try:
            arr = json.loads(raw)
        except json.JSONDecodeError as e:
            log.warning("FUN_FACT_SOURCES_JSON invalid JSON: %s", e)
            return []
        out: list[FactSource] = []
        if not isinstance(arr, list):
            return []
        for item in arr:
            if not isinstance(item, dict):
                continue
            pid = str(item.get("pool") or item.get("pool_id") or "").strip()
            url = str(item.get("url") or "").strip()
            jp = item.get("json_path")
            path = str(jp).strip() if jp not in (None, "") else None
            if pid and url:
                out.append(FactSource(pool_id=pid, url=url, json_path=path))
        return out

    sources: list[FactSource] = []
    mapping: list[tuple[str, str, str]] = [
        ("cat_facts", "FUN_CAT_UPSTREAM_URL", "FUN_CAT_JSON_PATH"),
        ("useless_facts", "FUN_USELESS_UPSTREAM_URL", "FUN_USELESS_JSON_PATH"),
        ("fun_facts", "FUN_FUN_UPSTREAM_URL", "FUN_FUN_JSON_PATH"),
    ]
    for pool_id, url_key, path_key in mapping:
        url = os.getenv(url_key, "").strip()
        if not url:
            continue
        jp = os.getenv(path_key, "").strip() or None
        sources.append(FactSource(pool_id=pool_id, url=url, json_path=jp))
    return sources


def _normalize_text(text: str) -> str:
    t = text.strip()
    t = re.sub(r"\s+", " ", t)
    return t[: _max_fact_chars()]


def _dedupe_key(text: str) -> str:
    return _normalize_text(text).casefold()


def extract_fact_from_json(data: Any, json_path: str | None) -> str | None:
    if json_path:
        cur: Any = data
        for part in json_path.split("."):
            part = part.strip()
            if not part:
                continue
            if isinstance(cur, dict) and part in cur:
                cur = cur[part]
            else:
                cur = None
                break
        if isinstance(cur, str) and cur.strip():
            return _normalize_text(cur)
        if cur is not None and not isinstance(cur, (dict, list)):
            s = str(cur).strip()
            if s:
                return _normalize_text(s)

    if isinstance(data, dict):
        for k in ("fact", "text", "data"):
            v = data.get(k)
            if isinstance(v, str) and v.strip():
                return _normalize_text(v)
    return None


def _pool_path(pool_id: str) -> Path:
    if not pool_id or ".." in pool_id or "/" in pool_id or "\\" in pool_id:
        raise ValueError("invalid pool_id")
    return _DATA / f"{pool_id}.json"


def load_pool_lines(pool_id: str, max_lines: int) -> list[str]:
    path = _pool_path(pool_id)
    if not path.exists():
        return []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError) as e:
        log.warning("Could not read %s: %s", path, e)
        return []
    if not isinstance(data, list):
        return []
    lines = [str(x).strip() for x in data if str(x).strip()]
    if len(lines) > max_lines:
        lines = lines[-max_lines:]
    return lines


def _atomic_write_pool(pool_id: str, lines: list[str]) -> None:
    path = _pool_path(pool_id)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".json.tmp")
    payload = json.dumps(lines, ensure_ascii=False, indent=2) + "\n"
    tmp.write_text(payload, encoding="utf-8")
    os.replace(tmp, path)


def append_fact_fifo(pool_id: str, fact: str, max_lines: int) -> bool:
    """Append normalized fact; drop oldest if over cap. Returns True if a new line was added."""
    norm = _normalize_text(fact)
    if not norm:
        return False
    key = _dedupe_key(norm)
    pool = load_pool_lines(pool_id, max_lines=max_lines)
    existing_keys = {_dedupe_key(x) for x in pool}
    if key in existing_keys:
        return False
    pool.append(norm)
    while len(pool) > max_lines:
        pool.pop(0)
    _atomic_write_pool(pool_id, pool)
    return True


def _parse_retry_after(response: httpx.Response) -> float | None:
    h = response.headers.get("Retry-After")
    if not h:
        return None
    h = h.strip()
    if h.isdigit():
        return float(h)
    try:
        dt = parsedate_to_datetime(h)
        if dt is None:
            return None
        if dt.tzinfo is None:
            dt = dt.replace(tzinfo=timezone.utc)
        now = datetime.now(timezone.utc)
        sec = (dt - now).total_seconds()
        return max(1.0, sec)
    except Exception:
        return None


_backoff_until: datetime | None = None


def _check_clear_backoff() -> bool:
    """Return True if we should skip network (still in backoff window)."""
    global _backoff_until
    now = datetime.now(timezone.utc)
    if _backoff_until and now < _backoff_until:
        return True
    _backoff_until = None
    return False


def _register_backoff_from_response(response: httpx.Response) -> None:
    global _backoff_until
    if response.status_code != 429:
        return
    wait = _parse_retry_after(response)
    if wait is None:
        wait = 60.0
    _backoff_until = datetime.now(timezone.utc) + timedelta(seconds=wait)
    log.warning("Upstream 429; no more upstream calls until %s", _backoff_until.isoformat())


async def _fetch_one(
    client: httpx.AsyncClient, src: FactSource
) -> tuple[str | None, httpx.Response | None]:
    if _check_clear_backoff():
        return None, None
    headers = {"User-Agent": _user_agent(), "Accept": "application/json"}
    try:
        r = await client.get(src.url, headers=headers, timeout=_request_timeout())
    except Exception as e:
        log.warning("GET %s failed: %s", src.url, e)
        return None, None
    if r.status_code == 429:
        _register_backoff_from_response(r)
        return None, r
    try:
        r.raise_for_status()
    except httpx.HTTPStatusError as e:
        log.warning("GET %s bad status: %s", src.url, e)
        return None, r
    try:
        data = r.json()
    except json.JSONDecodeError:
        log.warning("GET %s returned non-JSON", src.url)
        return None, r
    text = extract_fact_from_json(data, src.json_path)
    if not text:
        log.warning("GET %s: could not extract fact text", src.url)
        return None, r
    return text, r


async def harvest_sources(
    client: httpx.AsyncClient, sources: list[FactSource], *, round_robin: bool
) -> int:
    """Run one scheduler tick; return number of new facts added (all pools)."""
    global _ROUND_ROBIN_INDEX
    if not sources:
        return 0
    if _check_clear_backoff():
        state.last_error = "rate_limited_backoff"
        return 0

    max_lines = _pool_max_lines()
    n_per = _fetches_per_tick()
    delay = _inter_fetch_delay()
    added = 0

    if round_robin:
        idx = _ROUND_ROBIN_INDEX % len(sources)
        _ROUND_ROBIN_INDEX += 1
        batch = [sources[idx]]
    else:
        batch = list(sources)

    stop = False
    for src in batch:
        if stop:
            break
        for i in range(n_per):
            if i > 0 and delay > 0:
                await asyncio.sleep(delay)
            text, response = await _fetch_one(client, src)
            if response is None and text is None:
                continue
            if text and append_fact_fifo(src.pool_id, text, max_lines):
                added += 1
            if response is not None and response.status_code == 429:
                stop = True
                break

    state.last_error = None
    return added


def fact_state_snapshot() -> dict[str, Any] | None:
    cfg = load_fact_sources()
    if not cfg:
        return None
    out: dict[str, Any] = {
        "sources_configured": len(cfg),
        "round_robin": state.round_robin,
        "last_tick_at": state.last_tick_at.isoformat() if state.last_tick_at else None,
        "last_added": state.last_added,
        "last_error": state.last_error,
    }
    if _backoff_until:
        out["backoff_until"] = _backoff_until.isoformat()
    return out


async def fact_harvest_loop(interval_seconds: float) -> None:
    headers = {"User-Agent": _user_agent()}
    async with httpx.AsyncClient(follow_redirects=True, headers=headers) as client:
        while True:
            sources = load_fact_sources()
            state.sources_configured = len(sources)
            state.round_robin = _truthy_env("FACT_ROUND_ROBIN", True)
            if not sources:
                log.debug("No upstream fact sources configured; sleeping")
                await asyncio.sleep(interval_seconds)
                continue

            state.last_tick_at = datetime.now(timezone.utc)
            try:
                added = await harvest_sources(
                    client, sources, round_robin=state.round_robin
                )
                state.last_added = added
            except Exception as e:
                state.last_error = str(e)
                log.exception("Fact harvest tick failed: %s", e)
            await asyncio.sleep(interval_seconds)


def start_fact_harvest_task(interval_seconds: float) -> asyncio.Task:
    """Always start the loop; it sleeps when no FUN_* URLs are configured."""
    return asyncio.create_task(fact_harvest_loop(interval_seconds))
