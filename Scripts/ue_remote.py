"""Run a Python file in the local RuneSim editor using Epic's remote API."""
import argparse
import json
import os
import sys
import time
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("script", type=Path)
args = parser.parse_args()
engine = Path(os.environ.get("UE_ROOT", "C:/Program Files/Epic Games/UE_5.8"))
sys.path.insert(0, str(engine / "Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python"))
import remote_execution

remote = remote_execution.RemoteExecution()
remote.start()
try:
    deadline = time.monotonic() + 8
    nodes = []
    while time.monotonic() < deadline:
        nodes = [n for n in remote.remote_nodes if n.get("project_name") == "RuneSim"]
        if nodes:
            break
        time.sleep(0.2)
    if len(nodes) != 1:
        raise RuntimeError(f"Expected one RuneSim editor; found {remote.remote_nodes}")
    remote.open_command_connection(nodes[0]["node_id"])
    result = remote.run_command(args.script.read_text(encoding="utf-8"))
    result.pop('command', None)
    print(json.dumps(result, indent=2))
    sys.exit(0 if result.get("success") else 1)
finally:
    remote.stop()
