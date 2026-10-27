"""Generate shared relative-operation identities without changing saved IDs."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def render_catalog(map_catalog: dict[str, Any]) -> tuple[str, str]:
    """Build the native registry and the matching immutable Movy menu module."""
    operations: list[dict[str, Any]] = map_catalog['operations']
    selectors: list[dict[str, Any]] = map_catalog['selectors']
    assert [operation['id'] for operation in operations] == list(range(len(operations)))
    assert len({operation['symbol'] for operation in operations}) == len(operations)
    map_symbol_to_operation: dict[str, dict[str, Any]] = {
        operation['symbol']: operation for operation in operations
    }
    relatives: list[dict[str, Any]] = [
        dict(operation['relative'], name=operation['name'], motion=operation['id'])
        for operation in operations if 'relative' in operation
    ]
    for field in ('choice', 'token', 'trigger_index', 'menu_order'):
        assert len({relative[field] for relative in relatives}) == len(relatives)
    assert all(0 < relative['token'] < 64 for relative in relatives)
    assert len({item['choice'] for item in relatives + selectors}) == len(relatives + selectors)
    assert len({item['token'] for item in relatives + selectors}) == len(relatives + selectors)
    selector_base: int = min(item['token'] for item in selectors)
    assert [item['token'] for item in selectors] == list(range(selector_base, selector_base + len(selectors)))
    assert selector_base >= 2048
    assert selector_base + len(selectors) <= 65536
    choice_count: int = max(item['choice'] for item in relatives + selectors) + 1
    map_choice_to_name: dict[int, str] = {item['choice']: item['name'] for item in relatives + selectors}
    for index in range(16):
        assert index + 13 not in map_choice_to_name
        map_choice_to_name[index + 13] = 'Motif ' + str(index + 1)
    assert sorted(map_choice_to_name) == list(range(choice_count))

    header: list[str] = [
        '/* Generated from data/operations.json; do not edit. */',
        '#ifndef HB_OPERATION_CATALOG_H', '#define HB_OPERATION_CATALOG_H',
        'enum {',
    ]
    header.extend(f"    {operation['symbol']} = {operation['id']}," for operation in operations)
    header.extend([
        '    HB_MO_MIXED_LAST = HB_MO_MIXED_13', '};',
        f'#define HB_MO_OPERATION_COUNT {len(operations)}',
        'static const char *MO_OPERATIONS[] = {' + ','.join(json.dumps(item['name']) for item in operations) + '};',
        f'#define HB_AR_CHOICE_COUNT {choice_count}',
        'static const char *HB_AR_NAMES[] = {' + ','.join(json.dumps(map_choice_to_name[index]) for index in range(choice_count)) + '};',
        'typedef struct {',
        '    int motion,token,role,modifier,semitones,degree,quality,connector_above,trigger_index;',
        '    int preferred_token,alternate_token;',
        '    const char *name,*short_name;',
        '} hb_ro_definition;',
        'static const hb_ro_definition HB_RELATIVE_OPERATIONS[HB_AR_CHOICE_COUNT] = {',
    ])
    for relative in relatives:
        values: list[int] = [int(relative[field]) for field in (
            'motion', 'token', 'role', 'modifier', 'semitones', 'degree', 'quality', 'connector_above', 'trigger_index')]
        header.append('    [' + str(relative['choice']) + '] = {' + ','.join(map(str, values)) +
                      ',0,0,' + json.dumps(relative['name']) + ',' + json.dumps(relative['short']) + '},')
    for selector in selectors:
        preferred_token: int = map_symbol_to_operation[selector['preferred']]['relative']['token']
        alternate_token: int = map_symbol_to_operation[selector['alternate']]['relative']['token']
        motion: int = map_symbol_to_operation[selector['motion']]['id']
        header.append(f"    [{selector['choice']}] = {{{motion},{selector['token']},0,0,0,-1,9,0,-1," +
                      f'{preferred_token},{alternate_token},' + json.dumps(selector['name']) + ',' + json.dumps(selector['short']) + '},')
    header.append('};')
    for field, limit, name in (('motion', len(operations), 'MOTION'), ('role', 18, 'ROLE'), ('token', 64, 'TOKEN'), ('trigger_index', 19, 'TRIGGER')):
        map_index_to_choice: dict[int, int] = {
            relative[field]: relative['choice'] + 1 for relative in relatives
            if field != 'role' or relative['role'] > 0
        }
        if field == 'motion':
            map_index_to_choice.update({map_symbol_to_operation[item['motion']]['id']: item['choice'] + 1 for item in selectors})
        assert all(0 <= index < limit for index in map_index_to_choice)
        header.append(f'static const unsigned char HB_RO_BY_{name}[{limit}] = {{' +
                      ','.join(str(map_index_to_choice.get(index, 0)) for index in range(limit)) + '};')
    header.append(f'#define HB_RO_SELECTOR_BASE {selector_base}')
    header.append('static const char *HB_RO_SELECTOR_NAMES[] = {"Captured",' +
                  ','.join(json.dumps(item['name']) for item in selectors) + '};')
    header.append(f'#define HB_RO_SELECTOR_COUNT {len(selectors) + 1}')
    header.append('static const unsigned char HB_RO_BY_SELECTOR[] = {' +
                  ','.join(str(item['choice'] + 1) for item in selectors) + '};')
    header.append('typedef struct { const char *alias,*name; } hb_ro_alias;')
    header.append('static const hb_ro_alias HB_RO_ALIASES[] = {')
    for relative in relatives:
        for alias in relative['aliases']:
            header.append('    {' + json.dumps(alias) + ',' + json.dumps(relative['name']) + '},')
    header.extend(['};', '#endif', ''])

    # Presentation order is independent of the preserved numeric assignment IDs.
    menu: list[dict[str, Any]] = [
        {field: relative[field] for field in ('name', 'short', 'group', 'choice', 'token', 'motion')}
        for relative in sorted(relatives, key=lambda item: item['menu_order']) if relative['choice'] != 2
    ]
    menu.extend(dict(name=item['name'], short=item['short'], group='Smart Approaches',
                     choice=item['choice'], token=item['token'], motion=map_symbol_to_operation[item['motion']]['id']) for item in selectors)
    labels: list[dict[str, Any]] = [dict(name=item['name'], short=item['short'], token=item['token']) for item in relatives + selectors]
    typescript: str = ('// Generated from HarmonyBus data/operations.json; no runtime/controller imports.\n' +
                      'export const RELATIVE_OPERATIONS = ' + json.dumps(menu, separators=(',', ':')) + ';\n' +
                      'export const APPROACH_OPERATION_CHOICES = RELATIVE_OPERATIONS.map(operation => operation.name);\n' +
                      'const labels = ' + json.dumps(labels, separators=(',', ':')) + ';\n' +
                      'export const OPERATION_SHORT_NAMES: Readonly<Record<string,string>> = Object.fromEntries(labels.map(operation => [operation.name, operation.short]));\n' +
                      'export const OPERATION_TOKEN_LABELS: Readonly<Record<number,string>> = Object.fromEntries(labels.map(operation => [operation.token, operation.short]));\n')
    return '\n'.join(header), typescript


def write_generated(path: Path, contents: str, check: bool) -> None:
    """Write a deterministic output, or fail if the committed output is stale."""
    if check:
        if not path.exists() or path.read_text() != contents:
            raise ValueError(f'Stale generated operation catalog: {path}')
    else:
        path.write_text(contents)


def main() -> None:
    """Regenerate native definitions and optionally the matching Movy catalog."""
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--movy', type=Path)
    arguments: argparse.Namespace = parser.parse_args()
    root: Path = Path(__file__).resolve().parents[1]
    map_catalog: dict[str, Any] = json.loads((root / 'data/operations.json').read_text())
    header: str
    typescript: str
    header, typescript = render_catalog(map_catalog)
    write_generated(root / 'core/include/operation_catalog.h', header, arguments.check)
    if arguments.movy:
        write_generated(arguments.movy / 'integration/operation-catalog.ts', typescript, arguments.check)


if __name__ == '__main__':
    main()
