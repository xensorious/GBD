"""Original Bramblebound pixel artwork. Lighting comes from upper left.

No external source graphics: silhouettes, color ramps and details are hand drawn
as reproducible indexed pixels. See assets.py for export and collision maps.
"""
from PIL import Image, ImageDraw

HEX = ['000000','183b40','fff2d3','ffffff','39765a','79bd66','b4df78','e8cf8f',
       'c3a16d','95764e','379eaf','75d2ce','28647b','e4efc8','e8b156','c1854d',
       '75483b','e9bc80','ffe0b2','c55d3d','f08b45','ffc46c','fff0b3','47394d',
       '4b9fbe','91e0df','def8e9','dc7899','ffb1b2','347b58','78bd65','c1e990',
       '536b76','8fa2a3','c4cbc0','e7e1bc','c48c3f','ffdb70','fff4bb','314d5c',
       '215b51','d8edb8','245b48','f3ce89','ab626b','e0a784','739d91','c2e7d6',
       'b7794e','f1c7a1','a95d3b','f8dfb0','ffa66b','637c50','234853','a0d0a1',
       '172c40','24485b','278b91','63d7cc','fff2cf','fff9e8','f5bf5b','b8743b',
       'd1e9eb','607988','9fb9ba','e26d68','eadab3','28536b','84cbdc','102a39',
       '112532','f2abc2']

def color(h):
    h=h.lower().lstrip('#')
    if h not in HEX: HEX.append(h)
    return HEX.index(h)

GRASS_DARK=color('569b5c'); GRASS_LIT=color('95ce72'); GRASS_DEEP=color('417b56')
GRASS_SUN=color('c6e387'); GRASS_SHADOW=color('609b5a')
LEAF_EDGE=color('214b42'); LEAF_DARK=color('2c6950'); LEAF_MID=color('438c53')
LEAF_LIGHT=color('69af58'); LEAF_SUN=color('9bd069'); LEAF_GLOW=color('c9e482')
BARK_DARK=color('624b3e'); BARK_MID=color('946346'); BARK_LIGHT=color('c59157')
SAND_DARK=color('c0a06b'); SAND_LIGHT=color('f5dfa7'); SAND_GLOW=color('fff0c1')
WATER_DARK=color('247991'); WATER_MID=color('318fa4'); WATER_LIGHT=color('66c6cd')
WATER_GLINT=color('c4f2df'); FOAM=color('e2f4c3'); BANK=color('6d9d65')
STONE_DARK=color('526c73'); STONE_MID=color('8ba29d'); STONE_LIGHT=color('b7c7b2')
STONE_TOP=color('e0e2c3'); ROOF_DARK=color('923f37'); ROOF_MID=color('c45c41')
ROOF_LIGHT=color('e7834d'); ROOF_SUN=color('ffaf69'); WALL_SHADE=color('cbac77')
WALL_LIGHT=color('f7e2ae'); WINDOW_DARK=color('274b66'); WINDOW_BLUE=color('5b9eb6')
WINDOW_LIGHT=color('a5e0df'); SKIN_SHADOW=color('cc8e6f'); SKIN_LIGHT=color('ffddb2')
SKY_LOW=color('caeee3'); SKY_HIGH=color('88cad1'); HILL_FAR=color('a0ce9b')
HILL_MID=color('6eae83'); HILL_NEAR=color('3b7d66')
RGB=[tuple(bytes.fromhex(h)) for h in HEX]
assert len(RGB)<=256
PAL=sum((list(c) for c in RGB),[])+[0]*(768-len(RGB)*3)

def image(w,h,fill=0):
    im=Image.new('P',(w,h),fill);im.putpalette(PAL);return im
def paste(dst,src,xy):
    # Make the mask from index bytes, not palette luminance (transparent index0).
    mask=Image.frombytes('L',src.size,bytes(255 if v else 0 for v in src.tobytes()))
    dst.paste(src,xy,mask)
def poly(d,points,fill,edge=None):d.polygon(points,fill=fill,outline=edge)
def eye(d,x,y,wide=False):
    d.rectangle((x,y,x+(1 if wide else 0),y+2),fill=1);d.point((x,y),fill=3)

def draw_creature(n,back=False):
    """Six recognizable silhouettes, retaining the original species identities."""
    im=image(32,32);d=ImageDraw.Draw(im)
    if n==0: # Mossprig: leaf-eared woodland rabbit.
        poly(d,[(23,21),(28,18),(30,12),(31,20),(28,26),(23,27)],29,1)
        poly(d,[(27,23),(29,16),(29,23),(26,25)],30)
        poly(d,[(8,16),(4,7),(5,2),(8,1),(14,14)],29,1)
        poly(d,[(7,4),(11,14),(8,11),(6,6)],31)
        poly(d,[(17,14),(19,5),(24,1),(26,3),(25,10),(23,17)],29,1)
        poly(d,[(22,5),(24,3),(23,11),(20,15)],31);d.line((22,5,21,12),fill=6)
        d.ellipse((8,15,26,29),fill=29,outline=1)
        d.ellipse((7,12,25,26),fill=30,outline=1)
        poly(d,[(9,15),(13,13),(20,13),(22,15),(18,15),(16,17),(13,15)],31)
        d.ellipse((9,21,22,27),fill=31)
        poly(d,[(9,24),(11,26),(21,26),(23,24),(22,28),(10,28)],55)
        d.ellipse((5,26,13,30),fill=29,outline=1);d.line((7,27,11,27),fill=30)
        d.ellipse((19,26,27,30),fill=29,outline=1);d.line((21,27,24,27),fill=30)
        poly(d,[(4,20),(7,18),(8,22),(6,24)],29,1)
        poly(d,[(24,19),(27,21),(26,24),(23,23)],29,1)
        if back:
            d.ellipse((10,14,24,24),fill=29)
            poly(d,[(12,15),(16,13),(21,15),(20,19),(15,21)],30)
            d.ellipse((15,23,21,28),fill=31,outline=29)
        else:
            eye(d,10,18,True);eye(d,20,18,True)
            d.line((11,21,12,21),fill=28);d.point((22,21),fill=28)
            d.point((16,22),fill=1);d.line((15,24,17,24),fill=29);d.point((9,16),fill=31)
    elif n==1: # Cindlet: ember fox, cream muzzle, flame-shaped tail.
        poly(d,[(22,27),(27,26),(30,22),(31,14),(28,17),(28,8),(24,14),(22,13),(23,20),(19,23)],16,1)
        poly(d,[(24,24),(28,22),(29,17),(27,18),(26,13),(24,19),(23,19)],20)
        poly(d,[(25,23),(27,21),(26,18),(24,22)],22)
        d.ellipse((9,17,25,29),fill=19,outline=1)
        poly(d,[(10,17),(18,15),(22,19),(20,26),(15,28),(11,25)],20)
        poly(d,[(12,21),(18,21),(18,28),(13,27)],22)
        poly(d,[(5,18),(4,5),(6,3),(12,9),(17,8),(22,3),(24,5),(24,18),(21,23),(15,25),(8,23)],16,1)
        poly(d,[(6,8),(7,6),(10,11),(8,14)],23);poly(d,[(19,11),(22,6),(23,13)],23)
        poly(d,[(7,15),(10,10),(19,10),(22,14),(21,20),(16,23),(10,22)],20)
        poly(d,[(7,17),(11,19),(15,18),(19,19),(22,16),(20,22),(15,24),(9,22)],22)
        d.line((10,11,13,10),fill=21);d.point((8,14),fill=21)
        d.rectangle((8,27,13,30),fill=16);d.line((9,27,12,27),fill=20)
        d.rectangle((20,27,25,30),fill=16);d.line((21,27,24,27),fill=20)
        if back:
            poly(d,[(7,15),(10,10),(20,10),(22,15),(20,21),(15,23),(9,21)],19)
            poly(d,[(9,12),(13,10),(18,10),(15,13),(11,15)],20);d.line((13,19,17,19),fill=16)
        else:
            eye(d,10,15,True);eye(d,19,15,True)
            d.point((15,19),fill=1);d.line((14,21,16,21),fill=16);d.point((8,18),fill=52)
    elif n==2: # Rillip: river axolotl with coral branching gills.
        poly(d,[(21,25),(27,24),(31,18),(29,13),(27,18),(25,17),(24,21),(20,22)],12,1)
        poly(d,[(25,23),(28,21),(29,17),(27,20),(25,20)],25)
        d.ellipse((8,16,25,29),fill=24,outline=1);d.ellipse((11,20,22,28),fill=26)
        for x,sgn in [(5,-1),(26,1)]:
            for yy in (10,15,19):
                poly(d,[(x,yy+2),(x+sgn*4,yy),(x+sgn*4,yy-3),(x+sgn*2,yy-1),(x,yy)],44,1)
                d.line((x,yy,x+sgn*3,yy-2),fill=28)
        poly(d,[(6,10),(10,7),(21,7),(26,11),(26,18),(23,22),(9,22),(5,18),(5,13)],24,1)
        poly(d,[(8,11),(11,8),(20,8),(24,11),(24,17),(21,20),(10,20),(7,17)],25)
        poly(d,[(8,17),(13,18),(20,17),(24,17),(21,22),(10,22)],26)
        d.line((11,9,17,9),fill=26);d.point((9,11),fill=3)
        d.ellipse((5,26,12,30),fill=24,outline=1);d.line((7,27,10,27),fill=25)
        d.ellipse((20,26,27,30),fill=24,outline=1);d.line((22,27,25,27),fill=25)
        if back:
            d.ellipse((9,10,23,21),fill=24)
            poly(d,[(10,12),(13,9),(20,10),(22,13),(17,15)],25)
            d.point((15,18),fill=12);d.point((19,17),fill=12)
        else:
            eye(d,10,13,True);eye(d,21,13,True)
            d.line((14,18,17,18),fill=12);d.point((13,17),fill=12)
            d.line((8,17,9,17),fill=28);d.line((23,17,24,17),fill=28)
    elif n==3: # Pebblit: slate turtle carrying a little sprout.
        d.ellipse((6,23,13,30),fill=32,outline=1);d.ellipse((22,22,29,29),fill=32,outline=1)
        poly(d,[(8,17),(10,10),(17,7),(24,9),(29,16),(29,23),(24,27),(12,28),(7,23)],32,1)
        poly(d,[(10,15),(13,10),(19,9),(25,12),(27,18),(25,23),(19,26),(12,24),(9,20)],33)
        poly(d,[(13,13),(18,10),(23,12),(25,17),(20,21),(14,19)],34,32)
        poly(d,[(14,13),(18,11),(21,12),(22,14),(17,15)],35)
        d.line((10,17,14,19,13,24),fill=32);d.line((20,21,22,25),fill=32)
        d.line((25,17,28,18),fill=32)
        d.ellipse((2,19,15,28),fill=34,outline=1)
        poly(d,[(4,20),(9,19),(12,22),(11,25),(4,25)],35)
        d.rectangle((4,27,10,30),fill=32);d.line((5,27,9,27),fill=34)
        poly(d,[(17,9),(17,5),(14,4),(12,1),(16,2),(18,5),(21,1),(24,1),(22,5),(19,6),(19,9)],29,1)
        d.line((14,2,17,5),fill=31);d.line((22,2,19,5),fill=30)
        if back:d.ellipse((4,20,13,26),fill=33);d.point((25,21),fill=35)
        else:eye(d,5,21,True);d.point((3,25),fill=32)
    elif n==4: # Luma: four-wing lantern moth with jewel-like wing eyes.
        poly(d,[(14,17),(10,5),(5,2),(1,4),(1,13),(6,20),(11,22),(4,23),(3,27),(7,31),(14,25)],36,1)
        poly(d,[(18,17),(22,5),(27,2),(31,4),(31,13),(26,20),(21,22),(28,23),(29,27),(25,31),(18,25)],36,1)
        poly(d,[(3,6),(6,4),(10,7),(12,15),(9,18),(5,15),(3,11)],37)
        poly(d,[(29,6),(26,4),(22,7),(20,15),(23,18),(27,15),(29,11)],37)
        d.ellipse((5,9,10,15),fill=19);d.ellipse((23,9,28,15),fill=19)
        d.ellipse((6,10,9,13),fill=38);d.ellipse((24,10,27,13),fill=38)
        d.point((7,10),fill=3);d.point((25,10),fill=3)
        poly(d,[(6,25),(12,23),(11,27),(7,29),(5,27)],20)
        poly(d,[(26,25),(20,23),(21,27),(25,29),(27,27)],20)
        d.ellipse((13,13,20,28),fill=23,outline=1)
        d.ellipse((14,18,19,25),fill=37);d.ellipse((15,19,18,23),fill=38)
        d.ellipse((12,10,21,17),fill=37,outline=1)
        d.line((14,11,11,5,10,5),fill=1);d.line((19,11,22,5,23,5),fill=1)
        d.point((10,4),fill=37);d.point((23,4),fill=37)
        if back:d.ellipse((14,11,19,16),fill=36);d.line((16,18,16,25),fill=38)
        else:eye(d,14,12);eye(d,19,12);d.point((17,15),fill=22)
    else: # Burrbit: warm face, green bramble spines and a berry.
        poly(d,[(4,24),(2,17),(5,17),(3,11),(8,13),(8,6),(13,10),(16,3),(19,9),(25,6),(24,13),(30,12),(27,19),(31,24),(25,28),(10,29)],29,1)
        poly(d,[(6,18),(7,14),(11,16),(11,10),(15,13),(17,8),(20,13),(24,10),(23,17),(27,16),(25,22),(28,24),(21,26),(11,26)],30)
        poly(d,[(12,15),(13,11),(16,14),(18,10),(18,16),(22,14),(21,20),(15,22)],31)
        d.line((8,19,10,23),fill=29);d.line((22,20,21,24),fill=29)
        d.ellipse((5,18,21,29),fill=48,outline=1)
        poly(d,[(7,21),(11,19),(17,20),(20,23),(18,27),(9,28),(5,25)],18)
        d.ellipse((4,26,10,30),fill=16,outline=1);d.line((6,27,8,27),fill=48)
        d.ellipse((20,26,27,30),fill=16,outline=1);d.line((22,27,25,27),fill=48)
        d.ellipse((22,12,25,15),fill=44,outline=1);d.point((23,12),fill=28)
        if back:
            poly(d,[(7,21),(13,17),(21,21),(20,27),(9,27)],29)
            poly(d,[(10,20),(13,18),(17,20),(14,23)],30)
        else:
            eye(d,9,22,True);eye(d,18,22);d.rectangle((4,24,5,25),fill=1)
            d.line((10,26,13,26),fill=16);d.point((8,24),fill=49)
    return im

creatures=[draw_creature(n) for n in range(6)]
creature_backs=[draw_creature(n,True) for n in range(6)]

def field_creature(n):
    """Purpose drawn 16x20 sprites, rather than aliased battle downsampling."""
    im=image(16,20);d=ImageDraw.Draw(im)
    if n==0:
        poly(d,[(4,9),(2,2),(4,1),(7,8)],29,1);poly(d,[(9,8),(11,1),(13,2),(12,10)],30,1)
        d.ellipse((3,7,13,17),fill=30,outline=1);d.ellipse((5,12,11,17),fill=31)
        d.rectangle((3,16,6,18),fill=29);d.rectangle((10,16,13,18),fill=29)
        d.point((5,11),fill=1);d.point((11,11),fill=1);d.point((8,14),fill=1)
    elif n==1:
        poly(d,[(11,15),(15,12),(15,6),(12,10),(10,10)],20,1)
        d.ellipse((4,10,12,18),fill=19,outline=1)
        poly(d,[(2,11),(2,3),(6,6),(10,5),(12,2),(13,11),(11,14),(4,14)],20,1)
        poly(d,[(3,11),(7,12),(12,10),(10,14),(6,14)],22)
        d.point((5,9),fill=1);d.point((10,9),fill=1);d.point((8,12),fill=1)
        d.rectangle((4,17,6,18),fill=16);d.rectangle((10,17,12,18),fill=16)
    elif n==2:
        for x,s in [(2,-1),(13,1)]:
            for y in (6,9,12):d.line((x,y,x+s*2,y-2),fill=27)
        d.ellipse((4,10,12,18),fill=24,outline=1);d.ellipse((2,5,13,14),fill=25,outline=1)
        d.line((5,12,10,12),fill=26);d.point((5,9),fill=1);d.point((10,9),fill=1)
        d.rectangle((3,16,5,18),fill=24);d.rectangle((10,16,12,18),fill=24)
    elif n==3:
        d.ellipse((4,7,15,17),fill=32,outline=1);poly(d,[(6,8),(11,6),(14,10),(12,15),(7,15)],34,32)
        d.ellipse((0,12,7,17),fill=35,outline=1);d.point((2,14),fill=1)
        d.line((9,7,9,3),fill=29);d.line((9,4,6,2),fill=30);d.line((9,5,12,2),fill=30)
        d.rectangle((3,17,5,18),fill=32);d.rectangle((12,16,14,18),fill=32)
    elif n==4:
        poly(d,[(7,9),(3,2),(0,3),(0,9),(4,12),(1,15),(3,18),(7,14)],37,1)
        poly(d,[(9,9),(12,2),(15,3),(15,9),(12,12),(15,15),(13,18),(9,14)],37,1)
        d.rectangle((7,7,9,16),fill=23);d.ellipse((6,5,10,9),fill=37,outline=1)
        d.point((3,7),fill=20);d.point((12,7),fill=20);d.line((8,12,8,14),fill=38)
    else:
        poly(d,[(2,15),(0,10),(3,10),(2,5),(6,7),(8,2),(10,7),(14,5),(13,10),(15,12),(14,17),(4,18)],29,1)
        poly(d,[(4,10),(5,7),(8,9),(9,5),(11,10),(13,9),(12,15),(6,16)],30)
        d.ellipse((1,11,10,17),fill=18,outline=1);d.point((4,13),fill=1);d.point((8,13),fill=1)
        d.point((1,15),fill=1);d.rectangle((3,17,5,18),fill=16);d.rectangle((11,17,13,18),fill=16)
    return im
creature_fields=[field_creature(n) for n in range(6)]

def draw_person(n,facing,frame):
    """Five original people, four facings and two original-size walk frames."""
    im=image(16,24);d=ImageDraw.Draw(im)
    shirt=[24,29,27,36,19][n];light=[25,30,28,37,20][n];dark=[12,42,44,15,16][n]
    skin=18 if n!=3 else 49;skinlight=SKIN_LIGHT if n!=3 else 18
    hair=[23,BARK_DARK,16,23,23][n]
    left=22-frame;right=21+frame
    d.rectangle((4,17,7,left),fill=39);d.rectangle((9,17,12,right),fill=39)
    d.rectangle((4,left,7,left+1),fill=1);d.rectangle((9,right,12,right+1),fill=1)
    d.point((4,left),fill=33);d.point((9,right),fill=33)
    poly(d,[(4,10),(11,10),(13,13),(12,18),(4,18),(3,14)],dark,1)
    d.rectangle((5,11,11,16),fill=shirt);d.line((5,11,9,11),fill=light)
    d.rectangle((3,13,4,16-frame),fill=skin);d.rectangle((12,13+frame,13,16),fill=skin)
    d.point((3,13),fill=skinlight);d.rectangle((5,9,10,11),fill=skin)
    poly(d,[(4,3),(6,1),(10,1),(12,3),(12,8),(10,10),(6,10),(4,8)],hair,1)
    d.rectangle((5,4,11,8),fill=skin);d.line((6,4,9,4),fill=skinlight)
    d.point((5,7),fill=skinlight);d.line((7,9,9,9),fill=SKIN_SHADOW)
    d.rectangle((4,2,11,4),fill=hair);d.point((5,2),fill=16)
    if n==0: # Traveller: coral cap, blue coat, cream satchel.
        poly(d,[(3,3),(4,1),(11,1),(12,3),(12,5),(3,5)],19,1)
        d.line((5,2,10,2),fill=20);d.line((3,5,13,5),fill=21)
        d.line((6,11,10,16),fill=43);d.rectangle((9,14,12,17),fill=43);d.line((10,15,11,15),fill=48)
    elif n==1: # Mara: white bun, green field coat and gold clasp.
        d.ellipse((8,0,12,3),fill=35,outline=1);d.rectangle((4,2,11,4),fill=35)
        d.line((5,2,9,2),fill=3);d.point((8,12),fill=37);d.line((8,13,8,17),fill=42)
    elif n==2: # Eli: scarf, pink coat, long brown hair.
        d.rectangle((3,4,4,11),fill=hair);d.rectangle((11,5,12,11),fill=hair)
        d.line((5,11,11,11),fill=2);d.rectangle((10,12,11,15),fill=2);d.point((6,3),fill=52)
    elif n==3: # Nell: straw gardening hat and apron.
        poly(d,[(3,3),(5,1),(10,1),(12,3),(12,5),(3,5)],37,1)
        d.line((2,5,13,5),fill=36);d.line((5,2,9,2),fill=38)
        d.rectangle((6,12,10,17),fill=2);d.line((7,14,9,14),fill=43)
    else: # Iris/Tavi: dark hair, red jacket, turquoise kerchief.
        d.rectangle((3,3,5,7),fill=hair);d.line((5,2,9,2),fill=39)
        d.line((5,11,11,11),fill=25);d.rectangle((10,12,11,14),fill=24)
    if facing==0:
        d.point((6,7),fill=1);d.point((10,7),fill=1);d.point((6,6),fill=skinlight)
    elif facing==1:
        d.rectangle((10,4,12,8),fill=hair);d.point((4,7),fill=skin)
        d.point((5,7),fill=1);d.line((10,12,10,17),fill=dark)
    elif facing==2:
        d.rectangle((3,4,5,8),fill=hair);d.point((12,7),fill=skin)
        d.point((11,7),fill=1);d.line((5,12,5,17),fill=dark)
    else:
        d.rectangle((4,5,11,9),fill=hair);d.line((5,6,10,6),fill=16 if n!=1 else 34)
        d.rectangle((5,11,11,16),fill=shirt)
        if n==0:
            d.line((5,12,10,12),fill=43);d.rectangle((5,13,11,17),fill=43)
            d.rectangle((6,14,10,16),fill=48);d.point((8,14),fill=37)
        elif n==1:d.line((7,12,7,17),fill=42)
    return im
people=[draw_person(n,facing,frame) for n in range(5) for facing in range(4) for frame in range(2)]

def draw_tree(variant):
    im=image(32,48);d=ImageDraw.Draw(im)
    d.ellipse((7,41,27,47),fill=GRASS_SHADOW)
    poly(d,[(12,27),(20,27),(20,42),(24,46),(16,45),(9,47),(12,41)],BARK_DARK,1)
    poly(d,[(13,29),(16,29),(16,43),(12,45),(14,39)],BARK_LIGHT)
    d.line((18,31,18,42),fill=BARK_MID);d.line((16,37,20,35),fill=BARK_DARK)
    if variant==2:
        poly(d,[(16,0),(23,10),(21,11),(28,20),(25,21),(31,31),(27,36),(5,36),(1,32),(7,22),(4,22),(11,12),(9,12)],LEAF_EDGE)
        poly(d,[(16,2),(21,10),(18,11),(24,20),(21,21),(27,29),(23,33),(8,33),(6,30),(13,22),(10,22),(15,13),(12,13)],LEAF_DARK)
        poly(d,[(16,3),(18,9),(16,10),(19,16),(15,16),(18,21),(13,22),(17,28),(10,28),(9,30),(7,30),(12,22),(10,21),(15,13),(13,12)],LEAF_MID)
        d.line((14,14,16,13),fill=LEAF_LIGHT);d.line((11,24,15,24),fill=LEAF_LIGHT)
        return im
    poly(d,[(13,0),(20,1),(23,4),(27,4),(28,8),(30,10),(29,16),(31,20),(30,27),(26,30),(26,33),(21,35),(15,34),(9,36),(5,32),(2,30),(1,24),(0,20),(3,15),(2,10),(6,7),(7,3)],LEAF_EDGE)
    poly(d,[(13,2),(19,3),(22,6),(26,6),(27,10),(28,11),(27,16),(29,21),(28,26),(24,28),(23,31),(18,32),(14,31),(9,33),(6,29),(4,29),(3,24),(2,20),(5,16),(4,11),(8,9),(9,5)],LEAF_DARK)
    poly(d,[(11,4),(18,3),(22,7),(25,8),(25,14),(22,17),(26,20),(24,25),(20,27),(14,25),(10,28),(6,26),(5,21),(7,17),(5,13),(9,10)],LEAF_MID)
    poly(d,[(11,5),(16,4),(20,6),(22,10),(20,13),(15,12),(12,16),(8,15),(7,12),(10,10)],LEAF_LIGHT)
    poly(d,[(10,6),(14,5),(17,6),(16,8),(12,8),(11,11),(8,12),(9,9)],LEAF_SUN)
    poly(d,[(9,19),(13,16),(17,17),(19,20),(17,22),(12,22),(11,24),(7,23)],LEAF_LIGHT)
    poly(d,[(9,19),(12,18),(14,18),(13,20),(9,21)],LEAF_SUN)
    d.line((18,28,22,28),fill=LEAF_MID);d.line((24,13,26,12),fill=LEAF_LIGHT)
    d.point((11,5),fill=LEAF_GLOW);d.point((7,18),fill=LEAF_SUN)
    if variant==1:
        for x,y in [(22,16),(12,26),(25,25)]:
            d.ellipse((x-1,y-1,x+1,y+1),fill=44);d.point((x-1,y-1),fill=28)
    return im

def draw_house(variant):
    im=image(80,88);d=ImageDraw.Draw(im)
    d.ellipse((6,77,76,87),fill=GRASS_SHADOW)
    d.rectangle((10,37,69,81),fill=BARK_DARK,outline=1)
    d.rectangle((12,39,67,76),fill=WALL_SHADE);d.rectangle((13,40,63,70),fill=WALL_LIGHT)
    d.rectangle((12,73,67,79),fill=STONE_MID);d.line((12,73,67,73),fill=STONE_TOP)
    for x in (13,27,42,57):d.line((x,75,x,79),fill=STONE_DARK)
    d.line((12,80,67,80),fill=STONE_DARK)
    d.rectangle((12,40,15,73),fill=BARK_MID);d.line((13,42,13,72),fill=BARK_LIGHT)
    d.rectangle((64,40,67,73),fill=BARK_DARK)
    d.line((16,67,63,67),fill=WALL_SHADE);d.line((16,69,63,69),fill=17)
    d.rectangle((56,4,64,23),fill=STONE_DARK,outline=1)
    d.rectangle((57,5,62,21),fill=STONE_MID);d.line((56,5,64,5),fill=STONE_TOP)
    d.line((58,10,62,10),fill=STONE_LIGHT);d.point((61,14),fill=STONE_DARK)
    rd=ROOF_DARK if variant!=2 else 12;rm=ROOF_MID if variant!=2 else 24
    rl=ROOF_LIGHT if variant!=2 else 10;rs=ROOF_SUN if variant!=2 else 25
    poly(d,[(3,39),(17,11),(61,11),(77,39),(75,43),(4,43)],rd,1)
    poly(d,[(6,37),(19,13),(59,13),(73,37)],rm)
    for yy in (17,23,29,35):
        left=18-(yy-13)//2;right=60+(yy-13)//2
        d.line((left,yy,right,yy),fill=rl,width=2);d.line((left-1,yy+2,right+1,yy+2),fill=rd)
        for xx in range(left+5+(yy//6%2)*4,right-2,9):d.line((xx,yy-2,xx+2,yy+1),fill=rd)
    d.line((19,12,59,12),fill=rs);d.line((5,38,74,38),fill=rl)
    d.line((4,41,75,41),fill=rd);d.line((7,43,72,43),fill=1)
    d.rectangle((32,51,49,79),fill=BARK_DARK,outline=1)
    d.rectangle((34,54,47,78),fill=16);d.rectangle((35,55,46,75),fill=BARK_MID)
    d.line((37,57,37,73),fill=BARK_LIGHT);d.line((43,57,43,74),fill=BARK_DARK)
    d.rectangle((37,56,44,61),fill=WINDOW_DARK);d.line((38,56,42,56),fill=WINDOW_BLUE)
    d.point((45,68),fill=37)
    d.rectangle((30,78,51,81),fill=STONE_DARK);d.line((31,78,50,78),fill=STONE_TOP)
    d.rectangle((29,82,53,85),fill=STONE_MID);d.line((30,82,52,82),fill=STONE_TOP)
    for x in (18,53):
        d.rectangle((x,49,x+10,63),fill=BARK_DARK,outline=1)
        d.rectangle((x+1,50,x+9,61),fill=WINDOW_DARK)
        d.rectangle((x+2,51,x+5,56),fill=WINDOW_BLUE);d.line((x+2,51,x+5,51),fill=WINDOW_LIGHT)
        d.line((x+5,50,x+5,61),fill=17);d.line((x+1,56,x+9,56),fill=17)
        d.line((x-1,64,x+11,64),fill=BARK_DARK);d.line((x,63,x+10,63),fill=BARK_LIGHT)
        d.rectangle((x-2,50,x-1,61),fill=rm);d.rectangle((x+11,50,x+12,61),fill=rm)
    d.rectangle((33,44,47,49),fill=2,outline=BARK_DARK)
    poly(d,[(38,47),(38,45),(42,45),(43,46),(40,48)],29);d.point((40,46),fill=31)
    for x in (19,55):
        d.rectangle((x-1,65,x+7,69),fill=16);d.line((x,65,x+6,65),fill=BARK_LIGHT)
        for xx in (x,x+4):
            d.line((xx+1,64,xx+1,61),fill=29)
            d.line((xx,61,xx+2,61),fill=28 if variant!=1 else 37)
            d.point((xx+1,60),fill=3 if variant!=1 else 38)
    return im

def draw_sign():
    im=image(24,28);d=ImageDraw.Draw(im)
    d.ellipse((5,24,18,27),fill=GRASS_SHADOW)
    d.rectangle((9,13,12,26),fill=BARK_DARK,outline=1);d.line((10,17,10,25),fill=BARK_LIGHT)
    poly(d,[(2,3),(18,3),(20,5),(20,16),(2,16),(1,14),(1,5)],BARK_DARK,1)
    d.rectangle((3,4,18,13),fill=17);d.line((3,4,17,4),fill=43)
    d.line((5,7,15,7),fill=BARK_DARK);d.line((5,10,12,10),fill=BARK_MID)
    d.point((3,6),fill=48);d.point((18,11),fill=48)
    return im

def draw_fence():
    im=image(16,24);d=ImageDraw.Draw(im)
    d.rectangle((0,9,15,12),fill=BARK_DARK);d.line((0,9,15,9),fill=17)
    d.rectangle((0,17,15,19),fill=BARK_DARK);d.line((0,17,15,17),fill=BARK_MID)
    for x in (2,11):
        poly(d,[(x,3),(x+2,1),(x+4,3),(x+4,22),(x,22)],BARK_DARK,1)
        d.rectangle((x+1,4,x+3,20),fill=BARK_MID);d.line((x+1,4,x+1,19),fill=17)
    return im

def draw_rock():
    im=image(20,22);d=ImageDraw.Draw(im)
    d.ellipse((0,16,19,21),fill=GRASS_SHADOW)
    poly(d,[(1,17),(2,9),(7,4),(14,4),(18,10),(19,17),(15,20),(5,20)],STONE_DARK,1)
    poly(d,[(3,14),(4,9),(8,5),(13,5),(16,10),(14,15),(8,17)],STONE_MID)
    poly(d,[(5,9),(8,6),(12,6),(13,9),(10,10),(7,13),(4,13)],STONE_LIGHT)
    d.line((6,8,10,7),fill=STONE_TOP);d.line((13,11,15,13),fill=STONE_DARK)
    d.point((3,17),fill=29);d.point((4,16),fill=30)
    return im

def draw_well():
    im=image(28,34);d=ImageDraw.Draw(im)
    d.ellipse((0,25,25,33),fill=GRASS_SHADOW)
    d.ellipse((1,16,24,31),fill=STONE_DARK,outline=1);d.rectangle((2,20,23,26),fill=STONE_MID)
    d.ellipse((1,14,24,25),fill=STONE_LIGHT,outline=1)
    d.ellipse((5,16,20,22),fill=WINDOW_DARK);d.line((8,18,15,18),fill=25)
    d.line((5,26,20,26),fill=STONE_DARK);d.line((8,25,8,29),fill=STONE_DARK)
    d.rectangle((4,7,6,20),fill=BARK_DARK);d.line((4,7,4,18),fill=BARK_LIGHT)
    d.rectangle((19,7,21,20),fill=BARK_DARK)
    poly(d,[(0,11),(8,2),(18,2),(25,11),(24,13),(1,13)],ROOF_DARK,1)
    poly(d,[(3,10),(9,3),(17,3),(23,10)],ROOF_MID);d.line((7,6,19,6),fill=ROOF_LIGHT)
    d.line((2,11,23,11),fill=ROOF_LIGHT)
    return im

def draw_bed():
    im=image(16,24);d=ImageDraw.Draw(im)
    d.rectangle((0,11,15,22),fill=16,outline=BARK_DARK)
    d.rectangle((1,12,14,20),fill=9);d.line((0,22,15,22),fill=1)
    d.line((0,11,15,11),fill=BARK_LIGHT)
    for x,y in [(4,11),(11,17)]:
        d.line((x,y,x,y-7),fill=29)
        poly(d,[(x,y-4),(x-3,y-5),(x-3,y-7),(x-1,y-7)],30,29)
        poly(d,[(x,y-5),(x+1,y-8),(x+4,y-8),(x+3,y-5)],31,29)
        d.point((x,y-7),fill=6)
    return im

def draw_lantern():
    im=image(24,44);d=ImageDraw.Draw(im)
    d.ellipse((3,37,21,43),fill=GRASS_SHADOW)
    d.rectangle((11,5,14,41),fill=BARK_DARK,outline=1);d.line((12,7,12,39),fill=BARK_LIGHT)
    poly(d,[(4,8),(7,3),(18,3),(20,8)],12,1);d.line((7,4,17,4),fill=24)
    d.rectangle((5,9,19,23),fill=1);d.rectangle((6,10,18,21),fill=36)
    d.rectangle((8,11,16,20),fill=37);d.rectangle((10,12,14,19),fill=38)
    d.line((7,10,7,21),fill=48);d.line((17,10,17,21),fill=16)
    d.rectangle((5,22,19,24),fill=12);d.line((7,22,17,22),fill=24)
    return im

# Kinds0 broad tree,1 berry tree,2 fir,3..5 houses,6 sign,7 fence,
# 8 rock,9 well,10 planting bed,11 trail lantern. All widths are multiples4.
scenery=[draw_tree(i) for i in range(3)]+[draw_house(i) for i in range(3)]+[draw_sign(),draw_fence(),draw_rock(),draw_well(),draw_bed(),draw_lantern()]
