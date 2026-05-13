"""Tests for fact_harvest: JSON extraction, FIFO pools, and mocked HTTP."""

from __future__ import annotations

import asyncio
import json
from pathlib import Path

import httpx
import pytest

import fact_harvest


def test_extract_fact_from_json_path() -> None:
    data = {"nested": {"fact": "  Hello world  "}}
    assert fact_harvest.extract_fact_from_json(data, "nested.fact") == "Hello world"


def test_extract_fact_fallback_keys() -> None:
    assert fact_harvest.extract_fact_from_json({"fact": "x y"}, None) == "x y"
    assert fact_harvest.extract_fact_from_json({"text": "a"}, None) == "a"
    assert fact_harvest.extract_fact_from_json({"data": "b"}, None) == "b"
    assert fact_harvest.extract_fact_from_json(
        {"data": ["legacy meowfacts line"]}, None
    ) == "legacy meowfacts line"


def test_load_fact_sources_legacy_default_upstreams(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("FUN_FACT_SOURCES_JSON", raising=False)
    monkeypatch.delenv("FUN_CAT_UPSTREAM_URL", raising=False)
    monkeypatch.delenv("FUN_USELESS_UPSTREAM_URL", raising=False)
    monkeypatch.delenv("FUN_FUN_UPSTREAM_URL", raising=False)
    sources = fact_harvest.load_fact_sources()
    assert len(sources) == 2
    assert sources[0].pool_id == "cat_facts"
    assert sources[0].url == fact_harvest.DEFAULT_CAT_FACT_UPSTREAM_URL
    assert sources[0].json_path is None
    assert sources[1].pool_id == "useless_facts"
    assert sources[1].url == fact_harvest.DEFAULT_USELESS_FACT_UPSTREAM_URL
    assert sources[1].json_path is None


def test_load_fact_sources_cat_env_overrides_default(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("FUN_FACT_SOURCES_JSON", raising=False)
    monkeypatch.setenv("FUN_CAT_UPSTREAM_URL", "https://custom.example/cat")
    monkeypatch.delenv("FUN_USELESS_UPSTREAM_URL", raising=False)
    monkeypatch.delenv("FUN_FUN_UPSTREAM_URL", raising=False)
    sources = fact_harvest.load_fact_sources()
    assert len(sources) == 2
    assert sources[0].url == "https://custom.example/cat"
    assert sources[1].url == fact_harvest.DEFAULT_USELESS_FACT_UPSTREAM_URL


def test_load_fact_sources_useless_env_overrides_default(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("FUN_FACT_SOURCES_JSON", raising=False)
    monkeypatch.delenv("FUN_CAT_UPSTREAM_URL", raising=False)
    monkeypatch.setenv("FUN_USELESS_UPSTREAM_URL", "https://custom.example/useless")
    monkeypatch.delenv("FUN_FUN_UPSTREAM_URL", raising=False)
    sources = fact_harvest.load_fact_sources()
    assert len(sources) == 2
    assert sources[0].url == fact_harvest.DEFAULT_CAT_FACT_UPSTREAM_URL
    assert sources[1].url == "https://custom.example/useless"


def test_append_fifo_dedupe_and_cap(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    monkeypatch.setattr(fact_harvest, "_DATA", tmp_path)
    monkeypatch.setenv("FACT_POOL_MAX_LINES", "3")
    assert fact_harvest.append_fact_fifo("cat_facts", "alpha", 3) is True
    assert fact_harvest.append_fact_fifo("cat_facts", "alpha", 3) is False
    assert fact_harvest.append_fact_fifo("cat_facts", "beta", 3) is True
    assert fact_harvest.append_fact_fifo("cat_facts", "gamma", 3) is True
    assert fact_harvest.append_fact_fifo("cat_facts", "delta", 3) is True
    raw = (tmp_path / "cat_facts.json").read_text(encoding="utf-8")
    arr = json.loads(raw)
    assert arr == ["beta", "gamma", "delta"]


def test_load_pool_truncates_legacy_long_file(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    monkeypatch.setattr(fact_harvest, "_DATA", tmp_path)
    long_list = [f"f{i}" for i in range(40)]
    (tmp_path / "cat_facts.json").write_text(
        json.dumps(long_list),
        encoding="utf-8",
    )
    lines = fact_harvest.load_pool_lines("cat_facts", max_lines=30)
    assert len(lines) == 30
    assert lines[0] == "f10"
    assert lines[-1] == "f39"


def test_harvest_round_robin_mock_http(
    monkeypatch: pytest.MonkeyPatch, tmp_path: Path
) -> None:
    fact_harvest._ROUND_ROBIN_INDEX = 0  # noqa: SLF001
    monkeypatch.setattr(fact_harvest, "_DATA", tmp_path)
    monkeypatch.delenv("FUN_CAT_UPSTREAM_URL", raising=False)
    monkeypatch.delenv("FUN_USELESS_UPSTREAM_URL", raising=False)
    monkeypatch.delenv("FUN_FUN_UPSTREAM_URL", raising=False)
    monkeypatch.delenv("FUN_FACT_SOURCES_JSON", raising=False)
    monkeypatch.setenv(
        "FUN_FACT_SOURCES_JSON",
        json.dumps(
            [
                {"pool": "cat_facts", "url": "https://a.example/f"},
                {"pool": "useless_facts", "url": "https://b.example/f"},
            ]
        ),
    )
    monkeypatch.setenv("FACT_ROUND_ROBIN", "1")
    monkeypatch.setenv("FACT_POOL_MAX_LINES", "10")
    monkeypatch.setenv("FACT_FETCHES_PER_SOURCE_PER_CYCLE", "1")

    def handler(request: httpx.Request) -> httpx.Response:
        if request.url.host == "a.example":
            return httpx.Response(200, json={"fact": "cat line"})
        if request.url.host == "b.example":
            return httpx.Response(200, json={"text": "useless line"})
        return httpx.Response(404)

    transport = httpx.MockTransport(handler)
    sources = fact_harvest.load_fact_sources()
    assert len(sources) == 2

    async def _two_ticks() -> tuple[int, int]:
        async with httpx.AsyncClient(transport=transport) as client:
            n1 = await fact_harvest.harvest_sources(client, sources, round_robin=True)
            n2 = await fact_harvest.harvest_sources(client, sources, round_robin=True)
            return n1, n2

    n, n2 = asyncio.run(_two_ticks())
    assert n == 1
    assert n2 == 1
    cat = json.loads((tmp_path / "cat_facts.json").read_text(encoding="utf-8"))
    assert cat == ["cat line"]
    use = json.loads((tmp_path / "useless_facts.json").read_text(encoding="utf-8"))
    assert use == ["useless line"]


def test_harvest_429_sets_backoff(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    monkeypatch.setattr(fact_harvest, "_DATA", tmp_path)
    monkeypatch.setenv("FUN_FACT_SOURCES_JSON", json.dumps([{"pool": "cat_facts", "url": "https://x/h"}]))
    fact_harvest._backoff_until = None  # noqa: SLF001

    def handler(request: httpx.Request) -> httpx.Response:
        return httpx.Response(429, headers={"Retry-After": "12"})

    transport = httpx.MockTransport(handler)
    sources = fact_harvest.load_fact_sources()

    async def _once() -> int:
        async with httpx.AsyncClient(transport=transport) as client:
            return await fact_harvest.harvest_sources(client, sources, round_robin=False)

    assert asyncio.run(_once()) == 0
    assert fact_harvest._backoff_until is not None  # noqa: SLF001
