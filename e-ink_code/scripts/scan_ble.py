from bleak import BleakScanner
import asyncio

def _bytes_repr(data):
    """Format dict of key -> bytes for display."""
    if not data:
        return "(none)"
    return {k: v.hex() if isinstance(v, bytes) else v for k, v in data.items()}

def format_adv(device, adv):
    """Format full advertisement data for a device."""
    lines = [
        f"  Address: {device.address}",
        f"  Local name: {adv.local_name or '(none)'}",
        f"  Service UUIDs: {list(adv.service_uuids) if adv.service_uuids else '(none)'}",
        f"  Service data: {_bytes_repr(adv.service_data) if adv.service_data else '(none)'}",
        f"  Manufacturer data: {_bytes_repr(adv.manufacturer_data) if adv.manufacturer_data else '(none)'}",
        f"  TX power: {adv.tx_power}",
    ]
    rssi = getattr(device, "rssi", None)
    if rssi is not None:
        lines.append(f"  RSSI: {rssi} dBm")
    return "\n".join(lines)

async def scan():
    print("Scanning for BLE devices (10s)...")
    print("Using detection_callback to capture full advertisement data.\n")

    seen = {}
    def on_detection(device, advertisement_data):
        key = device.address
        if key not in seen:
            seen[key] = (device, advertisement_data)

    scanner = BleakScanner(detection_callback=on_detection)
    await scanner.start()
    await asyncio.sleep(10.0)
    await scanner.stop()

    print(f"Found {len(seen)} devices:\n")
    for addr, (d, adv) in seen.items():
        name = adv.local_name or d.name or "Unknown"
        rssi = getattr(d, "rssi", None)
        rssi_str = f" RSSI: {rssi}" if rssi is not None else ""
        print(f"{name}: {d.address}{rssi_str}")
        is_esp = "E-Ink" in (name or "") or (d.address and d.address.lower() == "58:8c:81:a4:f4:6a")
        if is_esp:
            print("  *** THIS IS YOUR ESP32! ***")
        print(format_adv(d, adv))
        print()

asyncio.run(scan())