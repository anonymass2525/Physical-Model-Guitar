#!/usr/bin/env python3
"""Physical Model Guitar: synchronous 4-input measurement recorder.

The recorded samples are REAL only when an actual audio interface is attached.
This program never fabricates signals or marks calibration as verified.
All four channels must come from ONE clocked audio interface. An extra capture
session or mixed clocks is not allowed for phase-coherent 2-source fitting.
"""
from __future__ import annotations
import argparse
import datetime as dt
import hashlib
import json
from pathlib import Path
import numpy as np
import soundfile as sf

ORDER = ["string_velocity_reference", "bridge_velocity_reference", "mic_left_pressure", "mic_right_pressure"]

def default_manifest(guitar: str, geometry: str, sample_rate: int, interface: str, input_map=(1,2,3,4)):
    return {
        "schema": "PMG18_same_guitar_synchronous_four_channel_v1",
        "guitar_id": guitar,
        "mic_geometry_id": geometry,
        "capture_interface": interface,
        "sample_rate": sample_rate,
        "channel_order": ORDER,
        "adc_input_map_one_based": list(input_map),
        "is_physical_capture": False,
        "capture_clock": "one_hardware_adc_clock",
        "sensor_channels": [
            {"channel": 1, "serial": "", "quantity": "string_velocity", "unit": "m/s",
             "units_per_full_scale": None, "calibration_method": "", "hardware_delay_samples": None},
            {"channel": 2, "serial": "", "quantity": "bridge_velocity", "unit": "m/s",
             "units_per_full_scale": None, "calibration_method": "", "hardware_delay_samples": None},
            {"channel": 3, "serial": "", "quantity": "mic_left_pressure", "unit": "Pa",
             "units_per_full_scale": None, "calibration_method": "", "hardware_delay_samples": None},
            {"channel": 4, "serial": "", "quantity": "mic_right_pressure", "unit": "Pa",
             "units_per_full_scale": None, "calibration_method": "", "hardware_delay_samples": None},
        ],
        "notes": "Fill sensor serials, sensitivity, and independently measured front-end delay before fitting. Bridge accelerometer must be converted to calibrated velocity in the analog chain or preprocessed in advance.",
        "takes": [],
    }

def atomic_json(path: Path, value):
    temp = path.with_suffix(".json.tmp")
    temp.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temp.replace(path)

def record(manifest_path: Path, device, seconds: float, string_num: int, fret: int,
           excitation: str, velocity: str, take_tag: str):
    # Delay import: analysis and synthetic tests do NOT require hardware package.
    try:
        import sounddevice as sd
    except ImportError as exc:
        raise RuntimeError("Install PortAudio binding: python -m pip install sounddevice") from exc
    m = json.loads(manifest_path.read_text(encoding="utf-8"))
    fs = int(m["sample_rate"])
    if m.get("channel_order") != ORDER:
        raise ValueError("4-channel sensor order mismatch; fix wiring/manifest first")
    if not (0 < seconds <= 60):
        raise ValueError("take length must be 0..60 seconds")
    if not 1 <= string_num <= 6 or not 0 <= fret <= 24:
        raise ValueError("string/fret outside range")
    take_id = f"take_{len(m['takes'])+1:03d}_{take_tag}"
    if any(x['file'] == f"{take_id}.wav" for x in m['takes']):
        raise ValueError("duplicate capture filename")
    info = sd.query_devices(device)
    if info["max_input_channels"] < 4:
        raise RuntimeError(f"Need FOUR inputs on ONE device; selected device has {info['max_input_channels']}")
    print(f"Capturing {seconds:g}s at {fs}Hz/4ch from {info['name']}. Verify sensor gains and placement.")
    samples = sd.rec(int(round(seconds * fs)), samplerate=fs, channels=4,
                     dtype="float32", device=device, blocking=False)
    status = sd.wait()
    if status:
        raise RuntimeError(f"PortAudio reported recording overflow/error: {status}; discard this take")
    if not np.isfinite(samples).all():
        raise RuntimeError("Non-finite capture; discard")
    mapping=m.get("adc_input_map_one_based", [1,2,3,4])
    if len(mapping)!=4 or sorted(mapping)!=[1,2,3,4]:
        raise ValueError("ADC input map must be a permutation of physical channels 1..4")
    samples=samples[:,[int(k)-1 for k in mapping]]
    peaks = np.max(np.abs(samples), axis=0)
    if np.any(peaks >= .98):
        raise RuntimeError(f"Clipping/insufficient headroom: {peaks.tolist()}; discard")
    root = manifest_path.parent
    out = root / f"{take_id}.wav"
    if out.exists():
        raise RuntimeError(f"Will not overwrite {out}")
    sf.write(out, samples, fs, subtype="FLOAT")
    digest = hashlib.sha256(out.read_bytes()).hexdigest()
    m["takes"].append({
        "file": out.name, "sha256": digest, "guitar_id": m["guitar_id"],
        "mic_geometry_id": m["mic_geometry_id"],
        "capture_interface": m["capture_interface"],
        "simultaneous_capture": True,
        "recorded_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "string": string_num, "fret": fret, "excitation": excitation,
        "velocity_class": velocity, "partition": "train",
        "peak_full_scale_by_channel": peaks.tolist(),
    })
    atomic_json(manifest_path, m)
    print(f"Recorded {out.name} SHA256={digest} (calibration still unverified)")

def main():
    p = argparse.ArgumentParser(description="Four-channel same-guitar capture; never simulates physical data")
    s = p.add_subparsers(dest="operation", required=True)
    d = s.add_parser("devices", help="List capture devices and supported input counts")
    i = s.add_parser("init")
    i.add_argument("--manifest", required=True)
    i.add_argument("--guitar-id", required=True)
    i.add_argument("--geometry-id", required=True)
    i.add_argument("--interface", required=True)
    i.add_argument("--sample-rate", type=int, default=48000)
    i.add_argument("--input-map", default="1,2,3,4",
                   help="ADC physical inputs to logical string, bridge, left mic, right mic; e.g. 3,4,1,2")
    r = s.add_parser("record")
    r.add_argument("--manifest", required=True)
    r.add_argument("--device", type=int, default=None)
    r.add_argument("--seconds", type=float, default=4.0)
    r.add_argument("--string", type=int, required=True)
    r.add_argument("--fret", type=int, required=True)
    r.add_argument("--excitation", choices=["pluck", "angled_pluck", "bridge_drive"], required=True)
    r.add_argument("--velocity", choices=["soft", "medium", "hard"], default="medium")
    r.add_argument("--tag", default="guitar")
    a = p.parse_args()
    if a.operation == "devices":
        try:
            import sounddevice as sd
        except ImportError as exc:
            raise RuntimeError("python -m pip install sounddevice") from exc
        print(sd.query_devices())
    elif a.operation == "init":
        path = Path(a.manifest).resolve()
        if path.exists():
            raise RuntimeError("Manifest already exists; refusing to overwrite")
        path.parent.mkdir(parents=True, exist_ok=True)
        if not 22050 <= a.sample_rate <= 192000:
            raise ValueError("unsupported sample rate")
        mapping=[int(x.strip()) for x in a.input_map.split(",")]
        if sorted(mapping)!=[1,2,3,4]:
            raise ValueError("--input-map must be a permutation of 1,2,3,4")
        atomic_json(path, default_manifest(a.guitar_id, a.geometry_id, a.sample_rate, a.interface, mapping))
        print(f"Created {path}. Fill sensor metadata; do not set is_physical_capture until physically verified.")
    else:
        record(Path(a.manifest).resolve(), a.device, a.seconds, a.string, a.fret,
               a.excitation, a.velocity, a.tag)

if __name__ == "__main__":
    main()
