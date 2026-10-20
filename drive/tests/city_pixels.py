"""Actual-frame road leakage checks, independent of the GPU's depth flags.

Project the fixed map into each captured camera, erode street boundaries, and
exclude raised buildings/vehicles. Sidewalk/curb material inside the remaining
asphalt is a visible failure even when the DS reports no geometry overflow.
"""
import ctypes as C
import numpy as np

class Road(C.Structure):
 _fields_=[(n,C.c_int16) for n in ('x1','y1','x2','y2')]+[('half',C.c_uint8),('highway',C.c_uint8)]
class Lot(C.Structure):
 _fields_=[(n,C.c_int) for n in ('x','y','w','h','kind')]
class Shape(C.Structure):
 _fields_=[(n,C.c_uint8) for n in ('half_width','half_length','draw_half_width','draw_half_length')]

class RoadPixels:
 def __init__(self,lib,baseline=False):
  self.lib=lib;self.baseline=baseline;self.zoom=1.0 if baseline else 7/8
  self.roads=list((Road*16).in_dll(lib,'city_roads'))
  lib.city_lot_bounds.argtypes=[C.c_int,C.c_int];lib.city_lot_bounds.restype=Lot
  lib.city_vehicle_shape.argtypes=[C.c_int];lib.city_vehicle_shape.restype=Shape
  self.y,self.x=np.indices((192,256));self.previous=None
  self.frames=0;self.samples=0;self.leaks=0;self.material_pixels=0;self.worst=0;self.worst_image=None
 def camera(self,c):
  cx,cy=c.camera_x/256,c.camera_y/256
  if self.baseline:cx,cy=int(cx),int(cy)
  return cx,cy,self.lib.city_cos(c.angle)/256,self.lib.city_sin(c.angle)/256
 def road_mask(self,cam):
  cx,cy,cs,sn=cam;right=(self.x+.5-128)/self.zoom;down=(self.y+.5-112)/(self.zoom*222/256)
  norm=cs*cs+sn*sn;wx=cx+(right*cs-down*sn)/norm;wy=cy+(right*sn+down*cs)/norm
  mask=np.zeros((192,256),dtype=bool)
  for r in self.roads:
   mask|=(wx>r.x1-r.half+10)&(wx<r.x2+r.half-10)&(wy>r.y1-r.half+10)&(wy<r.y2+r.half-10)
  return mask
 def project(self,cam,x,y,h=0):
  cx,cy,cs,sn=cam
  return 128+((x-cx)*cs+(y-cy)*sn)*self.zoom,112+((-(x-cx)*sn+(y-cy)*cs)*222/256-h/2)*self.zoom
 def omit(self,mask,points,pad=5):
  xs,ys=zip(*points)
  x1=max(0,int(min(xs))-pad);x2=min(256,int(max(xs))+pad+1)
  y1=max(0,int(min(ys))-pad);y2=min(192,int(max(ys))+pad+1)
  if x2>x1 and y2>y1:mask[y1:y2,x1:x2]=False
 def sample(self,e,g,c):
  cam=self.camera(c);mask=self.road_mask(cam)
  # Intersect adjacent observed views to tolerate the one-LCD-frame graphics
  # pipeline, rather than counting the edge of a moving curb as road leakage.
  if self.previous is not None:mask&=self.road_mask(self.previous)
  mask[:25]=False
  for view in (cam,self.previous if self.previous is not None else cam):
   cx,cy,_,_=view
   for row in range(max(0,int(cy-300)//128-1),min(31,int(cy+300)//128+1)+1):
    for col in range(max(0,int(cx-256)//128-1),min(31,int(cx+256)//128+1)+1):
     lot=self.lib.city_lot_bounds(col,row)
     if lot.kind:
      self.omit(mask,[self.project(view,x,y,h) for x in (lot.x-3,lot.x+lot.w+3) for y in (lot.y-3,lot.y+lot.h+3) for h in (0,60)])
   actors=[(c.x/256,c.y/256,g.vehicle)]+[(car.x,car.y,car.model) for car in c.cars]
   for x,y,model in actors:
    if abs(x-cx)>300 or abs(y-cy)>350:continue
    shape=self.lib.city_vehicle_shape(model)
    radius=31 if self.baseline else shape.draw_half_length+shape.draw_half_width
    # A conservative square also covers previous positions and contact shadow.
    self.omit(mask,[self.project(view,x+dx,y+dy,h) for dx in (-radius,radius) for dy in (-radius,radius) for h in (0,5)],6)
  pixels=np.asarray(e.image())[:192,:,:3]
  # Exact untextured RGB555 sidewalk and curb colors after the core's 565
  # conversion; ordinary white/yellow lane paint is deliberately distinct.
  leakage=np.zeros((192,256),dtype=bool)
  for color in ((180,190,164),(115,149,139)):
   leakage|=np.all(pixels==color,axis=2)
  count=int(np.count_nonzero(mask&leakage))
  self.material_pixels+=int(np.count_nonzero(leakage))
  self.frames+=1;self.samples+=int(np.count_nonzero(mask));self.leaks+=count
  if count>self.worst or self.worst_image is None:self.worst=count;self.worst_image=e.image().copy()
  self.previous=cam
 def result(self,output,strict=True):
  assert self.frames>150 and self.samples>20000,('Too few actual road samples',self.frames,self.samples)
  assert self.material_pixels>1000,('Underlay classifier never saw the actual material',self.material_pixels)
  if strict:assert self.worst<=4 and self.leaks<=self.samples//2000,('Road material flashing/leakage',self.worst,self.leaks,self.samples)
  self.worst_image.save(output/'city-road-stability.png')
  return {'frames':self.frames,'interior_asphalt_pixels':self.samples,'sidewalk_curb_pixels':self.leaks,'observed_material_pixels':self.material_pixels,
          'worst_frame_pixels':self.worst,'camera_zoom':self.zoom,'passed':self.worst<=4 and self.leaks<=self.samples//2000}
