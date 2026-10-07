"""Separate facial performance curves from the bounded CSS package manifest."""
import copy
import hashlib
import json
import re


LIMIT = 16*1024*1024
TOTAL_LIMIT = 64*1024*1024


def valid_id(value):
    return isinstance(value, str) and re.fullmatch(r'[A-Za-z0-9_.-]{1,96}', value) and value not in ('.', '..')


def encode(value):
    return (json.dumps(value, separators=(',', ':'), allow_nan=False)+'\n').encode()


def externalize(manifest):
    if manifest.get('kind') != 'animation_library' or 'facial_resources' in manifest:
        raise ValueError('Expected an inline animation-library manifest')
    result = copy.deepcopy(manifest)
    faces = result['catalog']['animation_library'].get('faces', [])
    resources, descriptors, seen = {}, [], set()
    if not isinstance(faces, list) or len(faces) > 16:
        raise ValueError('Invalid facial rig list')
    for face in faces:
        rig = face['id']
        if not valid_id(rig) or rig in seen:
            raise ValueError('Invalid or duplicate facial rig identity')
        seen.add(rig)
        if 'performances' not in face:
            continue
        performances = face.pop('performances')
        if not isinstance(performances, list) or len(performances) > 64:
            raise ValueError('Invalid performance list')
        file = rig+'.face.json'
        payload = encode(dict(schema=1, rig=rig, performances=performances))
        if len(payload) > LIMIT:
            raise ValueError('Facial resource exceeds 16 MiB')
        resources[file] = payload
        descriptors.append(dict(rig=rig, file=file, bytes=len(payload), sha256=hashlib.sha256(payload).hexdigest()))
    if sum(map(len, resources.values())) > TOTAL_LIMIT:
        raise ValueError('Facial resources exceed 64 MiB')
    if descriptors:
        result['facial_resources'] = descriptors
    if len(encode(result)) > 256*1024:
        raise ValueError('Manifest exceeds the unchanged 256 KiB limit')
    if resolve(result, resources) != manifest:
        raise ValueError('Facial resource round-trip changed the catalog')
    return result, resources


def resolve(manifest, resources):
    """Verify packaged bytes and reconstruct the inline authoring document."""
    result = copy.deepcopy(manifest)
    descriptors = result.pop('facial_resources', None)
    if descriptors is None:
        return result
    if result.get('kind') != 'animation_library' or not isinstance(descriptors, list) or not 1 <= len(descriptors) <= 16:
        raise ValueError('Invalid facial resource list')
    faces = result['catalog']['animation_library']['faces']
    if not isinstance(faces, list) or not 1 <= len(faces) <= 16:
        raise ValueError('Facial resources require declared rigs')
    seen, total = set(), 0
    for entry in descriptors:
        if set(entry) != {'rig', 'file', 'bytes', 'sha256'}:
            raise ValueError('Invalid facial resource descriptor')
        rig, file = entry['rig'], entry['file']
        if (not valid_id(rig) or
                rig in seen or file != rig+'.face.json' or type(entry['bytes']) is not int or entry['bytes'] < 1):
            raise ValueError('Invalid facial resource identity or size')
        seen.add(rig)
        targets = [face for face in faces if face['id'] == rig]
        if len(targets) != 1 or 'performances' in targets[0]:
            raise ValueError('Missing/duplicate rig or mixed inline and external performances')
        payload = resources[file]
        total += len(payload)
        if (not payload or len(payload) > LIMIT or total > TOTAL_LIMIT or len(payload) != entry['bytes'] or
                hashlib.sha256(payload).hexdigest() != entry['sha256']):
            raise ValueError('Facial resource checksum or size mismatch')
        data = json.loads(payload)
        if (not isinstance(data, dict) or set(data) != {'schema', 'rig', 'performances'} or
                type(data['schema']) is not int or data['schema'] != 1 or data['rig'] != rig or
                not isinstance(data['performances'], list) or len(data['performances']) > 64):
            raise ValueError('Unsupported facial resource payload')
        targets[0]['performances'] = data['performances']
    return result
