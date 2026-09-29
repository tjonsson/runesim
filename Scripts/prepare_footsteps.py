"""Convert a small CC0 concrete-footstep set to mono PCM for Unreal."""
from pathlib import Path
import soundfile as sf
root=Path(__file__).resolve().parents[1]
out=root/'Art/LivingWorld/Prepared/Audio'
out.mkdir(parents=True,exist_ok=True)
for source in sorted((root/'Art/LivingWorld/Sourced/KenneyImpact/Source/Audio').glob('footstep_concrete_*.ogg')):
    data,rate=sf.read(source,always_2d=True)
    sf.write(out/(source.stem+'.wav'),data.mean(axis=1),rate,subtype='PCM_16')
    print(source.name,round(len(data)/rate,3),'seconds')
