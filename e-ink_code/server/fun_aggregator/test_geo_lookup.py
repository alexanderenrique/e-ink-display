"""Unit tests for ISS geo / ocean labels."""

from geo_lookup import describe_nearest_place


def test_mid_pacific_ocean():
    assert describe_nearest_place(-15.0, -160.0) == "Roughly over: the southern Pacific"


def test_mid_atlantic_ocean():
    assert describe_nearest_place(25.0, -40.0) == "Roughly over: the northern Atlantic"


def test_near_city_still_wins():
    assert describe_nearest_place(37.77, -122.42).startswith("Roughly near:")


def test_remote_land_fallback():
    # Central Sahara — far from cities, not in ocean boxes
    assert describe_nearest_place(23.0, 10.0) == "middle of nowhere"
