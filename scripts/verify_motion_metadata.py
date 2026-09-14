"""Validate actual DSP metadata for standalone and Movy hosts."""
import json
from pathlib import Path
import subprocess
import tempfile


def main() -> None:
    """Compile the production fixture and compare supported operation lists."""
    root: Path = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory() as directory:
        executable: Path = Path(directory) / 'metadata'
        subprocess.run(['cc', '-std=c11', '-D_POSIX_C_SOURCE=200809L', '-Wno-format-zero-length', '-O1',
                        str(root / 'tests/motion_metadata_dump.c'), str(root / 'src/harmony_core.c'), '-lm', '-o', str(executable)], check=True)
        output: str = subprocess.check_output([str(executable)], text=True)
    maps: list[dict] = [{parameter['key']: parameter for parameter in json.loads(line)} for line in output.splitlines()]
    map_module: dict = json.loads((root / 'modules/harmonybus/module.json').read_text())
    map_canonical: dict = {parameter['key']: parameter for parameter in map_module['capabilities']['chain_params']}
    for map_metadata in maps:
        assert map_metadata['motion_lane']['options'] == [str(slot) for slot in range(1,17)]
        assert set(map_metadata) == set(map_canonical)
        for key, parameter in map_canonical.items():
            if key not in ('motion_operation','motion_enabled','motion_offset','motion_from','motion_through'):
                assert map_metadata[key] == parameter, key
    assert maps[0]['motion_from']['options'] == [str(cycle) for cycle in range(1,17)]
    assert maps[1]['motion_from']['options'] == ['1']
    assert maps[4]['motion_from']['options'] == [str(cycle) for cycle in range(1,9)]
    assert maps[4]['motion_through']['options'] == maps[4]['motion_from']['options']
    assert maps[4]['motion_every']['options'] == [str(cycle) for cycle in range(1,17)]
    assert map_module['capabilities']['ui_hierarchy']['levels']['motion_conditions']['knobs'] == ['motion_lane','motion_every','motion_from','motion_through','motion_cycle','motion_enabled','motion_condition_range','motion_condition_status']
    assert maps[0]['motion_offset'] == map_canonical['motion_offset']
    assert maps[3]['motion_offset']['name'] == 'Decay %' and maps[3]['motion_offset']['min'] == 0
    assert maps[3]['motion_advance']['options'] == ['Clock','Note','Chord']
    assert maps[3]['motion_operation']['options'][-2:] == ['Ratchet','MIDI Echo']
    assert not any(option.startswith('Clip ') for option in maps[0]['motion_operation']['options'])
    assert maps[1]['motion_operation']['options'] == map_canonical['motion_operation']['options']
    assert [option for option in maps[2]['motion_operation']['options'] if option.startswith('Clip ')] == ['Clip Reverse']
    assert maps[2]['motion_enabled']['readOnly']
    print('Runtime metadata: valid complete JSON; standalone hides unsupported choices and preserves the selected clip assignment')


if __name__ == '__main__':
    main()
