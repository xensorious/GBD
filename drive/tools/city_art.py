"""Crop original overhead coastal scenery into the bounded DS sprite bank."""
from PIL import Image

PROP_BOXES=((0,0,324,318),(326,0,644,315),(645,0,963,310),(966,0,1286,318),
 (0,319,327,591),(344,326,633,592),(645,311,966,598),(965,319,1286,597),
 (81,598,258,950),(415,635,563,950),(731,603,888,951),(1038,584,1189,952),
 (0,955,410,1223),(412,956,680,1223),(710,954,911,1223),(928,955,1286,1223))

def prop_sprites(source):
    result=[]
    for i,box in enumerate(PROP_BOXES):
        im=source.crop(box).convert('RGBA')
        # Isolated alpha specks from atlas generation are not scenery.
        alpha=im.getchannel('A').point(lambda a:255 if a>=150 else 0)
        im.putalpha(alpha);bounds=alpha.getbbox();im=im.crop(bounds)
        size=(46,46) if i<8 else (24,46) if i<12 else (46,36) if i==12 else (44,44)
        im.thumbnail(size,Image.Resampling.LANCZOS)
        canvas=Image.new('RGBA',(48,48));canvas.alpha_composite(im,((48-im.width)//2,(48-im.height)//2))
        canvas.putalpha(canvas.getchannel('A').point(lambda a:255 if a>=125 else 0))
        result.append(canvas)
    return result
