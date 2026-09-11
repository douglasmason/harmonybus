from pathlib import Path
import json

MODULE = Path('modules/harmonybus/module.json')
module = json.loads(MODULE.read_text())

ui = module['capabilities']['ui_hierarchy']
levels = ui['levels']
source_level = levels['follower_source']

root_keys = [
    'follower_root_policy',
    'follower_explicit_root',
    'inferred_root',
    'used_root',
]

by_key = {
    item.get('key'): item
    for item in source_level['params']
    if isinstance(item, dict) and item.get('key')
}
missing = [key for key in root_keys if key not in by_key]
if missing:
    raise SystemExit(f'missing follower root controls: {missing}')

# Panel 1: follower mapping/performance controls.
source_level['name'] = 'Follower Map'
source_level['params'] = [
    item for item in source_level['params']
    if item.get('key') not in root_keys
]
source_level['knobs'] = [
    'content_map',
    'follower_scale',
    'travel_map',
    'split_map',
    'approach',
    'approach_mode',
    'retrigger_held',
]

# Panel 2: all root selection/status controls together.
levels['follower_root'] = {
    'name': 'Follower Root',
    'params': [by_key[key] for key in root_keys],
    'knobs': root_keys,
}

# Present the two follower panels together from the root page.
root_params = levels['root']['params']
for item in root_params:
    if item.get('level') == 'follower_source':
        item['label'] = 'Foll Map'

if not any(item.get('level') == 'follower_root' for item in root_params):
    source_index = next(
        index for index, item in enumerate(root_params)
        if item.get('level') == 'follower_source'
    )
    root_params.insert(source_index + 1, {
        'level': 'follower_root',
        'label': 'Foll Root',
    })

MODULE.write_text(json.dumps(module, indent=2) + '\n')
