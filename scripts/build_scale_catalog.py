"""Generate native/menu scale definitions and the pinned Movy catalog from one source."""
from pathlib import Path
import argparse,json

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');parser.add_argument('--movy',type=Path);args=parser.parse_args()
    root=Path(__file__).resolve().parents[1];data=json.loads((root/'data/scales.json').read_text());scales=data['scales']
    keyboard=sorted(scales,key=lambda s:s['keyboard_id']);native=sorted((s for s in scales if s['hb_id']),key=lambda s:s['hb_id'])
    assert [s['keyboard_id'] for s in keyboard]==list(range(len(keyboard)))
    assert [s['hb_id'] for s in native]==list(range(1,len(native)+1))
    assert len({s['id'] for s in scales})==len(scales)
    parallel=sorted([dict(name=s['name'],parallel_id=s['parallel_id']) for s in native]+data['parallel_operations'],key=lambda s:s['parallel_id'])
    assert [s['parallel_id'] for s in parallel]==list(range(1,len(parallel)+1))
    def write(path,text):
        if args.check:assert path.read_text()==text,f'Stale generated catalog: {path}'
        else:path.write_text(text)
    header='/* Generated from data/scales.json; do not edit. */\n'
    header+=f'#define HB_SCALE_COUNT {len(native)+1}\n'
    header+='static const char *HB_SCALE_NAMES[]='+json.dumps(['Infer']+[s['name'] for s in native]).replace('[','{').replace(']','}')+';\n'
    header+='static const unsigned HB_SCALE_MASKS[]={'+','.join(map(str,[0]+[sum(1<<i for i in s['intervals']) for s in native]))+'};\n'
    header+='static const int HB_SCALE_DOMINANT[]={'+','.join(map(str,[0]+[int(s['chord_quality']=='dominant') for s in native]))+'};\n'
    header+='static const char *HB_PARALLEL_NAMES[]='+json.dumps(['Off']+[s['name'] for s in parallel]).replace('[','{').replace(']','}')+';\n'
    header+='static const int HB_PARALLEL_SCALE_IDS[]={0,'+','.join(str(next((n['hb_id'] for n in native if n['parallel_id']==s['parallel_id']),0)) for s in parallel)+'};\n'
    header+=f'#define HB_PARALLEL_COUNT {len(parallel)+1}\n'
    write(root/'src/scale_catalog.h',header)
    path=root/'modules/harmonybus/module.json';module=json.loads(path.read_text())
    for p in module['capabilities']['chain_params']:
        if p['key']=='follower_scale':p['options']=['Infer']+[s['name'] for s in native]
        if p['key']=='parallel_scale':p['options']=[s['name'] for s in parallel]
    write(path,json.dumps(module,separators=(',',':'))+'\n')
    if args.movy:
        ts='// Generated from HarmonyBus data/scales.json; verified against the pinned HB build.\n'
        ts+='export const SCALES = '+json.dumps([dict(name=s['keyboard_name'],degrees=s['intervals']) for s in keyboard])+';\n'
        ts+='export const FOLLOWER_SCALE_NAMES = '+json.dumps([s['name'] for s in native])+';\n'
        ts+='export const FOLLOWER_KEYBOARD_SCALES = '+json.dumps([s['keyboard_id'] for s in native])+';\n'
        ts+='export const PARALLEL_SCALES = '+json.dumps([s['name'] for s in parallel])+';\n'
        write(args.movy/'integration/scale-catalog.ts',ts)
if __name__=='__main__':main()
