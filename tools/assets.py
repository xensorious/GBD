"""Export original polished indexed artwork and unchanged collision maps.

Tall objects are not baked into map_art: the runtime sorts them with actors by
WORLD ground-contact depth. See art_overhaul.py for all hand-drawn sprite art.
"""
from pathlib import Path
from PIL import Image, ImageDraw
import random
from art_overhaul import *

OUT=Path(__file__).resolve().parents[1]/'build'
OUT.mkdir(exist_ok=True)
maps=[]
for m in range(2):
    grid=[[0 for x in range(32)] for y in range(24)]
    for y in range(24):
        for x in range(32):
            if x in (0,31) or y in (0,23):grid[y][x]=3
            elif (x*11+y*17+m*7)%29==0:grid[y][x]=4
    maps.append(grid)
def box(m,x,y,w,h,t):
    for j in range(y,y+h):
        for i in range(x,x+w):maps[m][j][i]=t
box(0,15,0,2,23,1);box(0,3,13,26,2,1);box(0,12,8,7,4,6)
for x,y in [(4,6),(22,5),(25,17)]:box(0,x,y,4,4,8)
box(0,5,10,2,4,1);box(0,23,9,2,5,1);box(0,22,10,6,3,13)
box(0,3,17,7,3,2);box(0,3,17,7,1,15);maps[0][17][6]=12
for x in range(21,29):maps[0][9][x]=10;maps[0][15][x]=10
maps[0][15][24]=1;maps[0][15][17]=11;maps[0][3][13]=11
box(1,3,23,3,1,1);box(1,4,4,2,19,1);box(1,4,10,24,2,1);box(1,26,4,2,15,1)
box(1,14,0,2,24,2);box(1,14,10,2,2,7);box(1,6,14,6,5,5);box(1,9,5,4,4,5);box(1,20,12,9,4,5)
box(1,6,11,3,3,6);box(1,24,3,6,4,6)
for x in range(17,24):maps[1][4][x]=3
for y in range(2,8):maps[1][y][2]=3
maps[1][20][6]=11;maps[1][5][23]=11
for m,x,y,w,h in [(0,2,2,2,4),(0,6,2,4,2),(0,11,20,2,3),(0,28,2,2,3),
                   (1,6,2,2,2),(1,11,2,2,2),(1,6,21,7,2),(1,18,6,2,2),
                   (1,21,7,2,2),(1,29,16,2,5),(1,18,21,5,2)]:box(m,x,y,w,h,3)
box(1,8,2,3,2,2);box(1,18,17,5,3,2)
for m,x,y in [(0,11,6),(0,20,12),(0,20,17),(0,10,15),(1,12,10),(1,17,10),(1,6,16),(1,23,12),(1,25,17)]:
    if maps[m][y][x]==0:maps[m][y][x]=4

GLYPHS={
'A':'01110 10001 10001 11111 10001 10001 10001','B':'11110 10001 10001 11110 10001 10001 11110',
'C':'01111 10000 10000 10000 10000 10000 01111','D':'11110 10001 10001 10001 10001 10001 11110',
'E':'11111 10000 10000 11110 10000 10000 11111','F':'11111 10000 10000 11110 10000 10000 10000',
'G':'01111 10000 10000 10111 10001 10001 01111','H':'10001 10001 10001 11111 10001 10001 10001',
'I':'111 010 010 010 010 010 111','J':'00111 00010 00010 00010 10010 10010 01100',
'K':'10001 10010 10100 11000 10100 10010 10001','L':'10000 10000 10000 10000 10000 10000 11111',
'M':'10001 11011 10101 10101 10001 10001 10001','N':'10001 11001 10101 10011 10001 10001 10001',
'O':'01110 10001 10001 10001 10001 10001 01110','P':'11110 10001 10001 11110 10000 10000 10000',
'Q':'01110 10001 10001 10001 10101 10010 01101','R':'11110 10001 10001 11110 10100 10010 10001',
'S':'01111 10000 10000 01110 00001 00001 11110','T':'11111 00100 00100 00100 00100 00100 00100',
'U':'10001 10001 10001 10001 10001 10001 01110','V':'10001 10001 10001 10001 10001 01010 00100',
'W':'10001 10001 10001 10101 10101 10101 01010','X':'10001 10001 01010 00100 01010 10001 10001',
'Y':'10001 10001 01010 00100 00100 00100 00100','Z':'11111 00001 00010 00100 01000 10000 11111',
'0':'01110 10001 10011 10101 11001 10001 01110','1':'00100 01100 00100 00100 00100 00100 01110',
'2':'01110 10001 00001 00010 00100 01000 11111','3':'11110 00001 00001 01110 00001 00001 11110',
'4':'00010 00110 01010 10010 11111 00010 00010','5':'11111 10000 10000 11110 00001 00001 11110',
'6':'01110 10000 10000 11110 10001 10001 01110','7':'11111 00001 00010 00100 01000 01000 01000',
'8':'01110 10001 10001 01110 10001 10001 01110','9':'01110 10001 10001 01111 00001 00001 01110',
'.':'0 0 0 0 0 0 1',',':'0 0 0 0 0 1 1','!':'00100 00100 00100 00100 00100 00000 00100',
'?':'01110 10001 00001 00010 00100 00000 00100',':':'0 1 0 0 1 0 0',
'-':'0 0 0 11111 0 0 0','/':'00001 00010 00010 00100 01000 01000 10000',
"'":'00100 00100 00000 00000 00000 00000 00000','+':'00000 00100 00100 11111 00100 00100 00000',
'(':'00010 00100 01000 01000 01000 00100 00010',')':'01000 00100 00010 00010 00010 00100 01000',
'>':'10000 01000 00100 00010 00100 01000 10000','<':'00001 00010 00100 01000 00100 00010 00001',
'=':'00000 00000 11111 00000 11111 00000 00000','%':'11001 11010 00010 00100 01000 01011 10011'
}
font=[]
for c in range(32,128):
    rows=GLYPHS.get(chr(c).upper(),'0 0 0 0 0 0 0').split()
    # The three-pixel I is centered within the fixed five-pixel cell.
    font.extend(int(('0'+v+'0') if chr(c).upper()=='I' else v.ljust(5,'0'),2) for v in rows)

def ground_map(m,grid):
    """Continuous terrain, only low flowers/grass/bridge remain in map_art."""
    im=image(512,384,5);d=ImageDraw.Draw(im);rng=random.Random(807+m)
    # Broad, restrained patches prevent the lawn reading as noisy wallpaper.
    for y in range(0,384,4):
        for x in range(0,512,4):
            if rng.randrange(11)==0:
                d.line((x,y,x+2,y),fill=GRASS_LIT)
            elif rng.randrange(17)==0:
                d.line((x,y,x+1,y),fill=GRASS_DARK)
    def near(x,y):return grid[y][x] if 0<=x<32 and 0<=y<24 else 3
    for ty,row in enumerate(grid):
        for tx,t in enumerate(row):
            x,y=tx*16,ty*16;r=random.Random(3701+tx*41+ty*173+m*907)
            if t==1: # Warm winding footpath, rounded grass fringes at exposed edges.
                d.rectangle((x,y,x+15,y+15),fill=7)
                for k in range(5):
                    xx,yy=x+r.randrange(16),y+r.randrange(16)
                    d.line((xx,yy,xx+1,yy),fill=SAND_LIGHT if k%2 else 8)
                for dx,dy in [(-1,0),(1,0),(0,-1),(0,1)]:
                    if near(tx+dx,ty+dy) in (1,6,7):continue
                    if dx:
                        ex=x if dx<0 else x+15
                        d.line((ex,y,ex,y+15),fill=SAND_DARK)
                        for oy in (2,7,12):
                            d.line((ex, y+oy,ex+(1 if dx<0 else -1),y+oy+2),fill=GRASS_DARK)
                    else:
                        ey=y if dy<0 else y+15
                        d.line((x,ey,x+15,ey),fill=SAND_DARK)
                        if dy>0:d.line((x+1,ey-1,x+14,ey-1),fill=SAND_LIGHT)
                        for ox in (3,10):d.point((x+ox,ey),fill=GRASS_DARK)
            elif t==2: # Blue-green water with directional shoreline and glints.
                d.rectangle((x,y,x+15,y+15),fill=10)
                for k in range(3):
                    xx=x+r.randrange(12);yy=y+r.randrange(14)
                    d.line((xx,yy,xx+2+r.randrange(3),yy),fill=WATER_LIGHT if k else WATER_MID)
                if (tx+ty)%4==0:
                    d.line((x+5,y+8,x+8,y+8),fill=WATER_GLINT);d.point((x+9,y+7),fill=WATER_LIGHT)
                for dx,dy in [(-1,0),(1,0),(0,-1),(0,1)]:
                    if near(tx+dx,ty+dy) in (2,7,12):continue
                    if dx:
                        ex=x if dx<0 else x+15;inside=1 if dx<0 else -1
                        d.line((ex,y,ex,y+15),fill=BANK)
                        d.line((ex+inside,y,ex+inside,y+15),fill=WATER_DARK)
                        for oy in (2,9):d.line((ex+inside*2,y+oy,ex+inside*2,y+oy+3),fill=WATER_LIGHT)
                    else:
                        ey=y if dy<0 else y+15;inside=1 if dy<0 else -1
                        d.line((x,ey,x+15,ey),fill=BANK)
                        d.line((x,ey+inside,x+15,ey+inside),fill=WATER_DARK)
                        d.line((x+2,ey+inside*2,x+12,ey+inside*2),fill=FOAM if dy>0 else WATER_LIGHT)
            elif t==6: # Staggered little cobbles join into a single plaza.
                d.rectangle((x,y,x+15,y+15),fill=STONE_DARK)
                for sy in range(0,16,8):
                    offset=0 if (ty*2+sy//8)%2 else -4
                    for sx in range(offset,16,8):
                        x0=max(x,x+sx);x1=min(x+15,x+sx+6)
                        if x1<x0:continue
                        d.rectangle((x0,y+sy,x1,y+sy+6),fill=STONE_LIGHT if (tx+ty+sx)%3 else STONE_MID)
                        d.line((x0,y+sy,x1,y+sy),fill=STONE_TOP)
                        d.line((x0,y+sy+6,x1,y+sy+6),fill=STONE_MID)
            elif t==7: # Footbridge: visible bank-to-bank deck with rails.
                d.rectangle((x,y,x+15,y+15),fill=16)
                for sy in range(1,16,4):
                    d.rectangle((x,y+sy,x+15,y+sy+2),fill=BARK_MID)
                    d.line((x,y+sy,x+15,y+sy),fill=17)
                    if sy==9:d.line((x+4,y+sy+1,x+9,y+sy+1),fill=BARK_DARK)
                if near(tx,ty-1)!=7:
                    d.rectangle((x,y,x+15,y+2),fill=BARK_DARK);d.line((x,y,x+15,y),fill=43)
                if near(tx,ty+1)!=7:
                    d.rectangle((x,y+13,x+15,y+15),fill=BARK_DARK);d.line((x,y+13,x+15,y+13),fill=17)
            elif t==5: # Wild meadow: leaf clusters, not stripes.
                for k in range(7):
                    xx=x+2+r.randrange(12);yy=y+4+r.randrange(11)
                    d.line((xx-1,yy,xx-3,yy-4),fill=GRASS_DEEP)
                    d.line((xx,yy,xx+2,yy-5),fill=29)
                    d.line((xx+1,yy,xx+4,yy-3),fill=GRASS_LIT)
                    d.point((xx+2,yy-5),fill=GRASS_SUN)
            elif t==4:
                for xx,yy in [(x+4,y+6),(x+11,y+12)]:
                    d.line((xx,yy,xx,yy+3),fill=29)
                    d.line((xx-1,yy,xx+1,yy),fill=28)
                    d.line((xx,yy-1,xx,yy+1),fill=28);d.point((xx,yy),fill=38)
                    d.point((xx+1,yy+3),fill=6)
            elif t in (8,13):
                # Foundation/soil grounds exist, but roof, wall and plants do not.
                if t==13:
                    d.rectangle((x,y+6,x+15,y+15),fill=GRASS_SHADOW)
    # Small ground-only environment details never change walkability.
    for tx,ty in [(11,6),(20,17),(10,15)] if m==0 else [(12,10),(25,17)]:
        if grid[ty][tx] in (0,4):
            x,y=tx*16,ty*16
            d.ellipse((x+3,y+9,x+9,y+12),fill=STONE_MID)
            d.line((x+4,y+9,x+7,y+9),fill=STONE_TOP)
    return im

map_images=[ground_map(m,grid) for m,grid in enumerate(maps)]
scene_objects=[];scene_starts=[0]
for m,grid in enumerate(maps):
    objects=[]
    for y,row in enumerate(grid):
        for x,t in enumerate(row):
            depth=(y+1)*16
            if t==3:
                kind=(x*7+y*11+m*3)%9
                kind=2 if kind==0 else 1 if kind==1 else 0
                objects.append((x*16-8,depth-48,depth,kind))
            elif t==10:objects.append((x*16,depth-24,depth,7))
            elif t==11:objects.append((x*16-4,depth-28,depth,6))
            elif t==12:objects.append((x*16-6,depth-34,depth,9))
            elif t==15:objects.append((x*16-2,depth-22,depth,8))
            elif t==13:objects.append((x*16,depth-24,depth,10))
    if m==0:
        for kind,(x,y) in enumerate([(4,6),(22,5),(25,17)],3):
            objects.append((x*16-8,(y+4)*16-88,(y+4)*16,kind))
    else:
        for x,y in [(7,12),(26,3)]:objects.append((x*16-4,(y+1)*16-44,(y+1)*16,11))
    scene_objects.extend(sorted(objects,key=lambda o:(o[2],o[0])))
    scene_starts.append(len(scene_objects))

# Precompose static scenery once, retaining a per-pixel foot-depth plane. The
# runtime can then depth-test actors against this plane without redrawing all
# static props on every frame. Zero means terrain, positive means depth / 16.
map_scenes=[];map_depths=[]
for m,ground in enumerate(map_images):
    composed=ground.copy();depth_plane=Image.new('L',ground.size,0)
    for x,y,depth,kind in scene_objects[scene_starts[m]:scene_starts[m+1]]:
        prop=scenery[kind];paste(composed,prop,(x,y))
        mask=Image.frombytes('L',prop.size,bytes(255 if v else 0 for v in prop.tobytes()))
        depth_plane.paste(depth//16,(x,y,x+prop.width,y+prop.height),mask)
    map_scenes.append(composed);map_depths.append(depth_plane)

def scenic_backdrop(title=False):
    im=image(240,160,SKY_HIGH);d=ImageDraw.Draw(im)
    d.rectangle((0,27,239,57),fill=SKY_LOW)
    # Pixel clouds and distant ridges establish perspective without gradients.
    for x,y,w in [(8,18,34),(93,12,42),(174,25,47)]:
        d.rectangle((x,y+3,x+w,y+7),fill=26)
        d.rectangle((x+5,y,x+w-8,y+5),fill=26)
        d.line((x+7,y,x+w-13,y),fill=3)
    poly(d,[(0,54),(15,47),(36,50),(57,39),(78,44),(106,35),(130,45),(151,40),(178,48),(204,40),(239,47),(239,85),(0,85)],HILL_FAR)
    poly(d,[(0,68),(23,58),(45,60),(70,48),(96,56),(117,52),(143,62),(166,49),(190,57),(216,51),(239,64),(239,99),(0,99)],HILL_MID)
    poly(d,[(0,83),(25,76),(54,82),(78,66),(102,75),(130,69),(155,80),(181,68),(208,73),(239,69),(239,117),(0,117)],HILL_NEAR)
    # A sunlit river recedes toward the middle of the horizon.
    poly(d,[(130,63),(137,63),(132,75),(140,84),(133,92),(154,108),(170,132),(151,160),(90,160),(119,134),(126,114),(116,96),(125,83),(122,74)],10)
    for yy,x0,x1 in [(77,126,133),(89,125,133),(106,130,145),(121,132,153),(140,118,155),(150,108,147)]:
        d.line((x0,yy,x1,yy),fill=WATER_LIGHT)
        d.line((x0+3,yy+2,x1-4,yy+2),fill=25)
    poly(d,[(0,102),(43,90),(80,95),(116,119),(101,146),(95,160),(0,160)],5)
    poly(d,[(239,88),(206,90),(174,104),(153,131),(160,160),(239,160)],GRASS_DARK)
    rng=random.Random(911)
    for k in range(140):
        xx=rng.randrange(240);yy=rng.randrange(93,160)
        if im.getpixel((xx,yy)) in (5,GRASS_DARK):
            d.line((xx,yy,xx+2,yy),fill=GRASS_LIT if k%3 else GRASS_DEEP)
    if title:
        # Foreground trees frame the vista. A village roof sits at the far bend.
        house=scenery[3].resize((40,44),Image.Resampling.NEAREST)
        paste(im,house,(174,62))
        for xx,yy in [(-12,44),(12,57),(219,46),(201,69),(-3,92),(219,109)]:
            paste(im,scenery[0],(xx,yy))
        # Little flowers across the foreground riverbank.
        for x,y in [(27,136),(39,142),(195,131),(211,141),(69,151)]:
            d.line((x,y,x,y-3),fill=29);d.line((x-1,y-4,x+1,y-4),fill=28);d.point((x,y-4),fill=38)
    else:
        # Battle clears retain a horizon and quiet readable sprite platforms.
        d.rectangle((0,90,239,159),fill=5)
        poly(d,[(0,111),(34,104),(58,111),(93,108),(137,113),(181,108),(239,119),(239,159),(0,159)],GRASS_LIT)
        d.ellipse((130,63,229,97),fill=GRASS_DEEP)
        d.ellipse((134,62,225,92),fill=6)
        d.ellipse((141,65,217,86),fill=GRASS_LIT)
        d.line((150,67,199,67),fill=GRASS_SUN)
        d.ellipse((-12,105,121,149),fill=GRASS_DEEP)
        d.ellipse((-9,102,118,142),fill=6)
        d.ellipse((1,105,106,131),fill=GRASS_LIT)
        for x,y in [(6,86),(17,93),(222,100),(229,106),(119,109)]:
            d.line((x,y,x-2,y-4),fill=29);d.line((x,y,x+2,y-5),fill=GRASS_SUN)
    return im
title=scenic_backdrop(True);battle=scenic_backdrop(False)

def array(f,name,data,ctype='unsigned char',width=24):
    f.write(f'static const {ctype} {name}[{len(data)}] __attribute__((aligned(4))) = {{\n')
    for j in range(0,len(data),width):f.write(','.join(str(x) for x in data[j:j+width])+',\n')
    f.write('};\n')
with (OUT/'assets.h').open('w') as f:
    f.write('/* Generated original artwork. tools/assets.py / art_overhaul.py. */\n')
    array(f,'palette',[(r>>3)|((g>>3)<<5)|((b>>3)<<10) for r,g,b in RGB],'unsigned short')
    array(f,'creature_art',b''.join(im.tobytes() for im in creatures))
    array(f,'creature_back_art',b''.join(im.tobytes() for im in creature_backs))
    array(f,'creature_field_art',b''.join(im.tobytes() for im in creature_fields))
    array(f,'people_art',b''.join(im.tobytes() for im in people))
    array(f,'font_art',font)
    array(f,'map_tiles',[v for grid in maps for row in grid for v in row])
    array(f,'map_art',b''.join(im.tobytes() for im in map_images))
    array(f,'map_scene_art',b''.join(im.tobytes() for im in map_scenes))
    array(f,'map_scene_depth',b''.join(im.tobytes() for im in map_depths))
    array(f,'title_art',title.tobytes());array(f,'battle_art',battle.tobytes())
    offsets=[];offset=0
    for im in scenery:offsets.append(offset);offset+=im.width*im.height
    array(f,'scenery_art',b''.join(im.tobytes() for im in scenery))
    array(f,'scenery_offsets',offsets,'unsigned int')
    array(f,'scenery_widths',[im.width for im in scenery],'unsigned short')
    array(f,'scenery_heights',[im.height for im in scenery],'unsigned short')
    f.write('typedef struct {short x,y,depth; unsigned short kind;} SceneObject;\n')
    f.write('static const SceneObject scene_objects[] = {\n')
    for obj in scene_objects:f.write('{'+','.join(map(str,obj))+'},\n')
    f.write('};\n');array(f,'scene_starts',scene_starts,'unsigned short')

# Developer previews: composed world, ground-only layers and original sprite art.
for m,im in enumerate(map_images):
    im.convert('RGB').save(OUT/f'ground-{m}.png')
    map_scenes[m].convert('RGB').save(OUT/f'world-{m}.png')
    map_depths[m].point(lambda v:v*10).save(OUT/f'world-depth-{m}.png')
title.convert('RGB').save(OUT/'title-art.png');battle.convert('RGB').save(OUT/'battle-art.png')
sheet=image(384,200,2)
for n in range(6):
    paste(sheet,creatures[n].resize((64,64),Image.Resampling.NEAREST),(n*64,0))
    paste(sheet,creature_backs[n].resize((64,64),Image.Resampling.NEAREST),(n*64,67))
    paste(sheet,creature_fields[n].resize((32,40),Image.Resampling.NEAREST),(n*64+16,140))
sheet.convert('RGB').save(OUT/'creatures.png')
prop_sheet=image(480,128,2)
for i,im in enumerate(scenery):
    if i<6:paste(prop_sheet,im,(i*80,0))
    else:paste(prop_sheet,im,((i-6)*64,90))
prop_sheet.convert('RGB').save(OUT/'scenery.png')
people_sheet=image(320,192,2)
for i,im in enumerate(people):paste(people_sheet,im.resize((32,48),Image.Resampling.NEAREST),((i%10)*32,(i//10)*48))
people_sheet.convert('RGB').save(OUT/'people.png')
print(f'Generated {len(RGB)} colors, six creatures, forty people frames, {len(scene_objects)} depth-sorted scenery objects.')
