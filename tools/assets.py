"""Original indexed pixel artwork and a tiny hand-drawn font, generated reproducibly."""
from pathlib import Path
from PIL import Image, ImageDraw
import random

OUT = Path(__file__).resolve().parents[1] / 'build'
OUT.mkdir(exist_ok=True)
HEX = ['000000','263b40','f4ecd6','ffffff','487357','79a66a','a9c77c','d3c99b',
       'b09b77','867857','659fa1','86bfba','416b7c','dce8cf','d9a75b','b87848',
       '8b5340','e8c091','f2d4aa','c06848','e39254','f2bd6d','f9dfa0','593c48',
       '6f9dba','9ccbd0','cadfda','cd8996','e6acb1','5b865e','96ba76','c3d69a',
       '677277','919592','b9b7a3','d4cdb2','c7a65d','f2dc87','f8eabc','485b62',
       '355e57','c6d8b8','375e48','edcf98','aa6f68','d59d7b','789088','bbd6ca',
       'bd8e64','e2bda0','ba6f4f','ead3b0','edaa72','6b7b59','344c51','8fbc95']
RGB = [tuple(bytes.fromhex(h)) for h in HEX]
PAL = sum((list(c) for c in RGB), []) + [0] * (768-len(RGB)*3)
def image(w,h,fill=0):
    im=Image.new('P',(w,h),fill); im.putpalette(PAL); return im
def outline(d,box,fill,edge=1): d.ellipse(box,fill=fill,outline=edge)

tiles=[]
for k in range(16):
    im=image(16,16,5); d=ImageDraw.Draw(im); rng=random.Random(172+k)
    if k in (0,4,5,13):
        for _ in range(12):
            x,y=rng.randrange(16),rng.randrange(16);d.point((x,y),fill=6 if _%3 else 4)
    if k==1:
        d.rectangle((0,0,15,15),fill=7)
        for _ in range(9):d.point((rng.randrange(16),rng.randrange(16)),fill=8)
    if k==2:
        d.rectangle((0,0,15,15),fill=10)
        d.line((2,4,6,4),fill=11);d.line((10,10,14,10),fill=11);d.point((1,13),12)
    if k==3:
        d.rectangle((6,8,9,15),fill=16)
        d.polygon([(8,0),(14,5),(12,5),(15,10),(13,13),(2,13),(0,10),(3,5),(1,5)],fill=42)
        d.polygon([(8,1),(12,5),(10,5),(13,9),(6,9),(4,7)],fill=4)
        d.line((3,11,11,11),fill=40);d.point((8,3),fill=6)
    if k==4:
        for x,y in [(4,5),(11,11)]:
            d.rectangle((x,y,x+1,y+3),fill=4);d.line((x-1,y,x+2,y),fill=28);d.point((x,y-1),fill=38)
    if k==5:
        for y in (5,11,15):
            for x in (1,5,10,14):d.line((x,y,x-1,y-3),fill=4);d.line((x,y,x+2,y-2),fill=6)
    if k==6:
        d.rectangle((0,0,15,15),fill=34)
        d.line((0,7,15,7),fill=32);d.line((7,0,7,6),fill=32);d.line((3,8,3,15),fill=32)
    if k==7:
        d.rectangle((0,0,15,15),fill=15)
        for y in range(1,16,4):d.line((0,y,15,y),fill=17);d.line((0,y+2,15,y+2),fill=16)
        d.rectangle((0,0,1,15),fill=16);d.rectangle((14,0,15,15),fill=16)
    if k==8:
        d.rectangle((0,0,15,15),fill=18);d.line((0,7,15,7),fill=17);d.line((7,0,7,6),fill=17)
    if k==9:
        d.rectangle((0,0,15,15),fill=19)
        for y in range(2,16,5):d.line((0,y,15,y),fill=16)
        d.line((7,0,7,15),fill=20)
    if k==10:
        d.rectangle((0,5,15,7),fill=17);d.rectangle((2,2,4,14),fill=15);d.rectangle((11,2,13,14),fill=15)
        d.line((0,8,15,8),fill=16)
    if k==11:
        d.rectangle((7,7,9,15),fill=16);d.rounded_rectangle((1,1,14,10),radius=1,fill=17,outline=16)
        d.line((4,4,11,4),fill=16);d.line((4,7,9,7),fill=16)
    if k==12:
        d.ellipse((0,4,15,15),fill=34,outline=32);d.ellipse((2,5,13,13),fill=10);d.line((5,8,10,8),fill=26)
    if k==13:
        d.rectangle((1,2,14,13),fill=16)
        for x in (3,8,12):d.line((x,3,x,12),fill=8);d.ellipse((x-1,3,x+2,6),fill=6)
    if k==14:
        d.rectangle((0,0,15,15),fill=7);d.rectangle((5,7,11,15),fill=16);d.polygon([(2,8),(8,1),(14,8)],fill=19)
    if k==15:
        d.rectangle((0,0,15,15),fill=5);d.ellipse((2,6,13,13),fill=32);d.ellipse((3,4,12,11),fill=34)
    tiles.append(im)

creatures=[]
for n in range(6):
    im=image(32,32);d=ImageDraw.Draw(im)
    if n==0: # Mossprig: leaf-eared woodland rabbit
        d.polygon([(10,15),(6,5),(9,1),(14,13)],fill=29,outline=1)
        d.polygon([(17,14),(21,1),(25,4),(23,17)],fill=30,outline=1)
        d.line((10,5,12,12),fill=31);d.line((22,5,20,13),fill=31)
        outline(d,(8,13,25,28),30);outline(d,(5,16,22,27),30)
        d.ellipse((9,21,19,27),fill=31);d.ellipse((5,26,12,29),fill=29);d.ellipse((19,26,26,29),fill=29)
        d.polygon([(24,21),(30,15),(29,25),(25,27)],fill=29,outline=1)
        d.rectangle((9,18,10,20),fill=1);d.rectangle((18,18,19,20),fill=1);d.point((9,18),fill=3)
        d.point((14,22),fill=16);d.line((11,24,15,24),fill=29)
    elif n==1: # Cindlet: ember fox
        d.polygon([(21,24),(28,24),(30,16),(27,18),(27,9),(23,15),(20,14)],fill=20,outline=1)
        d.polygon([(26,21),(28,17),(26,17),(25,13),(23,19)],fill=22)
        outline(d,(8,15,24,28),19)
        d.polygon([(5,19),(5,5),(12,10),(20,5),(23,18),(19,23),(10,24)],fill=20,outline=1)
        d.polygon([(7,8),(10,12),(7,13)],fill=23);d.polygon([(19,8),(21,14),(17,12)],fill=23)
        d.polygon([(6,18),(13,20),(22,17),(19,24),(10,25)],fill=22)
        d.rectangle((9,15,10,17),fill=1);d.rectangle((18,15,19,17),fill=1);d.point((14,20),fill=1)
        d.rectangle((9,27,13,29),fill=16);d.rectangle((20,27,24,29),fill=16)
    elif n==2: # Rillip: river axolotl
        d.polygon([(21,26),(30,26),(30,18),(26,20),(24,14),(21,19)],fill=24,outline=1)
        outline(d,(7,15,24,28),24)
        for x,sgn in [(6,-1),(24,1)]:
            for y in (10,15,19):d.line((x,y,x+sgn*4,y-3),fill=27,width=2)
        outline(d,(5,8,25,23),25)
        d.ellipse((9,18,20,25),fill=26);d.rectangle((9,13,10,16),fill=1);d.rectangle((20,13,21,16),fill=1)
        d.line((13,19,17,19),fill=12);d.point((10,13),fill=3)
        d.rectangle((6,26,11,28),fill=25);d.rectangle((20,26,25,28),fill=25)
    elif n==3: # Pebblit: stone-shell turtle
        outline(d,(4,12,27,27),32);d.polygon([(6,15),(12,8),(23,10),(28,19),(22,25),(7,25)],fill=33,outline=1)
        d.polygon([(12,10),(20,11),(23,18),(17,23),(9,19)],fill=34,outline=32)
        d.line((14,11,13,18,18,22),fill=35);d.ellipse((2,20,13,28),fill=35,outline=1)
        d.rectangle((5,27,10,29),fill=32);d.rectangle((22,26,27,29),fill=32)
        d.rectangle((5,22,6,24),fill=1);d.point((4,19),fill=6);d.line((14,8,19,7),fill=29)
    elif n==4: # Luma: lantern moth
        d.polygon([(14,16),(7,4),(1,5),(1,15),(9,21),(3,24),(5,29),(14,23)],fill=36,outline=1)
        d.polygon([(18,16),(25,4),(30,5),(30,15),(23,21),(29,24),(27,29),(18,23)],fill=37,outline=1)
        d.ellipse((3,8,10,16),fill=38);d.ellipse((22,8,28,16),fill=38)
        d.ellipse((6,23,10,26),fill=20);d.ellipse((23,23,26,26),fill=20)
        outline(d,(13,12,19,27),23);outline(d,(12,10,20,17),37)
        d.line((14,11,11,5),fill=1);d.line((18,11,21,5),fill=1)
        d.point((14,13),fill=1);d.point((18,13),fill=1);d.ellipse((14,20,18,24),fill=38)
    else: # Burrbit: bramble hedgehog
        d.polygon([(4,23),(2,16),(7,17),(5,8),(11,12),(13,5),(17,11),(23,7),(23,14),(29,14),(26,21),(30,24),(23,28),(9,28)],fill=29,outline=1)
        d.polygon([(7,18),(10,13),(14,15),(18,10),(20,17),(25,16),(23,23),(10,25)],fill=30)
        outline(d,(4,19,20,28),18);d.rectangle((5,27,10,29),fill=16);d.rectangle((20,27,25,29),fill=16)
        d.rectangle((8,21,9,23),fill=1);d.rectangle((16,21,17,23),fill=1);d.point((4,24),fill=1)
    creatures.append(im)

people=[]
for n in range(5):
    for facing in range(4):
        for frame in range(2):
            im=image(16,24);d=ImageDraw.Draw(im)
            shirt=[24,29,27,36,19][n]
            d.ellipse((3,20,13,23),fill=4)
            d.rectangle((4,16,7,21-frame),fill=1);d.rectangle((9,16,12,20+frame),fill=1)
            d.rectangle((3,10,12,17),fill=shirt);d.rectangle((2,12,4,16),fill=18);d.rectangle((12,12,14,16),fill=18)
            d.rectangle((4,3,12,10),fill=18);d.rectangle((3,2,12,5),fill=23)
            if n==0:
                d.rectangle((2,1,12,4),fill=19);d.rectangle((6,0,12,2),fill=20)
                d.rectangle((2,5,4,8),fill=23);d.rectangle((4,11,7,14),fill=43)
            if facing==0:d.point((6,7),fill=1);d.point((10,7),fill=1)
            elif facing==1:d.point((4,7),fill=1);d.rectangle((10,4,12,9),fill=23)
            elif facing==2:d.point((12,7),fill=1);d.rectangle((3,4,5,9),fill=23)
            else:d.rectangle((3,4,12,8),fill=23)
            people.append(im)

building=image(64,64);d=ImageDraw.Draw(building)
d.rectangle((6,22,57,59),fill=18,outline=1);d.rectangle((9,40,54,59),fill=17)
d.polygon([(1,26),(12,7),(48,7),(62,26)],fill=16,outline=1)
for y in (11,16,21):d.line((10-(y-11)//2,y,50+(y-11)//2,y),fill=19,width=3)
d.rectangle((27,38,39,60),fill=16);d.rectangle((29,40,37,59),fill=1);d.point((35,49),fill=37)
for x in (12,44):d.rectangle((x,32,x+9,42),fill=12,outline=16);d.line((x+1,33,x+1,39),fill=25);d.line((x+5,33,x+5,41),fill=17)
d.rectangle((23,25,42,32),fill=2,outline=16);d.line((29,28,36,28),fill=29);d.line((32,26,32,31),fill=29)

# Precompose static scenery so the GBA spends its time on actors and input.
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
map_images=[]
for m,grid in enumerate(maps):
    im=image(512,384)
    for y,row in enumerate(grid):
        for x,t in enumerate(row):im.paste(tiles[t],(x*16,y*16))
    if m==0:
        for x,y in [(4,6),(22,5),(25,17)]:im.paste(building,(x*16,y*16-6),building.point(lambda v:255 if v else 0).convert('1'))
    map_images.append(im)
title=image(240,160,40);td=ImageDraw.Draw(title);td.rectangle((0,96,239,159),fill=4)
for i in range(12):
    tree=tiles[3].resize((48,48),Image.Resampling.NEAREST)
    # Grass under the tree is deliberately part of the silhouette.
    title.paste(tree,((i*43+11)%260-10,80+(i%3)*8))

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
    font.extend(int(v.ljust(5,'0'),2) for v in rows)

def array(f,name,data,ctype='unsigned char',width=24):
    f.write(f'static const {ctype} {name}[{len(data)}] = {{\n')
    for j in range(0,len(data),width):f.write(','.join(str(x) for x in data[j:j+width])+',\n')
    f.write('};\n')
with (OUT/'assets.h').open('w') as f:
    f.write('/* Generated original artwork. Run tools/assets.py to rebuild. */\n')
    array(f,'palette',[(r>>3)|((g>>3)<<5)|((b>>3)<<10) for r,g,b in RGB],'unsigned short')
    array(f,'creature_art',sum((list(im.tobytes()) for im in creatures),[]))
    array(f,'people_art',sum((list(im.tobytes()) for im in people),[]))
    array(f,'font_art',font)
    array(f,'map_tiles',[v for grid in maps for row in grid for v in row])
    array(f,'map_art',sum((list(im.tobytes()) for im in map_images),[]))
    array(f,'title_art',list(title.tobytes()))
sheet=image(256,128,2)
for n,im in enumerate(creatures):sheet.paste(im.resize((64,64),Image.Resampling.NEAREST),(n%4*64,n//4*64),im.resize((64,64),Image.Resampling.NEAREST).point(lambda x:255 if x else 0).convert('1'))
sheet.convert('RGB').save(OUT/'creatures.png')
print('Generated palette, original sprites, tiles and font.')
