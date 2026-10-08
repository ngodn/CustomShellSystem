"""Read the player's post-process owner without changing gameplay or customization."""
import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from css import GAME, atomic, wait_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--catalog', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    mod = GAME / 'Binaries/Win64/ue4ss/Mods/CustomShellSystem'

    def probe(**request):
        rid = str(time.time_ns())
        atomic(mod / 'request.json', dict(id=rid, action='css_probe', request=request))
        reply = wait_json(mod / 'runtime/css-probe.json', lambda j: j.get('id') == rid)
        if not reply['ok']:
            raise RuntimeError(reply['error'])
        return reply['result']

    def get(target, property):
        return probe(op='get', target=target, property=property)

    def path(handle, key='name'):
        return handle[key].split(' ', 1)[1] if handle else ''

    player = probe(op='player')
    mesh = get(player['pawn'], 'Mesh')
    asset = probe(op='call', target=mesh, function='GetSkeletalMeshAsset', args={})['ReturnValue']
    authored = get(asset, 'PostProcessAnimBlueprint')
    post = get(mesh, 'PostProcessAnimInstance')
    override = get(mesh, 'OverridePostProcessAnimBP')
    skeleton = get(asset, 'Skeleton')
    status = json.loads((mod / 'runtime/status.json').read_text())
    library = json.loads(args.catalog.read_text())['catalog']['animation_library']
    adapters = [a for a in library['adapters'] if
                a['source_class'] == path(authored) and a['skeleton'] == path(skeleton)]
    allowed = {path(authored), *(a['target_class'] for a in adapters)}
    matched = path(post, 'class') in allowed
    result = dict(player=player, mesh=mesh, asset=asset, authored=authored,
                  post=post, override=override, skeleton=skeleton, allowed_classes=sorted(allowed),
                  owner_matches=matched, animation=status.get('animation'),
                  hidden_materials=get(mesh, 'LODInfo'))
    # Ensure a user-driven appearance switch did not mix two captures.
    current_asset = probe(op='call', target=mesh, function='GetSkeletalMeshAsset', args={})['ReturnValue']
    result['stable_capture'] = current_asset == asset and get(mesh, 'PostProcessAnimInstance') == post
    if path(post, 'class') in {a['target_class'] for a in library['adapters']}:
        result['performance'] = {k: get(post, k) for k in
                                 ('CSSPerformanceEnabled', 'CSSPerformanceWeight', 'CSSPerformanceTime')}
    atomic(args.output, result)
    print(json.dumps({k: result[k] for k in ('owner_matches', 'stable_capture', 'allowed_classes')}, indent=2))
    return 0 if matched and result['stable_capture'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
