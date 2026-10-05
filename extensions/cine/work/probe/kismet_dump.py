# Compact reading of CUE4Parse kismet bytecode: one line per statement, nested expressions inline.
import json, sys, re
def expr(x):
    if isinstance(x, list): return '[' + ', '.join(expr(v) for v in x) + ']'
    if not isinstance(x, dict): return json.dumps(x)
    t = x.get('Token')
    if t is None:
        if 'ObjectName' in x: return x['ObjectName'].split("'")[-2] if "'" in x['ObjectName'] else x['ObjectName']
        if 'Name' in x and 'Type' in x: return x['Name']
        if 'TagName' in x: return 'Tag(' + x['TagName'] + ')'
        return '{' + ', '.join(f'{k}={expr(v)}' for k, v in x.items() if k not in ('StatementIndex',)) + '}'
    t = t.replace('EX_', '')
    if t in ('LocalVariable', 'InstanceVariable', 'DefaultVariable', 'LocalOutVariable'):
        v = x.get('Variable', {}); p = v.get('Property', {}) if isinstance(v, dict) else {}
        return ('self.' if t == 'InstanceVariable' else '') + str(p.get('Name', v.get('Name', '?')))
    if t in ('CallMath', 'FinalFunction', 'LocalFinalFunction', 'VirtualFunction', 'LocalVirtualFunction'):
        f = x.get('Function')
        if isinstance(f, str): return f"{f}({', '.join(expr(p) for p in x.get('Parameters', []))})"
        name = f.get('ObjectName', '?').split("'")[-2] if isinstance(f, dict) and 'ObjectName' in f else (x.get('FunctionName') or x.get('Name') or '?')
        if isinstance(f, dict) and 'ObjectName' in f: name = f['ObjectName'].split("'")[1] if "'" in f['ObjectName'] else f['ObjectName']
        return f"{name.split(':')[-1]}({', '.join(expr(p) for p in x.get('Parameters', []))})"
    if t == 'Context' or t == 'ClassContext': return f"{expr(x.get('ObjectExpression'))}.{expr(x.get('ContextExpression'))}"
    if t in ('LetBool', 'Let', 'LetObj', 'LetValueOnPersistentFrame', 'LetDelegate', 'LetMulticastDelegate', 'LetWeakObjPtr'): return f"{expr(x.get('Variable'))} = {expr(x.get('Expression'))}"
    if t == 'JumpIfNot': return f"if not ({expr(x.get('BooleanExpression'))}) goto {x.get('CodeOffset')}"
    if t == 'Jump': return f"goto {x.get('CodeOffset')}"
    if t == 'ComputedJump': return f"goto computed {expr(x.get('CodeOffsetExpression'))}"
    if t == 'PushExecutionFlow': return f"push {x.get('PushingAddress')}"
    if t == 'PopExecutionFlow': return 'pop'
    if t == 'PopExecutionFlowIfNot': return f"pop if not ({expr(x.get('BooleanExpression'))})"
    if t == 'Return': return 'return ' + expr(x.get('ReturnExpression'))
    if t == 'EndOfScript': return 'end'
    if t == 'Self': return 'self'
    if t in ('True', 'False', 'NoObject', 'Nothing'): return t.lower()
    if t in ('IntConst', 'FloatConst', 'DoubleConst', 'ByteConst', 'IntConstByte', 'Int64Const', 'UInt64Const'): return str(x.get('Value'))
    if t == 'NameConst': return 'Name(' + str(x.get('Value')) + ')'
    if t == 'StringConst' or t == 'UnicodeStringConst': return json.dumps(x.get('Value'))
    if t == 'ObjectConst': return 'Obj(' + expr(x.get('Object')) + ')'
    if t == 'StructConst': return 'Struct(' + expr(x.get('Properties', x.get('Value'))) + ')'
    if t == 'StructMemberContext': return f"{expr(x.get('StructExpression'))}.{expr(x.get('Property'))}"
    if t == 'Cast' or t.endswith('Cast') or t in ('InterfaceContext', 'PrimitiveCast', 'DynamicCast', 'ObjToInterfaceCast'): return f"cast({expr(x.get('Target', x.get('Expression')))})"
    if t == 'SwitchValue': return f"switch({expr(x.get('IndexTerm'))}) {expr(x.get('Cases'))} default {expr(x.get('DefaultTerm'))}"
    if t == 'CallMulticastDelegate' or t == 'BindDelegate': return f"{t}({expr(x.get('Delegate'))}, {expr(x.get('Parameters', x.get('Function')))})"
    if t == 'ArrayConst' or t == 'SetConst' or t == 'MapConst': return t + expr(x.get('Elements', x.get('Value')))
    if t == 'InstanceDelegate': return 'Delegate(' + str(x.get('FunctionName')) + ')'
    if t == 'Skip': return 'skip ' + expr(x.get('SkipExpression'))
    rest = {k: v for k, v in x.items() if k not in ('Token', 'StatementIndex')}
    return t + '(' + ', '.join(f'{k}={expr(v)}' for k, v in rest.items()) + ')'
want = sys.argv[2:] if len(sys.argv) > 2 else None
d = json.load(open(sys.argv[1])); objs = d if isinstance(d, list) else d.get('Exports', d)
for o in objs:
    if o.get('Type') != 'Function': continue
    if want and not any(re.search(w, o.get('Name', ''), re.I) for w in want): continue
    print('== FUNCTION', o.get('Name'))
    for st in o.get('ScriptBytecode') or []:
        print(f"  {st.get('StatementIndex', '?'):>5}: {expr(st)}"[:400])
