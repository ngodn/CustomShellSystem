"""Plot verified source cloth weights on the fitted sweater, in centimeters."""
import json
from pathlib import Path
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

work = Path(__file__).resolve().parents[2] / 'work/eve26'
source = json.loads((work / 'knit-cloth-map.json').read_text())
assert source['topology_identical']
points = np.asarray(source['points_m']) * 100
out = work / 'knit-cloth-map.png'
assert not out.exists()
fig, axes = plt.subplots(2, 2, figsize=(10, 12), constrained_layout=True)
for row, name in enumerate(('dForce Influence', 'dForce Pin')):
    weights = np.asarray(source['weights'][name])
    for col, horizontal in enumerate((0, 1)):
        ax = axes[row, col]
        plot = ax.scatter(points[:, horizontal], points[:, 2], c=weights, s=1,
            cmap='viridis', vmin=0, vmax=1, rasterized=True)
        ax.set(title=f'{name}, {"front projection" if horizontal == 0 else "side projection"}',
            xlabel=f'{"X" if horizontal == 0 else "Y"} (cm)', ylabel='Z (cm)')
        ax.set_aspect('equal')
    fig.colorbar(plot, ax=axes[row, :], label='Authored weight')
fig.suptitle('Knitwear source maps on verified fitted topology\nProjections include front and back surfaces; these are not Unreal distance masks.')
fig.savefig(out, dpi=140)
print(out)
