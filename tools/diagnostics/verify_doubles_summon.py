"""Verify recorded Peony summon cases without treating them as visual acceptance."""
import argparse
import json
import math
from pathlib import Path


def verify(folder, form, mode):
    result = json.loads((folder / 'result.json').read_text())
    before = json.loads((folder / 'state-before.json').read_text())
    after = json.loads((folder / 'state-after.json').read_text())
    assert result['passed'] and before == after
    assert before['doubles'][form] == mode
    assert result['input_sent'] and result['key_observed'] and result['keys_released']
    assert result['resources_before']['Health'] == result['resources_after']['Health']
    assert result['state_unchanged'] and result['cine_unchanged'] and result['process_unchanged'] and result['view_unchanged']
    assert not result.get('error') and not result.get('cleanup_error')
    start, end = result['status_before'], result['status_after']
    assert end['active'] == end['cached'] == 0 and not end['error']
    assert end['fallbacks'] == start['fallbacks'] == 0
    assert end['prepared']-start['prepared'] == end['removed']-start['removed'] == (1 if mode == 'css' else 0)
    samples = [json.loads(line) for line in (folder / 'samples.jsonl').read_text().splitlines()]
    active = [a for row in samples for a in row['observed']['actors'] if a['active']]
    expected_id = 'AstralCopy' if form == 'faithful' else 'AstralClonePrimary'
    assert active and {a['id'] for a in active} == {expected_id}
    mesh = result['source']['components'][0]['mesh']['name']
    bones = 0
    for actor in active:
        assert len(actor['poses']) == (2 if mode == 'css' else 1)
        matches = [p for p in actor['poses'] if p['mesh']['name'] == mesh]
        assert len(matches) == (1 if mode == 'css' else 0)
        for pose in actor['poses']:
            assert all(name in pose['bones'] for name in ('pelvis','thigh_l','calf_l','foot_l','thigh_r','calf_r','foot_r','head'))
            for transform in pose['bones'].values():
                assert all(math.isfinite(x) for value in transform.values() for x in value.values())
                bones += 1
    checks = dict(form=form, mode=mode, active_observations=len(active), finite_bone_transforms=bones,
                  actor_kind=expected_id, mesh_selection=True, group_cleanup=True)
    if mode == 'css' and result.get('details'):
        visual = next(v for v in result['details'] if v['mesh']['name'] == mesh)
        source = result['source']['components'][0]['physics']
        assert visual['post']['class'].split(' ',1)[1] == source['animation_class']['name'].split(' ',1)[1]
        assert visual['collision'] == 0
        assert visual['flags']['bDisableClothSimulation'] == source['cloth_disabled']
        assert visual['flags']['bDisableRigidBodyAnimNode'] == source['rigid_body_disabled']
        mapping = {'frequency':'CSSBodyRegionFrequencies', 'damping':'CSSBodyRegionDampingRatios',
            'motion':'CSSBodyRegionMotionAmounts', 'enabled':'CSSBodyRegionEnabled',
            'global_frequency':'CSSBodyFrequency', 'global_damping':'CSSBodyDampingRatio',
            'global_motion':'CSSBodyMotionAmount', 'use_regions':'CSSBodyUseRegionSettings'}
        assert all(visual['inputs'][name] == source['body_rig'][key] for key,name in mapping.items())
        checks['body_input_fields_equal'] = len(mapping)
        parents = {m['parent']['name'] for m in visual['materials'] if m and m['parent']}
        checks['material_parent_count'] = len(parents)
        if form == 'stray':
            assert len(visual['materials']) == 46 and parents
            assert all('/Shared/Astral/Materials/' in name for name in parents)
            for name in ('Fabric','FlowFabric','FlowPanel','FlowStrap','SourceGlow','Stocking'):
                assert any(p.endswith('/M_PeonyV2_'+name+'.M_PeonyV2_'+name) for p in parents)
            checks['peony_ghost_templates'] = 6
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    rows = [verify(args.directory / (form+'-'+mode), form, mode)
            for form in ('faithful','stray') for mode in ('default','css')]
    output = dict(cases=rows, summon_checks_passed=True,
        scope='Peony, one Faithful copy and one Stray primary per press. Not secondary clone, cloth motion/contact, damage, all customizations or overall restoration proof.')
    print(json.dumps(output, indent=2))


if __name__ == '__main__':
    main()
