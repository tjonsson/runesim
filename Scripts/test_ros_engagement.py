"""End-to-end ROS2 engagement check through rosbridge (run while the simulator is playing MainLevel).

Publishes std_msgs/String commands on /runesim/ptz/<id>/engage and reads the PTZ's
/runesim/ptz/<id>/state JSON. Virtual engagement only; restores designation/tracking at the end.

  Saved/LivingWorld/testenv/Scripts/python Scripts/test_ros_engagement.py ws://192.168.18.9:9090/runesim
"""
import asyncio
import json
import sys
import time
from pathlib import Path

import websockets

URL = sys.argv[1] if len(sys.argv) > 1 else 'ws://127.0.0.1:9090'
PREFIX = '/runesim/ptz/' + (sys.argv[2] if len(sys.argv) > 2 else 'ptz_1')
REPORT = Path(__file__).resolve().parents[1] / 'Saved/LivingWorld/ros-engagement.json'


async def main():
    states = []
    async with websockets.connect(URL, open_timeout=10, max_size=2 ** 20) as ws:
        await ws.send(json.dumps({'op': 'subscribe', 'topic': PREFIX + '/state', 'type': 'std_msgs/msg/String'}))
        await ws.send(json.dumps({'op': 'advertise', 'topic': PREFIX + '/engage', 'type': 'std_msgs/msg/String'}))

        async def reader():
            async for raw in ws:
                message = json.loads(raw)
                if message.get('topic') == PREFIX + '/state':
                    data = json.loads(message['msg']['data'])
                    data['received'] = time.monotonic()
                    states.append(data)

        task = asyncio.create_task(reader())

        async def send(command, wait):
            await ws.send(json.dumps({'op': 'publish', 'topic': PREFIX + '/engage', 'msg': {'data': command}}))
            await asyncio.sleep(wait)
            return states[-1].get('engagement', {}) if states else {}

        await asyncio.sleep(2)
        assert states, 'No PTZ state received; is the simulator connected to this rosbridge?'
        before = states[-1].get('engagement', {})
        steps = []
        steps.append(('designate', await send('designate', 1.5)))
        steps.append(('track_on', await send('track_on', 3)))
        steps.append(('fire', await send('fire', 1.0)))
        # Wait for the interceptor to resolve (or 25 s).
        deadline = time.monotonic() + 25
        while time.monotonic() < deadline and states and states[-1].get('engagement', {}).get('in_flight', 0) > 0:
            await asyncio.sleep(.5)
        steps.append(('resolved', states[-1].get('engagement', {})))
        steps.append(('clear', await send('clear', 1.0)))
        task.cancel()
    frames = [s['frame'] for s in states]
    after = steps[-2][1]
    result = {
        'url': URL, 'prefix': PREFIX, 'state_messages': len(states),
        'frames_monotonic': all(b >= a for a, b in zip(frames, frames[1:])),
        'before': before, 'steps': steps,
        'checks': {
            'engagement_state_published': 'engagement' in states[-1],
            'designation_via_ros': bool(steps[0][1].get('target')),
            'tracking_via_ros': steps[1][1].get('tracking') is True,
            'launch_via_ros': after.get('launches', 0) > before.get('launches', 0),
            'resolved': after.get('in_flight', 1) == 0 and (after.get('hits', 0) + after.get('misses', 0)) > (before.get('hits', 0) + before.get('misses', 0)),
            'clear_via_ros': steps[-1][1].get('tracking') is False and not steps[-1][1].get('target'),
        }}
    result['passed'] = all(result['checks'].values())
    REPORT.write_text(json.dumps(result, indent=2))
    print(json.dumps(result['checks']), 'status:', after.get('status'))
    return 0 if result['passed'] else 1


sys.exit(asyncio.run(main()))
