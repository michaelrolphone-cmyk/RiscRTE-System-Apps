#!/usr/bin/env python3
"""Convert an exact 800x480 image to black=1, MSB-first packed display bytes."""
import argparse,pathlib

def pack(image,dither=False):
 from PIL import Image
 if image.size!=(800,480):raise ValueError('Image must be exactly 800x480; resize/crop explicitly')
 # Composite alpha onto white; a transparent background should not become black.
 rgba=image.convert('RGBA');base=Image.new('RGBA',rgba.size,'white');base.alpha_composite(rgba)
 mono=base.convert('L').convert('1',dither=Image.Dither.FLOYDSTEINBERG if dither else Image.Dither.NONE)
 return bytes(b^255 for b in mono.tobytes())
if __name__=='__main__':
 from PIL import Image
 p=argparse.ArgumentParser();p.add_argument('image');p.add_argument('output');p.add_argument('--dither',action='store_true');a=p.parse_args()
 with Image.open(a.image) as im:pathlib.Path(a.output).write_bytes(pack(im,a.dither))
