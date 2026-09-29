"""Set the packaged test demo to the verified 69-agent airborne showcase.

Run while the packaged application is stopped. Preserves graphics preferences.
"""
from pathlib import Path
import re
import os,csv,subprocess
if os.name=='nt':
    running=subprocess.run(['tasklist','/FO','CSV','/NH','/FI','IMAGENAME eq RuneSim.exe'],capture_output=True,text=True,check=True)
    if any(row and row[0].lower()=='runesim.exe' for row in csv.reader(running.stdout.splitlines())):
        raise SystemExit('Close RuneSim and wait for it to exit before changing saved preferences.')
root=Path(__file__).resolve().parents[1]
path=root/'Saved/LivingWorld/Packaged/Windows/RuneSim/Saved/Config/Windows/GameUserSettings.ini'
text=path.read_text() if path.exists() else ''
section='[/Script/RuneSim.LivingWorldPreferences]'
options='Options=(bEnabled=True,Preset=Custom,Population=Mixed,CrowdDensity=1,TrafficDensity=1,Planes=4,Helicopters=4,Drones=10,BirdFlocks=4,FlockSize=10,bReactive=True,ActivityRadiusMeters=500.000000,MaxActors=120,Seed=4242,AmbientVolumeDb=-12.000000)'
if section in text:
    text=re.sub(re.escape(section)+r'[^\[]*',section+'\n'+options+'\n\n',text,count=1)
else:text+='\n'+section+'\n'+options+'\n'
path.parent.mkdir(parents=True,exist_ok=True)
path.write_text(text)
print('Configured the packaged showcase: 58 air + 8 people + 3 vehicles.')
