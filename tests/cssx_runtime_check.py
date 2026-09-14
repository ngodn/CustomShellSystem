import ctypes as c,json,tempfile,sys
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
    instance=api.create(c.byref(host),str(root));assert instance
    def request(value):
        result=[]
        @SINK
        def sink(out,data,size): result.append(c.string_at(data,size))
        ok=api.request(instance,json.dumps(value).encode(),sink,None)
        decoded=json.loads(b''.join(result));assert ok,decoded;return decoded
    library=request({'op':'library'});assert len(library['extensions'])==2
    assert next(e for e in library['extensions'] if e['id']=='broken')['available'] is False
    assert next(e for e in library['extensions'] if e['id']=='working')['available'] is True
    before=request({'op':'model','id':'working'});assert before['sections'][0]['controls'][0]['value'] is False
    request({'op':'event','id':'working','event':{'id':'toggle','value':True}})
    assert request({'op':'model','id':'working'})['sections'][0]['controls'][0]['value'] is True
    assert json.loads((root/'state/extensions/working.json').read_text())['on'] is True
    assert api.tick(instance,.2)==1
    assert api.stop(instance)==1
    api.destroy(instance)
    print('Lua extension load, Unicode state path, model, action, persistence, runaway-script isolation, tick and shutdown passed')
