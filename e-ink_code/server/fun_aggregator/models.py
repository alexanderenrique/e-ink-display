from __future__ import annotations

from pydantic import BaseModel, Field


class FunSlide(BaseModel):
    layout: str = Field(default="default")
    text: str
    #: Optional UTC epoch cap from message expires_at; firmware holds for two refresh cycles (special only).
    display_hold_until_epoch: int | None = Field(default=None, ge=0)
