"""Authored panel templates and overhead pixel art for Coastline's 48 vehicles.

Coordinates belong to the WHOLE neutral vehicle, never to a paint scanline.
A decal is a stencil intersected with actual paint; openings stay openings.
The byte atlas carries four fitted stencil bits plus a 16-color material index.
"""
import array
import sys
from PIL import Image, ImageDraw

# Width, length, nose inset, windshield Y, roof length, body family.
# These are individual model designs, not class-wide silhouettes.
PROFILES = (
 (9,35,2,14,11,'hatch'), (10,38,3,16,10,'coupe'), (10,39,1,12,17,'wagon'),
 (4,36,1,13,9,'bike'), (10,37,3,17,10,'sport'), (10,38,1,15,12,'sedan'),
 (10,37,2,15,12,'convertible'), (11,40,1,12,18,'suv'), (9,35,1,14,10,'rally'),
 (11,39,4,17,10,'exotic'), (9,34,2,13,10,'hatch'), (10,36,3,15,9,'coupe'),
 (10,41,0,11,20,'van'), (11,40,0,11,18,'armor'), (10,37,2,16,10,'sport'),
 (10,38,2,14,13,'sedan'), (9,34,3,14,9,'sport'), (11,38,3,16,9,'exotic'),
 (10,39,4,13,17,'canopy'), (10,36,3,16,9,'convertible'), (11,41,0,11,19,'armor'),
 (11,39,4,13,17,'canopy'), (10,41,1,14,17,'luxury'), (11,38,4,18,8,'wedge'),
 (11,41,0,13,9,'truck'), (5,35,0,14,10,'skates'), (4,39,0,17,10,'cycle'),
 (5,36,1,14,10,'moped'), (6,31,0,13,9,'segway'), (5,36,0,13,10,'board'),
 (6,40,1,16,10,'horse'), (11,39,2,17,11,'muscle'), (10,38,3,17,9,'coupe'),
 (10,37,2,15,12,'convertible'), (11,41,0,12,18,'suv'), (10,40,1,17,12,'muscle'),
 (11,39,4,18,8,'wedge'), (11,40,3,17,9,'exotic'), (11,39,3,17,9,'midengine'),
 (11,40,3,16,11,'hyper'), (10,41,1,15,15,'luxury'), (11,42,1,16,14,'limousine'),
 (11,42,0,12,9,'truck'), (5,37,2,15,10,'bike'), (5,38,2,15,10,'bike'),
 (5,37,2,14,10,'bike'), (12,42,0,11,17,'armor'), (12,42,5,19,10,'bat'),
)

# Rear lamp placements in the rear-view atlas (normalized neutral coordinates).
# Rectangles include the dark bezel so neither paint nor decals eat into it.
LAMP_BANDS = (
 (.80,.91,.29),(.65,.87,.27),(.68,.91,.25),(.53,.74,.36),(.65,.88,.28),(.66,.91,.25),
 (.78,.88,.25),(.65,.90,.24),(.63,.87,.30),(.67,.89,.30),(.64,.87,.27),(.66,.89,.30),
 (.63,.92,.23),(.62,.90,.25),(.65,.88,.28),(.67,.91,.25),(.64,.88,.30),(.65,.88,.30),
 (.63,.91,.25),(.66,.88,.28),(.62,.91,.25),(.66,.89,.30),(.65,.91,.23),(.66,.88,.30),
 (.62,.91,.22),(.68,.90,.25),(.68,.90,.25),(.64,.84,.30),(.68,.90,.25),(.68,.90,.25),
 (.68,.90,.25),(.65,.86,.34),(.63,.84,.28),(.65,.87,.28),(.54,.83,.18),(.68,.85,.49),
 (.66,.84,.37),(.62,.84,.39),(.63,.84,.35),(.70,.84,.48),(.60,.88,.21),(.60,.87,.21),
 (.65,.91,.18),(.53,.73,.34),(.53,.73,.34),(.53,.73,.34),(.41,.82,.24),(.68,.95,.33),
)

# Conservative interior window/cabin rectangles; surrounding pillars remain
# enamel. Rear-view roof proportions differ substantially between models.
GLASS = (
 (.29,.71,.62,.74),(.30,.70,.51,.65),(.27,.73,.46,.70),None,
 (.36,.64,.43,.67),(.28,.72,.57,.68),(.24,.76,.36,.63),(.31,.69,.27,.69),
 (.28,.72,.59,.71),(.37,.63,.23,.68),(.28,.72,.59,.70),(.28,.72,.56,.68),
 None,None,(.28,.72,.56,.73),(.28,.72,.57,.69),(.28,.72,.57,.71),
 (.28,.72,.54,.69),(.38,.62,.29,.62),(.26,.74,.31,.53),None,
 (.38,.62,.30,.66),(.29,.71,.51,.70),(.35,.65,.29,.57),(.25,.75,.54,.70),
 None,None,None,None,None,None,
 (.28,.72,.43,.57),(.28,.72,.38,.56),(.23,.77,.28,.58),(.26,.74,.45,.60),
 (.27,.73,.41,.59),(.39,.61,.28,.55),(.39,.61,.26,.61),(.33,.67,.29,.57),
 (.32,.68,.30,.57),(.28,.72,.42,.66),(.28,.72,.42,.66),(.23,.77,.37,.58),
 None,None,None,(.31,.69,.48,.63),(.39,.61,.31,.56),
)
def protected_glass(model,x,y):
    box=GLASS[model]
    return bool(box and box[0]<x<box[1] and box[2]<y<box[3])


def protected_lamp(model,r,g,b,x,y):
    if 25<=model<=30: return False
    top,bottom,side=LAMP_BANDS[model]
    red_factory={0:(.80,.91,.29),6:(.78,.88,.25),14:(.79,.89,.28),
                 16:(.79,.91,.30),24:(.75,.88,.22),38:(.66,.77,.35),43:(.57,.68,.34)}
    if model in red_factory:top,bottom,side=red_factory[model]
    # Red/amber emitters are protected even when the body itself is red. A
    # narrow spatial region prevents turning an entire red car into trim.
    rear=top<=y<=bottom and (x<side or x>1-side)
    warm_factory=model in (2,15,17,32,44)
    red=r>65 and r>g*(2.8 if warm_factory else 1.45) and r>b*1.30
    amber=not warm_factory and r>120 and g>55 and r>g*1.1 and g>b*1.8
    # High-mounted center brake light, separate from the main lamp pair.
    center=model in (31,34,35,39,41,42,46) and .40<x<.60 and .30<y<.67 and red
    return (rear and (red or amber)) or center


def decal_mask(model,u,v):
    """Four fixed stencils, each fitted once to this model's painted envelope."""
    w,length,nose,wind,roof,family=PROFILES[model]
    if not 0<=u<=1 or not 0<=v<=1: return 0
    # Paired longitudinal stripes, shoulder two-tone, diagonal velocity,
    # inset contour. Their paths remain straight through protected openings.
    twin=(.24<=u<=.36 or .64<=u<=.76)
    shoulder=.23
    two=(u<shoulder or u>1-shoulder)
    sweep=.16+.60*v
    velocity=sweep<=u<=sweep+.15 or (.58<v<.72 and u>.80)
    edge=.13+(.09 if .25<v<.75 else .04)
    contour=(edge<=u<=edge+.075 or 1-edge-.075<=u<=1-edge or (.12<v<.18 and .34<u<.66))
    if family in ('bike','cycle','moped','skates','segway','board','horse'):
        contour=(u<.16 or u>.84 or (.10<v<.18 and .25<u<.75))
    if family=='segway':twin=(.04<u<.20 or .80<u<.96)
    return int(twin)|(int(two)<<1)|(int(velocity)<<2)|(int(contour)<<3)


def overhead_sprite(model):
    """32x48 true overhead designs; palettes match the chase-camera originals.

    Roles1..7 enamel shades,8/9 glass,10 rubber,11 wheel metal,
    12 headlights,13 tail lamps,14 cabin,15 chrome. Stencils only on enamel.
    """
    w,length,nose,wind,roof,family=PROFILES[model]
    im=Image.new('P',(32,48),0);d=ImageDraw.Draw(im)
    y0=3;end=y0+length;cx=16
    def rect(box,c):d.rectangle(box,fill=c)
    def line(points,c,width=1):d.line(points,fill=c,width=width)
    def poly(points,c):d.polygon(points,fill=c)
    if family not in ('bike','cycle','moped','skates','segway','board','horse'):
        # Rounded shoulders and wheel arches; sports noses taper independently
        # from their wide rear quarters. The outline is painted, never a box.
        shape=[(cx-w+nose,y0),(cx+w-nose,y0),(cx+w-1,y0+3),(cx+w,wind-3),
               (cx+w-1,wind+3),(cx+w-1,end-7),(cx+w,end-5),(cx+w-2,end),
               (cx-w+2,end),(cx-w,end-5),(cx-w+1,end-7),(cx-w+1,wind+3),(cx-w,wind-3),(cx-w+1,y0+3)]
        poly(shape,1)
        poly([(x+(1 if x<cx else -1),y+(1 if y<24 else -1)) for x,y in shape],3)
        line([(cx-w+2,wind-4),(cx-w+2,end-5)],5)
        line([(cx+w-2,wind-4),(cx+w-2,end-5)],2)
        # Hood bevel and two panel creases.
        line([(cx-w+nose+2,y0+2),(cx+w-nose-2,y0+2)],6)
        line([(cx-w+3,y0+4),(cx-w+4,wind-2)],5)
        line([(cx+w-3,y0+4),(cx+w-4,wind-2)],2)
        line([(cx-w+3,wind-2),(cx+w-3,wind-2)],2)
        # Four discrete rubber tires with tread and a small exposed rim face.
        for wx in (cx-w-1,cx+w-1):
            for wy in (wind-4,end-9):
                rect((wx,wy,wx+2,wy+6),10)
                rect((wx+1,wy+2,wx+1,wy+4),11)
                line([(wx,wy+1),(wx+2,wy+1)],8)
        rw=w-3
        # Side mirrors are chrome, never recolored by paint or decals.
        rect((cx-w-2,wind,cx-w,wind+2),15);rect((cx+w,wind,cx+w+2,wind+2),15)
        poly([(cx-rw+1,wind),(cx+rw-1,wind),(cx+rw,wind+5),(cx-rw,wind+5)],8)
        line([(cx-rw+2,wind+1),(cx+rw-2,wind+1)],9)
        line([(cx-rw+1,wind+2),(cx-rw+2,wind+4)],9)
        # Roof enamel panel, rear window and thin pillars separate the glass.
        rect((cx-rw+1,wind+6,cx+rw-1,wind+roof-1),4)
        line([(cx-rw+2,wind+6),(cx+rw-2,wind+6)],6)
        back=min(end-4,wind+roof+4)
        poly([(cx-rw+1,wind+roof),(cx+rw-1,wind+roof),(cx+rw-1,back),(cx-rw+1,back)],8)
        line([(cx-rw+2,wind+roof+1),(cx+rw-2,wind+roof+1)],9)
        # Side glass, black seals and the door shut lines.
        for xx in (cx-w+2,cx+w-2):
            line([(xx,wind+5),(xx,back-1)],8)
            rect((xx,wind+roof//2,xx,wind+roof//2+1),2)
            line([(xx,back+1),(xx,end-3)],2)
        line([(cx-w+3,end-2),(cx+w-3,end-2)],5)
        # Lamps and bumper remain independent materials in every finish.
        for xx in (cx-w+nose+1,cx+w-nose-3):
            rect((xx,y0+1,xx+2,y0+2),12)
        for xx in (cx-w+2,cx+w-4):rect((xx,end-2,xx+2,end-1),13)
        line([(cx-w+3,end),(cx+w-3,end)],10)
        rect((15,end-1,17,end-1),15)
        # The model families expose different recognizable hardware.
        if family in ('suv','wagon','van','armor','limousine'):
            for xx in (cx-rw+2,cx+rw-2):line([(xx,wind+7),(xx,wind+roof-2)],15)
            if family in ('suv','van','armor'):
                for yy in range(wind+8,wind+roof-1,3):line([(cx-rw+3,yy),(cx+rw-3,yy)],5)
        if family in ('sport','rally','coupe','muscle','exotic','wedge','midengine','hyper'):
            wing= end-4 if family in ('exotic','wedge','midengine','hyper') or model==32 else end-2
            if model in (8,17,31,32,36,37,38,39):rect((cx-w+1,wing,cx+w-1,wing+1),10)
            if family in ('muscle','wedge'):rect((14,8,18,11),8);line([(15,8),(17,8)],15)
        if family in ('midengine','exotic','hyper','wedge'):
            rect((cx-rw+2,back-1,cx+rw-2,end-5),8)
            for yy in range(back,end-4,2):line([(cx-rw+3,yy),(cx+rw-3,yy)],15)
        if family=='canopy':
            poly([(13,wind),(19,wind),(22,wind+9),(19,wind+19),(13,wind+19),(10,wind+9)],8)
            line([(14,wind+2),(12,wind+9),(14,wind+16)],9)
        if family=='truck':
            rect((cx-w+2,wind+roof+2,cx+w-2,end-3),10)
            rect((cx-w+3,wind+roof+3,cx+w-3,end-4),14)
            for xx in range(cx-w+4,cx+w-3,3):line([(xx,wind+roof+4),(xx,end-5)],15)
            line([(cx-w+2,wind+roof+1),(cx+w-2,wind+roof+1)],15)
        if family=='convertible':
            rect((cx-rw,wind+6,cx+rw,back),10)
            for xx in (cx-5,cx+2):
                rect((xx,wind+8,xx+3,back-1),14);rect((xx,wind+7,xx+3,wind+9),15)
        if family=='armor':
            rect((cx-5,wind+8,cx+5,wind+14),2);rect((cx-4,wind+9,cx+4,wind+13),4)
            for xx in (cx-w+2,cx+w-2):
                for yy in range(wind+4,end-4,5):rect((xx,yy,xx,yy),15)
        if family=='bat':
            poly([(4,22),(7,15),(9,37),(5,44)],2);poly([(28,22),(25,15),(23,37),(27,44)],2)
            line([(6,24),(6,39)],6);line([(26,24),(26,39)],5)
            poly([(16,13),(21,25),(19,33),(13,33),(11,25)],8)
            line([(16,15),(18,26)],9);rect((14,38,18,42),10);rect((15,39,17,41),13)
        if model==35:line([(cx-w+2,end-2),(cx+w-2,end-2)],13)
        if model==39:line([(cx-w+3,end-2),(cx+w-3,end-2)],13)
    elif family=='horse':
        # Mane, muzzle, four legs, saddle and rider viewed from above.
        poly([(12,9),(12,6),(11,2),(14,4),(18,4),(21,2),(20,7),(20,12),(22,20),(22,32),(20,38),(12,38),(10,32),(10,20)],14)
        line([(13,9),(12,16)],15);line([(19,9),(20,16)],15)
        for xx in (10,20):
            rect((xx,18,xx+1,23),10);rect((xx,32,xx+1,39),10)
        line([(16,35),(17,44)],10,2);rect((12,22,20,30),10)
        rect((13,20,19,28),3);rect((14,15,18,20),12);rect((13,15,19,16),10)
        rect((10,23,12,28),11);rect((20,23,22,28),11)
    else:
        # Bikes retain different tank/fairing widths; novelty equipment is
        # distinct from the rider, so clothing/skin never becomes a decal.
        if family in ('bike','cycle','moped'):
            rect((14,3,18,11),10);rect((14,35,18,44),10)
            line([(16,4),(16,10)],11);line([(16,36),(16,42)],11)
            poly([(16-w,11),(16-w+1,7),(16+w-1,7),(16+w,11),(16+w,31),(16,36),(16-w,31)],2)
            poly([(14,10),(18,10),(16+w-1,17),(18,29),(14,29),(17-w,17)],4)
            line([(12,12),(20,12)],15);rect((15,7,17,8),12)
            rect((14,32,18,33),13);rect((15,27,17,31),10)
            if model in (43,44,45):
                line([(17-w,17),(13,25)],5);line([(15+w,17),(19,25)],6)
                rect((20,30,21,36),15)
            if family=='cycle':
                rect((12,10,20,32),0);line([(16,10),(12,27),(19,27),(16,10),(16,35)],4)
                rect((13,25,19,27),10)
        if family=='board':
            d.rounded_rectangle((11,17,21,43),radius=4,fill=3)
            line([(13,20),(13,39)],6);rect((10,21,22,23),11);rect((10,36,22,38),11)
            for xx in (9,22):rect((xx,20,xx+1,24),10);rect((xx,35,xx+1,39),10)
        if family=='segway':
            rect((8,27,24,31),3);rect((7,25,11,35),10);rect((21,25,25,35),10)
            rect((9,28,10,32),11);rect((22,28,23,32),11);line([(16,14),(16,28)],15,2)
        # Rider helmet, shoulders, arms and separated legs, all protected.
        poly([(12,17),(10,23),(11,28),(14,25),(14,34),(18,34),(18,25),(21,28),(22,23),(20,17)],14)
        rect((12,20,13,24),15);rect((19,20,20,24),15)
        d.ellipse((12,10,20,18),fill=12);rect((13,14,19,16),8)
        if family=='skates':
            rect((10,31,14,41),3);rect((18,31,22,41),3)
            for xx in (9,14,17,22):
                for yy in (33,38):
                    rect((xx,yy,xx+2,yy+3),10);rect((xx+1,yy+1,xx+1,yy+2),11)
        if family=='moped':rect((12,30,20,35),3);rect((15,35,17,36),13)
    # Factory colors authored to match each source vehicle (RGB5).
    factory=((25,3,4),(3,24,26),(30,15,2),(27,24,4),(17,4,27),(29,27,18),
      (26,7,5),(3,8,19),(18,26,2),(19,23,25),(28,26,18),(4,18,28),
      (27,28,28),(11,13,4),(25,2,3),(28,22,8),(28,3,4),(30,16,2),
      (25,26,25),(5,24,17),(6,10,9),(16,3,25),(3,8,24),(3,21,28),
      (27,4,4),(3,12,29),(26,3,4),(4,20,12),(14,17,18),(27,9,4),
      (4,10,25),(17,3,29),(31,14,1),(2,25,26),(29,29,28),(3,10,29),
      (21,29,2),(28,29,28),(29,3,4),(2,8,24),(20,2,6),(26,27,28),
      (2,9,24),(29,3,4),(30,13,1),(6,24,2),(23,19,12),(5,6,8))[model]
    def rgb5(r,g,b):return 0x8000|max(0,min(31,r))|(max(0,min(31,g))<<5)|(max(0,min(31,b))<<10)
    palette=[0]
    for light in (10,17,24,28,30,34,40):palette.append(rgb5(*(min(31,c*light//31) for c in factory)))
    palette += [rgb5(3,6,8),rgb5(13,21,23),rgb5(2,3,4),rgb5(16,18,20),rgb5(30,30,27),rgb5(30,4,5),rgb5(12,8,6),rgb5(23,25,26)]
    # Preserve the source riders' outfits and helmets independently of the
    # painted equipment. These protected palette entries never take decals.
    outfits={3:(5,6,8),25:(3,10,25),26:(26,27,26),27:(9,13,5),
             28:(3,3,4),29:(27,3,4),43:(27,2,3),44:(3,4,5),45:(3,5,4)}
    if model in outfits:palette[14]=rgb5(*outfits[model])
    helmets={25:(29,24,5),26:(2,18,23),27:(28,27,23),28:(28,28,28),29:(3,4,5)}
    if model in helmets:palette[12]=rgb5(*helmets[model])
    if model==26:rect((14,20,18,27),13)
    # Stencils are fitted to each vehicle's full envelope. Glass, tires,
    # lights, rider, cargo bed, exhaust and hardware always clip them away.
    data=bytearray(im.tobytes())
    panels=[(at%32,at//32) for at,index in enumerate(data) if 1<=index<=7]
    pl=min(x for x,y in panels);pt=min(y for x,y in panels)
    pw=max(x for x,y in panels)-pl;ph=max(y for x,y in panels)-pt
    for at,index in enumerate(data):
        if 1<=index<=7:
            u=(at%32-pl)/max(1,pw);v=(at//32-pt)/max(1,ph)
            # Overhead uses physical panel coordinates, without the rear-view
            # novelty crop (the stencil is clipped directly to equipment).
            mask=decal_mask(model,u,v)
            data[at]=index|(mask<<4)
    review=Image.new('RGBA',(32,48))
    review.putdata([((palette[c&15]&31)*255//31,((palette[c&15]>>5)&31)*255//31,((palette[c&15]>>10)&31)*255//31,255) if c&15 else (0,0,0,0) for c in data])
    out=array.array('H',palette)
    if sys.byteorder!='little':out.byteswap()
    return bytes(data),out.tobytes(),review
