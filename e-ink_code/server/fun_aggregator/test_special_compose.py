"""Unit tests for special-message slide text composition."""

from special_messages import compose_slide_text


def test_header_and_body():
    assert compose_slide_text(header="Hi", body="There") == "Hi\nThere"


def test_header_only():
    assert compose_slide_text(header="Hi") == "Hi"


def test_body_only():
    assert compose_slide_text(body="There") == "There"


def test_text_fallback():
    assert compose_slide_text(text="One line") == "One line"
    assert compose_slide_text(text="Red\nBlack") == "Red\nBlack"


def test_header_body_overrides_text():
    assert compose_slide_text(text="ignored", header="Hi", body="There") == "Hi\nThere"


def test_empty_returns_none():
    assert compose_slide_text() is None
    assert compose_slide_text(text="  ") is None
