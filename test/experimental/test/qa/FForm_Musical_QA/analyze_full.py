#!/usr/bin/env python3
"""F-Form full musical QA: source-vs-neutral alignment and legacy-render metrics.

Operates on original unmodified PCM/float WAV, using the first two channels of 8ch
renders for music comparison, without rewriting source files. Does not certify PDC,
real-time behavior, perceptual quality or AudioSuite support.
"""
from __future__ import annotations
import argparse
import datetime as dt
import json
import math
import pathlib
import subprocess
import sys
import numpy as np
import soundfile as sf
from scipy.signal import correlate, correlation_lags

HERE=pathlib.Path(__file__).resolve().parent
DEFAULT=HERE/'local_audio'
FILES={
    'original':'00_ORIGINAL.wav',
    'neutral':'ITCH-01 — Time 1.00× Pitch 1.00×_Audio 2-7.1.wav',
    'up3':'PITCH-02 — Time 1.00× Pitch 1.19×_Audio 2-7.1.wav',
    'down3':'PITCH-03 — Time 1.00× Pitch 0.84×_Audio 2-7.1.wav',
    'up12':'PITCH-04 — Time 1.00× Pitch 2.00×_Audio 2-7.1.wav',
    'bypass_internal':'BYP-01 — Enabled ON OFF durante reproducción.wav',
    'bypass_host':'BYP-02 — Bypass nativo de Pro Tools.wav',
    'latency':'LAT-01 — Grabar el retorno de F-Form en otra pista.wav',
}

def sample(path,seconds,frames,channel=0):
    with sf.SoundFile(path) as f:
        f.seek(round(seconds*f.samplerate))
        y=f.read(frames,dtype='float64',always_2d=True)
        return y[:,channel],f.samplerate

def find_lag(reference,render,start=5.0,window=3.0,max_offset_ms=150.0):
    with sf.SoundFile(reference) as f: fs=f.samplerate
    n=round(window*fs)
    a,fsa=sample(reference,start,n);b,fsb=sample(render,start,n)
    if fsa != fsb or len(a)!=n or len(b)!=n: raise ValueError('Incomplete or mismatched source/render')
    a-=a.mean();b-=b.mean()
    corr=correlate(b,a,mode='full',method='fft')
    lags=correlation_lags(len(b),len(a),mode='full')
    ix=np.where(np.abs(lags)<=round(max_offset_ms*fs/1000))[0]
    best=int(lags[ix[np.argmax(corr[ix])]])
    return best,fs

def compare_one_window(source,render,start,seconds,lag):
    with sf.SoundFile(source) as f: fs=f.samplerate; channels=f.channels
    with sf.SoundFile(render) as f: fs2=f.samplerate; channels2=f.channels
    if fs!=fs2 or channels<2 or channels2<2: raise ValueError('Expected same fs and >=2 channels')
    sample_start=round(start*fs)
    n=round(seconds*fs)
    source_start=sample_start
    rendered_start=sample_start+lag
    if rendered_start<0 or source_start<0:return dict(start_seconds=start,error='negative read seek')
    with sf.SoundFile(source) as s, sf.SoundFile(render) as r:
        s.seek(source_start);r.seek(rendered_start)
        a=s.read(n,dtype='float64',always_2d=True)[:,:2]
        b=r.read(n,dtype='float64',always_2d=True)[:,:2]
    if len(a)!=n or len(b)!=n:return dict(start_seconds=start,error='window exceeds file')
    baseline_rms=float(np.sqrt(np.mean(a*a)))
    if baseline_rms<1e-8:return dict(start_seconds=start,error='near silence; null skipped')
    gain=float(np.sum(a*b)/np.sum(a*a))
    residual=float(np.sqrt(np.mean((b-gain*a)**2)))
    denom=float(np.sqrt(np.mean((gain*a)**2)))
    corr=float(np.corrcoef(a[:,0],b[:,0])[0,1])
    return dict(start_seconds=start,duration_seconds=seconds,lag_samples=lag,
                fitted_gain=gain,relative_null_db=round(20*math.log10(max(residual/denom,1e-15)),2),
                correlation_left=round(corr,12),method='identical clock, common sample lag, scalar gain fit')

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--data',type=pathlib.Path,default=DEFAULT)
    ap.add_argument('--out',type=pathlib.Path,default=HERE/'reports'/'musical_full.json')
    args=ap.parse_args()
    paths={k:args.data/('' if k=='original' else 'renders')/nm for k,nm in FILES.items()}
    missing=[str(p) for p in paths.values() if not p.is_file()]
    if missing:raise SystemExit('Missing: '+'; '.join(missing))
    args.out.parent.mkdir(parents=True,exist_ok=True)
    legacy=args.out.parent/'musical_legacy_refresh.json'
    cmd=[sys.executable,str(HERE/'analyze.py'),'--neutral',str(paths['neutral']),
         '--up3',str(paths['up3']),'--down3',str(paths['down3']),'--up12',str(paths['up12']),
         '--bypass-internal',str(paths['bypass_internal']),'--bypass-host',str(paths['bypass_host']),
         '--latency',str(paths['latency']),'--output',str(legacy)]
    subprocess.run(cmd,check=True)
    old=json.loads(legacy.read_text(encoding='utf-8'))
    with sf.SoundFile(paths['original']) as s, sf.SoundFile(paths['neutral']) as r:
        frames_source=len(s); frames_render=len(r); fs=s.samplerate
        source_metadata=dict(frames=frames_source,sample_rate=fs,channels=s.channels,subtype=s.subtype,
                             duration_seconds=frames_source/fs)
        neutral_metadata=dict(frames=frames_render,sample_rate=r.samplerate,channels=r.channels,
                              subtype=r.subtype,duration_seconds=frames_render/r.samplerate)
    lag,fs=find_lag(paths['original'],paths['neutral'])
    windows=[compare_one_window(paths['original'],paths['neutral'],s,1,lag) for s in (5,15,30,60,100,130)]
    local_windows=[compare_one_window(paths['original'],paths['neutral'],round(t/10,1),0.1,lag) for t in range(145,171,2)]
    out=dict(report_kind='FForm Musical QA full with original',
             created_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
             original=source_metadata,neutral=neutral_metadata,
             original_vs_neutral=dict(offset_samples=lag,offset_milliseconds=lag*1000/fs,
                                      length_difference_samples=frames_render-frames_source,
                                      windows=windows,local_14_5_to_17s=local_windows,limitations=[
              'Measured export-to-export offset: does not identify DSP latency or Pro Tools PDC cause',
              'Windows measured after lag alignment. No automated proof of audio quality or perceptual transparency',
              'Bypass recordings contain state changes; a single-window null does not certify host bypass']),
             previous_musical_report=old)
    args.out.write_text(json.dumps(out,indent=2,ensure_ascii=False),encoding='utf-8')
    md=args.out.with_suffix('.md')
    md.write_text('# F-Form Musical QA — original incorporado\n\n'
       +f'Original: {frames_source} frames; neutro: {frames_render} frames (diferencia {frames_render-frames_source}).\n\n'
       +f'Offset estimado entre exportaciones: **{lag} frames / {lag*1000/fs:.3f} ms** (NO atribuir todavía a PDC o al DSP).\n\n'
       +'| Inicio ventana (s) | Ganancia ajustada | Null relativo (dB) | Correlación L |\n|---:|---:|---:|---:|\n'
       +''.join(f"| {v['start_seconds']} | {v.get('fitted_gain','–')} | {v.get('relative_null_db','–')} | {v.get('correlation_left','–')} |\n" for v in windows)
       +'\n### Control localizado en torno a 16 s (ventanas 100 ms)\n\n| Inicio (s) | Null relativo (dB) |\n|---:|---:|\n'
       +''.join(f"| {v['start_seconds']} | {v.get('relative_null_db','–')} |\n" for v in local_windows)
       +'\n**Atención:** hay una divergencia localizada alrededor de 16–16,7 s pese a que el resto de ventanas neutras casi anulan. No atribuirla al DSP sin examinar sesión, edición, automatización y sincronía de exportación.\n'
       +'\nLos WAV de ocho canales se analizan mediante sus canales 1 y 2, sin alterarlos. '
       +'La estimación de pitch musical incluida en el JSON procede de `analyze.py`, con una resolución aproximada de 5 cents. '
       +'No hay en este paquete un render de time-stretch offline con cambio de duración. '
       +'El código SHIFT2 de Gemini no está instalado en F-Form y no es una prueba AAX.\n',encoding='utf-8')
    print('Full report:',args.out)
    print('Offset:',lag,'samples =',lag*1000/fs,'ms; length delta:',frames_render-frames_source)
    for v in windows:print('Window',v['start_seconds'],'s: null',v.get('relative_null_db'),'dB, gain',v.get('fitted_gain'))
    print('Musical relative pitch:',{k:v['estimated_pitch_shift_cents'] for k,v in old['relative_pitch'].items()})

if __name__=='__main__':main()
