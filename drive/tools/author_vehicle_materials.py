"""Author/review the checked-in pixel material atlas; never run at game runtime.

Polygon control points are measured against the canonical neutral artwork.
The resulting indexed PNG is the material authority, so color/hue guesses and
normalized bounding rectangles cannot change labels during ordinary builds.
"""
from pathlib import Path
import hashlib,json,sys,colorsys
from PIL import Image,ImageDraw,ImageFilter
import build_assets as b
from vehicle_material_shapes import D

OUT=b.ART/'materials';OUT.mkdir(exist_ok=True)
ATLAS=Image.new('P',(48*8,72*6));ATLAS.putpalette([0,0,0,220,220,220,0,170,240,255,60,60]+[0]*756)
manifest={'width':48,'height':72,'columns':8,'roles':{'0':'protected','1':'paint or equipment finish','2':'wheel/metal finish','3':'original lamp'},'models':[]}
for v,source in enumerate(b.vehicle_sources()[0]):
 im=b.vehicle_pose(source,0);art=list(b.pixels(im));data=D[v];mask=Image.new('L',(48,72));d=ImageDraw.Draw(mask)
 for poly in data['body']:d.polygon(poly,fill=1)
 labels=list(b.pixels(mask))
 # Cold authoring refinement: include the antialiased enamel edge around a
 # traced contour. Stored output pixels, not these color tests, are built.
 factory_palette=b.overhead_sprite(v)[1]
 c=int.from_bytes(factory_palette[8:10],'little')
 fr,fg,fb=c&31,(c>>5)&31,(c>>10)&31
 fh,fs,_=colorsys.rgb_to_hsv(fr/31,fg/31,fb/31)
 def enamel(rgb,bright=.18):
  r,g,blue=rgb;h,s,value=colorsys.rgb_to_hsv(r/255,g/255,blue/255)
  hue=min(abs(h-fh),1-abs(h-fh))
  return value>bright and (hue<.075 and s>.25 if fs>.20 else s<.25 and value>.68)
 if v<=24 and v!=3 or 31<=v<=42 or v==46:
  extended=list(b.pixels(mask.filter(ImageFilter.MaxFilter(5))))
  for at,(r,g,blue,a) in enumerate(art):
   if a>=b.ALPHA and extended[at] and enamel((r,g,blue)):labels[at]=1
 for at,(r,g,blue,a) in enumerate(art):
  hi,lo=max(r,g,blue),min(r,g,blue)
  # One-time authoring guard for outlines, seams and dark plastic inside a
  # traced panel. The saved PNG records each decision as an explicit pixel.
  if a<b.ALPHA or hi<(12 if v==47 else 30) or (v!=47 and hi<75 and hi-lo<hi*.18):labels[at]=0
 if v==30:
  for at,(r,g,blue,a) in enumerate(art):
   if labels[at]==1 and not (blue>r*1.1 and blue>g*1.05):labels[at]=0
 mask.putdata(labels);solid=Image.new('L',(48,72));sd=ImageDraw.Draw(solid)
 for poly in data.get('solid',[]):sd.polygon(poly,fill=1)
 for at,force in enumerate(b.pixels(solid)):
  if force and art[at][3]>=b.ALPHA:labels[at]=1
 base_labels=labels.copy()
 mask.putdata(labels);d=ImageDraw.Draw(mask)
 for poly in data['protected']:d.polygon(poly,fill=0)
 # Refine painted lips on a glass/cabin border by a single source pixel.
 after_cut=list(b.pixels(mask));refined=after_cut.copy()
 if v<=24 and v!=3 or 31<=v<=42 or v==46:
  for _ in range(2):
   before=refined.copy()
   for at,(r,g,blue,a) in enumerate(art):
    x,y=at%48,at//48
    if before[at]==0 and base_labels[at]==1 and enamel((r,g,blue),.50):
     if any(before[ny*48+nx]==1 for nx,ny in ((x-1,y),(x+1,y),(x,y-1),(x,y+1)) if 0<=nx<48 and 0<=ny<72):refined[at]=1
 trim=Image.new('L',(48,72));td=ImageDraw.Draw(trim)
 for poly in data.get('neutral_trim',[]):td.polygon(poly,fill=255)
 for at,inside in enumerate(b.pixels(trim)):
  if inside:
   r,g,blue,a=art[at];hi=max(r,g,blue);lo=min(r,g,blue)
   if hi-lo<hi*(.18 if v==20 else .25):refined[at]=0
 enamel_area=Image.new('L',(48,72));ed=ImageDraw.Draw(enamel_area)
 for poly in data.get('enamel_details',[]):ed.polygon(poly,fill=255)
 for at,inside in enumerate(b.pixels(enamel_area)):
  if inside and art[at][3]>=b.ALPHA and enamel(art[at][:3],.30):refined[at]=1
 mask.putdata(refined);d=ImageDraw.Draw(mask)
 for poly in data['lamps']:d.polygon(poly,fill=3)
 labeled=list(b.pixels(mask))
 # Colored enamel immediately bordering a lens is still bodywork. Red-body
 # cars use the measured lens silhouettes directly, since their hues match.
 if v not in (0,6,14,16,24,38,40) and fs>.20:
  for at,(r,g,blue,a) in enumerate(art):
   if labeled[at]==3 and base_labels[at]==1 and enamel((r,g,blue)):labeled[at]=1
 mask.putdata(labeled)
 metal=Image.new('L',(48,72));md=ImageDraw.Draw(metal)
 for poly in data['metal']:md.polygon(poly,fill=255)
 labels=list(b.pixels(mask));regions=list(b.pixels(metal))
 for at,(r,g,blue,a) in enumerate(art):
  hi,lo=max(r,g,blue),min(r,g,blue)
  if regions[at] and a>=b.ALPHA and hi>30 and hi-lo<max(24,hi*.60):labels[at]=2
  if a<b.ALPHA:labels[at]=0
 mask.putdata(labels)
 ATLAS.paste(mask,(v%8*48,v//8*72))
 manifest['models'].append({'id':v,'rgba_sha256':hashlib.sha256(im.tobytes()).hexdigest(),'paint_pixels':labels.count(1),'metal_pixels':labels.count(2),'lamp_pixels':labels.count(3),**data})
 print(v,'paint',labels.count(1),'metal',labels.count(2),'lamp',labels.count(3))
ATLAS.save(OUT/'vehicle-materials.png')
(OUT/'vehicle-materials.json').write_text(json.dumps(manifest,indent=2)+'\n')
