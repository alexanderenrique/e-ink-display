from __future__ import annotations

from pydantic import BaseModel, Field


class FunSlide(BaseModel):
    layout: str = Field(default="default")
    text: str
    #: UTC epoch seconds until which the firmware should prefer this slide over normal rotation (special endpoint only).
    display_hold_until_epoch: int | None = Field(default=None, ge=0)
