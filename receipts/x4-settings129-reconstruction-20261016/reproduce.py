import os,sys,shlex
sys.path.insert(0,os.environ.get("SYSTEM_SOURCE","/tmp/x4-settings-producer-20261016/system-history")+"/scripts")
import build_portable_settings as b
b.portable_quick_build.RESIDENT_CLIENT_VERSIONS["settings"]="1.3.29"
run=b.subprocess.run
def compile_run(command,**kw):
    command=list(command)
    if command and str(command[0]).endswith("xtensa-esp32s3-elf-gcc"):
        command+=shlex.split(os.environ.get("EXTRA_CC_FLAGS",""))
        if os.environ.get("CATALOG_LAST"):
            for item in list(command):
                if str(item).endswith("/catalog.c"):
                    command.remove(item);command.append(item)
    return run(command,**kw)
b.subprocess.run=compile_run
p=b.argument_parser();a=p.parse_args();b.build(a,p)
