"""Soft-limit fixtures for BodyAngularV11 (-CSSBodyVersion=11).

Each fixture runs with SoftZone 0 (today's hard clamp) and 0.25 at 15, 30, 60,
120, 144 and 240 FPS, with region settings on so RegionTravel applies:

- tight: 2 degrees on every region, default tuning (2 Hz, 0.7, motion 1);
- each built-in template (Firm, Natural, Bouncy, Soft, Earthquake) with its
  rig tuning row and travel from native/src/physics_presets.cpp;
- stiff: 8 Hz, 0.7, motion 1, 2 degrees, where wL reaches its 1.5/h cap
  (60, 120 and 240 FPS only).

Motions: piecewise translation (10 cm triangle over 2 s), smooth translation
(10 sin^2(pi t)) and, beyond the design's two, a piecewise vertical bounce
(5 cm, 2.5 cycles/s). Each case lasts 6 s so the 4 s after the input stops
measure settling. Samples are lean: angles, limits, counters and rotations.

-CSSBodySoftZones=0,0.5 and -CSSBodyRates=120 run another zone set (named
sz<percent>) into controlrig-body-angular-v11-softlimit-<label>.json, with
-CSSBodyLabel naming it; the default run is unchanged.
"""
import json
from pathlib import Path
import sys

import unreal

sys.path.insert(0, str(Path(__file__).resolve().parent))
from body_rig_paths import MEASURED, stage
import probe_controlrig_body_timing as timed

# chest, glute, thigh, belly -> brust001/002, butt001/002, thigh_twist_02_l/r, belly
REGIONS = [0, 0, 1, 1, 2, 2, 3]
# {frequency, damping ratio, motion amount} per region, then travel degrees.
TEMPLATES = {
    'firm': ([(2.60, .65, .60), (2.50, .65, .65), (2.80, .72, .40), (2.70, .68, .45)], (4, 8, 3, 5)),
    'natural': ([(2.15, .48, 1.00), (2.10, .50, 1.00), (2.35, .58, .75), (2.20, .52, .85)], (6, 10, 4, 6)),
    'bouncy': ([(1.70, .28, 1.80), (1.65, .30, 1.80), (1.85, .38, 1.35), (1.75, .32, 1.50)], (10, 14, 6, 8)),
    'soft': ([(1.25, .18, 2.60), (1.20, .20, 2.50), (1.40, .25, 2.00), (1.30, .22, 2.20)], (14, 16, 7, 9)),
    'earthquake': ([(.85, .06, 4.20), (.85, .08, 4.20), (.95, .10, 3.50), (.90, .08, 3.80)], (20, 18, 10, 12)),
    'tight': ([(2., .7, 1.)]*4, (2, 2, 2, 2)),
    'stiff': ([(8., .7, 1.)]*4, (2, 2, 2, 2)),
}
MOTIONS = {'piecewise': {}, 'smooth': dict(smooth=True), 'bounce': dict(bounce=2.5, amplitude=5, axis=2)}
RATES = (15, 30, 60, 120, 144, 240)
ZONES = {'sz0': 0., 'sz25': .25}


def option(name, default):
    return next((a.split('=', 1)[1] for a in timed.ARGS if a.startswith(f'-CSSBody{name}=')), default)


def cases():
    zones = ZONES
    if option('SoftZones', ''):
        zones = {f'sz{round(float(z)*100)}': float(z) for z in option('SoftZones', '').split(',')}
    rates = tuple(int(r) for r in option('Rates', '').split(',')) if option('Rates', '') else None
    result = []
    for template, (tuning, travel) in TEMPLATES.items():
        expand = lambda column: [tuning[r][column] for r in REGIONS]
        for motion, extra in MOTIONS.items():
            for zone, value in zones.items():
                for fps in rates or ((60, 120, 240) if template == 'stiff' else RATES):
                    result.append(dict(name=f'{template}_{motion}_{zone}_{fps}', template=template, motion_kind=motion,
                        fps=fps, seconds=6, lean=True, region_settings=True, soft_zone=value,
                        region_travel=[float(travel[r]) for r in REGIONS],
                        tuning=dict(RegionFrequencies=expand(0), RegionDampingRatios=expand(1),
                                    RegionMotionAmounts=expand(2)), **extra))
    return result


def main():
    if timed.VERSION != 11:
        raise ValueError('Soft-limit fixtures require -CSSBodyVersion=11')
    output = stage()/f'controlrig-body-angular-v11-softlimit{"-"+timed.LABEL if timed.LABEL else ""}.json'
    if output.exists():
        raise FileExistsError(output)
    bp = unreal.load_asset(timed.PACKAGE)
    if bp is None:
        raise RuntimeError('Saved V11 body rig is missing')
    names = [h['bone'] for h in json.loads((MEASURED/'body-helper-layout-v1.json').read_text())['helpers']]
    selected = cases()
    selection = next((a.split('=', 1)[1] for a in timed.ARGS if a.startswith('-CSSBodyCases=')), '')
    if selection:
        prefixes = selection.split(',')
        selected = [c for c in selected if any(c['name'].startswith(p) for p in prefixes)]
    results = []
    for case in selected:
        results.append(timed.evaluate(bp, names, case))
        unreal.log('Soft-limit fixture complete: '+case['name'])
    output.write_text(json.dumps(dict(package=timed.PACKAGE, helpers=names, cases=results,
        scope='Direct timed V11 rig, soft-limit fixtures. No AnimBP, cook or live acceptance.'))+'\n')


if __name__ == '__main__':
    main()
