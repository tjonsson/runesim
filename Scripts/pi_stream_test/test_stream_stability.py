"""Measure sustained playback from the existing read-only browser monitor.

Run on the Pi while the viewer and monitor are running. Does not reconnect or
change stream settings. Exit 1 means playback stalled, restarted, or metrics expired.
"""
import argparse
import json
from pathlib import Path
import statistics
import time
import re


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--seconds', type=int, default=600)
    parser.add_argument('--warmup', type=int, default=60)
    parser.add_argument('--report', default='stability-report', help='Report basename inside logs (preserves other runs)')
    parser.add_argument('--minimum-median-fps', type=float, default=0,
                        help='Optional presented-frame-rate acceptance floor')
    args = parser.parse_args()
    if args.seconds < 10 or args.warmup < 0:
        parser.error('seconds must be at least 10 and warmup must be nonnegative')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,79}', args.report):
        parser.error('report must be a simple basename, up to 80 letters/digits/dashes/underscores')
    if not 0 <= args.minimum_median_fps <= 240:
        parser.error('minimum-median-fps must be finite and between 0 and 240')
    root = Path(__file__).resolve().parent / 'logs'
    root.mkdir(exist_ok=True)
    samples = []

    def sample():
        now = time.time()
        try:
            data = json.loads((root / 'browser-status.json').read_text())
            rtc = data.get('inbound_video') or {}
            return dict(time_unix=now, age=now-data['time_unix'],
                        page_id=data.get('page_id'), advancing=data.get('frames_advanced', False),
                        fps=data.get('observed_fps'), presented=data.get('presented_frames'),
                        dropped=data.get('dropped_frames'), received=rtc.get('framesReceived'),
                        decoded=rtc.get('framesDecoded'), bytes_received=rtc.get('bytesReceived'),
                        freezes=rtc.get('freezeCount'), freeze_seconds=rtc.get('totalFreezesDuration'),
                        error=data.get('error'))
        except (OSError, ValueError, KeyError) as exc:
            return dict(time_unix=now, error=str(exc), advancing=False, age=999)

    deadline = time.monotonic() + args.warmup
    while True:
        first = sample()
        if first['advancing'] and 0 <= first['age'] < 10:
            break
        if time.monotonic() >= deadline:
            break
        time.sleep(2)
    start = time.monotonic()
    while True:
        data = sample()
        data['elapsed'] = round(time.monotonic()-start, 2)
        samples.append(data)
        bad = [s for s in samples if not s['advancing'] or not 0 <= s['age'] < 10]
        restarts = sum(a.get('page_id') != b.get('page_id') for a,b in zip(samples,samples[1:]))
        resets = sum(b.get('presented') is not None and a.get('presented') is not None
                     and b['presented'] < a['presented'] for a,b in zip(samples,samples[1:]))
        rates = [s['fps'] for s in samples if s.get('fps') is not None]
        result = dict(completed=data['elapsed'] >= args.seconds, duration_seconds=data['elapsed'],
                      stalled_or_stale_samples=len(bad), browser_restarts=restarts,
                      counter_resets=resets, minimum_fps=min(rates,default=0),
                      median_fps=statistics.median(rates) if rates else 0,
                      samples=samples)
        freeze_delta = ((samples[-1].get('freezes') or 0) - (samples[0].get('freezes') or 0))
        result['new_webrtc_freezes'] = freeze_delta if not (restarts or resets) else None
        result['minimum_median_fps_required'] = args.minimum_median_fps
        result['frame_rate_passed'] = bool(rates) and result['median_fps'] >= args.minimum_median_fps
        result['presentation_frames_dropped'] = ((samples[-1].get('dropped') or 0) -
                                                 (samples[0].get('dropped') or 0)) if not (restarts or resets) else None
        result['passed'] = result['completed'] and result['frame_rate_passed'] and not (bad or restarts or resets or freeze_delta)
        temporary = root / (args.report + '.tmp')
        temporary.write_text(json.dumps(result, indent=2))
        temporary.replace(root / (args.report + '.json'))
        print(json.dumps({k:v for k,v in result.items() if k!='samples'}), flush=True)
        if result['completed']:
            return 0 if result['passed'] else 1
        time.sleep(min(5, max(0.1, args.seconds-data['elapsed'])))


if __name__ == '__main__':
    raise SystemExit(main())
