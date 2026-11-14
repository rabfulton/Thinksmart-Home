"""Small shared WAV reader for offline wake-word checks."""
import math, wave
import numpy as np
from scipy import signal

def read_pcm(path):
    with wave.open(str(path)) as w:
        if w.getnchannels()!=1 or w.getsampwidth()!=2:raise ValueError('PCM16 mono required')
        rate=w.getframerate();pcm=w.readframes(w.getnframes())
    if rate==16000:return pcm
    x=np.frombuffer(pcm,dtype='<i2').astype(np.float32);g=math.gcd(rate,16000)
    return np.clip(signal.resample_poly(x,16000//g,rate//g),-32768,32767).astype('<i2').tobytes()
