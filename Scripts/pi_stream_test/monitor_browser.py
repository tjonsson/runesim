"""Read-only Chromium video diagnostics; requires websocket-client in .test_venv."""
import base64
import json
from pathlib import Path
import time
import urllib.request
import websocket

root = Path(__file__).resolve().parent
output = root/'logs'
output.mkdir(exist_ok=True)
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
previous = None
request_id = 0
last_screenshot_time = 0
last_screenshot_session = None

def call(socket, method, params=None):
    global request_id
    request_id += 1
    socket.send(json.dumps({'id': request_id, 'method': method, 'params': params or {}}))
    while True:
        response = json.loads(socket.recv())
        if response.get('id') == request_id:
            if 'error' in response: raise RuntimeError(response['error'])
            return response.get('result', {})

expression = """JSON.stringify((()=>{
 if(window.pixelStreaming && !window.runesimStatsListener){
   window.runesimStatsListener=true;
   window.pixelStreaming.addEventListener('statsReceived', e=>{
     window.runesimInbound={...e.data.aggregatedStats.inboundVideoStats, sampled_at:Date.now()/1000};
   });
 }
 const v=document.querySelector('video');
 const q=v?.getVideoPlaybackQuality?.();
 return {title:document.title,url:location.href,width:v?.videoWidth||0,height:v?.videoHeight||0,
 current_time:v?.currentTime||0,paused:v?.paused??true,
 decoded_frames:window.runesimInbound?.framesDecoded??v?.webkitDecodedFrameCount??0,
 total_frames:q?.totalVideoFrames||0,
 presented_frames:Math.max(0,(q?.totalVideoFrames||0)-(q?.droppedVideoFrames||0)),
 dropped_frames:q?.droppedVideoFrames||0,
 inbound_video:window.runesimInbound||null,
 page_text:document.body.innerText.slice(0,300)};
})())"""

while True:
    try:
        tabs = json.load(opener.open('http://127.0.0.1:9222/json/list', timeout=3))
        tab = next(t for t in tabs if t.get('type') == 'page' and 'StreamerId=ptz-1' in t.get('url',''))
        connection = websocket.create_connection(tab['webSocketDebuggerUrl'], timeout=5, suppress_origin=True)
        try:
            result = call(connection, 'Runtime.evaluate', {'expression': expression, 'returnByValue': True})
            data = json.loads(result['result']['value'])
            now = time.time()
            data['time_unix'] = now
            data['page_id'] = tab['id']
            same_session = bool(previous and data['page_id'] == previous['page_id']
                                and data['presented_frames'] >= previous['presented_frames']
                                and data['current_time'] >= previous['current_time'])
            data['frames_advanced'] = bool(same_session and data['presented_frames'] > previous['presented_frames']
                                          and data['current_time'] > previous['current_time'])
            data['observed_fps'] = (round((data['presented_frames']-previous['presented_frames']) /
                                        (now-previous['time_unix']), 2) if same_session else None)
            if data['frames_advanced'] and (data['page_id'] != last_screenshot_session or now-last_screenshot_time > 60):
                capture = call(connection, 'Page.captureScreenshot', {'format':'png'})
                (output/'stream-proof.png').write_bytes(base64.b64decode(capture['data']))
                last_screenshot_time = now
                last_screenshot_session = data['page_id']
            previous = data
        finally:
            connection.close()
    except Exception as error:
        data = {'time_unix':time.time(), 'frames_advanced':False, 'error':str(error)}
        previous = None
    temporary = output/'browser-status.tmp'
    temporary.write_text(json.dumps(data,indent=2))
    temporary.replace(output/'browser-status.json')
    print(json.dumps(data),flush=True)
    time.sleep(5)
