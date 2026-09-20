#!/usr/bin/env python3
"""F-Form QA: generates synthetic WAVs and audits external renders.
This tool does NOT execute, load, or emulate F-Form.
"""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path
from fractions import Fraction
import numpy as np
import soundfile as sf
from scipy.ndimage import uniform_filter1d
from scipy.signal import find_peaks

SR = 48000
DURATION = 12.0

def write_wave(path: Path, data: np.ndarray, sr: int = SR) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    sf.write(str(path), np.asarray(data, dtype=np.float32), sr, subtype="FLOAT")

def markers(duration: float = DURATION) -> np.ndarray:
    times = []
    for n in range(100):
        t = .55 + .82*n + .043*math.sin(n*1.73)
        if t >= duration-.4:
            break
        times.append(t)
    return np.asarray(times)

def make_sync(channels: int, offsets_ms: list[float], sr: int = SR, duration: float = DURATION, signs=None) -> np.ndarray:
    out = np.zeros((round(duration*sr), channels), dtype=np.float32)
    t = markers(duration)
    sigma = .0007*sr
    window = int(math.ceil(5*sigma))
    signs = signs or [1]*channels
    for ch, shift_ms in enumerate(offsets_ms):
        for tp in t:
            center = round((tp+shift_ms*.001)*sr)
            lo, hi = max(0,center-window), min(len(out), center+window+1)
            ind = np.arange(lo, hi)
            out[lo:hi,ch] += signs[ch]*.75*np.exp(-.5*((ind-center)/sigma)**2)
    return out

def make_tone(sr: int=SR, duration: float=DURATION, freq: float=440.) -> np.ndarray:
    n = np.arange(round(sr*duration))
    fade = np.minimum(1., np.minimum(n, len(n)-1-n)/(sr*.1)).clip(0,1)
    return (.55*np.sin(2*np.pi*freq*n/sr)*fade).astype(np.float32)[:,None]

def generate(outdir: Path, duration: float=DURATION, sr: int=SR) -> list[dict]:
    specs = [
        ("sync_mono", "sync",make_sync(1,[0], sr, duration)),
        ("sync_stereo_identical", "sync",make_sync(2,[0,0], sr, duration)),
        ("sync_stereo_antiphase", "sync",make_sync(2,[0,0], sr, duration, [1,-1])),
        ("sync_stereo_offset", "sync",make_sync(2,[0,12], sr, duration)),
        ("sync_5_1_offset", "sync",make_sync(6,[0,10,20,30,40,50], sr, duration)),
        ("tone_440_mono", "tone",make_tone(sr, duration)),
        ("silence_stereo", "silence",np.zeros((round(duration*sr),2), dtype=np.float32)),
    ]
    manifest=[]
    for name, kind, data in specs:
        target=outdir/(name+".wav")
        write_wave(target,data,sr)
        manifest.append(dict(name=name,kind=kind,file=str(target),frames=len(data),channels=data.shape[1],sample_rate=sr))
    (outdir/"manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    return manifest

def read_wav(file: Path):
    samples, sample_rate = sf.read(str(file),dtype="float64",always_2d=True)
    return samples, sample_rate

def find_markers(samples: np.ndarray, sample_rate: int) -> np.ndarray:
    energy = uniform_filter1d(samples*samples, size=max(3,round(sample_rate*.003)), mode="constant")
    peak = float(np.max(energy)) if len(energy) else 0
    if peak < 1e-8:
        return np.array([], dtype=int)
    candidates, _ = find_peaks(energy, height=peak*.12, prominence=peak*.10,
                              distance=round(sample_rate*.35))
    return candidates

def estimate_freq(sig: np.ndarray, sr: int) -> float | None:
    a=int(.2*sr); b=len(sig)-a
    if b-a < sr//2: return None
    sig = sig[a:b]
    if not np.any(np.abs(sig)>1e-8): return None
    w = np.hanning(len(sig))
    spectrum = abs(np.fft.rfft(sig*w))
    freqs = np.fft.rfftfreq(len(sig), 1/sr)
    valid = np.where((freqs>=70)&(freqs<=10000))[0]
    if len(valid)==0: return None
    k=int(valid[np.argmax(spectrum[valid])])
    # sub-bin parabolic interpolation in log amplitude
    offset=0.
    if 0<k<len(spectrum)-1:
        p=np.log(np.maximum(spectrum[k-1:k+2],1e-30))
        den=p[0]-2*p[1]+p[2]
        if abs(den)>1e-12: offset=float(.5*(p[0]-p[2])/den)
    return (k+offset)*sr/len(sig)

def evaluate(source: Path, render: Path, kind: str, ratio: float,
             pitch_semitones: float=0, output_tolerance_samples: int=1,
             offset_tolerance_ms: float=2., drift_tolerance_ms: float=1.,
             jitter_tolerance_ms: float=2., interchannel_tolerance_ms: float=1.,
             pitch_tolerance_cents: float=5.) -> dict:
    if not math.isfinite(ratio) or ratio<=0: raise ValueError("ratio must be finite and positive")
    x,srx=read_wav(source); y,sry=read_wav(render)
    if srx != sry: raise ValueError(f"Sample rate mismatch: {srx} vs {sry}")
    if x.shape[1] != y.shape[1]: raise ValueError(f"Channels mismatch: {x.shape[1]} vs {y.shape[1]}")
    expected=round(len(x)*ratio)
    result=dict(source=str(source), render=str(render),kind=kind,ratio=ratio,
       pitch_semitones=pitch_semitones,sample_rate=srx,channels=x.shape[1],
       input_frames=len(x),output_frames=len(y),expected_output_frames=expected,
       frame_error=len(y)-expected,criteria={}, metrics={})
    criteria=result["criteria"]
    def check(name: str, passed: bool, details=None):
        criteria[name]={"pass":bool(passed),"details":details}
    check("exact_duration", abs(len(y)-expected)<=output_tolerance_samples,
          f"delta {len(y)-expected} samples; allowed ±{output_tolerance_samples}")
    check("finite_output",bool(np.all(np.isfinite(y))))
    clipped=int(np.count_nonzero(np.abs(y)>=1.0))
    result["metrics"]["abs_peak"]=float(np.max(np.abs(y))) if y.size else 0.
    result["metrics"]["samples_abs_ge_1"]=clipped
    # Do not automatically call > 0 dBFS invalid for floating-point audio;
    # only flag it for examination.
    if kind=="silence":
        peak=float(np.max(np.abs(y))) if y.size else 0.
        check("silence", peak<1e-7, f"abs peak {peak:.3g}")
    elif kind=="tone":
        fx=estimate_freq(x[:,0],srx); fy=estimate_freq(y[:,0],sry)
        if fx is None or fy is None: check("pitch",False,"No measurable tone")
        else:
            target=fx*2**(pitch_semitones/12)
            cents=1200*math.log2(fy/target)
            result["metrics"].update(source_frequency_hz=fx,render_frequency_hz=fy,
                      expected_frequency_hz=target,pitch_error_cents=cents)
            check("pitch",abs(cents)<=pitch_tolerance_cents,
                  f"{cents:+.3f} cents; allowed ±{pitch_tolerance_cents}")
    elif kind=="sync":
        chan_offsets=[]; chan_drifts=[]; chan_jitters=[]
        for ch in range(x.shape[1]):
            ins=find_markers(x[:,ch],srx)
            outs=find_markers(y[:,ch],sry)
            met={"input_count":len(ins),"output_count":len(outs),
                 "input_positions":ins.tolist(),"output_positions":outs.tolist()}
            result["metrics"][f"channel_{ch}"]=met
            matched=(len(ins)==len(outs) and len(ins)>2)
            check(f"markers_ch{ch}",matched,f"input {len(ins)}, output {len(outs)}")
            if not matched: continue
            # Compare to rounded ideal map so whole-sample alignment is visible.
            ideal=np.rint(ins*ratio)
            delta=outs-ideal
            offset=float(np.median(delta))
            drift=float(delta[-1]-delta[0])
            residual=delta-offset
            jitter=float(np.max(abs(residual)))
            met.update(offset_samples=offset,offset_ms=offset*1000/srx,
                       drift_samples=drift,drift_ms=drift*1000/srx,
                       maximum_anchor_residual_samples=jitter,
                       maximum_anchor_residual_ms=jitter*1000/srx,
                       anchor_deltas_samples=delta.tolist())
            chan_offsets.append(offset);chan_drifts.append(drift);chan_jitters.append(jitter)
            check(f"offset_ch{ch}",abs(offset)*1000/srx<=offset_tolerance_ms,
                  f"{offset*1000/srx:+.3f} ms; allowed ±{offset_tolerance_ms}")
            check(f"drift_ch{ch}",abs(drift)*1000/srx<=drift_tolerance_ms,
                  f"{drift*1000/srx:+.3f} ms; allowed ±{drift_tolerance_ms}")
            check(f"jitter_ch{ch}",jitter*1000/srx<=jitter_tolerance_ms,
                  f"{jitter*1000/srx:.3f} ms; allowed {jitter_tolerance_ms}")
        if len(chan_offsets)==x.shape[1] and len(chan_offsets)>1:
            difference=float(max(chan_offsets)-min(chan_offsets))
            result["metrics"]["interchannel_offset_spread_samples"]=difference
            check("interchannel_offset_spread",difference*1000/srx<=interchannel_tolerance_ms,
                f"{difference*1000/srx:.3f} ms; allowed {interchannel_tolerance_ms}")
    else: raise ValueError(f"Unknown kind {kind}")
    result["pass_all"]=all(item["pass"] for item in criteria.values())
    return result

def write_report(result:dict, out:Path) -> None:
    lines=["# F-Form QA report", "",f"**Result:** {'PASS' if result['pass_all'] else 'FAIL'}", "",
           f"Source: `{result['source']}`", f"Render: `{result['render']}`", "",
           f"Ratio (output/input): `{result['ratio']}`",f"Pitch shift: `{result['pitch_semitones']}` semitones", "",
           f"Input frames: `{result['input_frames']}`; output frames: `{result['output_frames']}`; expected: `{result['expected_output_frames']}`", "",
           "| Check | Result | Detail |", "|---|---|---|"]
    for name,info in result["criteria"].items():
        details=str(info.get("details") or "").replace("|","/")
        lines.append(f"| {name} | {'PASS' if info['pass'] else 'FAIL'} | {details} |")
    lines.extend(["","## Measurements","","```json",json.dumps(result["metrics"],indent=2,ensure_ascii=False),"```", "",
       "This report checks technical invariants, not perceptual equivalence or professional audio quality."])
    out.write_text("\n".join(lines)+"\n",encoding="utf-8")

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    subs=ap.add_subparsers(dest="command",required=True)
    g=subs.add_parser("generate",help="Generate unprocessed reference WAVs")
    g.add_argument("--out",type=Path,default=Path("stimuli"))
    g.add_argument("--duration",type=float,default=DURATION)
    a=subs.add_parser("analyze",help="Analyze a WAV rendered externally by F-Form")
    a.add_argument("--source",type=Path,required=True)
    a.add_argument("--render",type=Path,required=True)
    a.add_argument("--kind",choices=("sync","tone","silence"),required=True)
    a.add_argument("--ratio",default="1",help="Output duration / input duration: e.g. 24/25")
    a.add_argument("--pitch-semitones",type=float,default=0.)
    a.add_argument("--output-tolerance-samples",type=int,default=1)
    a.add_argument("--offset-tolerance-ms",type=float,default=2.)
    a.add_argument("--drift-tolerance-ms",type=float,default=1.)
    a.add_argument("--jitter-tolerance-ms",type=float,default=2.)
    a.add_argument("--interchannel-tolerance-ms",type=float,default=1.)
    a.add_argument("--pitch-tolerance-cents",type=float,default=5.)
    a.add_argument("--json",type=Path,default=Path("qa_result.json"))
    a.add_argument("--report",type=Path,default=Path("qa_report.md"))
    args=ap.parse_args()
    if args.command=="generate":
        if args.duration < 4: ap.error("duration should be at least 4 seconds")
        manifest=generate(args.out,args.duration)
        for item in manifest: print(item["file"])
        return
    ratio=float(Fraction(args.ratio))
    try:
        result=evaluate(args.source,args.render,args.kind,ratio,args.pitch_semitones,
            args.output_tolerance_samples,args.offset_tolerance_ms,args.drift_tolerance_ms,
            args.jitter_tolerance_ms,args.interchannel_tolerance_ms,args.pitch_tolerance_cents)
    except (ValueError,RuntimeError,OSError) as exc:
        ap.error(str(exc))
    args.json.parent.mkdir(parents=True,exist_ok=True)
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.json.write_text(json.dumps(result,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    write_report(result,args.report)
    print(f"{'PASS' if result['pass_all'] else 'FAIL'}: {args.report} | {args.json}")
    for name, info in result["criteria"].items():
        print(f"  {'PASS' if info['pass'] else 'FAIL'} {name}: {info['details'] or ''}")
    raise SystemExit(0 if result["pass_all"] else 1)

if __name__=="__main__": main()
