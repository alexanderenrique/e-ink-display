#!/usr/bin/env python3
"""Live check that fact-harvest (and refresh) upstream URLs return usable JSON.

Run from this directory (loads .env if present via dotenv):
  .venv/bin/python smoke_fact_upstreams.py

Honours the same env as the app: FUN_FACT_SOURCES_JSON, FUN_*_UPSTREAM_URL, etc.
Exit 0 if every probe succeeds, 1 otherwise.
"""

from __future__ import annotations

import json
import os
import sys
from typing import Any

import httpx
from dotenv import load_dotenv

from fact_harvest import extract_fact_from_json, load_fact_sources
from refresh import ISS_URL, USGS_URL


def _timeout() -> float:
    return float(os.getenv("FACT_UPSTREAM_TIMEOUT_SECONDS", "30"))


def _ua() -> str:
    return os.getenv(
        "FACT_UPSTREAM_USER_AGENT",
        "fun-aggregator/1.0 (fact harvest; +https://github.com/)",
    ).strip() or "fun-aggregator/1.0 (fact harvest; +https://github.com/)"


def probe_fact_source(pool_id: str, url: str, json_path: str | None) -> tuple[bool, str]:
    headers = {"User-Agent": _ua(), "Accept": "application/json"}
    try:
        with httpx.Client(follow_redirects=True, timeout=_timeout()) as client:
            r = client.get(url, headers=headers)
            r.raise_for_status()
            data: Any = r.json()
    except Exception as e:
        return False, f"HTTP/JSON error: {e}"

    text = extract_fact_from_json(data, json_path)
    if not text:
        snippet = json.dumps(data, ensure_ascii=False)[:200]
        return False, f"no extractable fact (json_path={json_path!r}) body≈ {snippet!r}..."
    preview = text[:120] + ("…" if len(text) > 120 else "")
    return True, preview


def probe_geojson_features(url: str) -> tuple[bool, str]:
    headers = {"User-Agent": _ua(), "Accept": "application/json"}
    try:
        with httpx.Client(follow_redirects=True, timeout=_timeout()) as client:
            r = client.get(url, headers=headers)
            r.raise_for_status()
            data = r.json()
    except Exception as e:
        return False, f"HTTP/JSON error: {e}"
    feats = data.get("features") if isinstance(data, dict) else None
    if not isinstance(feats, list) or len(feats) == 0:
        return False, "missing or empty features[]"
    return True, f"{len(feats)} feature(s); first mag={feats[0].get('properties', {}).get('mag')}"


def probe_iss(url: str) -> tuple[bool, str]:
    headers = {"User-Agent": _ua(), "Accept": "application/json"}
    try:
        with httpx.Client(follow_redirects=True, timeout=_timeout()) as client:
            r = client.get(url, headers=headers)
            r.raise_for_status()
            data = r.json()
    except Exception as e:
        return False, f"HTTP/JSON error: {e}"
    if not isinstance(data, dict):
        return False, "not an object"
    lat, lon = data.get("latitude"), data.get("longitude")
    if lat is None or lon is None:
        return False, f"missing latitude/longitude keys: {list(data.keys())[:8]}"
    return True, f"lat={lat} lon={lon}"


def main() -> int:
    load_dotenv()
    failed = False

    sources = load_fact_sources()
    print("=== fact_harvest.load_fact_sources() ===")
    if not sources:
        print("  (no sources — set FUN_FACT_SOURCES_JSON or use default cat/useless env path)")
    for src in sources:
        ok, msg = probe_fact_source(src.pool_id, src.url, src.json_path)
        status = "OK " if ok else "FAIL"
        print(f"  [{status}] {src.pool_id}: {src.url}")
        print(f"         {msg}")
        failed = failed or not ok

    print("\n=== refresh.py (earthquake + ISS slides) ===")
    ok, msg = probe_geojson_features(USGS_URL)
    print(f"  [{'OK ' if ok else 'FAIL'}] USGS: {USGS_URL}")
    print(f"         {msg}")
    failed = failed or not ok

    ok, msg = probe_iss(ISS_URL)
    print(f"  [{'OK ' if ok else 'FAIL'}] ISS: {ISS_URL}")
    print(f"         {msg}")
    failed = failed or not ok

    print()
    if failed:
        print("At least one probe failed.")
        return 1
    print("All probes passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
