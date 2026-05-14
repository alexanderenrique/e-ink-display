"""Nearest-city line for ISS position; matches firmware iss_geo_lookup.cpp logic."""

from __future__ import annotations

import json
import math
from pathlib import Path

_EARTH_RADIUS_KM = 6371.0
_DEG_TO_RAD = 0.017453292519943295
_NEAR_THRESHOLD_KM = 600.0


def _haversine_km(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    dlat = (lat2 - lat1) * _DEG_TO_RAD
    dlon = (lon2 - lon1) * _DEG_TO_RAD
    a1 = lat1 * _DEG_TO_RAD
    a2 = lat2 * _DEG_TO_RAD
    h = math.sin(dlat * 0.5) ** 2 + math.cos(a1) * math.cos(a2) * math.sin(dlon * 0.5) ** 2
    if h >= 1.0:
        h = 1.0
    c = 2.0 * math.atan2(math.sqrt(h), math.sqrt(1.0 - h))
    return _EARTH_RADIUS_KM * c


def _load_points() -> list[dict]:
    data_path = Path(__file__).resolve().parent / "data" / "geo_points.json"
    raw = json.loads(data_path.read_text(encoding="utf-8"))
    return raw


_POINTS: list[dict] | None = None


def _points() -> list[dict]:
    global _POINTS
    if _POINTS is None:
        _POINTS = _load_points()
    return _POINTS


def describe_nearest_place(lat: float, lon: float) -> str:
    best_km = 1.0e12
    best_label = "unknown area"
    for p in _points():
        d = _haversine_km(lat, lon, float(p["lat"]), float(p["lon"]))
        if d < best_km:
            best_km = d
            best_label = str(p["label"])
    dist_mi = best_km * 0.621371
    if best_km < _NEAR_THRESHOLD_KM:
        return f"Roughly near: {best_label}"
        return f"~{dist_mi:.0f} miles from {best_label}"
