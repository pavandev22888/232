#!/usr/bin/env python3
"""Read-only consistency check of the bounded GAP02 archival inventory."""
import collections
import hashlib
import json
from pathlib import Path
import re
import subprocess


def require(condition, message):
    if not condition:
        raise ValueError(message)


def pointer(document, selector):
    for token in selector.strip('/').split('/'):
        document = document[int(token)] if isinstance(document, list) else document[token]
    return document


def main():
    inventory_path = Path(__file__).with_name('inventory.json')
    data = json.loads(inventory_path.read_text())
    repo = Path(__file__).resolve().parents[4]
    source_index = {source['repository_path']: source for source in data['sources']}
    require(len(source_index) == len(data['sources']), 'Duplicate source path')
    json_cache = {}
    for rel, source in source_index.items():
        path = (repo / rel).resolve()
        require(path.is_relative_to(repo), f'Outside repository: {rel}')
        blob = path.read_bytes()
        require(len(blob) == source['bytes'], f'Size changed: {rel}')
        require(hashlib.sha256(blob).hexdigest() == source['sha256'], f'Hash changed: {rel}')
        if path.suffix == '.json':
            json_cache[rel] = json.loads(blob)
    for context in data['baseline_context']:
        blob = subprocess.check_output(
            ['git', 'show', f"{context['revision']}:{context['repository_path']}"], cwd=repo)
        require(len(blob) == context['bytes'], 'Baseline blob size changed')
        require(hashlib.sha256(blob).hexdigest() == context['sha256'], 'Baseline blob hash changed')
    archive = repo / Path(data['restored_workspace']).relative_to(data['repository_root']) / 'orchestration'
    proof_walks = 0
    for entry in data['proof_inventories']:
        spec = json_cache[entry['source']]
        require(len(spec['walks']) == entry['walk_count'] == len(entry['walks']), 'Proof walk count')
        for recorded, walk_entry in zip(entry['walks'], spec['walks']):
            require(recorded['archived_entry'] == walk_entry, 'Proof entry changed')
            expected = str((repo / entry['source']).parent.joinpath(walk_entry['file']).relative_to(repo))
            require(recorded['source'] == expected, 'Proof source path mismatch')
            walk = json_cache[recorded['source']]
            require(len(walk['instructions']) == recorded['instruction_records'], 'Walk record count')
            require(walk.get('limits') == recorded['limits'], 'Walk limits changed')
            if 'records' in walk_entry:
                require(walk_entry['records'] == len(walk['instructions']), 'Archived count mismatch')
            proof_walks += 1
    for image in data['image_hash_expectations']:
        require(source_index[image['source']]['sha256'] == image['sha256'], 'Image hash mismatch')
        for spec_source in image['proof_inventories']:
            spec = json_cache[spec_source]
            images = list(spec['images'].values()) if 'images' in spec else spec.get('inputs', spec.get('input_hashes', []))
            require(any(item['sha256'] == image['sha256'] and
                        str((archive.parent / item['path']).relative_to(repo)) == image['source']
                        for item in images), 'Image expectation not in proof inventory')
    selected = data['candidate_inventory']
    wide = json_cache[selected['source']]
    require(len(wide) == len(selected['records']) == selected['record_count'], 'Candidate count')
    classes = collections.Counter()
    contexts = 0
    walks = set()
    for index, (item, archived) in enumerate(zip(selected['records'], wide)):
        require(item['archive_pointer'] == f'/{index}', 'Candidate pointer mismatch')
        expected_source = str((archive / archived['source']).relative_to(repo))
        require(item['source'] == expected_source, 'Candidate source mismatch')
        walk = json_cache[item['source']]
        require(pointer(walk, item['source_pointer']) == item['record'] == archived['row'], 'Primary row mismatch')
        require(all(row in walk['instructions'] for row in archived['context']), 'Context row mismatch')
        require(len(archived['context']) == item['context_records_matched'], 'Context count mismatch')
        text = archived['row']['instruction']
        expr = re.search(r'\[([^]]+)\]', text).group(1)
        offset = int(re.search(r'\+0x([0-9a-f]+)$', expr).group(1), 16)
        width = next(width for token, width in [('XMMWORD', 16), ('QWORD', 8), ('DWORD', 4), ('WORD', 2), ('BYTE', 1)] if token + ' PTR' in text)
        touches = offset <= 0x68 < offset + width
        category = ('stack_relative' if expr.startswith(('esp', 'ebp')) else
                    'direct_68_untyped' if offset == 0x68 else
                    'wide_overlap_68_untyped' if touches else 'adjacent_not_covering_68')
        require((expr, hex(offset), width, touches, category) ==
                (item['memory_expression'], item['offset_hex'], item['memory_width_bytes'],
                 item['syntactically_touches_0x68'], item['classification']), 'Operand classification mismatch')
        require(item['object_identity'] == 'not_established_by_this_record' and
                item['consumer_proof'] is False, 'Unexpected identity/consumer promotion')
        classes[category] += 1
        contexts += len(archived['context'])
        walks.add(item['source'])
    unique = len({(item['row']['va'], item['row']['bytes']) for item in wide})
    require(unique == selected['unique_primary_address_bytes'], 'Unique row count')
    require(len(wide) - unique == selected['duplicate_primary_records'], 'Duplicate row count')
    require(dict(classes) == selected['classification_counts'], 'Classification totals')
    require(contexts == selected['context_record_count'], 'Context total')
    require(len(walks) == selected['source_walk_count'], 'Source walk total')
    require(selected['typed_T_consumer_proofs_accepted'] == 0, 'Unexpected consumer proof')
    for anchor in data['selected_anchors']:
        require(pointer(json_cache[anchor['source']], anchor['pointer']) == anchor['record'], 'Anchor mismatch')
    result = data['result']
    require(result['gap02_status'] == 'open' and result['consumer_implemented'] is False and
            result['gap_closed'] is False and result['fresh_behavioral_check'] is False,
            'GAP02 closure/behavior claim changed')
    print(json.dumps({'passed': True, 'scope': 'read_only_archival_record_and_hash_consistency_only',
                      'source_files_hashed': len(source_index), 'json_sources_parsed': len(json_cache),
                      'baseline_git_blobs': len(data['baseline_context']),
                      'image_hashes': len(data['image_hash_expectations']), 'proof_inventories': len(data['proof_inventories']),
                      'proof_walk_references': proof_walks, 'candidate_source_walks': len(walks),
                      'candidate_primary_records': len(wide), 'candidate_context_records': contexts,
                      'unique_primary_address_bytes': unique, 'classification_counts': dict(sorted(classes.items())),
                      'selected_local_anchors': len(data['selected_anchors']),
                      'fresh_behavioral_check': False, 'consumer_implemented': False, 'gap02_status': 'open'},
                     ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
