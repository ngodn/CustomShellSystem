"""Remove only zero influences rejected by the native proxy importer."""
import copy
import hashlib
import json
import math
from pathlib import Path

work = Path(__file__).resolve().parents[2]/'work/eve26'
receipts = []
for old,new in [('knit-proxy2.json','knit-proxy3.json'),
                ('knit-backstop-probe.json','knit-backstop-probe2.json')]:
    source = work/old
    output = work/new
    assert not output.exists()
    data = json.loads(source.read_text())
    result = copy.deepcopy(data)
    removed = 0
    for i,rows in enumerate(data['slots']['Collar-1']['weights']):
        assert all(math.isfinite(w) and 0 <= w <= 1 for _,w in rows)
        kept = [[n,w] for n,w in rows if w > 0]
        assert kept and sum(w for _,w in kept) == sum(w for _,w in rows)
        removed += len(rows)-len(kept)
        result['slots']['Collar-1']['weights'][i] = kept
    output.write_text(json.dumps(result,separators=(',',':'))+'\n')
    check = json.loads(output.read_text())
    check['slots']['Collar-1']['weights'] = data['slots']['Collar-1']['weights']
    assert check == data
    receipts.append(dict(source=old,output=new,removed_zero_entries=removed,
        input_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        output_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
        positive_weights_and_other_fields_unchanged=True))
receipt = work/'knit-proxy3-repair.json'
assert not receipt.exists()
receipt.write_text(json.dumps(receipts,indent=2)+'\n')
print(receipt.read_text())
