#!/usr/bin/env python3
"""Host-only raster preview of independent Documents draw commands. Not an OS screenshot."""
import argparse
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parent.parent

def render(commands,output):
    picture=Image.new('RGB',(900,565),'#000')
    draw=ImageDraw.Draw(picture)
    fonts={}
    for line in commands.splitlines():
        t,x,y,w,h,rgb,size,bold,txt=line.split('\t',8)
        t,x,y,w,h,size,bold=map(int,(t,x,y,w,h,size,bold))
        rgb='#'+rgb
        if t==0:
            draw.rectangle((x,y,x+w-1,y+h-1),fill=rgb)
        elif t==2:
            draw.line((x,y,x+w,y+h),fill=rgb,width=1)
        elif t==1:
            key=(size,bold)
            if key not in fonts:
                name='DejaVuSans-Bold.ttf' if bold else 'DejaVuSans.ttf'
                fonts[key]=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/'+name,max(9,size))
            draw.text((x,y),txt,font=fonts[key],fill=rgb)
        else:raise ValueError(t)
    output.parent.mkdir(exist_ok=True,parents=True)
    picture.save(output)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--view',choices=['text','review','page'],default='review')
    p.add_argument('--output',type=Path,required=True)
    args=p.parse_args()
    exe=ROOT/'build'/'preview-ui'
    exe.parent.mkdir(exist_ok=True)
    sources=[ROOT/'modules'/'documents'/s for s in ['model.c','docx.c','import.c','gateway.c','ui.c']]
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(ROOT/'include'),
                    *map(str,sources),str(ROOT/'tools'/'render_documents_ui.c'),'-o',str(exe)],check=True)
    output=subprocess.check_output([str(exe),args.view],text=True)
    render(output,args.output)
    print(f'Preview (not StyleOS guest): {args.output}')
