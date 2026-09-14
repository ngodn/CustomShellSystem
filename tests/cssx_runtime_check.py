import ctypes as c,json,tempfile,sys,shutil,os
from pathlib import Path
SINK=c.CFUNCTYPE(None,c.c_void_p,c.c_void_p,c.c_size_t)
REQUEST=c.CFUNCTYPE(c.c_int,c.c_void_p,c.c_char_p,SINK,c.c_void_p)
class Host(c.Structure): _fields_=[('abi',c.c_uint32),('size',c.c_uint32),('context',c.c_void_p),('request',REQUEST)]
CREATE=c.CFUNCTYPE(c.c_void_p,c.POINTER(Host),c.c_wchar_p)
TICK=c.CFUNCTYPE(c.c_int,c.c_void_p,c.c_double)
STOP=c.CFUNCTYPE(c.c_int,c.c_void_p)
DESTROY=c.CFUNCTYPE(None,c.c_void_p)
class API(c.Structure): _fields_=[('abi',c.c_uint32),('size',c.c_uint32),('create',CREATE),('tick',TICK),('request',REQUEST),('stop',STOP),('destroy',DESTROY)]
@REQUEST
def host_request(ctx,data,sink,out):
    response=b'{}';sink(out,c.cast(c.c_char_p(response),c.c_void_p),len(response));return 1
lib=c.CDLL(str(Path(sys.argv[1]).resolve()))
lib.cssx_get_runtime.restype=c.POINTER(API)
api=lib.cssx_get_runtime().contents
host=Host(1,c.sizeof(Host),None,host_request)
with tempfile.TemporaryDirectory(prefix='cssx-测试-') as temp:
    root=Path(temp)
    for name,source in [('working','''local state=cssx.request({op="state.load"})
return {model=function() return {sections={{id="main",title="Main",controls={{id="toggle",type="toggle",label="Toggle",value=state.on==true}}}}} end,
event=function(e) state.on=e.value; assert(cssx.request({op="state.save",value=state})); end}'''),('broken','while true do end')]:
        folder=root/'extensions'/name;folder.mkdir(parents=True)
        (folder/'main.lua').write_text(source)
        (folder/'extension.json').write_text(json.dumps(dict(schema=1,api=1,id=name,title=name,version='1.0.0',author='test',kind='lua',layout='tabs',entry='main.lua')))
    example=Path(__file__).resolve().parents[1]/'examples/extensions/lua-counter'
    shutil.copytree(example,root/'extensions/counter')
    for name,source in [
        ('memory', 'local x = string.rep("x", 70*1024*1024); return {}'),
        ('caught-loop', 'while true do pcall(function() while true do end end) end'),
        ('empty', 'return {model=function() return {sections=cssx.array()} end,event=function(e) end}'),
    ]:
        folder=root/'extensions'/name;folder.mkdir()
        (folder/'main.lua').write_text(source)
        (folder/'extension.json').write_text(json.dumps(dict(schema=1,api=1,id=name,title=name,version='1',author='test',kind='lua',layout='tabs',entry='main.lua')))
    instance=api.create(c.byref(host),str(root));assert instance
    def request(value):
        result=[]
        @SINK
        def sink(out,data,size): result.append(c.string_at(data,size))
        ok=api.request(instance,json.dumps(value).encode(),sink,None)
        decoded=json.loads(b''.join(result));assert ok,decoded;return decoded
    library=request({'op':'library'});assert len(library['extensions'])==6
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
    api.destroy(instance)
    instance=api.create(c.byref(host),str(root));assert instance
    assert counter()['sections'][0]['controls'][0]['value']==37
    assert api.stop(instance)==1
    api.destroy(instance)
    print('Lua isolation, memory limit, empty arrays, Unicode storage, persistence, output and atomic menu reload passed')
