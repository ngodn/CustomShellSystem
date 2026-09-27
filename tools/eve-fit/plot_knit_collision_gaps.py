"""Plot measured body coverage gaps without changing the collision candidate."""
import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

work = Path(__file__).resolve().parents[2] / 'work/eve26'
data = json.loads((work / 'knit-coverage2.json').read_text())
points = np.asarray(data['points_cm'])
output = work / 'knit-coverage2.png'
assert not output.exists()
fig, axes = plt.subplots(1, 2, figsize=(10, 10), constrained_layout=True)
for axis, horizontal in zip(axes, (0, 1)):
    plot = axis.scatter(points[:, horizontal], points[:, 2], c=data['gap_cm'],
        s=5, cmap='magma', vmin=0, vmax=3.1)
    axis.set(title='Front projection' if horizontal == 0 else 'Side projection',
        xlabel='X (cm)' if horizontal == 0 else 'Y (cm)', ylabel='Z (cm)')
    axis.set_aspect('equal')
fig.colorbar(plot, ax=axes, label='Body surface outside sphere union (cm)')
fig.suptitle('Knitwear collision coverage at rest\nNearby body vertices only; front and rear surfaces overlap in projection')
fig.savefig(output, dpi=130)
print(output)
