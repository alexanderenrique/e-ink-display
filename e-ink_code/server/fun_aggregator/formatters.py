"""Format earthquake and ISS strings to match firmware/apps/fun/fetch.cpp."""

from __future__ import annotations

import time
from typing import Any

from geo_lookup import describe_nearest_place


def is_pacific_dst(month: int, day: int) -> bool:
    """Match fetch.cpp isPacificDST (approximate calendar rule)."""
    if month < 3 or (month == 3 and day < 8):
        return False
    if month > 11 or (month == 11 and day > 7):
        return False
    return True


def format_earthquake_geojson(geo: dict[str, Any]) -> str:
    features = geo.get("features") or []
    if not features:
        return "Latest Earthquake\nNo recent earthquakes in feed"
    props = features[0].get("properties") or {}
    mag = float(props.get("mag", 0.0))
    place = str(props.get("place", ""))
    time_ms = int(props.get("time", 0))
    time_s = time_ms // 1000

    pacific = time_s - (8 * 3600)
    t = time.gmtime(pacific)
    is_dst = is_pacific_dst(t.tm_mon, t.tm_mday)
    if is_dst:
        pacific = time_s - (7 * 3600)
        t = time.gmtime(pacific)

    if t:
        tz_label = "PDT" if is_dst else "PST"
        time_str = time.strftime("%Y-%m-%d %H:%M ", t) + tz_label
    else:
        time_str = f"Time: {time_ms}"

    return f"Latest Earthquake\nM {mag:.1f} - {place}\n{time_str}"


def format_iss_payload(payload: dict[str, Any]) -> str:
    lat = float(payload["latitude"])
    lon = float(payload["longitude"])
    altitude_km = float(payload["altitude"])
    velocity_kph = float(payload["velocity"])
    altitude_miles = altitude_km * 0.621371
    velocity_mph = velocity_kph * 0.621371
    geo = describe_nearest_place(lat, lon)
    return (
        f"Where is the ISS?\n"
        f"{geo}\n"
        f"Altitude: {altitude_miles:.2f} mi\n"
        f"Velocity: {velocity_mph:.2f} mph\n"
    )
