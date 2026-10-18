"""Authored, orthographic Asterwild first-area layout.

The tile lattice is the source of ground, footprints, collision, and landmarks.
Run this file to regenerate src/world.c/h and the planning preview.  The asset
builder imports LAYOUT to draw its own original terrain and depth-sorted props.
Every actor and interaction rests on a 16-pixel tile center (8 modulo 16).
"""
from pathlib import Path
from collections import deque
import json

ROOT = Path(__file__).resolve().parents[1]
TILE = 16
WIDTH, HEIGHT = 1024, 768
COLS, ROWS = WIDTH // TILE, HEIGHT // TILE
GRASS, PATH, WATER, COBBLE, WOOD, TALL_GRASS, CLIFF, STEPS, SAND = range(9)
TERRAIN = [[WATER for _ in range(COLS)] for _ in range(ROWS)]
PROPS = []
EXTRA_SOLIDS = []

def terrain(x, y, w, h, kind):
    assert not (x % TILE or y % TILE or w % TILE or h % TILE)
    for ty in range(y // TILE, (y + h) // TILE):
        for tx in range(x // TILE, (x + w) // TILE):
            if 0 <= tx < COLS and 0 <= ty < ROWS:
                TERRAIN[ty][tx] = kind

def prop(kind, x, y, w, h, depth, solid=None, **extras):
    item = dict(kind=kind, x=x, y=y, w=w, h=h, depth=depth)
    if solid:
        item['solid'] = list(solid)
    item.update(extras)
    PROPS.append(item)
    return item

def tree(x, y, kind='tree'):
    """Tree coordinates name its single solid trunk tile, not its canopy."""
    w, h = (40, 64) if kind == 'pine' else (48, 64)
    prop(kind, x + 8 - w // 2, y + 16 - h, w, h, y + 16,
         (x, y, 16, 16), sway=True)

def shrub(x, y):
    prop('shrub', x - 8, y - 8, 32, 24, y + 16, (x, y, 16, 16), sway=True)

# A compact island silhouette with a sandy southern waterfront.  Shoreline
# corners deliberately follow the tile grammar rather than painted diagonals.
for r in [(80, 96, 832, 512), (112, 64, 656, 32),
          (48, 176, 32, 320), (912, 208, 64, 288),
          (144, 608, 576, 32), (304, 640, 304, 16)]:
    terrain(*r, GRASS)
terrain(80, 576, 832, 32, SAND)
terrain(144, 608, 576, 32, SAND)
terrain(304, 640, 304, 16, SAND)
terrain(48, 288, 48, 208, SAND)
terrain(912, 224, 64, 256, SAND)

# The wide main street is the navigational spine.  Town branches use square
# joins; the forest forms a legible loop rather than a narrow painted maze.
for r in [(448, 128, 48, 512), (144, 384, 512, 48),
          (272, 256, 32, 144), (272, 272, 320, 32),
          (208, 432, 48, 128), (496, 384, 400, 32),
          (864, 128, 32, 336), (672, 96, 224, 32),
          (672, 128, 32, 96), (704, 432, 192, 32),
          (672, 208, 32, 48), (496, 560, 160, 32)]:
    terrain(*r, PATH)
terrain(368, 352, 192, 144, COBBLE)
terrain(448, 496, 48, 144, COBBLE)
terrain(448, 608, 96, 112, WOOD)
terrain(400, 640, 192, 32, WOOD)
terrain(416, 496, 32, 64, PATH)
terrain(368, 512, 80, 32, PATH)
terrain(112, 448, 192, 112, GRASS)
for r in [(704, 336, 96, 32), (832, 224, 32, 64),
          (800, 128, 64, 48), (720, 464, 80, 32),
          (576, 112, 64, 48), (144, 304, 80, 48)]:
    terrain(*r, TALL_GRASS)

# A proper raised terrace: squared floor, blocked retaining walls, and one
# east-west stair access.  The rail sprites have independent depth planes.
terrain(640, 144, 192, 208, CLIFF)
terrain(656, 160, 160, 160, COBBLE)
terrain(592, 272, 64, 32, STEPS)
EXTRA_SOLIDS += [(640, 144, 192, 16), (816, 160, 16, 160),
                 (640, 160, 16, 112), (640, 304, 16, 48),
                 (656, 320, 176, 32), (592, 256, 48, 16),
                 (592, 304, 48, 16)]
# Flat tiles are never responsible for the rail occlusion.  South rail is a
# foreground object over a character on the staircase; north rail stays rear.
prop('stairs_rail_north', 592, 248, 64, 24, 272)
prop('stairs_rail_south', 592, 280, 64, 48, 328)
prop('terrace_front', 656, 312, 176, 40, 352)
prop('terrace_side', 816, 144, 16, 176, 320)
prop('terrace_back', 640, 136, 192, 24, 160)
prop('terrace_side', 640, 144, 16, 128, 272)
prop('terrace_side', 640, 304, 16, 48, 352)

# Three characteristic buildings; their solid footprints are exclusively the
# low wall/foundation, leaving the roof capable of covering somebody behind.
prop('house', 224, 160, 96, 96, 256, (224, 208, 96, 48))
prop('cottage', 128, 352, 80, 80, 432, (128, 400, 80, 32))
prop('boathouse', 576, 512, 80, 80, 592, (576, 560, 80, 32))
prop('lamp', 336, 232, 16, 40, 272, (336, 256, 16, 16))
prop('lamp', 528, 328, 16, 40, 368, (528, 352, 16, 16))
prop('lamp', 544, 568, 16, 40, 608, (544, 592, 16, 16))
prop('lamp', 784, 168, 16, 40, 208, (784, 192, 16, 16))
prop('fountain', 392, 352, 40, 48, 400, (400, 384, 32, 16))
prop('bench', 368, 448, 40, 24, 472, (368, 464, 32, 16))
prop('bench', 496, 464, 40, 24, 488, (496, 480, 32, 16))
prop('bench', 688, 176, 40, 24, 200, (688, 192, 32, 16))
# Rows of beds frame Edda's generous garden aisle without blocking her.
for x, y in [(112, 448), (160, 448), (272, 448),
             (112, 496), (160, 496), (272, 496),
             (112, 544), (160, 544), (272, 544)]:
    prop('flowerbed', x, y - 8, 32, 24, y + 16,
         (x, y, 32, 16), sway=True)

# Individual trunks let the depth renderer put heroes behind crowns and
# branches.  Their placement frames routes rather than disguising collision.
for x, y, kind in [
    (112,112,'tree'),(160,96,'tree'),(208,96,'pine'),(256,96,'tree'),
    (304,96,'pine'),(352,112,'tree'),(400,96,'tree'),(448,80,'pine'),
    (496,96,'tree'),(544,80,'tree'),(592,80,'pine'),(640,64,'tree'),
    (704,64,'pine'),(752,64,'tree'),(800,80,'pine'),(848,80,'tree'),
    (96,176,'tree'),(144,192,'willow'),(192,160,'tree'),
    (368,192,'tree'),(416,224,'tree'),(544,192,'pine'),(592,208,'tree'),
    (80,256,'pine'),(96,320,'tree'),(64,384,'willow'),(80,464,'tree'),
    (112,576,'pine'),(176,592,'tree'),(240,576,'tree'),(304,592,'willow'),
    (352,576,'tree'),(400,608,'tree'),(688,592,'tree'),(752,576,'pine'),
    (816,592,'tree'),(880,560,'willow'),(928,496,'tree'),(944,432,'pine'),
    (944,368,'tree'),(944,304,'tree'),(928,240,'willow'),(928,176,'tree'),
    (896,112,'pine'),(912,144,'tree'),
    (736,128,'tree'),(784,208,'tree'),(720,224,'tree'),
    (688,352,'pine'),(752,304,'tree'),(800,320,'pine'),
    (832,176,'tree'),(896,208,'tree'),(896,288,'pine'),
    (816,464,'tree'),(864,496,'tree'),(672,496,'pine'),
    (608,432,'tree'),(656,448,'willow'),(576,480,'tree'),
    (320,352,'tree'),(320,464,'willow'),(352,304,'tree'),
]:
    tree(x, y, kind)
for x, y in [(160,256),(192,288),(336,144),(384,256),(544,144),
             (576,336),(624,352),(720,400),(768,496),(864,336),
             (896,528),(352,528),(304,544),(96,528)]:
    shrub(x, y)

LANDMARKS = {
    'spawn': [488, 680],
    'camp': [392, 520],
    'npcs': [[280, 280], [456, 408], [232, 488], [744, 248]],
    'wilds': [[168, 328], [600, 136], [744, 360],
              [840, 248], [856, 152], [760, 488]],
    'flowers': [[344, 552], [824, 360], [904, 456]],
}
# Names in this dictionary are authoring and test metadata, not extra game
# rules.  Renderer uses these same rectangles for minimap geography.
REGIONS = {
    'harbor': [304, 576, 416, 144],
    'garden': [96, 432, 224, 144],
    'terrace': [592, 144, 240, 208],
    'forest': [656, 64, 288, 512],
    'village': [80, 96, 576, 480],
}


def blocked_grid():
    blocked = [[tile in (WATER, CLIFF) for tile in row] for row in TERRAIN]
    # Stair corridor takes precedence over the cliff-colored retaining wall.
    for item in PROPS:
        if item.get('solid'):
            x, y, w, h = item['solid']
            for ty in range(y // TILE, (y + h + TILE - 1) // TILE):
                for tx in range(x // TILE, (x + w + TILE - 1) // TILE):
                    if 0 <= tx < COLS and 0 <= ty < ROWS:
                        blocked[ty][tx] = True
    for x, y, w, h in EXTRA_SOLIDS:
        for ty in range(y // TILE, (y + h) // TILE):
            for tx in range(x // TILE, (x + w) // TILE):
                blocked[ty][tx] = True
    return blocked


def merged_solids(blocked):
    """Merge equal horizontal runs vertically without losing lattice edges."""
    active, result = {}, []
    for ty, row in enumerate(blocked):
        runs, x = [], 0
        while x < COLS:
            if not row[x]:
                x += 1
                continue
            start = x
            while x < COLS and row[x]:
                x += 1
            runs.append((start, x - start))
        following = {}
        for key in runs:
            if key in active:
                rect = active.pop(key)
                rect[3] += TILE
            else:
                rect = [key[0] * TILE, ty * TILE, key[1] * TILE, TILE]
            following[key] = rect
        result.extend(active.values())
        active = following
    result.extend(active.values())
    return sorted(result, key=lambda r: (r[1], r[0]))


BLOCKED = blocked_grid()
SOLIDS = merged_solids(BLOCKED)
LAYOUT = dict(width=WIDTH, height=HEIGHT, tile_size=TILE,
              tiles=TERRAIN, props=PROPS, solids=SOLIDS,
              landmarks=LANDMARKS, regions=REGIONS,
              stair_test=dict(behind=[616, 296], front=[616, 344],
                              clear=[568, 296], railing=[592, 280, 64, 48]))


def validate():
    spawn = tuple(c // TILE for c in LANDMARKS['spawn'])
    assert not BLOCKED[spawn[1]][spawn[0]], 'Spawn is blocked'
    todo, reached = deque([spawn]), {spawn}
    while todo:
        x, y = todo.popleft()
        for nx, ny in [(x-1,y),(x+1,y),(x,y-1),(x,y+1)]:
            if (0 <= nx < COLS and 0 <= ny < ROWS and
                    not BLOCKED[ny][nx] and (nx,ny) not in reached):
                reached.add((nx,ny)); todo.append((nx,ny))
    for kind, coords in LANDMARKS.items():
        points = coords if isinstance(coords[0], list) else [coords]
        for x, y in points:
            assert x % TILE == y % TILE == 8, (kind,x,y,'not centered')
            assert (x // TILE,y // TILE) in reached, (kind,x,y,'unreachable')
    for name in ('behind', 'front', 'clear'):
        x, y = LAYOUT['stair_test'][name]
        assert (x // TILE,y // TILE) in reached, ('stairs',name,'unreachable')
    return len(reached)


def generate():
    reached = validate()
    header = '''#ifndef ASTERWILD_WORLD_H\n#define ASTERWILD_WORLD_H\n\n#include "game.h"\n\n/* Authored orthographic 16px tile world; regenerate with tools/world_layout.py. */\nextern const WorldDef aster_world;\n\n#endif\n'''
    (ROOT / 'src/world.h').write_text(header)
    c = ['#include "world.h"', '',
         '/* Generated by tools/world_layout.py.  Terrain, scenery footprints,',
         ' * depth layers, and interactions share a cardinal 16-pixel lattice. */',
         'static const WorldSolid world_solids[] = {']
    c += ['    {' + ','.join(map(str,r)) + '},' for r in SOLIDS]
    c += ['};', '', 'const WorldDef aster_world = {',
          '    .solids = world_solids,',
          '    .solid_count = sizeof world_solids / sizeof world_solids[0],']
    for name, coords in LANDMARKS.items():
        if isinstance(coords[0], list):
            value = '{' + ','.join('{' + ','.join(map(str,p)) + '}' for p in coords) + '}'
        else:
            value = '{' + ','.join(map(str,coords)) + '}'
        c.append('    .' + name + ' = ' + value + ',')
    c += ['};', '']
    (ROOT / 'src/world.c').write_text('\n'.join(c))
    out = ROOT / 'build'
    out.mkdir(exist_ok=True)
    (out / 'world_layout.json').write_text(json.dumps(LAYOUT, indent=2) + '\n')
    try:
        from PIL import Image, ImageDraw, ImageFont
        colors = ['#6eac72','#d5bb82','#65b8ce','#d3ccaa','#a77a50',
                  '#4f965c','#92775e','#c9b18b','#e6d6a3']
        im = Image.new('RGB',(WIDTH,HEIGHT));d = ImageDraw.Draw(im)
        for y,row in enumerate(TERRAIN):
            for x,v in enumerate(row):
                d.rectangle((x*TILE,y*TILE,(x+1)*TILE-1,(y+1)*TILE-1),fill=colors[v])
        for p in sorted(PROPS,key=lambda p:p['depth']):
            x,y,w,h = [p[k] for k in ('x','y','w','h')]
            kind = p['kind']
            if kind in ('tree','pine','willow'):
                d.ellipse((x,y,x+w-1,y+h-12),fill='#306d4f',outline='#234b3a')
            elif kind == 'shrub':
                d.ellipse((x,y,x+w-1,y+h-1),fill='#427d58')
            elif kind in ('house','cottage','boathouse'):
                d.rectangle((x,y,x+w-1,y+h-1),fill='#9f655e',outline='#503e3b',width=2)
                d.line((x,y+h//2,x+w-1,y+h//2),fill='#f5d5ac',width=3)
                d.text((x+4,y+8),kind,fill='white')
            elif kind.startswith('terrace'):
                d.rectangle((x,y,x+w-1,y+h-1),fill='#947d63',outline='#625640')
            elif kind.startswith('stairs_rail'):
                d.rectangle((x,y+8,x+w-1,y+13),fill='#73543f')
                d.line((x,y+20,x+w-1,y+20),fill='#8d6e4e',width=2)
            else:
                d.rectangle((x,y,x+w-1,y+h-1),fill='#d5aa69',outline='#75563e')
        for name, coords in LANDMARKS.items():
            points=coords if isinstance(coords[0],list) else [coords]
            for i,(x,y) in enumerate(points):
                color={'spawn':'#1758d8','camp':'#f4f2d4','npcs':'#ffb4d3',
                       'wilds':'#d0ed4b','flowers':'#db6cd7'}[name]
                d.ellipse((x-6,y-6,x+6,y+6),fill=color,outline='#17394b',width=2)
                label = ['Mira','Jun','Edda','Vale'][i] if name=='npcs' else name+str(i)
                d.text((x+8,y-7),label,fill='#132f3a',stroke_width=1,stroke_fill='#f8edcb')
        im.save(out / 'world_layout.png')
    except ImportError:
        pass
    print(f'Authored {len(PROPS)} props, {len(SOLIDS)} collision rectangles, {reached} reachable tiles.')

if __name__ == '__main__':
    generate()
