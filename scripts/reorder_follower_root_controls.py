from pathlib import Path
import json

MODULE = Path('modules/harmonybus/module.json')
module = json.loads(MODULE.read_text())

level = module['capabilities']['ui_hierarchy']['levels']['follower_source']
params = level['params']

move_keys = [
    'follower_root_policy',
    'follower_explicit_root',
    'inferred_root',
    'used_root',
]

by_key = {item.get('key'): item for item in params if isinstance(item, dict) and item.get('key')}
missing = [key for key in move_keys if key not in by_key]
if missing:
    raise SystemExit(f'missing follower root controls: {missing}')

remaining = [item for item in params if item.get('key') not in move_keys]
split_index = next(
    index for index, item in enumerate(remaining)
    if item.get('key') == 'split_map'
)

level['params'] = (
    remaining[:split_index + 1]
    + [by_key[key] for key in move_keys]
    + remaining[split_index + 1:]
)

MODULE.write_text(json.dumps(module, indent=2) + '\n')
