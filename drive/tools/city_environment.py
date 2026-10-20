"""Pack original coastal architecture, living gardens and seamless lawn art."""
from PIL import Image
from collections import deque
import array,hashlib,json,math,sys

def components(alpha):
    width,height=alpha.size;data=alpha.tobytes();seen=bytearray(len(data));result=[]
    for at,value in enumerate(data):
        if value<150 or seen[at]:continue
        queue=[at];seen[at]=1;points=[];left=right=at%width;top=bottom=at//width
        while queue:
            p=queue.pop();points.append(p);x=p%width;y=p//width
            left=min(left,x);right=max(right,x);top=min(top,y);bottom=max(bottom,y)
            for n in (p-1 if x else -1,p+1 if x+1<width else -1,p-width if y else -1,p+width if y+1<height else -1):
                if n>=0 and not seen[n] and data[n]>=150:seen[n]=1;queue.append(n)
        result.append((len(points),(left,top,right+1,bottom+1),points))
    return sorted(result,reverse=True)

def isolated(im,keep_parts=False):
    im=im.convert('RGBA');groups=components(im.getchannel('A'));assert groups
    mask=bytearray(im.width*im.height);largest=groups[0][0]
    for size,box,points in groups:
        if size<max(30,largest//18) or (not keep_parts and size!=largest):continue
        for at in points:mask[at]=255
    alpha=Image.frombytes('L',im.size,bytes(mask));im.putalpha(alpha)
    return im.crop(alpha.getbbox())

def bank(images,width,height):
    sheet=Image.new('RGBA',(width*4,height*((len(images)+3)//4)))
    for n,im in enumerate(images):sheet.paste(im,(n%4*width,n//4*height))
    q=sheet.convert('RGB').quantize(colors=255,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
    alpha=sheet.getchannel('A').tobytes();indices=bytes(v+1 if alpha[i]>=110 else 0 for i,v in enumerate(q.tobytes()))
    pal=q.getpalette();words=array.array('H',[0]+[0x8000|(pal[i*3]>>3)|((pal[i*3+1]>>3)<<5)|((pal[i*3+2]>>3)<<10) for i in range(255)])
    if sys.byteorder!='little':words.byteswap()
    data=bytearray()
    for n in range(len(images)):
        for row in range(height):
            at=(n//4*height+row)*sheet.width+n%4*width;data.extend(indices[at:at+width])
    return bytes(data),words.tobytes(),sheet

def environment(art,out,raw,rgb555):
    source=Image.open(art/'city/buildings-source.png').convert('RGBA');buildings=[None]*16;boxes=[]
    # Discover complete buildings inside each column: generated gutters need
    # not coincide with exact quarter-height boundaries.
    for col in range(4):
        x=col*source.width//4;end=(col+1)*source.width//4
        stripe=source.crop((x,0,end,source.height));groups=components(stripe.getchannel('A'))[:4]
        assert len(groups)==4
        for row,(_,box,_) in enumerate(sorted(groups,key=lambda group:group[1][1])):
            im=isolated(stripe.crop(box));model=row*4+col
            # This is the map's architectural envelope, including a narrow
            # projecting facade; padding is bounded to match the solid lot.
            im=im.resize((60,76),Image.Resampling.LANCZOS)
            tile=Image.new('RGBA',(64,80));tile.alpha_composite(im,(2,2));tile.putalpha(tile.getchannel('A').point(lambda a:255 if a>=110 else 0))
            buildings[model]=tile;boxes.append({'style':model,'box':[box[0]+x,box[1],box[2]+x,box[3]]})
    data,palette,review=bank(buildings,64,80)
    raw('city_building_idx',data,'uint8_t',16*64*80);raw('city_building_palette',palette,'uint16_t',256)
    review.save(out/'city-buildings.png')
    plants=[];source=Image.open(art/'city/gardens-source.png').convert('RGBA')
    for model in range(16):
        x=model%4;y=model//4
        im=isolated(source.crop((x*source.width//4,y*source.height//4,(x+1)*source.width//4,(y+1)*source.height//4)),True)
        im.thumbnail((30,30),Image.Resampling.LANCZOS);tile=Image.new('RGBA',(32,32));tile.alpha_composite(im,((32-im.width)//2,(32-im.height)//2))
        tile.putalpha(tile.getchannel('A').point(lambda a:255 if a>=110 else 0));plants.append(tile)
    # A second canopy pose has fixed outer rows/base. Rooted planters never
    # bounce; only the upper foliage shears by a single texel in the breeze.
    for original in plants[:8]:
        moved=Image.new('RGBA',(32,32));src=original.load();dst=moved.load()
        for y in range(32):
            shift=1 if 4<y<23 else 0
            for x in range(32):dst[x,y]=src[max(0,x-shift),y]
        plants.append(moved)
    data,palette,review=bank(plants,32,32)
    raw('city_garden_idx',data,'uint8_t',24*32*32);raw('city_garden_palette',palette,'uint16_t',256)
    review.save(out/'city-gardens.png')
    source=Image.open(art/'city/grass-source.png').convert('RGB').resize((128,128),Image.Resampling.LANCZOS).crop((0,48,128,80))
    values=list(source.getdata());mean=[sum(c[i] for c in values)/len(values) for i in range(3)];target=(100,145,78)
    lawn=Image.new('RGB',(128,32));lawn.putdata([tuple(max(0,min(255,round(target[i]+(c[i]-mean[i])*.85))) for i in range(3)) for c in values])
    # Feather opposing edges so a moving camera cannot reveal a tile seam.
    px=lawn.load()
    for axis,size,other in ((0,128,32),(1,32,128)):
        for pos in range(4):
            blend=(4-pos)/4
            for cross in range(other):
                a=(pos,cross) if axis==0 else (cross,pos);b=(size-1-pos,cross) if axis==0 else (cross,size-1-pos)
                ca,cb=px[a],px[b];average=tuple((ca[i]+cb[i])/2 for i in range(3))
                px[a]=tuple(round(ca[i]*(1-blend)+average[i]*blend) for i in range(3));px[b]=tuple(round(cb[i]*(1-blend)+average[i]*blend) for i in range(3))
    repeat=Image.new('RGB',(384,32))
    for copy in range(3):repeat.paste(lawn,(copy*128,0))
    raw('city_lawn_rgb',rgb555(repeat),'uint16_t',384*32);lawn.save(out/'city-lawn.png')
    (out/'city-environment-manifest.json').write_text(json.dumps({'buildings':16,'garden_objects':16,'wind_poses':8,
        'building_size':[64,80],'garden_size':[32,32],'lawn_period':[128,32],'building_boxes':sorted(boxes,key=lambda b:b['style']),
        'source_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((art/'city').glob('*-source.png'))}},indent=2)+'\n')
