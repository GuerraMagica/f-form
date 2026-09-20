import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fform_qa import make_sync, make_tone, generate, evaluate, write_wave
import numpy as np

def test_identity_sync_and_channels(tmp_path):
    src=tmp_path/'src.wav'; dest=tmp_path/'dest.wav'
    x=make_sync(2,[0,12], duration=6)
    write_wave(src,x);write_wave(dest,x)
    result=evaluate(src,dest,'sync',1)
    assert result['pass_all'], result['criteria']
    assert result['metrics']['interchannel_offset_spread_samples']==0

def test_delay_detected(tmp_path):
    src=tmp_path/'src.wav'; dest=tmp_path/'dest.wav'
    x=make_sync(1,[0],duration=6)
    delay=480
    y=np.concatenate((np.zeros((delay,1),dtype=np.float32), x),axis=0)
    write_wave(src,x);write_wave(dest,y)
    r=evaluate(src,dest,'sync',1)
    assert not r['pass_all']
    assert not r['criteria']['offset_ch0']['pass']
    assert abs(r['metrics']['channel_0']['offset_samples']-delay)<=1

def test_drift_detected(tmp_path):
    src=tmp_path/'src.wav'; dest=tmp_path/'dest.wav'
    x=make_sync(1,[0],duration=8)
    y=np.zeros((len(x)+640,1),dtype=np.float32)
    for k in range(len(x)):
        target=round(k*(len(y)-1)/(len(x)-1))
        y[target,0]=x[k,0]
    write_wave(src,x);write_wave(dest,y)
    r=evaluate(src,dest,'sync',1,drift_tolerance_ms=1)
    assert not r['pass_all']
    assert not r['criteria']['drift_ch0']['pass']

def test_tone_octave_and_silence(tmp_path):
    src=tmp_path/'src.wav'; dest=tmp_path/'dest.wav'
    x=make_tone(duration=4)
    write_wave(src,x);write_wave(dest,x)
    r=evaluate(src,dest,'tone',1)
    assert r['pass_all'],r['criteria']
    bad=evaluate(src,dest,'tone',1,pitch_semitones=12)
    assert not bad['criteria']['pitch']['pass']
    s=tmp_path/'silence.wav'
    write_wave(s,np.zeros((48000,2),dtype=np.float32))
    assert evaluate(s,s,'silence',1)['pass_all']

def test_generated_manifest(tmp_path):
    result=generate(tmp_path,4)
    assert len(result)==7
    assert (tmp_path/'manifest.json').is_file()
