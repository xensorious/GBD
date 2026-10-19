#!/usr/bin/env python3
"""Offline crop and material authoring for Free Roam's actual overhead art.
Normal asset builds consume the reviewed PNG masks, not these color decisions.
"""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import json,hashlib,colorsys
ROOT=Path(__file__).resolve().parents[1];ART=ROOT/'art'/'overhead'

def crop_sprite(source,cell):
    w,h=source.size;x=cell%4;y=cell//4
    im=source.crop((x*w//4,y*h//4,(x+1)*w//4,(y+1)*h//4)).convert('RGBA')
    a=im.getchannel('A').point(lambda v:255 if v>=160 else 0)
    # Keep substantial components, removing disconnected generative specks.
    px=a.load();seen=set();keep=Image.new('L',im.size);out=keep.load()
    groups=[]
    for yy in range(im.height):
      for xx in range(im.width):
       if not px[xx,yy] or (xx,yy) in seen:continue
       todo=[(xx,yy)];seen.add((xx,yy));group=[]
       while todo:
        qx,qy=todo.pop();group.append((qx,qy))
        for dx,dy in ((-1,0),(1,0),(0,-1),(0,1)):
         nx,ny=qx+dx,qy+dy
         if 0<=nx<im.width and 0<=ny<im.height and px[nx,ny] and (nx,ny) not in seen:seen.add((nx,ny));todo.append((nx,ny))
       groups.append(group)
    largest=max(map(len,groups));
    for group in groups:
     if len(group)>=max(24,largest//12):
      for x,y in group:out[x,y]=255
    im.putalpha(keep);im=im.crop(keep.getbbox());im.thumbnail((42,68),Image.Resampling.LANCZOS)
    canvas=Image.new('RGBA',(48,72));canvas.alpha_composite(im,((48-im.width)//2,(72-im.height)//2))
    canvas.putalpha(canvas.getchannel('A').point(lambda v:255 if v>=112 else 0));return canvas

if __name__=='__main__':
 images=[]
 for page in range(3):
  path=ART/f'source-{page}.png'
  if not path.exists():break
  source=Image.open(path)
  images.extend(crop_sprite(source,i) for i in range(16))
 images[23]=images[23].transpose(Image.Transpose.ROTATE_180)
 atlas=Image.new('RGBA',(384,432))
 for v,im in enumerate(images):atlas.paste(im,(v%8*48,v//8*72))
 atlas.save(ART/'vehicles.png')
 review=Image.new('RGB',(768,1008),'#283f4c');d=ImageDraw.Draw(review)
 for v,im in enumerate(images):
  x=v%8*96;y=v//8*168;review.paste(im.resize((96,144),Image.Resampling.NEAREST),(x,y+20),im.resize((96,144),Image.Resampling.NEAREST));d.text((x+5,y+4),str(v),fill='white')
 review.save(ROOT/'build'/'overhead-source-review.png')

 # Material atlas: manual panel openings constrain one-time source analysis.
 # No hue tests or rectangles are used by the production packer or renderer.
 from overhead_materials import WINDOWS,OPENINGS,EQUIPMENT
 masks=Image.new('P',atlas.size,0)
 colors=[(42,62,76),(232,72,169),(242,209,101),(94,216,232),(77,87,99)]
 masks.putpalette(sum((list(c) for c in colors),[])+[0]*(768-len(colors)*3))
 manifest=[]
 for v,im in enumerate(images):
  mask=Image.new('P',(48,72),0);d=ImageDraw.Draw(mask)
  if v in EQUIPMENT:
   for box in EQUIPMENT[v]:d.rectangle(box,fill=1)
  else:d.rectangle((3,2,44,70),fill=1)
  a,b,c,e=WINDOWS[v]
  if b:
   d.polygon(((13,a+1),(17,a),(31,a),(35,a+1),(33,b-1),(15,b-1)),fill=0)
  if e:
   d.polygon(((15,c),(33,c),(34,e-1),(30,e),(18,e),(14,e-1)),fill=0)
  for poly in OPENINGS.get(v,[]):d.polygon(poly,fill=0)
  for yy in range(72):
   for xx in range(48):
    r,g,b,alpha=im.getpixel((xx,yy));maximum=max(r,g,b);minimum=min(r,g,b)
    if not alpha:mask.putpixel((xx,yy),0);continue
    sat=(maximum-minimum)/max(1,maximum)
    # Rubber and dark neutral trim, separated from even dark saturated enamel.
    if maximum<70 and sat<.36:mask.putpixel((xx,yy),4);continue
    # Glass and chrome on the shoulder stay independent of body paint.
    if (xx<12 or xx>35) and yy>14 and yy<60:
     if b>r*1.12 and g>r*1.05 and sat<.55:mask.putpixel((xx,yy),0)
     elif sat<.14 and maximum>110:mask.putpixel((xx,yy),2)
    # Motor/rider finishes apply only to painted equipment within their boxes.
    if v in EQUIPMENT and mask.getpixel((xx,yy))==1:
     if v==30 and not(b>r*1.2 and b>g):mask.putpixel((xx,yy),0)
     elif v in (3,26,27,43,44,45) and (sat<.25 or maximum<65):mask.putpixel((xx,yy),0)
  # Tiny lens samples follow actual visible corners; a red bumper is enamel.
  if v not in EQUIPMENT:
   for xx in (12,35):
    for yy in (64,65):
     r,g,b,alpha=im.getpixel((xx,yy))
     if alpha and r>g*1.6 and r>b*1.4 and r>100:mask.putpixel((xx,yy),3)
   # Source headlights, where visible from directly overhead, occupy these
   # isolated tips; neutral hood highlights are deliberately excluded.
   for xx in (14,33):
    for yy in (7,8):
     r,g,b,alpha=im.getpixel((xx,yy))
     if alpha and max(r,g,b)-min(r,g,b)<35 and min(r,g,b)>125:mask.putpixel((xx,yy),3)
  if v==47:
   for yy in range(72):
    for xx in range(48):
     if im.getpixel((xx,yy))[3] and not(20<=xx<=28 and 19<=yy<=53):mask.putpixel((xx,yy),1)
  # Colored enamel on window/engine-cover borders is outside the opening.
  original=mask.copy()
  if v not in EQUIPMENT:
   for yy in range(72):
    for xx in range(48):
     r,g,b,alpha=im.getpixel((xx,yy));maximum=max(r,g,b);minimum=min(r,g,b)
     if alpha and original.getpixel((xx,yy))==0 and maximum-minimum>maximum*.40 and maximum>50:
      if not(v==38 and 19<=xx<=29 and 45<=yy<=59):mask.putpixel((xx,yy),1)
  # Explicit wheel edges/hardware are separate, even when mostly hidden by
  # fenders in plan view. The steered poses rotate these actual axle parts.
  if v not in EQUIPMENT:
   bounds=im.getbbox();left,top,right,bottom=bounds
   for x in (left+2,right-3):
    for y in range(max(18,a),min(32,a+9 if a else 28)):
     if im.getpixel((x,y))[3] and mask.getpixel((x,y)) in (0,4):mask.putpixel((x,y),2)
  # Every model exposes some actual metal for its existing wheel finish.
  if not any(k==2 for k in mask.tobytes()):
   candidates=[(x,y) for y in range(72) for x in range(48) if mask.getpixel((x,y))==4]
   if candidates:mask.putpixel(candidates[len(candidates)//2],2)
  masks.paste(mask,(v%8*48,v//8*72))
  manifest.append({'model':v,'rgba_sha256':hashlib.sha256(im.tobytes()).hexdigest(),'roles':{str(k):mask.tobytes().count(k) for k in range(5)}})
 masks.save(ART/'materials.png');(ART/'materials.json').write_text(json.dumps(manifest,indent=2)+'\n')
