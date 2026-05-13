"""Background fetch of USGS + ISS; updates shared FunSlide cache."""

from __future__ import annotations

import asyncio
import logging
import os
from dataclasses import dataclass
from datetime import datetime, timezone

import httpx

from formatters import format_earthquake_geojson, format_iss_payload
from models import FunSlide

log = logging.getLogger(__name__)

USGS_URL = "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/2.5_day.geojson"
ISS_URL = "https://api.wheretheiss.at/v1/satellites/25544"


@dataclass
class DynamicState:
    earthquake: FunSlide | None = None
    iss: FunSlide | None = None
    last_earthquake_refresh: datetime | None = None
    last_iss_refresh: datetime | None = None
    last_error: str | None = None


state = DynamicState()


async def _refresh_once(client: httpx.AsyncClient) -> None:
    errs: list[str] = []
    try:
        r = await client.get(USGS_URL, timeout=30.0)
        r.raise_for_status()
        geo = r.json()
        text = format_earthquake_geojson(geo)
        state.earthquake = FunSlide(layout="earthquake", text=text)
        state.last_earthquake_refresh = datetime.now(timezone.utc)
    except Exception as e:
        errs.append(f"USGS: {e}")
        log.warning("USGS refresh failed: %s", e)

    try:
        r = await client.get(ISS_URL, timeout=30.0)
        r.raise_for_status()
        payload = r.json()
        text = format_iss_payload(payload)
        state.iss = FunSlide(layout="iss", text=text)
        state.last_iss_refresh = datetime.now(timezone.utc)
    except Exception as e:
        errs.append(f"ISS: {e}")
        log.warning("ISS refresh failed: %s", e)

    state.last_error = "; ".join(errs) if errs else None


async def refresh_loop(interval_seconds: float) -> None:
    async with httpx.AsyncClient(follow_redirects=True) as client:
        await _refresh_once(client)
        while True:
            await asyncio.sleep(interval_seconds)
            await _refresh_once(client)


def start_background_refresh(interval_seconds: float) -> asyncio.Task:
    return asyncio.create_task(refresh_loop(interval_seconds))


def refresh_interval_from_env() -> float:
    return float(os.getenv("REFRESH_SECONDS", "900"))
