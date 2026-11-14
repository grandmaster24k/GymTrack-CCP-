"""Optional asset builder. Requires Pillow only; not needed to build or run the C app.

The checked-in SVG is the editable vector master. This deterministic raster
version uses the same geometry to produce Windows sizes with clean alpha edges.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter
import math

HERE = Path(__file__).resolve().parent
S = 4
N = 256 * S


def rect_mask(box, radius):
    mask = Image.new("L", (N, N))
    ImageDraw.Draw(mask).rounded_rectangle(tuple(round(v*S) for v in box), radius=round(radius*S), fill=255)
    return mask


def gradient(stops, diagonal=False):
    img = Image.new("RGBA", (N, N))
    data = []
    for y in range(N):
        for x in range(N):
            t = (x+y)/(2*(N-1)) if diagonal else y/(N-1)
            lo, hi = stops[0], stops[-1]
            for a,b in zip(stops,stops[1:]):
                if a[0] <= t <= b[0]:
                    lo,hi=a,b
                    break
            f=(t-lo[0])/(hi[0]-lo[0])
            data.append(tuple(round(lo[1][i]*(1-f)+hi[1][i]*f) for i in range(3))+(255,))
    img.putdata(data)
    return img


base = Image.new("RGBA", (N,N))
# A genuinely round material badge: alpha remains clear outside the disc.
tile = rect_mask((12,12,244,244),116)
base.paste(gradient([(0,(44,70,108)),(.52,(24,35,64)),(1,(39,25,66))],True),(0,0),tile)
glow = Image.new("RGBA", (N,N))
gd = ImageDraw.Draw(glow)
gd.ellipse((18*S,18*S,157*S,163*S),fill=(52,164,255,104))
gd.ellipse((127*S,122*S,253*S,248*S),fill=(157,95,255,100))
glow=glow.filter(ImageFilter.GaussianBlur(31*S))
glow.putalpha(Image.composite(glow.getchannel("A"),Image.new("L",(N,N)),tile))
base=Image.alpha_composite(base,glow)
bd=ImageDraw.Draw(base)
bd.ellipse((12*S,12*S,244*S,244*S),outline=(134,166,214,125),width=2*S)
# Soft reflection under the surface, and a fine specular rim over it.
reflection=Image.new("RGBA",(N,N))
rd=ImageDraw.Draw(reflection)
rd.arc((20*S,18*S,236*S,235*S),202,302,fill=(207,235,255,110),width=5*S)
reflection=reflection.filter(ImageFilter.GaussianBlur(4*S))
reflection.putalpha(Image.composite(reflection.getchannel("A"),Image.new("L",(N,N)),tile))
base=Image.alpha_composite(base,reflection)
bd=ImageDraw.Draw(base)
bd.arc((17*S,17*S,239*S,239*S),201,292,fill=(211,237,255,155),width=1*S)
bd.arc((18*S,18*S,238*S,238*S),22,95,fill=(176,142,255,108),width=1*S)

metal=gradient([(0,(205,249,255)),(.25,(112,214,255)),(.45,(60,159,255)),(.65,(105,119,253)),(1,(172,95,234))],True)
bar=Image.new("RGBA",(N,N))
for box,radius in [((72,116,184,140),8),((51,88,73,168),9),((74,76,103,180),11),((153,76,182,180),11),((183,88,205,168),9),((43,119,51,137),4),((205,119,213,137),4)]:
    bar.paste(metal,(0,0),rect_mask(box,radius))
draw=ImageDraw.Draw(bar)
for box in [(80,82,95,88),(159,82,174,88),(57,94,66,100)]:
    draw.arc(tuple(v*S for v in box),180,270,fill=(220,250,255,210),width=2*S)
draw.line((109*S,121*S,147*S,121*S),fill=(216,241,255,170),width=2*S)
# Pillow's positive rotation matches the SVG's upward-right diagonal in screen coordinates.
bar=bar.rotate(35,resample=Image.Resampling.BICUBIC)
# A little more breathing room inside the circular outline.
inset=bar.resize((232*S,232*S),Image.Resampling.LANCZOS)
bar=Image.new("RGBA",(N,N));bar.alpha_composite(inset,(12*S,12*S))
shadow=Image.new("RGBA",(N,N),(0,5,16,0))
shadow.putalpha(bar.getchannel("A").filter(ImageFilter.GaussianBlur(6*S)).point(lambda a:round(a*.65)))
offset=Image.new("RGBA",(N,N)); offset.alpha_composite(shadow,(0,6*S))
base=Image.alpha_composite(Image.alpha_composite(base,offset),bar)
icon=base.resize((256,256),Image.Resampling.LANCZOS)
icon.save(HERE/"gymtrack.png")
icon.save(HERE/"gymtrack.ico",sizes=[(n,n) for n in (16,24,32,48,64,128,256)])
print("Wrote gymtrack.png and gymtrack.ico")
