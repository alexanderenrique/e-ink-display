"""Load static fact pools from data/*.json."""

from __future__ import annotations

import json
import random
from pathlib import Path
from typing import Any

_DATA = Path(__file__).resolve().parent / "data"


def _load_strings(name: str) -> list[str]:
    path = _DATA / f"{name}.json"
    if not path.exists():
        return []
    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, list):
        return [str(x).strip() for x in data if str(x).strip()]
    return []


def get_pool(name: str) -> list[str]:
    return _load_strings(name)


def sample_pool(name: str, count: int, cap: int) -> list[str]:
    pool = get_pool(name)
    if not pool:
        return []
    n = max(0, min(count, cap, len(pool)))
    if n == 0:
        return []
    if n >= len(pool):
        return random.sample(pool, k=len(pool))
    return random.sample(pool, k=n)


def format_cat_slide(body: str) -> str:
    return "Cat Facts\n" + body.strip()


def format_useless_slide(body: str) -> str:
    return "Fun Fact!\n" + body.strip()


def nonempty_fact_pool_ids() -> list[str]:
    """Pool ids backed by non-empty ``data/*_facts.json`` (list-of-strings files)."""
    out: list[str] = []
    for path in sorted(_DATA.glob("*_facts.json")):
        pid = path.stem
        if get_pool(pid):
            out.append(pid)
    return out


def format_slide_for_pool(pool_id: str, body: str) -> str:
    """Heading + body; matches legacy cat/useless strings for built-in pools."""
    if pool_id == "cat_facts":
        return format_cat_slide(body)
    if pool_id == "useless_facts":
        return format_useless_slide(body)
    base = pool_id[: -len("_facts")] if pool_id.endswith("_facts") else pool_id
    title = base.replace("_", " ").strip().title() + " Facts"
    return title + "\n" + body.strip()


def sample_mixed_slides(count: int, cap: int) -> list[dict[str, Any]]:
    """Build FunSlide dicts from a random mix of all registered non-empty fact pools."""
    from models import FunSlide

    pool_ids = nonempty_fact_pool_ids()
    if not pool_ids or count <= 0:
        return []
    n = max(0, min(count, cap))
    out: list[dict[str, Any]] = []
    attempts = 0
    max_attempts = max(n * 10, 20)
    while len(out) < n and attempts < max_attempts:
        attempts += 1
        pid = random.choice(pool_ids)
        bodies = sample_pool(pid, 1, 1)
        if not bodies:
            continue
        text = format_slide_for_pool(pid, bodies[0])
        out.append(FunSlide(layout="default", text=text).model_dump())
    return out
