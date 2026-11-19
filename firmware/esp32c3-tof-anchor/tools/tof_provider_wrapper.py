#!/usr/bin/env python3
"""Add bounded ToF evidence to existing robot.radio queries, preserving all responses."""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import sys
import time

def read_tof(path):
    try:
        data=json.loads(Path(path).read_text())
        age=max(0,time.time()-float(data["updated_at"]))
        data["collector_age_s"]=age
        if age>5:
            data["valid"]=False
            data["status"]="collector_stale"
            data["position"]=None
        return data
    except (OSError,ValueError,TypeError,KeyError):
        return {"schema":"rover.tof.solution/1","status":"collector_unavailable","valid":False,"position":None}

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--provider-source",required=True)
    p.add_argument("--tof-state",required=True)
    args,rest=p.parse_known_args()
    source=Path(args.provider_source)
    sys.path.insert(0,str(source.parent))
    spec=importlib.util.spec_from_file_location("tof_vision_provider",source)
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    original=module.execute
    def execute(action,cfg,remote,client=None):
        result=original(action,cfg,remote,client)
        if action.get("contract") in ("robot.radio.status:1","robot.radio.localize:1"):
            result=copy.deepcopy(result)
            if isinstance(result.get("result"),dict):
                result["result"]["tof"]=read_tof(args.tof_state)
        return result
    module.execute=execute
    return module.main(rest)
if __name__=="__main__":raise SystemExit(main())
