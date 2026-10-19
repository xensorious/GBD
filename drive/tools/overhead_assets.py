"""Pack reviewed overhead paintings, exact material masks and steered axles."""
import array,hashlib,json,math,sys
from PIL import Image,ImageDraw
from vehicle_details import decal_mask
from overhead_materials import WINDOWS

W,H,POSES=48,72,3

def overhead_frames(directory,model):
    source=Image.open(directory/'vehicles.png').convert('RGBA')
    masks=Image.open(directory/'materials.png')
    box=(model%8*W,model//8*H,model%8*W+W,model//8*H+H)
    im=source.crop(box);mask=masks.crop(box)
    manifest=json.loads((directory/'materials.json').read_text())
    assert hashlib.sha256(im.tobytes()).hexdigest()==manifest[model]['rgba_sha256'],('Unreviewed overhead source',model)
    frames=[];roles=[]
    for pose,turn in enumerate((-18,0,18)):
        art=im.copy();role=mask.copy()
        if model not in (3,25,26,27,28,29,30,43,44,45):
            axle=max(16,WINDOWS[model][0]-2)
            opaque=[x for x in range(W) if im.getpixel((x,axle))[3]]
            if opaque:
             for cx in (min(opaque)+1,max(opaque)-1):
              wheel=Image.new('RGBA',(9,15));metal=Image.new('L',(9,15));d=ImageDraw.Draw(wheel);m=ImageDraw.Draw(metal)
              d.rounded_rectangle((3,3,5,11),radius=1,fill=(20,26,31,255));m.rounded_rectangle((3,3,5,11),radius=1,fill=4)
              d.line((4,5,4,9),fill=(133,150,160,255));m.line((4,5,4,9),fill=2)
              wheel=wheel.rotate(turn,resample=Image.Resampling.NEAREST);metal=metal.rotate(turn,resample=Image.Resampling.NEAREST)
              art.alpha_composite(wheel,(cx-4,axle-7));role.paste(metal,(cx-4,axle-7),metal.point(lambda v:255 if v else 0))
        frames.append(art);roles.append(role)
    painted=[(x,y) for y in range(H) for x in range(W) if mask.getpixel((x,y))==1]
    assert painted,model
    left=min(x for x,y in painted);right=max(x for x,y in painted);top=min(y for x,y in painted);bottom=max(y for x,y in painted)
    tags=[]
    for art,role in zip(frames,roles):
        data=bytearray(W*H)
        for y in range(H):
         for x in range(W):
          at=y*W+x;r=role.getpixel((x,y))
          if not art.getpixel((x,y))[3]:continue
          if r==1:
           stencil=decal_mask(model,(x-left)/max(1,right-left),(y-top)/max(1,bottom-top))
           data[at]=1|(stencil<<2)
          elif r==2:data[at]=2
        tags.append(data)
    # Palette partitions guarantee equal-looking trim never becomes paint.
    groups={}
    for art,tag in zip(frames,tags):
     for rgba,t in zip(art.getdata(),tag):
      if rgba[3]:groups.setdefault(t,[]).append(rgba[:3])
    budget={t:(64 if t==0 else 8 if t==2 else 3) for t in groups}
    remaining=255-sum(budget.values());weights={t:math.sqrt(len(colors)) for t,colors in groups.items() if t not in (0,2)}
    while remaining>0:
     target=max(weights,key=lambda t:weights[t]/budget[t]);budget[target]+=1;remaining-=1
    palette=[0];flags=[0];maps={}
    for t,colors in sorted(groups.items()):
     sample=Image.new('RGB',(len(colors),1));sample.putdata(colors)
     q=sample.quantize(colors=budget[t],method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
     table=q.getpalette();offset=len(palette);used=max(q.tobytes())+1
     for i in range(used):
      r,g,b=table[i*3:i*3+3];palette.append(0x8000|(r>>3)|((g>>3)<<5)|((b>>3)<<10));flags.append(t)
     maps[t]={c:offset+index for c,index in zip(colors,q.tobytes())}
    assert len(palette)<=256
    indexed=[]
    for art,tag in zip(frames,tags):indexed.append(bytes(maps[t][rgba[:3]] if rgba[3] else 0 for rgba,t in zip(art.getdata(),tag)))
    palette.extend([0]*(256-len(palette)));flags.extend([0]*(256-len(flags)))
    words=array.array('H',palette)
    if sys.byteorder!='little':words.byteswap()
    return indexed,words.tobytes(),bytes(flags),[r.tobytes() for r in roles],frames[1]
