#!/usr/bin/env python3
"""Explicit destructive-to-test-packages Lua smoke test. Never flashes or erases Flash."""
import argparse
import json
import sys
import tempfile
import time
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT / "platform/tools"))
from companion import Companion, DeviceError, text
from compile_ignite import compile_project


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port",required=True)
    parser.add_argument("--report",type=Path,required=True)
    args = parser.parse_args()
    client = Companion(args.port)
    report = {}
    try:
        report["device"] = client.hello()
        client.launch("system.clock")
        report["baseline"] = client.diagnostics()
        for name in ("game","loop","memory","forbidden","parse","init-loop"):
            try: client.request("remove", text("test.lua-"+name))
            except DeviceError as error:
                if error.code != 7: raise
        project = json.loads((ROOT / "platform/examples/flappy-bird/project.json").read_text())
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def install(ident, source, version=1):
                project["id"],project["version"] = ident,version
                (root / "main.lua").write_text(source)
                client.install(compile_project(project,root))
            install("test.lua-game",(ROOT / "platform/examples/flappy-bird/main.lua").read_text())
            client.launch("test.lua-game")
            scenes=[]
            for _ in range(3):
                time.sleep(.12)
                scenes.append(client.request("scene"))
            assert len(set(scenes)) > 1, "game scene does not advance"
            assert client.info()["active"] == "test.lua-game"
            report["game"] = client.diagnostics()
            client.launch("system.clock")
            report["warmed_clock"] = client.diagnostics()
            for _ in range(30):
                client.launch("system.clock"); client.launch("test.lua-game")
            client.launch("system.clock")
            report["after_30_switches"] = client.diagnostics()
            assert report["after_30_switches"]["lua_used_bytes"] == 0
            assert report["after_30_switches"]["internal_free_bytes"] >= report["warmed_clock"]["internal_free_bytes"] - 256
            cases = {
                "loop": "return {update=function(dt) if dt>0 then while true do end end end}",
                "memory": "return {update=function(dt) if dt>0 then local s='01234567890123456789012345678901'; for i=1,20 do s=s..s end end end}",
                "forbidden": "return {update=function(dt) if dt>0 then io.open('x') end end}",
            }
            report["recovery"]={}
            for name,source in cases.items():
                install("test.lua-"+name,source)
                client.launch("test.lua-"+name)
                time.sleep(.25)
                assert client.info()["active"] == "system.clock", name+" did not recover"
                diagnostics = client.diagnostics()
                assert diagnostics["lua_used_bytes"] == 0
                expected = {"loop":15,"memory":10,"forbidden":1}[name]
                assert diagnostics["last_logic_error"] == expected, (name, diagnostics)
                report["recovery"][name]={"passed":True,"error":expected}
            for name,source in {"parse":"return "+"("*300+"1"+")"*300,
                                "init-loop":"while true do end"}.items():
                install("test.lua-"+name,source)
                try: client.launch("test.lua-"+name)
                except DeviceError as error:
                    assert error.code == {"parse":1,"init-loop":15}[name], (name,error.code)
                else: raise AssertionError(name+" unexpectedly launched")
                assert client.info()["active"] == "system.clock", name+" failed-launch recovery"
                report["recovery"][name]=True
            for name in ("game","loop","memory","forbidden","parse","init-loop"):
                client.request("remove", text("test.lua-"+name))
            report["final"] = client.diagnostics()
            assert report["final"]["main_stack_watermark_bytes"] > 4096
            assert report["game"]["lua_peak_bytes"] <= 32768
        report["passed"] = True
        args.report.write_text(json.dumps(report,indent=2)+"\n")
        print(json.dumps(report,indent=2))
    finally:
        client.close()


if __name__ == "__main__": main()
