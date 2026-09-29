"""Prepare licensed ambient recordings and deterministic mechanical sound designs.

Generated machinery loops are scenery sound design, not recordings of specific
airframes or calibrated acoustic signatures. Sources remain untouched.
"""
from pathlib import Path
import json
import numpy as np
import soundfile as sf
root=Path(__file__).resolve().parents[1];out=root/'Art/LivingWorld/Prepared/Audio';out.mkdir(parents=True,exist_ok=True)
report=[]
def save(name,data,rate,source,peak=.25):
    data=np.asarray(data,dtype=np.float64);data-=data.mean()
    fade=min(int(.12*rate),len(data)//8)
    t=np.linspace(0,1,fade)
    blend=data[-fade:]*(1-t)+data[:fade]*t
    loop=np.concatenate((data[fade:-fade],blend))
    loop*=peak/max(np.max(np.abs(loop)),1e-8)
    sf.write(out/(name+'.wav'),loop,rate,subtype='PCM_16')
    report.append({'name':name,'source':source,'seconds':len(loop)/rate,'peak_dbfs':float(20*np.log10(np.max(np.abs(loop)))),
                   'rms_dbfs':float(20*np.log10(np.sqrt(np.mean(loop**2)))),'loop_edge_delta':float(abs(loop[-1]-loop[0]))})
data,rate=sf.read(root/'Art/LivingWorld/Sourced/AmbientAudio/helicopter.mp3',always_2d=True)
save('helicopter_loop',data[int(5*rate):int(13*rate)].mean(axis=1),rate,'aquinn / Helicopter Sounds / CC0')
data,rate=sf.read(root/'Art/LivingWorld/Sourced/AmbientAudio/birds.ogg',always_2d=True)
save('woodland_birds',data.mean(axis=1),rate,'isaiah658 / Ambient Bird Sounds / CC0',.12)
rate=48000;t=np.arange(rate*8)/rate;rng=np.random.default_rng(4242)
noise=rng.normal(size=len(t));spectrum=np.fft.rfft(noise);freq=np.fft.rfftfreq(len(t),1/rate)
soft=np.fft.irfft(spectrum/(1+(freq/1500)**2),n=len(t));soft/=soft.std()
for name,fundamental in [('drone_loop',180),('utility_engine_loop',45),('jet_loop',95)]:
    harmonics=sum(np.sin(2*np.pi*fundamental*k*t+.1*np.sin(2*np.pi*.25*t))/k**1.4 for k in range(1,9))
    texture=soft*(.1 if name!='jet_loop' else .6)
    save(name,(harmonics+texture)*(.85+.15*np.sin(2*np.pi*.5*t)),rate,'Original RuneSim procedural sound design')
(out/'ambient-audio.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
