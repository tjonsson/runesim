"""Packaged MainLevel population scale test: engine frame rate at increasing agent caps.

Stops the supervised simulator between runs (normal close), rewrites only the Living World options
line in the packaged GameUserSettings.ini, relaunches, measures ~2.5 minutes, and finally restores
the original options. Writes Saved/LivingWorld/population-scale.json.
"""
import json
import re
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SETTINGS = ROOT / 'Saved/LivingWorld/Packaged/Windows/RuneSim/Saved/Config/Windows/GameUserSettings.ini'
LOG = ROOT / 'Saved/LivingWorld/Packaged/Windows/RuneSim/Saved/Logs/RuneSim.log'
CONFIGS = {
    120: 'CrowdDensity=3,TrafficDensity=3,Planes=3,Helicopters=3,Drones=17,BirdFlocks=3,FlockSize=12,MaxActors=120',
    200: 'CrowdDensity=3,TrafficDensity=3,Planes=8,Helicopters=8,Drones=20,BirdFlocks=6,FlockSize=16,MaxActors=200',
    300: 'CrowdDensity=3,TrafficDensity=3,Planes=12,Helicopters=12,Drones=24,BirdFlocks=8,FlockSize=20,MaxActors=300',
}


def ps(command):
    return subprocess.run(['powershell', '-NoProfile', '-Command', command], capture_output=True, text=True).stdout


def stop():
    ps("Get-Process RuneSim -ErrorAction SilentlyContinue | ForEach-Object { $_.CloseMainWindow() | Out-Null }")
    for _ in range(90):
        running = ps('Get-Process RuneSim -ErrorAction SilentlyContinue | Select-Object -ExpandProperty ProcessName')
        supervisor = ps("Get-CimInstance Win32_Process | Where-Object { $_.CommandLine -like '*watch_living_demo*' -and $_.ProcessId -ne $PID -and $_.Name -ne 'python.exe' } | Select-Object -ExpandProperty ProcessId")
        if 'RuneSim' not in running and not supervisor.strip():
            return
        time.sleep(1)
    raise RuntimeError('Simulator or supervisor did not exit')


def first_stamp():
    import datetime
    try:
        with open(LOG, encoding='utf-8', errors='ignore') as handle:
            for line in handle:
                m = re.match(r'\[(\d{4})\.(\d\d)\.(\d\d)-(\d\d)\.(\d\d)\.(\d\d)', line)
                if m:
                    return datetime.datetime(*map(int, m.groups()), tzinfo=datetime.timezone.utc).timestamp()
    except OSError:
        pass
    return 0


def set_options(fields):
    text = SETTINGS.read_text()
    line = re.search(r'^Options=\(.*\)$', text, re.M).group(0)
    new = line
    for pair in fields.split(','):
        key, value = pair.split('=')
        new = re.sub(key + r'=[^,)]*', pair, new)
    SETTINGS.write_text(text.replace(line, new))
    return line


def measure():
    out = subprocess.run(['python', str(ROOT / 'Scripts/log_frame_rate.py'), '--wait-seconds', '150', '--interval', '15'],
                         capture_output=True, text=True).stdout.strip().splitlines()
    rates = [float(l.split()[1]) for l in out if l.endswith('fps') and not l.startswith('after')]
    tail = sorted(rates[len(rates) // 3:])
    return {'median_fps': tail[len(tail) // 2] if tail else None, 'min_fps': tail[0] if tail else None, 'windows': len(rates)}


stop()
original = None
results = {}
try:
    for cap, fields in CONFIGS.items():
        previous = set_options(fields)
        original = original or previous
        launched = time.time()
        ps("& '" + str(ROOT / 'Scripts/run_living_demo.ps1') + "'")
        # Measure only the new session's log.
        for _ in range(120):
            if first_stamp() >= launched - 5:
                break
            time.sleep(2)
        results[cap] = measure()
        print(cap, results[cap], flush=True)
        stop()
finally:
    if original:
        text = SETTINGS.read_text()
        SETTINGS.write_text(re.sub(r'^Options=\(.*\)$', original.replace('\\', '\\\\'), text, flags=re.M))
    ps("& '" + str(ROOT / 'Scripts/run_living_demo.ps1') + "'")
(ROOT / 'Saved/LivingWorld/population-scale.json').write_text(json.dumps(results, indent=2))
print('SCALE', json.dumps(results))
