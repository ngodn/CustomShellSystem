"""Show measured collision sphyls against the unchanged skirt proxy."""
import json
import math
from pathlib import Path
import bpy
from mathutils import Vector

WORK = Path(__file__).resolve().parents[2] / 'work/eve26'
proxy = json.loads((WORK/'skirt-surface.json').read_text())
audit = json.loads((WORK/'skirt-collision-fit.json').read_text())
out = WORK/'collision-views'
out.mkdir(exist_ok=False)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
def point(p):
    return Vector((p[0]/100,-p[1]/100,p[2]/100))
mesh = bpy.data.meshes.new('Skirt')
tri = proxy['indices']
mesh.from_pydata([point(p) for p in proxy['positions']],[],[list(reversed(tri[i:i+3])) for i in range(0,len(tri),3)])
obj = bpy.data.objects.new('Skirt',mesh)
bpy.context.collection.objects.link(obj)
obj.color = (.1,.4,.8,1)
for face in mesh.polygons:
    face.use_smooth = True
for capsule in audit['capsules']:
    a,b = [point(p) for p in capsule['endpoints_cm']]
    axis = (b-a).normalized()
    u = axis.cross(Vector((0,0,1)))
    if u.length < .01:
        u = axis.cross(Vector((1,0,0)))
    u.normalize()
    v = axis.cross(u).normalized()
    radius = capsule['radius_cm']/100
    curve = bpy.data.curves.new(capsule['bone'],'CURVE')
    curve.dimensions = '3D'
    curve.bevel_depth = .0008
    for center,sign in [(a,-1),(b,1)]:
        for latitude in (0,math.pi/6,math.pi/3):
            ring = curve.splines.new('POLY')
            ring.points.add(47)
            for i,p in enumerate(ring.points):
                angle = i*2*math.pi/48
                pos = center+axis*(sign*radius*math.sin(latitude))+(u*math.cos(angle)+v*math.sin(angle))*(radius*math.cos(latitude))
                p.co = (*pos,1)
            ring.use_cyclic_u = True
    for angle in (0,math.pi/2,math.pi,3*math.pi/2):
        direction = u*math.cos(angle)+v*math.sin(angle)
        line = curve.splines.new('POLY')
        line.points.add(1)
        for p,center in zip(line.points,(a,b)):
            p.co = (*(center+direction*radius),1)
    obj = bpy.data.objects.new(capsule['bone'],curve)
    bpy.context.collection.objects.link(obj)
    obj.color = (1,.1,.03,1)
s = bpy.context.scene
s.render.engine = 'BLENDER_WORKBENCH'
s.display.shading.color_type = 'OBJECT'
s.display.shading.show_cavity = True
s.render.resolution_x = s.render.resolution_y = 900
s.render.resolution_percentage = 100
cam = bpy.data.objects.new('Camera',bpy.data.cameras.new('Camera'))
bpy.context.collection.objects.link(cam)
s.camera = cam
cam.data.type = 'ORTHO'
cam.data.ortho_scale = 1.05
for name,direction in [('front',(0,-3,0)),('side',(3,0,0))]:
    center = Vector((0,0,1.03))
    cam.location = center+Vector(direction)
    cam.rotation_euler = (center-cam.location).to_track_quat('-Z','Y').to_euler()
    s.render.filepath = str(out/(name+'.png'))
    bpy.ops.render.render(write_still=True)
