"""Original modular orthographic landscape for Asterwild's DS field.

compose_world(art_dir, output_dir) returns a full RGB world, an L depth plane,
and opaque two-pose animation patches.  All geometry comes from world_layout;
art never invents a collision footprint.  Patches contain the globally composed
alternate pose, including every foreground occluder, rather than cut-out trees.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageEnhance
import math
import random

from world_layout import LAYOUT, TERRAIN, PROPS, LANDMARKS

W, H, TILE = LAYOUT['width'], LAYOUT['height'], LAYOUT['tile_size']
GRASS, PATH, WATER, COBBLE, WOOD, TALL_GRASS, CLIFF, STEPS, SAND = range(9)

# The generated sheet uses four semantic columns and three rows, but the
# isolated silhouettes cross a few nominal cell edges.  These alpha bounds
# preserve the complete roofs, trunks, and foundations without adjacent props.
ATLAS_BOUNDS = {
    'house': (20, 14, 413, 379),
    'cottage': (432, 35, 769, 374),
    'boathouse': (796, 70, 1117, 386),
    'tree': (1128, 36, 1441, 392),
    'pine': (22, 392, 304, 749),
    'willow': (334, 393, 707, 749),
    'shrub': (746, 489, 1018, 750),
    'flowerbed': (1075, 521, 1413, 741),
    'lamp': (93, 769, 218, 1056),
    'fountain': (329, 769, 643, 1040),
    'bench': (733, 843, 1036, 1025),
    'floweringtree': (1127, 769, 1416, 1054),
}

# Ten distributed trees, six garden beds, and two route-side shrubs.  The tree
# immediately north of the stair approach is deliberately included for wind
# animation in the same view as the interactive foreground railing.
SWAY_TRUNKS = {(352, 304), (416, 224), (592, 208), (608, 432),
               (320, 464), (576, 480), (688, 352), (752, 304),
               (800, 320), (896, 288)}
SWAY_BEDS = {(112, 448), (160, 448), (272, 448),
             (112, 496), (160, 496), (272, 496)}
SWAY_SHRUBS = {(384, 256), (624, 352)}


def _neighbor(tx, ty):
    if 0 <= ty < len(TERRAIN) and 0 <= tx < len(TERRAIN[0]):
        return TERRAIN[ty][tx]
    return WATER


def _ground():
    """Paint coherent materials, then bevel their connected outside edges."""
    im = Image.new('RGB', (W, H))
    d = ImageDraw.Draw(im)
    base = {GRASS:'#77b573', PATH:'#ddc491', WATER:'#3baec5',
            COBBLE:'#c4c1a3', WOOD:'#b98150', TALL_GRASS:'#6ca866',
            CLIFF:'#9b8a6b', STEPS:'#cfb88c', SAND:'#e7d6a5'}
    for ty, row in enumerate(TERRAIN):
        for tx, kind in enumerate(row):
            x, y = tx*TILE, ty*TILE
            d.rectangle((x,y,x+15,y+15), fill=base[kind])
            r = random.Random(tx*8191 + ty*65537 + 197)
            if kind in (GRASS,TALL_GRASS):
                # Several-pixel clusters look like grass rather than film grain.
                tint = ['#7bb976','#72ae6c','#80ba76','#73b16e'][(tx//3+ty//2)%4]
                for _ in range(3):
                    xx, yy = x+r.randrange(14), y+r.randrange(14)
                    d.line((xx,yy,xx+2,yy), fill=tint)
                if kind == GRASS:
                    for _ in range(r.randrange(2)):
                        xx,yy=x+r.randrange(2,14),y+r.randrange(3,15)
                        d.line((xx-1,yy,xx,yy-2,xx+1,yy),fill='#609e60')
                        d.point((xx+1,yy-2),fill='#a0cb82')
                else:
                    for oy in (3,9,15):
                        for ox in (2,8,14):
                            xx,yy=x+ox+r.randrange(-1,2),y+oy
                            d.line((xx-2,yy,xx-1,yy-4,xx,yy-1,xx+2,yy-5,xx+2,yy),fill='#3e8755')
                            d.line((xx-1,yy-3,xx,yy-1),fill='#b0cf70')
            elif kind == PATH:
                for _ in range(2):
                    xx,yy=x+r.randrange(15),y+r.randrange(15)
                    d.line((xx,yy,xx+1,yy),fill=r.choice(['#d2b783','#e8d2a4']))
                if r.randrange(9)==0:
                    xx,yy=x+r.randrange(2,12),y+r.randrange(2,13)
                    d.line((xx,yy,xx+2,yy),fill='#b8a17d')
                    d.point((xx,yy-1),fill='#eee0b7')
            elif kind == WATER:
                # Small horizontal wavelets have deliberately irregular spacing.
                yy=y+r.randrange(2,14);xx=x+r.randrange(0,8)
                if r.randrange(3):
                    d.line((xx,yy,xx+r.randrange(4,9),yy),fill='#55bed0')
                    if r.randrange(3)==0:d.point((xx+2,yy-1),fill='#80d6dc')
            elif kind == SAND:
                for _ in range(3):
                    xx,yy=x+r.randrange(16),y+r.randrange(16)
                    d.point((xx,yy),fill=r.choice(['#dfcc98','#f3e5bc']))
                if r.randrange(25)==0:
                    xx,yy=x+r.randrange(3,13),y+r.randrange(3,13)
                    d.arc((xx-1,yy-1,xx+2,yy+1),0,180,fill='#bcaa80')
            elif kind == WOOD:
                # Narrow long boards and sparse iron pegs; no 16px tile seams.
                for xx in range(x,x+16):
                    if xx%8==0:d.line((xx,y,xx,y+15),fill='#775a3e')
                    elif xx%8==1:d.line((xx,y,xx,y+15),fill='#e0ae70')
                if (y//16)%4==(x//8)%4:
                    d.line((x,y+7,x+15,y+7),fill='#966840')
                    d.point((x+3,y+9),fill='#6c6557')
                    d.point((x+11,y+9),fill='#6c6557')
                if r.randrange(4)==0:
                    xx=x+r.randrange(3,13)
                    d.line((xx,y+2,xx,y+6),fill='#c8945e')
    # Stone paving is laid in offset courses over whole regions.  Texture
    # positions are global so the eye never sees the source collision grid.
    for y in range(0,H,7):
        row = y//7
        for x in range(-12 if row%2 else -4,W,15):
            cx,cy=x+7,y+3
            if not (0<=cx<W and 0<=cy<H):continue
            if _neighbor(cx//TILE,cy//TILE)!=COBBLE:continue
            r=random.Random(x*91+y*587)
            c=r.choice(['#d5d1b0','#cac9a9','#dcd8b8','#bdc1a5'])
            # Clip stones to the cobble material, including cut edges.
            for yy in range(max(0,y),min(H,y+6)):
                for xx in range(max(0,x),min(W,x+14)):
                    if _neighbor(xx//TILE,yy//TILE)==COBBLE:
                        color = '#9fa992' if yy==y+5 else ('#e5dfbb' if yy==y else c)
                        im.putpixel((xx,yy),color_rgb(color))
    d=ImageDraw.Draw(im)
    connected = (PATH,COBBLE,STEPS,WOOD)
    for ty,row in enumerate(TERRAIN):
        for tx,kind in enumerate(row):
            x,y=tx*TILE,ty*TILE
            if kind in (PATH,COBBLE):
                north,south,west,east=[_neighbor(tx+dx,ty+dy) for dx,dy in [(0,-1),(0,1),(-1,0),(1,0)]]
                if north not in connected:
                    d.line((x,y,x+15,y),fill='#77965c')
                    d.line((x,y+1,x+15,y+1),fill='#f0dca8')
                if south not in connected:
                    d.line((x,y+14,x+15,y+14),fill='#bca575')
                    d.line((x,y+15,x+15,y+15),fill='#879c61')
                if west not in connected:
                    d.line((x,y,x,y+15),fill='#8da369')
                    d.line((x+1,y+2,x+1,y+13),fill='#f0dca8')
                if east not in connected:
                    d.line((x+14,y+2,x+14,y+13),fill='#bdab7c')
                    d.line((x+15,y,x+15,y+15),fill='#80965f')
            if kind==WATER:
                # Shallow turquoise and broken white foam trace the actual
                # island, never a separate decorative painted coastline.
                for dx,dy in [(0,-1),(0,1),(-1,0),(1,0)]:
                    n=_neighbor(tx+dx,ty+dy)
                    if n in (WATER,WOOD):continue
                    if dx==-1:
                        d.rectangle((x,y,x+5,y+15),fill='#6acbc9');d.line((x+1,y+1,x+1,y+14),fill='#c6ece0')
                    elif dx==1:
                        d.rectangle((x+10,y,x+15,y+15),fill='#6acbc9');d.line((x+14,y+1,x+14,y+14),fill='#c6ece0')
                    elif dy==-1:
                        d.rectangle((x,y,x+15,y+5),fill='#72cfcc');d.line((x+1,y+1,x+14,y+1),fill='#d4efe2')
                    else:
                        d.rectangle((x,y+10,x+15,y+15),fill='#68c9ca');d.line((x+1,y+14,x+14,y+14),fill='#c6ece0')
            elif kind==WOOD:
                # Perimeter beams distinguish the pier from a wooden carpet.
                if _neighbor(tx-1,ty)!=WOOD:d.line((x,y,x,y+15),fill='#684b35',width=2)
                if _neighbor(tx+1,ty)!=WOOD:d.line((x+15,y,x+15,y+15),fill='#684b35',width=2)
                if _neighbor(tx,ty+1)==WATER:
                    d.line((x,y+14,x+15,y+14),fill='#e4b878',width=2)
                    d.line((x,y+15,x+15,y+15),fill='#674f3a')
    # Curated meadow flowers, away from paths and interaction landmarks.
    r=random.Random(610)
    for _ in range(165):
        x,y=r.randrange(88,928),r.randrange(112,600)
        if _neighbor(x//16,y//16)!=GRASS:continue
        if any(abs(x-a)<18 and abs(y-b)<18 for points in LANDMARKS.values()
               for a,b in (points if isinstance(points[0],list) else [points])):continue
        d.line((x,y,x,y-2),fill='#588f58')
        c=r.choice(['#f6e5a1','#f2eee0','#bad9e8'])
        d.line((x-1,y-3,x+1,y-3),fill=c);d.point((x,y-4),fill=c)
    # Stair treads run north-south: their illuminated east lips and darker
    # risers make the western entrance read as climbing toward the terrace.
    x,y,w,h=592,272,64,32
    for n in range(8):
        xx=x+n*8
        d.rectangle((xx,y,xx+7,y+h-1),fill=['#b7a17a','#c0aa80','#c9b389','#d1bd93'][min(3,n//2)])
        d.line((xx,y,xx,y+h-1),fill='#897c63')
        d.line((xx+1,y,xx+1,y+h-2),fill='#f0dfb6')
        d.line((xx+2,y+h-3,xx+7,y+h-3),fill='#ae9971')
        d.line((xx+2,y+1,xx+6,y+1),fill='#e0cea5')
    return im


def color_rgb(value):
    return tuple(bytes.fromhex(value.lstrip('#')))


def _masonry(w,h,kind):
    """Lit coping and staggered stone courses over deep retaining walls."""
    im=Image.new('RGBA',(w,h));d=ImageDraw.Draw(im)
    side=kind=='terrace_side'
    d.rectangle((0,0,w-1,h-1),fill='#676f65' if side else '#8a8c77')
    for y in range(6,h,8):
        offset=-10 if (y//8)%2 else 0
        for x in range(offset,w,20):
            c=['#9da18a','#a6a68d','#929b84'][(x//20+y//8)%3]
            d.rectangle((x+1,y+1,x+19,y+7),fill=c if not side else '#7b8879')
            d.line((x+2,y+1,x+18,y+1),fill='#b9ba9a' if not side else '#a4ac8e')
            d.line((x+19,y+2,x+19,y+7),fill='#626e63')
    d.rectangle((0,0,w-1,5),fill='#c9c6a2')
    d.line((0,0,w-1,0),fill='#f1e4bb')
    d.line((0,5,w-1,5),fill='#6f7b6a')
    d.line((0,h-2,w-1,h-2),fill='#5a6b5d')
    d.line((0,h-1,w-1,h-1),fill='#425d4e')
    if kind=='terrace_front':
        # Moss lives in a few lower mortar joints rather than coating the wall.
        for x in (14,48,83,131,160):
            if x<w:
                d.line((x,h-5,x+7,h-5),fill='#66865c')
                d.point((x+2,h-6),fill='#a1b779')
    return im


def _rail(w,h,south):
    im=Image.new('RGBA',(w,h));d=ImageDraw.Draw(im)
    top=5 if south else 3
    # Open balusters: transparent holes preserve the character behind the rail.
    bottom=h-5
    for x in (3,21,40,59):
        d.rectangle((x-2,top+3,x+2,bottom),fill='#5a5140')
        d.line((x-1,top+3,x-1,bottom-1),fill='#e3b46a')
        d.line((x+1,top+4,x+1,bottom),fill='#9b784d')
        d.rectangle((x-3,bottom-1,x+3,bottom+2),fill='#6e654b')
        d.line((x-3,bottom-1,x+2,bottom-1),fill='#b7aa75')
        d.rectangle((x-3,top-2,x+3,top+3),fill='#a78750')
        d.line((x-2,top-2,x+2,top-2),fill='#f1cf85')
    d.rectangle((0,top,w-1,top+5),fill='#775734')
    d.line((0,top,w-1,top),fill='#ffe0a0')
    d.line((0,top+1,w-1,top+1),fill='#d3a45e')
    d.line((0,top+5,w-1,top+5),fill='#493f32')
    # A slender lower rail reinforces depth while keeping large open spaces.
    low=bottom-9
    d.line((1,low,w-2,low),fill='#ad8b54',width=2)
    d.line((1,low+2,w-2,low+2),fill='#514a37')
    return im


def _camp():
    im=Image.new('RGBA',(30,25));d=ImageDraw.Draw(im)
    d.ellipse((2,13,28,23),fill='#779060')
    d.ellipse((5,14,25,23),fill='#6d6e56')
    for x,y in [(7,16),(11,14),(17,14),(22,16),(23,20),(18,22),(11,22),(6,20)]:
        d.ellipse((x-2,y-1,x+2,y+1),fill='#c4bba0',outline='#8b957b')
    d.line((10,20,20,17),fill='#66513c',width=3)
    d.line((10,17,19,20),fill='#987143',width=2)
    d.polygon([(12,19),(10,14),(14,15),(15,9),(19,15),(20,18),(17,21)],fill='#ef9944')
    d.polygon([(14,19),(14,15),(16,17),(17,13),(18,18),(16,20)],fill='#ffe2a0')
    return im


def _selected(p):
    solid=p.get('solid')
    if not solid:return False
    pos=tuple(solid[:2])
    if p['kind'] in ('tree','pine','willow'):return pos in SWAY_TRUNKS
    if p['kind']=='flowerbed':return pos in SWAY_BEDS
    if p['kind']=='shrub':return pos in SWAY_SHRUBS
    return False


def _sway(im, phase, sign):
    """Keep roots anchored and displace only the upper crown by one/two pixels."""
    w,h=im.size
    out=Image.new('RGBA',(w+4,h))
    for y in range(h):
        shift=0
        if phase:
            t=max(0.,1-y/(h*.78))
            shift=sign*(2 if t>.40 else 1 if t>.08 else 0)
        out.alpha_composite(im.crop((0,y,w,y+1)),(2+shift,y))
    return out


def compose_world(art_dir, output_dir):
    art_dir,output_dir=Path(art_dir),Path(output_dir)
    output_dir.mkdir(parents=True,exist_ok=True)
    atlas=Image.open(art_dir/'topdown-props.png').convert('RGBA')
    cells={name:atlas.crop(box) for name,box in ATLAS_BOUNDS.items()}
    ground=_ground().convert('RGBA')
    # Soft contact shadows are ground-only: they must never mask an actor.
    shadow=Image.new('RGBA',(W,H));sd=ImageDraw.Draw(shadow)
    for p in PROPS:
        if p['kind'] in ATLAS_BOUNDS:
            width=p['w'] if p['kind'] in ('house','cottage','boathouse') else int(p['w']*.78)
            cx=p['x']+p['w']//2;yy=p['y']+p['h']-3
            sd.ellipse((cx-width//2,yy-3,cx+width//2,yy+3),fill=(24,66,49,48))
    ground=Image.alpha_composite(ground,shadow)
    prepared=[]
    for index,p in enumerate(PROPS):
        kind=p['kind'];w,h=p['w'],p['h']
        if kind.startswith('terrace'):
            im=_masonry(w,h,kind)
        elif kind.startswith('stairs_rail'):
            im=_rail(w,h,kind.endswith('south'))
        else:
            variant=kind
            if kind=='tree' and tuple(p.get('solid',[0,0])[:2]) in {(352,304),(240,576)}:
                variant='floweringtree'
            im=cells[variant].resize((w,h),Image.Resampling.LANCZOS)
            im=ImageEnhance.Sharpness(im).enhance(1.12)
            # Two facing variants break repeated foliage silhouettes naturally.
            if kind in ('tree','pine','willow','shrub') and index%3==1:
                im=im.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
        animated=_selected(p)
        pad=2 if animated else 0
        images=[_sway(im,phase,(-1 if index%2 else 1)) for phase in (0,1)] if animated else [im,im]
        prepared.append(dict(x=p['x']-pad,y=p['y'],w=w+pad*2,h=h,
                             depth=p['depth'],images=images,animated=animated,
                             name=kind))
    fire=_camp()
    prepared.append(dict(x=LANDMARKS['camp'][0]-15,y=LANDMARKS['camp'][1]-20,
                         w=30,h=25,depth=LANDMARKS['camp'][1]+4,
                         images=[fire,fire],animated=False,name='camp'))
    prepared.sort(key=lambda p:p['depth'])
    worlds=[];depths=[]
    for phase in (0,1):
        world=ground.copy();depth=Image.new('L',(W,H))
        for p in prepared:
            image=p['images'][phase]
            world.alpha_composite(image,(p['x'],p['y']))
            alpha=image.getchannel('A').point(lambda a:255 if a>=110 else 0)
            depth.paste(max(1,min(255,p['depth']//4)),(p['x'],p['y'],p['x']+p['w'],p['y']+p['h']),alpha)
        worlds.append(world.convert('RGB'));depths.append(depth)
    patches=[]
    for p in prepared:
        if not p['animated']:continue
        x=max(0,p['x']);y=max(0,p['y'])
        x1=min(W,p['x']+p['w']);y1=min(H,p['y']+p['h'])
        box=(x,y,x1,y1)
        patches.append(dict(x=x,y=y,w=x1-x,h=y1-y,
                            image=worlds[1].crop(box),depth=depths[1].crop(box)))
    budget=sum(p['w']*p['h']*3 for p in patches)
    assert budget<=150*1024,(budget,'Animation patch budget exceeded')
    worlds[0].save(output_dir/'world.png')
    worlds[1].save(output_dir/'world-wind.png')
    depths[0].point(lambda v:min(255,v*2)).save(output_dir/'depth.png')
    ground.convert('RGB').save(output_dir/'ground.png')
    worlds[0].save(output_dir/'world-wind.gif',save_all=True,append_images=[worlds[1]],duration=420,loop=0)
    # Native-size views help judge whether texturing survives the DS display.
    for name,point in [('lodge',(312,248)),('commons',(456,408)),
                       ('stairs',(616,296)),('garden',(232,488)),('harbor',(488,632))]:
        x=max(0,min(W-256,point[0]-128));y=max(0,min(H-192,point[1]-96))
        worlds[0].crop((x,y,x+256,y+192)).save(output_dir/('scene-'+name+'.png'))
    print(f'Top-down landscape: {len(prepared)} scenery props; {len(patches)} animated patches, {budget:,} RGB555+depth bytes.')
    return worlds[0],depths[0],patches


if __name__=='__main__':
    root=Path(__file__).resolve().parents[1]
    compose_world(root/'art',root/'build')
