"""Plot fitted collider spheres against the unchanged body and tail proxy."""
import json
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Circle

w=Path(__file__).resolve().parents[2]/'work/eve26'
spheres=json.loads((w/'planet-tail-spheres.json').read_text())['spheres']
mesh=json.loads((w/'planet-export/planet.mesh.json').read_text())
audit=json.loads((w/'planet-export/planet.mesh.audit.json').read_text())
body=[p for p in mesh['points'][:audit['parts'][0]['points']] if 35<p[2]<125]
proxy=json.loads((w/'planet-tail-proxy.json').read_text())['positions_cm']
fig,axes=plt.subplots(1,2,figsize=(9,8))
for ax,(axis,label) in zip(axes,[(0,'Back X'),(1,'Side Y')]):
    ax.scatter([p[axis] for p in body],[p[2] for p in body],s=.3,c='#888888',alpha=.2)
    for sphere in spheres:
        c=sphere['center_cm'];ax.add_patch(Circle((c[axis],c[2]),sphere['radius_cm'],color='#5588aa',alpha=.2))
    ax.plot([p[axis] for p in proxy[::2]],[p[2] for p in proxy[::2]],color='#bd5511',linewidth=2)
    ax.set_aspect('equal');ax.autoscale();ax.set_xlabel(label+' (cm)');ax.set_ylabel('Z (cm)')
fig.suptitle('Prototype tail collision trial: body unchanged; coverage incomplete')
fig.tight_layout();fig.savefig(w/'planet-tail-collider.png',dpi=140)
