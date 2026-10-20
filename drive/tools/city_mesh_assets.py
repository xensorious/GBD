"""Orthographic wall elevations and overhead materials for real city geometry."""
from PIL import Image, ImageDraw, ImageFont
import array, hashlib, json, random, sys


def mesh_assets(art, out, raw):
    sheet = Image.new('RGBA', (256, 512))
    walls = Image.open(art/'city/facades-source.png').convert('RGB')
    colors = [(191,96,55),(58,126,137),(197,102,64),(219,225,218),
              (194,204,199),(213,192,172),(208,215,209),(182,91,47),
              (215,193,180),(215,227,217),(102,135,147),(182,91,54),
              (89,134,152),(207,173,142),(77,126,113),(106,124,134)]
    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 6)
    signs = ('','','','','CAFE','BLOOM','SUNMART','BAKERY',
             'POSTCARD','','','HOTEL','MARINA','QUAYSIDE','GARDENS','AUTO CARE')
    for style in range(16):
        col,row=style%4,style//4
        face=walls.crop((col*walls.width//4,row*walls.height//4,
                         (col+1)*walls.width//4,(row+1)*walls.height//4))
        face=face.resize((64,32), Image.Resampling.LANCZOS).convert('RGBA')
        if signs[style]:
            d=ImageDraw.Draw(face);text=signs[style];length=d.textbbox((0,0),text,font=font)[2]
            d.rectangle((31-length//2-2,1,33+length//2,8),fill=(32,74,76))
            d.text((32-length//2,1),text,font=font,fill=(248,236,199))
        sheet.paste(face,(col*64,row*32))
        # Separate straight wall and roof materials carry no baked 3/4 angle.
        sheet.paste(face.resize((32,32),Image.Resampling.LANCZOS),(style%8*32,192+style//8*32))
        roof=Image.new('RGBA',(32,32),colors[style]);d=ImageDraw.Draw(roof)
        rng=random.Random(style+90210)
        for y in range(32):
            for x in range(32):
                n=rng.randrange(-7,8)
                roof.putpixel((x,y),tuple(max(0,min(255,v+n)) for v in colors[style])+(255,))
        if style in (0,1,2,7,11,12):
            for y in range(0,32,4):
                d.line((0,y,31,y),fill=tuple(max(0,v-30) for v in colors[style]))
                d.line((0,y+1,31,y+1),fill=tuple(min(255,v+18) for v in colors[style]))
                for x in range((y//4%2)*4,32,8):
                    d.line((x,y,x,y+3),fill=tuple(max(0,v-20) for v in colors[style]))
        elif style==14:
            for p in range(0,32,8):
                d.line((p,0,p,31),fill=(190,218,192),width=2)
                d.line((0,p,31,p),fill=(190,218,192),width=2)
        else:
            d.rectangle((1,1,30,30),outline=tuple(min(255,v+25) for v in colors[style]))
            d.rectangle((5,5,13,13),fill=(143,161,160),outline=(83,106,110))
            for y in range(7,13,2):d.line((6,y,12,y),fill=(188,200,191))
            d.rectangle((20,19,27,26),fill=(78,115,129),outline=(221,227,211))
        sheet.paste(roof,(style%8*32,128+style//8*32))
    props=Image.open(out/'city-props.png').convert('RGBA')
    for p in range(5):
        sheet.paste(props.crop((p%4*48,p//4*48,p%4*48+48,p//4*48+48)),(p*48,256))
    garden=Image.open(out/'city-gardens.png').convert('RGBA')
    for p in range(24):
        sheet.paste(garden.crop((p%4*32,p//4*32,p%4*32+32,p//4*32+32)),(p%8*32,304+p//8*32))
    for p in range(8):
        n=p+8
        sheet.paste(props.crop((n%4*48,n//4*48,n%4*48+48,n//4*48+48)),(p%4*48,400+p//4*48))
    sheet.paste(Image.open(out/'city-lawn.png').convert('RGBA').resize((64,32),Image.Resampling.LANCZOS),(192,400))
    rng=random.Random(932)
    for origin,base in [((192,432),(56,68,76)),((224,432),(179,184,166)),((192,464),(40,132,155))]:
        w=64 if origin[1]==464 else 32
        tile=Image.new('RGBA',(w,32),base);d=ImageDraw.Draw(tile)
        for y in range(32):
            for x in range(w):
                n=rng.randrange(-5,6)
                tile.putpixel((x,y),tuple(max(0,min(255,c+n)) for c in base)+(255,))
        if origin==(224,432):
            for p in range(0,32,16):
                d.line((0,p,31,p),fill=(144,156,146));d.line((p,0,p,31),fill=(144,156,146))
        if origin[1]==464:
            for x,y in ((2,6),(23,23),(45,12)):d.line((x,y,x+9,y),fill=(69,157,173))
        sheet.paste(tile,origin)
    # Uppercase labels occupy a spare strip, so the 3D scene can also carry
    # its small caption without reserving a second engine/framebuffer.
    d=ImageDraw.Draw(sheet)
    label_font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',9)
    for letter in range(26):
        d.text((letter*8,495),chr(65+letter),font=label_font,fill=(255,255,255,255))
    # VRAM A holds exactly one 128 KiB indexed atlas. Zero is transparent.
    q=sheet.convert('RGB').quantize(colors=255,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
    alpha=sheet.getchannel('A').tobytes()
    data=bytes(v+1 if alpha[i]>=110 else 0 for i,v in enumerate(q.tobytes()))
    pal=q.getpalette()
    palette=array.array('H',[0]+[0x8000|(pal[i*3]>>3)|((pal[i*3+1]>>3)<<5)|((pal[i*3+2]>>3)<<10) for i in range(255)])
    if sys.byteorder!='little':palette.byteswap()
    raw('city_mesh_idx',data,'uint8_t',256*512)
    raw('city_mesh_palette',palette.tobytes(),'uint16_t',256)
    sheet.save(out/'city-mesh-textures.png')
    (out/'city-mesh-manifest.json').write_text(json.dumps({'size':[256,512], 'facades':16,
        'roof_materials':16,'native_geometry':True,'source_sha256':hashlib.sha256((art/'city/facades-source.png').read_bytes()).hexdigest()},indent=2)+'\n')
