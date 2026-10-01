"""Loopback rosbridge harness for the PTZ engage topic (websockets==14.2); not a ROS2 substitute.

Point the playing PTZ at ws://127.0.0.1:9095 (see Scripts/connect_ptz_loopback.py), then run this.
It answers the PTZ's /engage subscription with designate -> track_on -> fire and records the
engagement fields of the published state. Stops after 45 seconds.
"""
import asyncio
import json
from pathlib import Path
from websockets.asyncio.server import serve

report = {'connections': 0, 'engage_subscribed': False, 'sent': [], 'states': []}


async def handle(socket):
    report['connections'] += 1

    async def drive(topic):
        await asyncio.sleep(2)
        for command, wait in (('designate', 2), ('track_on', 3), ('fire', 12), ('clear', 1)):
            await socket.send(json.dumps({'op': 'publish', 'topic': topic, 'msg': {'data': command}}))
            report['sent'].append(command)
            await asyncio.sleep(wait)

    async for raw in socket:
        msg = json.loads(raw)
        if msg.get('op') == 'subscribe' and msg.get('topic', '').endswith('/engage'):
            report['engage_subscribed'] = msg.get('type') == 'std_msgs/msg/String'
            asyncio.create_task(drive(msg['topic']))
        if msg.get('op') == 'publish':
            report['states'].append(json.loads(msg['msg']['data']).get('engagement', {}))


async def main():
    async with serve(handle, '127.0.0.1', 9095):
        print('ENGAGE_LOOPBACK_LISTENING', flush=True)
        await asyncio.sleep(45)
    states = report['states']
    report['checks'] = {
        'subscribed_to_engage': report['engage_subscribed'],
        'designated': any(s.get('target') for s in states),
        'tracking': any(s.get('tracking') for s in states),
        'launched': len({s.get('launches') for s in states if 'launches' in s}) > 1,
        'cleared': bool(states) and not states[-1].get('target') and not states[-1].get('tracking'),
    }
    report['passed'] = all(report['checks'].values())
    report['states'] = states[::10]
    path = Path(__file__).resolve().parents[1] / 'Saved/LivingWorld/ros-engage-loopback.json'
    path.write_text(json.dumps(report, indent=2))
    print('ENGAGE_LOOPBACK', json.dumps(report['checks']))


asyncio.run(main())
