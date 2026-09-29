"""Loopback rosbridge protocol harness (websockets==14.2); not a ROS2 integration substitute.

Enable ROS on the demo PTZ during PIE while this harness runs. Stops after 30 seconds.
"""
import asyncio
import json
from pathlib import Path
from websockets.asyncio.server import serve

report={'connections':0,'subscribed':False,'advertised':False,'states':[]}
async def handle(socket):
    report['connections']+=1
    async for raw in socket:
        msg=json.loads(raw)
        if msg.get('op')=='subscribe':
            report['subscribed']=msg.get('type')=='geometry_msgs/msg/Vector3'
            await socket.send(json.dumps({'op':'publish','topic':msg['topic'],'msg':{'x':999,'y':-999,'z':1}}))
        if msg.get('op')=='advertise': report['advertised']=msg.get('type')=='std_msgs/msg/String'
        if msg.get('op')=='publish': report['states'].append(json.loads(msg['msg']['data']))
async def main():
    async with serve(handle,'127.0.0.1',9090):
        print('PTZ_TEST_LISTENING',flush=True)
        await asyncio.sleep(30)
    report['passed']=report['subscribed'] and report['advertised'] and bool(report['states']) and any(
        abs(s['pan_deg']-170)<1 and abs(s['tilt_deg']+80)<1 and abs(s['horizontal_fov_deg']-5)<1 for s in report['states'])
    path=Path(__file__).resolve().parents[1]/'Saved/LivingWorld/rosbridge-report.json'
    path.write_text(json.dumps(report,indent=2))
    print('PTZ_PROTOCOL_PASS',report['passed'])
asyncio.run(main())
