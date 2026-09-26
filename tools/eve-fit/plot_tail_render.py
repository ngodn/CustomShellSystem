"""Plot native tail render-mapping replay from back and side."""
import argparse
import json
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.collections import PolyCollection
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--input',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
assert not a.output.exists()
data=json.loads(a.input.read_text());assert len(data['sections'])==1
section=data['sections'][0];points=np.array(section['positions_cm']);faces=np.array(section['indices']).reshape(-1,3)
fig,axes=plt.subplots(1,2,figsize=(8,8))
for ax,(axis,label) in zip(axes,[(0,'X'),(1,'Y')]):
    ax.add_collection(PolyCollection(points[faces][:,:,[axis,2]],facecolor='#aa8844',edgecolor='#403520',linewidth=.1))
    ax.autoscale();ax.set_aspect('equal');ax.set_xlabel(label+' cm');ax.set_ylabel('Z cm')
fig.suptitle('Prototype tail, native mapping replay, frame '+str(data['frame']))
fig.tight_layout();fig.savefig(a.output,dpi=140)
