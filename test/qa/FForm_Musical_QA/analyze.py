#!/usr/bin/env python3
"""Preliminary F-Form musical QA: file integrity, stereo layout and relative pitch.
Requires numpy, scipy, soundfile. Does not claim perceptual quality or latency/PDC.
"""
import argparse, json, os
import numpy as np
import soundfile as sf
from scipy.signal import welch
from scipy.ndimage import gaussian_filter1d

def profile(path):
    with sf.SoundFile(path) as f:
        fs, channels, frames = f.samplerate, f.channels, len(f)
        peak = np.zeros(channels); sums = np.zeros(channels)
        clip = np.zeros(channels, dtype=np.int64)
        first = np.full(channels, -1, dtype=np.int64)
        last = np.full(channels, -1, dtype=np.int64)
        finite = True; n = 0
        for a in f.blocks(blocksize=131072, dtype='float32', always_2d=True):
            z = np.abs(a)
            peak = np.maximum(peak, z.max(axis=0))
            sums += (a.astype('float64')**2).sum(axis=0)
            clip += (z >= 1.0).sum(axis=0)
            finite &= bool(np.isfinite(a).all())
            for c in range(channels):
                ix = np.flatnonzero(z[:, c] > 1e-6)
                if len(ix):
                    if first[c] < 0: first[c] = n + int(ix[0])
                    last[c] = n + int(ix[-1])
            n += len(a)
        return dict(name=os.path.basename(path), sample_rate=fs, channels=channels,
                    frames=frames, duration_seconds=frames/fs, format=f.subtype,
                    peak=peak.tolist(), rms=np.sqrt(sums/frames).tolist(),
                    clipped_samples=clip.tolist(), finite_output=finite,
                    first_nonzero_sample=first.tolist(), last_nonzero_sample=last.tolist(),
                    active_channels=[i+1 for i,v in enumerate(peak) if v > 1e-6])

def crop(path, start, seconds):
    with sf.SoundFile(path) as f:
        f.seek(int(start*f.samplerate))
        a=f.read(int(seconds*f.samplerate), dtype='float32', always_2d=True)
        return np.mean(a[:,:min(2,f.channels)],axis=1), f.samplerate

def relative_pitch(reference, modified):
    xr, fs=crop(reference, 12, 18); xm, fsm=crop(modified,12,18)
    if fs != fsm: raise ValueError('Different sample rates: pitch comparison requires same FS')
    def logspec(x):
        fr, power=welch(x,fs=fs,nperseg=32768,noverlap=16384,scaling='spectrum')
        m=(fr>80)&(fr<12000); q=np.linspace(np.log(100),np.log(8000),1200)
        v=np.interp(q,np.log(fr[m]),np.log(np.maximum(power[m],1e-14)))
        return q, v-gaussian_filter1d(v,20)
    q, r=logspec(xr);_,v=logspec(xm)
    cents=np.arange(-1250,1251,5)
    scores=[]
    for c in cents:
        vshift=np.interp(q+c*np.log(2)/1200,q,v,left=np.nan,right=np.nan)
        mask=np.isfinite(vshift)&(q>np.log(180))&(q<np.log(6000))
        a=r[mask];b=vshift[mask]
        a=a-a.mean(); b=b-b.mean()
        scores.append(float(np.dot(a,b)/(np.linalg.norm(a)*np.linalg.norm(b)+1e-15)))
    ix=int(np.argmax(scores))
    return dict(estimated_pitch_shift_cents=int(cents[ix]),spectral_profile_correlation=round(scores[ix],3),
                resolution_cents=5, segment='12-30 s', limitation='Music-spectrum coarse relative estimate, not a precision pitch-meter')

def null_window(reference, comparison, start=5, duration=1):
    a,fs=crop(reference,start,duration);b,fsb=crop(comparison,start,duration)
    if fs!=fsb or len(a)!=len(b):return {'error':'sampling rate / duration mismatch'}
    d=np.sqrt(np.mean((a.astype('float64')-b.astype('float64'))**2))
    r=np.sqrt(np.mean(a.astype('float64')**2))
    return {'window_seconds':[start,start+duration], 'relative_null_db':round(float(20*np.log10(d/(r+1e-30)+1e-30)),1),
            'aligned_by_sample_index':True,'does_not_establish_entire_file_null':True}

parser=argparse.ArgumentParser()
parser.add_argument('--neutral',required=True)
parser.add_argument('--up3');parser.add_argument('--down3');parser.add_argument('--up12')
parser.add_argument('--bypass-internal');parser.add_argument('--bypass-host');parser.add_argument('--latency')
parser.add_argument('--output',default='report.json')
args=parser.parse_args()
names={'neutral':args.neutral,'up3':args.up3,'down3':args.down3,'up12':args.up12,
       'bypass_internal':args.bypass_internal,'bypass_host':args.bypass_host,'latency':args.latency}
out={'methodology':'F-Form QA musical preliminary. Neutral render as reference; no unprocessed original.', 'files':{},'relative_pitch':{},'null_at_5s':{}}
for k,path in names.items():
    if path:out['files'][k]=profile(path)
for k in ('up3','down3','up12'):
    if names[k]:out['relative_pitch'][k]=relative_pitch(args.neutral,names[k])
for k in ('bypass_internal','bypass_host','latency'):
    if names[k]:out['null_at_5s'][k]=null_window(args.neutral,names[k])
with open(args.output,'w',encoding='utf-8') as f:json.dump(out,f,indent=2,ensure_ascii=False)
print('Saved',args.output)
for k,p in out['files'].items():print(k,p['duration_seconds'],p['channels'],p['active_channels'],'peak',round(max(p['peak']),4))
print('pitch estimates:',out['relative_pitch'])
