"""
Fun facts aggregator API. Run: uvicorn main:app --host 0.0.0.0 --port 8080
(Use the venv's uvicorn on Raspberry Pi under systemd — see deploy/fun-aggregator.service.)

Environment (often /etc/fun-aggregator.env): FUN_API_KEY (shared secret header X-Fun-Key),
FUN_REQUIRE_API_KEY=1 to fail startup without FUN_API_KEY, FUN_RATE_LIMIT_SCREEN and FUN_RATE_LIMIT_BATCH
(SlowAPI per-IP defaults: 60/minute and 40/minute for GET fun endpoints.)

Upstream facts (optional): FUN_CAT_UPSTREAM_URL, FUN_USELESS_UPSTREAM_URL, FUN_FUN_UPSTREAM_URL,
FUN_CAT_JSON_PATH / FUN_USELESS_JSON_PATH / FUN_FUN_JSON_PATH (dot-separated JSON keys), or FUN_FACT_SOURCES_JSON.
FACT_REFRESH_SECONDS (default 400), FACT_ROUND_ROBIN (default 1), FACT_POOL_MAX_LINES (default 30),
FACT_FETCHES_PER_SOURCE_PER_CYCLE, FACT_INTER_SOURCE_DELAY_SECONDS, FACT_UPSTREAM_USER_AGENT, etc.

POST /v1/devices/register validates X-Fun-Key but is not SlowAPI-wrapped so JSON bodies parse reliably with current stack.

Device roster JSON: FUN_DEVICE_STORE (default data/devices.json).
Special-message queues: FUN_SPECIAL_STORE (default data/special_messages.json); default ``expires_at`` uses
FUN_SPECIAL_CALENDAR_DAY_UTC_OFFSET_HOURS=-7 calendar midnights (−8 for fixed PST).
Admin POST requires FUN_ADMIN_API_KEY (header X-Fun-Admin-Key).
"""

from __future__ import annotations

import asyncio
import logging
import os
from contextlib import asynccontextmanager
from typing import Annotated, Any

from dotenv import load_dotenv
from fastapi import FastAPI, Header, HTTPException, Query, Request
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field
from pydantic.functional_validators import field_validator
from slowapi import Limiter, _rate_limit_exceeded_handler
from slowapi.errors import RateLimitExceeded
from slowapi.util import get_remote_address
from starlette.responses import Response

import device_roster
import special_messages
from models import FunSlide
from fact_harvest import fact_interval_from_env, fact_state_snapshot, start_fact_harvest_task
from pools import format_cat_slide, format_useless_slide, sample_mixed_slides, sample_pool
from refresh import refresh_interval_from_env, start_background_refresh, state

load_dotenv()

log = logging.getLogger(__name__)

MODE_EARTHQUAKE = 1
MODE_CAT = 2
MODE_ISS = 3
MODE_USELESS = 4

MAX_BATCH_PER_SLOT = 25

_bg_task: asyncio.Task | None = None
_fact_task: asyncio.Task | None = None


def _require_fun_api_key_if_configured() -> None:
    """When FUN_REQUIRE_API_KEY is truthy, refuse to boot without FUN_API_KEY."""
    raw = os.getenv("FUN_REQUIRE_API_KEY", "").strip().lower()
    if raw not in ("1", "true", "yes", "on"):
        return
    key = os.getenv("FUN_API_KEY")
    if not key or not str(key).strip():
        raise RuntimeError(
            "FUN_REQUIRE_API_KEY is set but FUN_API_KEY is missing or empty. "
            "Set FUN_API_KEY in /etc/fun-aggregator.env or unset FUN_REQUIRE_API_KEY for local/dev."
        )


def _rate_screen() -> str:
    return os.getenv("FUN_RATE_LIMIT_SCREEN", "60/minute").strip() or "60/minute"


def _rate_batch() -> str:
    return os.getenv("FUN_RATE_LIMIT_BATCH", "40/minute").strip() or "40/minute"


limiter = Limiter(key_func=get_remote_address, default_limits=[])


class DeviceRegisterBody(BaseModel):
    friendly_name: str = Field(default="unnamed", max_length=160)
    hardware_mac: str | None = Field(default=None, max_length=48)


class SpecialEnqueueBody(BaseModel):
    """Enqueue one special slide copy per resolved device UUID (FIFO per device)."""

    text: str = Field(..., min_length=1, max_length=8000)
    layout: str | None = Field(default=None, max_length=64)
    device_ids: list[str] = Field(default_factory=list)
    group_ids: list[str] = Field(default_factory=list)
    groups: dict[str, list[str]] | None = Field(
        default=None,
        description="Optional: merge named groups before resolving group_ids (values are UUID strings).",
    )
    expires_at: str | None = Field(default=None, max_length=80)

    @field_validator("device_ids")
    @classmethod
    def _strip_devices(cls, v: list[str]) -> list[str]:
        return [x.strip() for x in v if isinstance(x, str) and x.strip()]


def _extract_x_fun_key(request: Request) -> str | None:
    """Header-based API key from the client (firmware sends X-Fun-Key)."""
    return request.headers.get("x-fun-key")


def _device_headers(request: Request) -> tuple[str | None, str | None]:
    return (
        request.headers.get("x-device-id"),
        request.headers.get("x-device-name"),
    )


def _log_client_identity(request: Request) -> None:
    did, dname = _device_headers(request)
    if did or dname:
        log.debug("fun client device_id=%r device_name=%r path=%s", did, dname, request.url.path)


def _check_fun_key(x_fun_key: str | None) -> None:
    expected = os.getenv("FUN_API_KEY")
    if expected and x_fun_key != expected:
        raise HTTPException(status_code=401, detail="Invalid or missing X-Fun-Key")


def _check_admin_api_key(header: str | None) -> None:
    """Require FUN_ADMIN_API_KEY and matching X-Fun-Admin-Key for admin routes."""
    expected = os.getenv("FUN_ADMIN_API_KEY")
    if not expected or not str(expected).strip():
        raise HTTPException(
            status_code=503,
            detail="FUN_ADMIN_API_KEY is not configured; special message admin is disabled",
        )
    if header != expected:
        raise HTTPException(status_code=401, detail="Invalid or missing X-Fun-Admin-Key")


@asynccontextmanager
async def lifespan(app: FastAPI):
    global _bg_task, _fact_task
    _require_fun_api_key_if_configured()
    device_roster.init_store()
    special_messages.init_store()
    interval = refresh_interval_from_env()
    import httpx
    from refresh import _refresh_once

    async with httpx.AsyncClient(follow_redirects=True) as client:
        await _refresh_once(client)
    _bg_task = start_background_refresh(interval)
    _fact_task = start_fact_harvest_task(fact_interval_from_env())
    yield
    for t in (_fact_task, _bg_task):
        if t:
            t.cancel()
            try:
                await t
            except asyncio.CancelledError:
                pass


app = FastAPI(title="Fun aggregator", lifespan=lifespan)
app.state.limiter = limiter
app.add_exception_handler(RateLimitExceeded, _rate_limit_exceeded_handler)


@app.get("/healthz")
async def healthz():
    body: dict[str, Any] = {"status": "ok"}
    snap = fact_state_snapshot()
    if snap:
        body["facts_upstream"] = snap
    return body


@app.post("/v1/admin/special")
async def admin_enqueue_special(
    body: SpecialEnqueueBody,
    x_fun_admin_key: Annotated[str | None, Header(alias="x-fun-admin-key")] = None,
):
    _check_admin_api_key(x_fun_admin_key)
    groups_update: dict[str, Any] | None = None
    if body.groups:
        groups_update = {str(k): v for k, v in body.groups.items()}
    result = special_messages.enqueue(
        body.text,
        body.layout,
        body.device_ids,
        body.group_ids,
        optional_groups_update=groups_update,
        expires_at=body.expires_at,
    )
    if not result["ok"]:
        raise HTTPException(status_code=400, detail=result.get("error", "enqueue failed"))
    return JSONResponse(content=result)


@app.get("/v1/fun/special")
@limiter.limit(_rate_screen())
async def fun_special(request: Request):
    """Pop one queued special slide for X-Device-Id, or return 204 if none."""
    _check_fun_key(_extract_x_fun_key(request))
    did, dname = _device_headers(request)
    device_roster.note_seen(did, dname)
    _log_client_identity(request)
    if not did or not did.strip():
        raise HTTPException(status_code=400, detail="X-Device-Id header required")
    popped = special_messages.pop_next_slide(did)
    if popped is None:
        return Response(status_code=204)
    return FunSlide(
        layout=popped["layout"],
        text=popped["text"],
        display_hold_until_epoch=popped.get("display_hold_until_epoch"),
    ).model_dump(exclude_none=True)


@app.post("/v1/devices/register")
async def devices_register(
    registration: DeviceRegisterBody,
    x_fun_key: Annotated[str | None, Header(alias="x-fun-key")] = None,
):
    _check_fun_key(x_fun_key)
    device_id = device_roster.register_device(registration.friendly_name, registration.hardware_mac)
    return JSONResponse(content={"device_id": device_id})


@app.get("/v1/fun/screen")
@limiter.limit(_rate_screen())
async def fun_screen(
    request: Request,
    m: int = Query(..., ge=1, le=4),
):
    _check_fun_key(_extract_x_fun_key(request))
    did, dname = _device_headers(request)
    device_roster.note_seen(did, dname)
    _log_client_identity(request)
    if m == MODE_EARTHQUAKE:
        if state.earthquake:
            return state.earthquake.model_dump()
        raise HTTPException(status_code=503, detail="Earthquake data not ready yet")
    if m == MODE_ISS:
        if state.iss:
            return state.iss.model_dump()
        raise HTTPException(status_code=503, detail="ISS data not ready yet")

    # Modes 2 and 4: direct fetch from literal pools (legacy single-source slides)
    if m == MODE_CAT:
        bodies = sample_pool("cat_facts", 1, 1)
        if not bodies:
            raise HTTPException(status_code=503, detail="No cat facts in pool")
        return FunSlide(layout="default", text=format_cat_slide(bodies[0])).model_dump()
    if m == MODE_USELESS:
        bodies = sample_pool("useless_facts", 1, 1)
        if not bodies:
            raise HTTPException(status_code=503, detail="No useless facts in pool")
        return FunSlide(layout="default", text=format_useless_slide(bodies[0])).model_dump()

    raise HTTPException(status_code=400, detail="Invalid mode")


@app.get("/v1/fun/facts/batch")
@limiter.limit(_rate_batch())
async def fun_facts_batch(
    request: Request,
    count_cat: int = Query(0, ge=0, le=MAX_BATCH_PER_SLOT),
    count_useless: int = Query(0, ge=0, le=MAX_BATCH_PER_SLOT),
):
    _check_fun_key(_extract_x_fun_key(request))
    did, dname = _device_headers(request)
    device_roster.note_seen(did, dname)
    _log_client_identity(request)
    out: dict[str, list[dict]] = {"cat": [], "useless": []}
    for body in sample_pool("cat_facts", count_cat, MAX_BATCH_PER_SLOT):
        out["cat"].append(FunSlide(layout="default", text=format_cat_slide(body)).model_dump())
    for body in sample_pool("useless_facts", count_useless, MAX_BATCH_PER_SLOT):
        out["useless"].append(
            FunSlide(layout="default", text=format_useless_slide(body)).model_dump()
        )
    return JSONResponse(content=out)


@app.get("/v1/fun/facts/mixed")
@limiter.limit(_rate_batch())
async def fun_facts_mixed(
    request: Request,
    count: int = Query(1, ge=1, le=MAX_BATCH_PER_SLOT),
):
    _check_fun_key(_extract_x_fun_key(request))
    did, dname = _device_headers(request)
    device_roster.note_seen(did, dname)
    _log_client_identity(request)
    slides = sample_mixed_slides(count, MAX_BATCH_PER_SLOT)
    if not slides:
        raise HTTPException(status_code=503, detail="No fact pools available")
    return JSONResponse(content={"facts": slides})
