"""Prepare the original Asterwild artwork for the DS's RGB555 bitmap screens."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import array, json, math, random
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art'; OUT=ROOT/'build';OUT.mkdir(exist_ok=True)
assets=[]
def raw(name,data,ctype,count):
    (OUT/(name+'.bin')).write_bytes(data)
    assets.append((name,ctype,count))
def rgb555(im):
    im=im.convert('RGBA');out=array.array('H')
    pixels=im.get_flattened_data() if hasattr(im,'get_flattened_data') else im.getdata()
    for r,g,b,a in pixels:out.append((0x8000|(r>>3)|((g>>3)<<5)|((b>>3)<<10)) if a>=110 else 0)
    return out.tobytes()
def sprite(im,w,h):
    im=im.convert('RGBA');mask=im.getchannel('A').point(lambda v:255 if v>=110 else 0)
    box=mask.getbbox()
    if not box:return Image.new('RGBA',(w,h))
    im=im.crop(box);im.thumbnail((w-2,h-2),Image.Resampling.LANCZOS)
    dst=Image.new('RGBA',(w,h));dst.alpha_composite(im,((w-im.width)//2,h-im.height-1));return dst
world=Image.open(ART/'harbor.png').convert('RGB').resize((1024,768),Image.Resampling.LANCZOS)
raw('world_art',rgb555(world),'uint16_t',1024*768)
raw('world_thumbnail',rgb555(world.resize((192,144),Image.Resampling.LANCZOS)),'uint16_t',192*144)
plane=Image.new('L',(1024,768));draw=ImageDraw.Draw(plane)
geometry=json.loads((ART/'world_geometry.json').read_text()) if (ART/'world_geometry.json').exists() else {}
for obj in sorted(geometry.get('occluders',[]),key=lambda o:o['depth']):
    draw.polygon([tuple(p) for p in obj['polygon']],fill=max(1,min(255,obj['depth']//4)))
raw('world_depth',plane.tobytes(),'uint8_t',1024*768)
world.save(OUT/'world.png');plane.point(lambda v:min(255,v*2)).save(OUT/'depth.png')
for stem,name in [('creatures-front','creature_front'),('creatures-back','creature_back')]:
    im=Image.open(ART/(stem+'.png')).convert('RGBA');sprites=[]
    for n in range(6):
        x=n%3;y=n//3;cell=im.crop((x*im.width//3,y*im.height//2,(x+1)*im.width//3,(y+1)*im.height//2))
        sprites.append(sprite(cell,96,96))
    raw(name,b''.join(rgb555(s) for s in sprites),'uint16_t',6*96*96)
    if name=='creature_front':raw('creature_field',b''.join(rgb555(s.resize((32,32),Image.Resampling.LANCZOS)) for s in sprites),'uint16_t',6*32*32)
    sheet=Image.new('RGBA',(576,96),(242,237,216,255))
    for i,s in enumerate(sprites):sheet.alpha_composite(s,(i*96,0))
    sheet.save(OUT/(name+'.png'))
im=Image.open(ART/'hero.png').convert('RGBA');heroes=[]
for row in range(4):
 for col in range(4):heroes.append(sprite(im.crop((col*im.width//4,row*im.height//4,(col+1)*im.width//4,(row+1)*im.height//4)),28,42))
raw('hero_art',b''.join(rgb555(s) for s in heroes),'uint16_t',16*28*42)
im=Image.open(ART/'people.png').convert('RGBA');people=[];portraits=[]
for n in range(4):
 cell=im.crop((n*im.width//4,0,(n+1)*im.width//4,im.height));people.append(sprite(cell,28,46))
 box=cell.getchannel('A').point(lambda v:255 if v>=110 else 0).getbbox();cell=cell.crop(box)
 portraits.append(sprite(cell.crop((0,0,cell.width,int(cell.height*.56))),72,90))
raw('people_art',b''.join(rgb555(s) for s in people),'uint16_t',4*28*46)
raw('portrait_art',b''.join(rgb555(s) for s in portraits),'uint16_t',4*72*90)
battle=Image.open(ART/'battle.png').convert('RGB').resize((256,192),Image.Resampling.LANCZOS)
raw('battle_art',rgb555(battle),'uint16_t',256*192)
# Original title composition from the village vista; typography is baked at full DS resolution.
title=world.resize((256,192),Image.Resampling.LANCZOS).convert('RGBA')
shade=Image.new('RGBA',title.size,(10,28,42,80));title=Image.alpha_composite(title,shade);d=ImageDraw.Draw(title)
fontroot=Path('/usr/share/fonts/truetype/dejavu')
serif=ImageFont.truetype(str(fontroot/'DejaVuSerif-Bold.ttf'),30)
small=ImageFont.truetype(str(fontroot/'DejaVuSans.ttf'),10)
for dx,dy in [(1,2),(0,2),(-1,2)]:d.text((128+dx,61+dy),'Asterwild',font=serif,anchor='mt',fill='#102e3d',stroke_width=1)
d.text((128,61),'Asterwild',font=serif,anchor='mt',fill='#fff0ca')
d.line((52,105,204,105),fill='#f1c779',width=1)
d.text((128,112),'L A N T E R N  I S L E S',font=small,anchor='mt',fill='#fff0ca')
raw('title_art',rgb555(title),'uint16_t',256*192);title.save(OUT/'title.png')
# Two antialiased proportional ASCII faces, top aligned in a 12x14 cell.
for size,name in [(10,'font'),(9,'font_small')]:
 f=ImageFont.truetype(str(fontroot/'DejaVuSans.ttf'),size);glyphs=[];adv=[]
 for code in range(32,128):
  ch=chr(code);im=Image.new('L',(12,14));ImageDraw.Draw(im).text((0,-2),ch,font=f,fill=255)
  glyphs.append(im.tobytes());adv.append(max(2,round(f.getlength(ch))))
 raw(name+'_alpha',b''.join(glyphs),'uint8_t',96*12*14)
 raw(name+'_advance',bytes(adv),'uint8_t',96)
# An original short 8-bit soundtrack: plucked pentatonic melody over gentle chords and sea air.
rate=12000; beat=.4;length=beat*32; samples=bytearray();rng=random.Random(370)
melody=[76,79,81,79,74,76,72,74,76,79,83,81,79,76,74,72,74,76,79,81,83,81,79,76,74,72,74,76,79,76,74,72]
chords=[(48,55,60),(45,52,57),(53,60,65),(43,50,55)]
for i in range(round(rate*length)):
 t=i/rate;b=int(t/beat);u=t%beat;f=440*2**((melody[b]-69)/12)
 v=.28*math.sin(2*math.pi*f*t)*math.exp(-u*6)+.09*math.sin(2*math.pi*f*2*t)*math.exp(-u*11)
 for note in chords[b//8]:v+=.035*math.sin(2*math.pi*440*2**((note-69)/12)*t)
 v+=.009*(rng.random()*2-1)*(1+math.sin(t*.7))
 # Fade the loop seam so it never clicks.
 v*=min(1,t/.1,(length-t)/.1)
 samples.append(int(max(-127,min(127,v*127)))&255)
raw('music_pcm',samples,'int8_t',len(samples))
with (OUT/'assets.h').open('w') as f:
 f.write('#ifndef ASTER_ASSETS_H\n#define ASTER_ASSETS_H\n#include <stdint.h>\n')
 for name,ctype,count in assets:f.write(f'extern const {ctype} {name}[{count}];\n')
 f.write('#define MUSIC_BYTES '+str(len(samples))+'\n#define MUSIC_RATE '+str(rate)+'\n#endif\n')
with (OUT/'assets.c').open('w') as f:
 f.write('/* Binary assets stay compact and compile without enormous C initializers. */\n')
 for name,ctype,count in assets:
  f.write('__asm__(".section .rodata\\n.balign 4\\n.global '+name+'\\n'+name+':\\n.incbin \\\"build/'+name+'.bin\\\"\\n.previous\\n");\n')
print('Prepared',len(assets),'original DS assets;',sum((OUT/(n+'.bin')).stat().st_size for n,_,_ in assets),'bytes')
