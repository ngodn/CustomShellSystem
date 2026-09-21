"""Drive the portable extension runtime through ctypes: legacy ABI 1/2 and ABI 3
native fixtures, Lua examples, budgets, suspension, hook cleanup, retryable stop.
Usage: lifecycle_check.py RUNTIME_SO FIXTURE1 FIXTURE2 FIXTURE3 FIXTURE4 EXAMPLES_DIR
"""
import ctypes as c, json, os, shutil, sys, tempfile
from pathlib import Path

if len(sys.argv) != 7:
    raise SystemExit(__doc__)
SINK = c.CFUNCTYPE(None, c.c_void_p, c.c_void_p, c.c_size_t)
REQUEST = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_char_p, SINK, c.c_void_p)


class Frame(c.Structure):
    _fields_ = [('abi', c.c_uint32), ('size', c.c_uint32), ('seconds', c.c_double), ('world_generation', c.c_uint32),
                ('world_ready', c.c_int32), ('in_menu', c.c_int32), ('camera_yaw', c.c_double),
                ('player_x', c.c_double), ('player_y', c.c_double), ('player_z', c.c_double), ('player_yaw', c.c_double),
                ('velocity_x', c.c_double), ('velocity_y', c.c_double), ('velocity_z', c.c_double),
                ('viewport_w', c.c_double), ('viewport_h', c.c_double), ('pawn', c.c_uint64), ('controller', c.c_uint64)]


hook_requests = []


@REQUEST
def host_request(ctx, data, sink, out):
    request = json.loads(data)
    if request.get('op', '').startswith('hooks.'):
        hook_requests.append(request)
    response = b'{}'
    if request.get('op') == 'oversized':
        response = b'{}' + b' ' * (1024 * 1024)
    sink(out, c.cast(c.c_char_p(response), c.c_void_p), len(response))
    return 1


lib = c.CDLL(str(Path(sys.argv[1]).resolve()))
lib.cssx_test_create.restype = c.c_void_p
lib.cssx_test_create.argtypes = [REQUEST, c.c_void_p, c.c_wchar_p]
lib.cssx_test_tick.argtypes = [c.c_void_p, c.c_double]
lib.cssx_test_render.argtypes = [c.c_void_p, c.POINTER(Frame)]
lib.cssx_test_needs_frame.argtypes = [c.c_void_p]
lib.cssx_test_request.argtypes = [c.c_void_p, c.c_char_p, SINK, c.c_void_p]
lib.cssx_test_stop.argtypes = [c.c_void_p]
lib.cssx_test_destroy.argtypes = [c.c_void_p]

fixtures = [Path(p).resolve() for p in sys.argv[2:6]]
examples = Path(sys.argv[6]).resolve()
checks = 0


def check(condition, message):
    global checks
    if not condition:
        raise SystemExit(f'FAILED check {checks}: {message}')
    checks += 1


def manifest(folder, **fields):
    data = dict(schema=2, api=3, id=folder.name, title=folder.name, version='1.0.0', author='test', kind='lua', entry='main.lua')
    data.update(fields)
    (folder / 'extension.json').write_text(json.dumps(data))


def lua(root, name, source, **fields):
    folder = root / 'extensions' / name
    folder.mkdir(parents=True)
    (folder / 'main.lua').write_text(source)
    manifest(folder, **fields)


def native(root, name, fixture, api, schema=2):
    folder = root / 'extensions' / name
    folder.mkdir(parents=True)
    target = folder / ('ext' + fixture.suffix)
    shutil.copy2(fixture, target)
    manifest(folder, kind='native', entry=target.name, api=api, schema=schema)


with tempfile.TemporaryDirectory(prefix='cssx-测试-') as temp:
    root = Path(temp)
    lua(root, 'working', '''local state=cssx.request({op="state.load"})
return {model=function() return {sections={{id="main",title="Main",controls={{id="toggle",type="toggle",label="Toggle",value=state.on==true}}}}} end,
event=function(e) state.on=e.value; assert(cssx.request({op="state.save",value=state})); end}''')
    lua(root, 'broken', 'while true do end')
    lua(root, 'memory', 'local x = string.rep("x", 70*1024*1024); return {}')
    lua(root, 'caught-loop', 'while true do pcall(function() while true do end end) end')
    lua(root, 'empty', 'return {model=function() return {sections=cssx.array()} end,event=function(e) end}')
    lua(root, 'oversized', 'return {model=function() assert(cssx.request({op="oversized"})); return {sections=cssx.array()} end,event=function() end}')
    lua(root, 'tick-failure', '''cssx.request({op="hooks.add",extension="spoofed"})
return {model=function() return {sections=cssx.array()} end,event=function() end,
tick=function() error("tick failed") end,
stop=function() assert(cssx.request({op="state.save",value={cleaned=true}})) end}''')
    lua(root, 'cleanup-retry', '''local attempts=0
return {model=function() return {sections=cssx.array()} end,event=function() end,
tick=function() error("tick failed") end,
stop=function() attempts=attempts+1; assert(cssx.request({op="state.save",value={attempts=attempts}}));
if attempts<3 then error("cleanup needs retry") end end}''')
    lua(root, 'unknown-api', 'return {}', api=9)
    shutil.copytree(examples / 'lua-counter', root / 'extensions/counter')
    shutil.copytree(examples / 'ui-kit', root / 'extensions/ui-kit')
    native(root, 'abi1', fixtures[0], api=1, schema=1)
    native(root, 'abi2', fixtures[1], api=2, schema=1)
    native(root, 'abi2-render', fixtures[2], api=2, schema=1)
    native(root, 'abi3', fixtures[3], api=3)
    native(root, 'abi-mismatch', fixtures[3], api=1, schema=1)   # manifest says api 1, DLL says ABI 3

    handle = lib.cssx_test_create(host_request, None, str(root))
    check(handle, 'runtime created')

    out = {}

    @SINK
    def sink(ctx, data, size):
        out['bytes'] = out.get('bytes', b'') + c.string_at(data, size)

    def request(value):
        out['bytes'] = b''
        ok = lib.cssx_test_request(handle, json.dumps(value).encode(), sink, None)
        return ok, json.loads(out['bytes'])

    ok, library = request({'op': 'library'})
    check(ok, 'library request')
    ids = {e['id']: e for e in library['extensions']}
    check('unknown-api' not in ids, 'unknown api rejected at discovery')
    check(any('unknown-api' in e['path'] for e in library['errors']), 'unknown api reported in errors')
    for name in ('working', 'empty', 'examples.counter', 'cssx.ui-kit', 'abi1', 'abi2', 'abi2-render', 'abi3'):
        check(ids[name]['available'], f'{name} available: {ids[name]["error"]}')
    for name in ('broken', 'memory', 'caught-loop', 'oversized', 'abi-mismatch'):
        check(not ids[name]['available'], f'{name} suspended')
    check('does not match' in ids['abi-mismatch']['error'], 'abi mismatch message')
    check(ids['abi3']['status'].get('summary', '').startswith('frames'), 'abi3 status summary')
    check(ids['abi3']['schema'] == 2 and ids['abi3']['api'] == 3, 'schema/api echoed')

    ok, model = request({'op': 'model', 'id': 'working'})
    check(ok and model['sections'][0]['controls'][0]['value'] is False, 'initial model')
    ok, _ = request({'op': 'event', 'id': 'working', 'event': {'id': 'toggle', 'value': True}})
    check(ok, 'event accepted')
    ok, model = request({'op': 'model', 'id': 'working'})
    check(model['sections'][0]['controls'][0]['value'] is True, 'state saved through event')
    check(json.loads((root / 'state/working.json').read_text())['on'] is True, 'state file in standalone layout')
    ok, _ = request({'op': 'event', 'id': 'working', 'event': {'id': 'missing', 'value': True}})
    check(not ok, 'unknown control rejected')
    ok, _ = request({'op': 'event', 'id': 'abi3', 'event': {'id': 'go'}})
    check(ok, 'abi3 event + direct invalidate')
    ok, library2 = request({'op': 'library'})
    check(library2['revision'] > library['revision'], 'revision advanced by invalidate/event')

    # Frame dispatch: only abi2-render requests frames; its second frame fails and suspends it.
    check(lib.cssx_test_needs_frame(handle) == 1, 'render consumer present')
    frame = Frame(abi=3, size=c.sizeof(Frame), seconds=0.016, world_ready=1)
    check(lib.cssx_test_render(handle, c.byref(frame)) == 1, 'frame 1')
    check(lib.cssx_test_render(handle, c.byref(frame)) == 1, 'frame 2 dispatch (extension fails, runtime survives)')
    ok, library3 = request({'op': 'library'})
    ids = {e['id']: e for e in library3['extensions']}
    check(not ids['abi2-render']['available'] and 'render' in ids['abi2-render']['error'], 'render failure suspended the extension')
    check(lib.cssx_test_needs_frame(handle) == 0, 'no render consumer after suspension')
    check(ids['abi2-render']['cost']['render_calls'] == 2, 'render cost recorded')

    # Ticks: tick-failure suspends on its first coalesced tick and its owned hooks are cleared.
    for _ in range(3):
        check(lib.cssx_test_tick(handle, 0.1) == 1, 'tick')
    ok, library4 = request({'op': 'library'})
    ids = {e['id']: e for e in library4['extensions']}
    check(not ids['tick-failure']['available'], 'tick failure suspended')
    check(json.loads((root / 'state/tick-failure.json').read_text()) == {'cleaned': True}, 'stop ran after tick failure')
    check(any(r['op'] == 'hooks.clear' and r['extension'] == 'tick-failure' for r in hook_requests), 'hooks cleared for failed extension')
    check(all(r.get('extension') != 'spoofed' for r in hook_requests), 'extension id cannot be spoofed')
    check(not ids['cleanup-retry']['available'] and 'unload blocked' in ids['cleanup-retry']['error'], 'cleanup retry blocks unload')
    check(ids['working']['cost']['tick_calls'] >= 1, 'tick cost recorded')

    # Stop: first attempt fails because cleanup-retry needs a retry; second succeeds.
    check(lib.cssx_test_stop(handle) == 0, 'first stop blocked by retryable cleanup')
    check(lib.cssx_test_stop(handle) == 1, 'second stop succeeds')
    check(json.loads((root / 'state/cleanup-retry.json').read_text()) == {'attempts': 3}, 'cleanup attempted at suspension, at first stop and at second stop')
    lib.cssx_test_destroy(handle)
    log = (root / 'logs/cssx.jsonl').read_text().splitlines()
    check(any('Extension suspended' in line for line in log), 'framework log written')
    check((root / 'logs/abi3/current.jsonl').exists(), 'abi3 direct log landed in logs/<id>/')
print(f'{checks} lifecycle checks passed')
