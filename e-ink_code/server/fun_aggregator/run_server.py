"""Uvicorn entrypoint for systemd and manual runs.

Default listen address: ``0.0.0.0:8081`` (override with ``FUN_LISTEN_HOST`` / ``FUN_LISTEN_PORT``).
"""

from __future__ import annotations

import os

import uvicorn

_DEFAULT_HOST = "0.0.0.0"
_DEFAULT_PORT = 8081


def listen_host() -> str:
    return os.getenv("FUN_LISTEN_HOST", _DEFAULT_HOST).strip() or _DEFAULT_HOST


def listen_port() -> int:
    raw = os.getenv("FUN_LISTEN_PORT", str(_DEFAULT_PORT)).strip()
    try:
        port = int(raw)
    except ValueError:
        port = _DEFAULT_PORT
    return max(1, min(port, 65535))


if __name__ == "__main__":
    uvicorn.run("main:app", host=listen_host(), port=listen_port())
