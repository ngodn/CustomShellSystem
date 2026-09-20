import ctypes as c,json,tempfile,sys,shutil,os
from pathlib import Path
if len(sys.argv)!=5:
    raise SystemExit('Usage: cssx_runtime_check.py RUNTIME ABI1_FIXTURE ABI2_NO_RENDER_FIXTURE ABI2_RENDER_FIXTURE')
SINK=c.CFUNCTYPE(None,c.c_void_p,c.c_void_p,c.c_size_t)
REQUEST=c.CFUNCTYPE(c.c_int,c.c_void_p,c.c_char_p,SINK,c.c_void_p)
class Host(c.Structure): _fields_=[('abi',c.c_uint32),('size',c.c_uint32),('context',c.c_void_p),('request',REQUEST),('hud',c.c_void_p)]
CREATE=c.CFUNCTYPE(c.c_void_p,c.POINTER(Host),c.c_wchar_p)
TICK=c.CFUNCTYPE(c.c_int,c.c_void_p,c.c_double)
STOP=c.CFUNCTYPE(c.c_int,c.c_void_p)
DESTROY=c.CFUNCTYPE(None,c.c_void_p)
RENDER=c.CFUNCTYPE(c.c_int,c.c_void_p,c.c_void_p)
class API(c.Structure): _fields_=[('abi',c.c_uint32),('size',c.c_uint32),('create',CREATE),('tick',TICK),('request',REQUEST),('stop',STOP),('destroy',DESTROY),('render',RENDER),('needs_frame',STOP)]
hook_requests=[]
@REQUEST
def host_request(ctx,data,sink,out):
    request=json.loads(data)
    if request.get('op','').startswith('hooks.'):
        hook_requests.append(request)
    response=b'{}';sink(out,c.cast(c.c_char_p(response),c.c_void_p),len(response))
    if json.loads(data).get('op')=='oversized':
        trailing=b' '*(1024*1024)
        sink(out,c.cast(c.c_char_p(trailing),c.c_void_p),len(trailing))
    return 1
lib=c.CDLL(str(Path(sys.argv[1]).resolve()))
lib.cssx_get_runtime.restype=c.POINTER(API)
api=lib.cssx_get_runtime().contents
assert api.abi==2, 'Update the harness when the runtime ABI changes'
assert api.size>=c.sizeof(API), 'Runtime does not provide the frame-demand query'
host=Host(2,c.sizeof(Host),None,host_request,None)
with tempfile.TemporaryDirectory(prefix='cssx-测试-') as temp:
    root=Path(temp)
    invalid_host=Host(1,c.sizeof(Host),None,host_request,None)
    assert not api.create(c.byref(invalid_host),str(root)), 'Old host ABI accepted'
    invalid_host=Host(2,Host.hud.offset,None,host_request,None)
    assert not api.create(c.byref(invalid_host),str(root)), 'Truncated host accepted'
    for name,source in [('working','''local state=cssx.request({op="state.load"})
return {model=function() return {sections={{id="main",title="Main",controls={{id="toggle",type="toggle",label="Toggle",value=state.on==true}}}}} end,
event=function(e) state.on=e.value; assert(cssx.request({op="state.save",value=state})); end}'''),('broken','while true do end')]:
        folder=root/'extensions'/name;folder.mkdir(parents=True)
        (folder/'main.lua').write_text(source)
        (folder/'extension.json').write_text(json.dumps(dict(schema=1,api=1,id=name,title=name,version='1.0.0',author='test',kind='lua',layout='tabs',entry='main.lua')))
    example=Path(__file__).resolve().parents[1]/'examples/extensions/lua-counter'
    shutil.copytree(example,root/'extensions/counter')
    shutil.copytree(example.parent/'ui-kit',root/'extensions/ui-kit')
    for name,source in [
        ('memory', 'local x = string.rep("x", 70*1024*1024); return {}'),
        ('caught-loop', 'while true do pcall(function() while true do end end) end'),
        ('empty', 'return {model=function() return {sections=cssx.array()} end,event=function(e) end}'),
        ('oversized', 'return {model=function() assert(cssx.request({op="oversized"})); return {sections=cssx.array()} end,event=function() end}'),
        ('tick-failure', '''cssx.request({op="hooks.add",extension="spoofed"})
return {model=function() return {sections=cssx.array()} end,event=function() end,
tick=function() error("tick failed") end,
stop=function() assert(cssx.request({op="state.save",value={cleaned=true}})) end}'''),
        ('cleanup-retry', '''local attempts=0
return {model=function() return {sections=cssx.array()} end,event=function() end,
tick=function() error("tick failed") end,
stop=function() attempts=attempts+1; assert(cssx.request({op="state.save",value={attempts=attempts}}));
if attempts==1 then error("cleanup needs retry") end end}'''),
    ]:
        folder=root/'extensions'/name;folder.mkdir()
        (folder/'main.lua').write_text(source)
        (folder/'extension.json').write_text(json.dumps(dict(schema=1,api=1,id=name,title=name,version='1',author='test',kind='lua',layout='tabs',entry='main.lua')))
    instance=api.create(c.byref(host),str(root));assert instance
    assert not api.needs_frame(instance), 'Lua-only extensions must not trigger HUD preparation'
    def request(value):
        result=[]
        @SINK
        def sink(out,data,size): result.append(c.string_at(data,size))
        ok=api.request(instance,json.dumps(value).encode(),sink,None)
        decoded=json.loads(b''.join(result));assert ok,decoded;return decoded
    library=request({'op':'library'});assert len(library['extensions'])==10
    assert 'exceeds 1 MiB' in next(e for e in library['extensions'] if e['id']=='oversized')['error']
    assert next(e for e in library['extensions'] if e['id']=='caught-loop')['available'] is False
    assert next(e for e in library['extensions'] if e['id']=='broken')['available'] is False
    assert next(e for e in library['extensions'] if e['id']=='working')['available'] is True
    assert next(e for e in library['extensions'] if e['id']=='memory')['available'] is False
    assert request({'op':'model','id':'empty'})['sections']==[]
    counter=lambda:request({'op':'model','id':'examples.counter'})
    assert counter()['sections'][0]['controls'][0]['value']==0
    request({'op':'event','id':'examples.counter','event':{'id':'counter','value':37}})
    assert counter()['sections'][0]['controls'][0]['value']==37
    request({'op':'event','id':'examples.counter','event':{'id':'export'}})
    assert (root/'output/extensions/examples.counter/counter.txt').read_text()=='37\n'
    gallery=lambda:request({'op':'model','id':'cssx.ui-kit'})
    request({'op':'event','id':'cssx.ui-kit','event':{'id':'quality','value':'strong'}})
    assert gallery()['sections'][0]['controls'][1]['value']=='strong'
    request({'op':'event','id':'cssx.ui-kit','event':{'id':'strength','value':75}})
    request({'op':'event','id':'cssx.ui-kit','event':{'id':'name','value':'颜色 测试'}})
    assert gallery()['sections'][0]['controls'][5]['value']=='颜色 测试'
    request({'op':'event','id':'cssx.ui-kit','event':{'id':'run'}})
    assert gallery()['sections'][1]['controls'][0]['busy'] is True
    for _ in range(26): assert api.tick(instance,.2)==1
    assert json.loads((root/'state/extensions/tick-failure.json').read_text())['cleaned'] is True
    assert [r['op'] for r in hook_requests]==['hooks.add','hooks.clear']
    assert all(r['extension']=='tick-failure' for r in hook_requests)
    assert json.loads((root/'state/extensions/cleanup-retry.json').read_text())['attempts']==1
    failed=next(e for e in request({'op':'library'})['extensions'] if e['id']=='cleanup-retry')
    assert failed['available'] is False and 'cleanup incomplete' in failed['error']
    feedback=gallery()['sections'][1]['controls']
    assert feedback[0]['busy'] is False and feedback[1]['value']==1 and feedback[2]['value'] is False
    menu=root/'extensions/counter/menu.json'
    original=menu.read_text();stamp=menu.stat().st_mtime_ns
    menu.write_text('{"schema":1,"sections":false}')
    os.utime(menu,ns=(stamp+2_000_000_000,stamp+2_000_000_000))
    for _ in range(12): assert api.tick(instance,.2)==1
    assert counter()['sections'][0]['controls'][0]['value']==37
    assert next(e for e in request({'op':'library'})['extensions'] if e['id']=='examples.counter')['available']
    menu.unlink()
    for _ in range(12): assert api.tick(instance,.2)==1
    assert counter()['sections'][0]['controls'][0]['value']==37
    changed=json.loads(original);changed['sections'][0]['title']='Reloaded'
    menu.write_text(json.dumps(changed));os.utime(menu,ns=(stamp+4_000_000_000,stamp+4_000_000_000))
    for _ in range(12): assert api.tick(instance,.2)==1
    assert counter()['sections'][0]['title']=='Reloaded'
    assert counter()['sections'][0]['controls'][0]['value']==37
    before=request({'op':'model','id':'working'});assert before['sections'][0]['controls'][0]['value'] is False
    request({'op':'event','id':'working','event':{'id':'toggle','value':True}})
    assert request({'op':'model','id':'working'})['sections'][0]['controls'][0]['value'] is True
    assert json.loads((root/'state/extensions/working.json').read_text())['on'] is True
    assert api.tick(instance,.2)==1
    assert api.stop(instance)==1
    assert json.loads((root/'state/extensions/cleanup-retry.json').read_text())['attempts']==2
    api.destroy(instance)
    instance=api.create(c.byref(host),str(root));assert instance
    assert counter()['sections'][0]['controls'][0]['value']==37
    assert api.stop(instance)==0
    assert api.stop(instance)==1
    api.destroy(instance)
    print('Lua isolation, memory limit, empty arrays, Unicode storage, persistence, output and atomic menu reload passed')

for mode,fixture in enumerate([None,*sys.argv[2:]],0):
    with tempfile.TemporaryDirectory(prefix='cssx-frame-demand-') as temp:
        root=Path(temp)
        if fixture:
            folder=root/'extensions/fixture';folder.mkdir(parents=True)
            shutil.copy2(fixture,folder/'fixture.dll')
            (folder/'extension.json').write_text(json.dumps(dict(schema=1,api=1,id='fixture',title='Fixture',version='1',author='test',kind='native',layout='tabs',entry='fixture.dll')))
        instance=api.create(c.byref(host),str(root));assert instance
        try:
            library=request({'op':'library'})
            assert not library['errors']
            assert all(e['available'] for e in library['extensions']),library
            assert len(library['extensions'])==int(fixture is not None)
            assert bool(api.needs_frame(instance))==(mode==3)
            if mode==3:
                frame=c.create_string_buffer(256)
                assert api.render(instance,frame)==1 and api.needs_frame(instance)
                assert api.render(instance,frame)==1 and not api.needs_frame(instance)
                assert not request({'op':'library'})['extensions'][0]['available']
        finally:
            assert api.stop(instance)==1
            assert not api.needs_frame(instance)
            api.destroy(instance)
print('Empty runtime, ABI 1, ABI 2 without render, active render, suspension and stop frame demand passed')
