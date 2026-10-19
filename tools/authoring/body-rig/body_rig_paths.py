"""Where the copied body rig tools read from and write to.

The unchanged helpers (graph wrapper, quaternion nodes, planes, contacts, probe
utilities) stay in the SeduXtress authoring library and are imported from
there read-only. Everything this directory writes goes to CSS_BODY_RIG_STAGE.
"""
import os
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
AUTHORING = Path(os.environ.get('CSS_AUTHORING_TOOLS',
    REPO.parent/'CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/tools'))
# Measured layouts and envelopes the builders read (never written here).
MEASURED = AUTHORING.parent/'work/nextgen-audit'

if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))
if str(AUTHORING) not in sys.path:
    sys.path.append(str(AUTHORING))


def stage():
    value = os.environ.get('CSS_BODY_RIG_STAGE')
    if not value:
        raise RuntimeError('Set CSS_BODY_RIG_STAGE to the private output directory')
    path = Path(value)
    path.mkdir(parents=True, exist_ok=True)
    return path
