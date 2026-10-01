"""Engine frame rate over time from an Unreal log's [timestamp][frame%1000] prefixes.

  python Scripts/log_frame_rate.py [log] [--interval 10] [--wait-seconds 150]
Defaults to the packaged simulator log. With --wait-seconds, waits until the log spans that long.
"""
import argparse
import datetime
import re
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('log', nargs='?', default=str(ROOT / 'Saved/LivingWorld/Packaged/Windows/RuneSim/Saved/Logs/RuneSim.log'))
parser.add_argument('--interval', type=float, default=10.)
parser.add_argument('--wait-seconds', type=float, default=0.)
args = parser.parse_args()
PATTERN = re.compile(r'\[(\d{4})\.(\d\d)\.(\d\d)-(\d\d)\.(\d\d)\.(\d\d):(\d{3})\]\[\s*(\d+)\]')


def samples():
    points, wraps, previous = [], 0, None
    for line in open(args.log, encoding='utf-8', errors='ignore'):
        m = PATTERN.match(line)
        if not m:
            continue
        stamp = datetime.datetime(*map(int, m.groups()[:6])) + datetime.timedelta(milliseconds=int(m.group(7)))
        frame = int(m.group(8))
        if previous is not None and frame < previous:
            wraps += 1000
        previous = frame
        points.append((stamp, wraps + frame))
    return points


deadline = time.monotonic() + args.wait_seconds + 600
while args.wait_seconds and time.monotonic() < deadline:
    points = samples()
    if points and (points[-1][0] - points[0][0]).total_seconds() >= args.wait_seconds:
        break
    time.sleep(5)
points = samples()
last = None
rates = []
for stamp, frame in points:
    if last is None or (stamp - last[0]).total_seconds() >= args.interval:
        if last:
            rate = (frame - last[1]) / (stamp - last[0]).total_seconds()
            rates.append(rate)
            print(stamp.time().isoformat(timespec='seconds'), f'{rate:5.1f} fps')
        last = (stamp, frame)
if rates:
    tail = sorted(rates[len(rates) // 3:])
    print(f'after warm-up: median {tail[len(tail) // 2]:.1f} fps, min {tail[0]:.1f} fps')
