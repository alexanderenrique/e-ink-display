"""Nearest-city and coarse ocean labels for ISS position."""

from __future__ import annotations

import json
import math
from pathlib import Path

_EARTH_RADIUS_KM = 6371.0
_DEG_TO_RAD = 0.017453292519943295
_NEAR_THRESHOLD_KM = 1000.0

# Very coarse lat/lon boxes (checked in order). Used only when far from catalog cities.
_OCEAN_REGIONS: tuple[tuple[float, float, float, float, str], ...] = (
    # lat_min, lat_max, lon_min, lon_max, label
    (66.0, 90.0, -180.0, 180.0, "Arctic Ocean"),
    (-90.0, -50.0, -180.0, 180.0, "Southern Ocean"),
    (0.0, 66.0, -80.0, 0.0, "northern Atlantic"),
    (-50.0, 0.0, -70.0, 25.0, "southern Atlantic"),
    (0.0, 66.0, 120.0, 180.0, "northern Pacific"),
    (0.0, 66.0, -180.0, -120.0, "northern Pacific"),
    (-50.0, 0.0, 120.0, 180.0, "southern Pacific"),
    (-50.0, 0.0, -180.0, -120.0, "southern Pacific"),
    (0.0, 66.0, -165.0, -70.0, "northern Pacific"),
    (-50.0, 0.0, -165.0, -70.0, "southern Pacific"),
    (0.0, 30.0, 40.0, 100.0, "northern Indian Ocean"),
    (-50.0, 0.0, 20.0, 120.0, "southern Indian Ocean"),
)


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


def _normalize_lon(lon: float) -> float:
    return ((lon + 180.0) % 360.0) - 180.0


def _in_lon_band(lon: float, lon_min: float, lon_max: float) -> bool:
    lon = _normalize_lon(lon)
    lon_min = _normalize_lon(lon_min)
    lon_max = _normalize_lon(lon_max)
    if lon_min <= lon_max:
        return lon_min <= lon <= lon_max
    return lon >= lon_min or lon <= lon_max


def _describe_ocean_region(lat: float, lon: float) -> str | None:
    for lat_min, lat_max, lon_min, lon_max, label in _OCEAN_REGIONS:
        if lat_min <= lat <= lat_max and _in_lon_band(lon, lon_min, lon_max):
            return label
    return None


def describe_nearest_place(lat: float, lon: float) -> str:
    best_km = 1.0e12
    best_label = "unknown area"
    for p in _points():
        d = _haversine_km(lat, lon, float(p["lat"]), float(p["lon"]))
        if d < best_km:
            best_km = d
            best_label = str(p["label"])
    if best_km < _NEAR_THRESHOLD_KM:
        return f"Roughly near: {best_label}"
    ocean = _describe_ocean_region(lat, lon)
    if ocean:
        return f"Roughly over: the {ocean}"
    return "middle of nowhere"
