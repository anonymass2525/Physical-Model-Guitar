#!/usr/bin/env python3
"""Conservative physically calibrated guitar capture qualification and two-path fit.

Does not fabricate measured data; synthetic tests are explicitly labelled.
A user-declared capture is not independently authenticated by this tool.
Absolute string/body source isolation, directivity, and universal transfer
are NOT established by a conditional least-squares two-reference fit.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
from typing import Any
import numpy as np
import soundfile as sf
from JointRadiation17 import fit as conditional_fit
from MeasurementStation18 import ORDER

REQUIRED_UNITS = ("m/s", "m/s", "Pa", "Pa")

def valid_sensor(channel: dict[str, Any], idx: int) -> None:
    if int(channel.get("channel", -1)) != idx + 1:
        raise ValueError(f"channel {idx+1}: physical sensor order required")
    if channel.get("unit") != REQUIRED_UNITS[idx]:
        raise ValueError(f"channel {idx+1}: expected calibrated {REQUIRED_UNITS[idx]}")
    if not str(channel.get("serial", "")).strip() or not str(channel.get("calibration_method", "")).strip():
        raise ValueError(f"channel {idx+1}: sensor serial and calibration method required")
    g = channel.get("units_per_full_scale")
    if g is None or not np.isfinite(float(g)) or float(g) <= 0:
        raise ValueError(f"channel {idx+1}: positive measured units_per_full_scale required")
    delay = channel.get("hardware_delay_samples")
    if delay is None or not np.isfinite(float(delay)) or abs(float(delay)) > 256:
        raise ValueError(f"channel {idx+1}: measured, finite hardware_delay_samples required")


def inspect(manifest: dict, root: Path) -> dict:
    if manifest.get("schema") != "PMG18_same_guitar_synchronous_four_channel_v1":
        raise ValueError("wrong rig schema")
    if manifest.get("capture_clock") != "one_hardware_adc_clock":
        raise ValueError("no proof all channels used a single capture clock")
    if manifest.get("is_physical_capture") is not True:
        raise ValueError("physical capture not user-declared; synthetic data cannot become a real preset")
    if manifest.get("channel_order") != ORDER:
        raise ValueError("channel order must be string velocity / bridge velocity / two microphones")
    if not all(str(manifest.get(k, "")).strip() for k in ("guitar_id", "mic_geometry_id", "capture_interface")):
        raise ValueError("same-guitar, microphone geometry and interface identifiers required")
    fs = int(manifest.get("sample_rate", 0))
    if not 22050 <= fs <= 192000:
        raise ValueError("unsupported sample rate")
    sensors = manifest.get("sensor_channels", [])
    if len(sensors) != 4:
        raise ValueError("exactly four documented sensor channels required")
    for i, ch in enumerate(sensors): valid_sensor(ch, i)
    if len(manifest.get("takes", [])) < 8:
        raise ValueError("8 or more independent synchronous takes needed (at least 6 train, 2 holdout)")
    files, hashes = set(), set()
    train, holdout = [], []
    exc = set()
    note_ids = set()
    per_take = []
    for t in manifest["takes"]:
        if any(t.get(k) != manifest[k] for k in ("guitar_id", "mic_geometry_id", "capture_interface")):
            raise ValueError("changed guitar / microphone geometry / interface; cannot fit together")
        if t.get("simultaneous_capture") is not True:
            raise ValueError("all four channels must be recorded simultaneously")
        partition = t.get("partition")
        if partition not in ("train", "holdout"):
            raise ValueError("each take must be explicitly train or holdout")
        if t.get("excitation") not in ("pluck", "angled_pluck", "bridge_drive"):
            raise ValueError("excitation provenance required for identifiability audit")
        if not 1 <= int(t.get("string", 0)) <= 6 or not 0 <= int(t.get("fret", -1)) <= 24:
            raise ValueError("record physical string and fret for each take")
        name = str(t.get("file", ""))
        if Path(name).name != name or name in files:
            raise ValueError("duplicate, empty or unsafe take path")
        files.add(name)
        path = root / name
        if not path.exists():
            raise FileNotFoundError(path)
        if path.suffix.lower() != ".wav":
            raise ValueError("lossless WAV required")
        raw = path.read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        if t.get("sha256") != digest or digest in hashes:
            raise ValueError(f"missing/incorrect SHA256 or repeated take: {name}")
        hashes.add(digest)
        snd = sf.info(path)
        if snd.samplerate != fs or snd.channels != 4 or snd.frames < fs:
            raise ValueError(f"{name}: needs four 1s+ channels at common rate")
        if snd.subtype not in ("FLOAT", "DOUBLE", "PCM_24", "PCM_32"):
            raise ValueError(f"{name}: use floating PCM or 24+bit PCM, not lossy or 16bit")
        signal, _ = sf.read(path, always_2d=True, dtype="float32")
        if not np.isfinite(signal).all():
            raise ValueError(f"{name}: nonfinite data")
        peak = np.max(np.abs(signal), axis=0)
        rms = np.sqrt(np.mean(np.asarray(signal, dtype="float64") ** 2, axis=0))
        if np.max(peak) >= .98:
            raise ValueError(f"{name}: clipping/insufficient headroom: {peak.tolist()}")
        if np.min(rms) < 1e-5:
            raise ValueError(f"{name}: empty/below-noise channel: {rms.tolist()}")
        if np.any(rms > .8):
            raise ValueError(f"{name}: implausibly loud/clipped sensor chain")
        # A full 20 ms near-zero dropout of ANY required channel invalidates its phase.
        max_gap = max(12, int(.020 * fs))
        for ch in range(4):
            zero = np.abs(signal[:, ch]) < 1.e-9
            if len(zero) >= max_gap and np.any(
                np.cumsum(zero, dtype=np.int64)[max_gap-1:] -
                np.r_[0, np.cumsum(zero, dtype=np.int64)[:-max_gap]] >= max_gap):
                raise ValueError(f"{name}: channel {ch+1} has 20 ms dropout/zero gap")
        if partition == "train":
            train.append(t)
            exc.add(t["excitation"])
            note_ids.add((t["string"], t["fret"]))
        else:
            holdout.append(t)
        per_take.append({"file": name, "partition": partition, "peaks_fs": peak.round(5).tolist(),
                         "rms_fs": rms.round(6).tolist(), "sha256": digest})
    if len(train) < 6 or len(holdout) < 2:
        raise ValueError("6 train and 2 holdout physical takes required")
    if len(exc) < 2 or len(note_ids) < 2:
        raise ValueError("train captures need at least two excitation types and two string/fret targets")
    return {"samples":len(files), "train":train, "holdout":holdout,
            "per_take":per_take,"excitation_classes":sorted(exc),"guitar_id":manifest["guitar_id"],
            "mic_geometry_id":manifest["mic_geometry_id"]}


def calibrate_complex_path(path: dict, mic: int, source: int, sensors: list, fs: int) -> dict:
    f = np.asarray(path["frequency_hz"], dtype=float)
    h = np.asarray(path["transfer_real"], dtype=float) + 1j*np.asarray(path["transfer_imag"], dtype=float)
    g = float(sensors[2+mic]["units_per_full_scale"]) / float(sensors[source]["units_per_full_scale"])
    # Observed signal path includes relative ADC/front-end delay, which must
    # have been measured using an independent synchronized calibration pulse.
    delta_samples = float(sensors[2+mic]["hardware_delay_samples"]) - float(sensors[source]["hardware_delay_samples"])
    h = h * g * np.exp(1j*2*np.pi*f*delta_samples/fs)
    return {"mic": mic, "source": ["string", "bridge"][source], "frequency_hz":f.tolist(),
            "pressure_per_velocity_real":np.real(h).round(9).tolist(),
            "pressure_per_velocity_imag":np.imag(h).round(9).tolist(),
            "phase_radians":np.angle(h).round(6).tolist(),
            "front_end_relative_delay_removed_samples":round(delta_samples,6),
            "unit":"Pa/(m/s)", "peak_arrival_ms_before_frontend_correction":path["peak_arrival_ms"],
            "late_amplitude_tau_s":path["late_amplitude_tau_s"],
            "estimated_noncausal_ratio":path["noncausal_ir_energy_ratio"]}


def qualify(manifest_path: Path, nperseg=4096, min_improvement=0.05) -> dict:
    m = json.loads(manifest_path.read_text(encoding="utf8"))
    root = manifest_path.parent
    checked = inspect(m,root)
    train=checked["train"]
    hold=checked["holdout"]
    evaluations=[]
    fit_outputs=[]
    for h in hold:
        # Fit EXCLUSIVELY on training recordings; each holdout is genuinely unseen.
        fmanifest = {"sample_rate":m["sample_rate"],"channel_order":ORDER,
            "guitar_id":m["guitar_id"],"mic_geometry_id":m["mic_geometry_id"],
            "takes":train+[h]}
        result=conditional_fit(fmanifest,root,nperseg=nperseg)
        e2=result["heldout_two_source_normalized_stft_error"]
        e1=result["heldout_body_only_normalized_stft_error"]
        gain=float((e1-e2)/max(e1,1e-12))
        evaluations.append({"file":h["file"],"body_only_error":e1,"two_reference_error":e2,
                            "improvement_fraction":round(gain,6),
                            "identifiable":bool(result["identified_conditional_transfer"])})
        fit_outputs.append(result)
    passed=all(x["identifiable"] and x["improvement_fraction"]>=min_improvement for x in evaluations)
    # The actual coefficients/phase are NOT exported for installation when holdout fails.
    physical_paths=[]
    if passed:
        # Audited single training-only fit, not fit to any holdout note.
        src=fit_outputs[0]["path_estimations"]
        for row in src:
            physical_paths.append(calibrate_complex_path(row,row["mic"],
                0 if row["source"]=="string" else 1,m["sensor_channels"],m["sample_rate"]))
    return {
        "schema":"PMG18_conditional_physical_4channel_fit_v1",
        "guitar_id":m["guitar_id"],"mic_geometry_id":m["mic_geometry_id"],
        "recording_count":checked["samples"],"training_count":len(train),"holdout_count":len(hold),
        "sensor_calibration":m["sensor_channels"],"capture_quality":checked["per_take"],
        "source_excitation_classes":checked["excitation_classes"],
        "heldout":evaluations,"all_holdouts_passed":bool(passed),
        "candidate_replaces_provisional_coefficients":False,
        "candidate_reason":("Conditional measured coefficient candidate requires human technical and blind-listening review"
                           if passed else "REJECTED: unchanged provisional coefficients; improve independent capture and holdout"),
        "calibrated_complex_transfer":physical_paths,
        "source_separation_proven":False,
        "captured_by_this_analysis":False,
        "physical_provenance_is_user_declared_not_independently_verified":True,
        "warning":"The linear transfer functions are conditional on simultaneously measured string and bridge references, not isolated free-field source contributions. Microphone orientation, acoustic room, and analog sensor phase calibration are part of the result. Do not import automatically into plugin DSP."
    }


def main():
    p=argparse.ArgumentParser()
    p.add_argument("--manifest",required=True)
    p.add_argument("--out",required=True)
    p.add_argument("--nperseg",type=int,default=4096)
    p.add_argument("--min-improvement",type=float,default=0.05)
    a=p.parse_args()
    report=qualify(Path(a.manifest).resolve(),a.nperseg,a.min_improvement)
    Path(a.out).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf8")
    print(json.dumps({k:v for k,v in report.items() if k not in ("calibrated_complex_transfer","capture_quality","sensor_calibration")},ensure_ascii=False,indent=2))

if __name__=="__main__": main()
