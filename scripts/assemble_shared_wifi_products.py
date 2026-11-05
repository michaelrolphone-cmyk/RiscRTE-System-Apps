#!/usr/bin/env python3
"""Assemble a fail-closed Wi-Fi candidate without removing installed authority."""
import json
from shared_wifi_products import *
def assemble(before,sd,elf,manifest,provider_root):
    boot,index,row,old=selected(before);new=json.loads(manifest)
    parity(before,row,old,new)
    added=grants(before,row,new)
    after,new_sd,changed,changed_sd,licenses=provider(before,sd,provider_root)
    boot=json.loads(after['boot.json']);boot['app_capabilities'][index]['grants']=added
    after['boot.json']=encode(boot);after[row['manifest']]=manifest
    after[old['file_name']]=elf;changed.update((row['manifest'],old['file_name']))
    return after,new_sd,changed,changed_sd,licenses
def main():
    a=arguments(__doc__,('providers',))
    elf,manifest,lanes,build,read_image,magic,decode,encode_image=load(a)
    staged={};receipt={'schema':1,'status':'assembled-not-qualified','products':{},'published':False}
    for lane,(size,stored,before,sd) in lanes.items():
        after,new_sd,changed,changed_sd,licenses=assemble(before,sd,elf,manifest,a.providers/lane/'scene-host')
        packed,image,packing=pack(after,stored,changed,size,build,read_image,magic,decode,encode_image)
        staged[lane]=(after,new_sd)
        receipt['products'][lane]={'before_selection':selected(before)[2],'after_selection':selected(after)[2],'changed_internal':sorted(changed),'changed_sd':sorted(changed_sd),'licenses':licenses,'packing':packing}
    a.output.mkdir(parents=True)
    for lane,(after,sd) in staged.items():write(a.output/lane/'after',after);write(a.output/lane/'after-sd',sd)
    (a.output/'assembly.json').write_bytes(encode(receipt))
if __name__=='__main__':main()
