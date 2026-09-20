#!/usr/bin/env python3
import hashlib,json, pathlib,sys
root=pathlib.Path(__file__).resolve().parent
manifest=json.loads((root/'manifest_sha256.json').read_text(encoding='utf8'))
ok=True
for entry in manifest['files']:
 p=root/entry['path']
 if not p.is_file(): print('MISSING',entry['path']);ok=False;continue
 h=hashlib.sha256()
 with p.open('rb') as f:
  for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
 good=p.stat().st_size==entry['bytes'] and h.hexdigest()==entry['sha256']
 print('PASS' if good else 'FAIL',entry['path']);ok &=good
print('VERIFIED' if ok else 'FAILED',len(manifest['files']),'files')
sys.exit(0 if ok else 1)
