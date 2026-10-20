import assert from 'node:assert/strict';
import { SurfaceInput, fm1NoteEvent } from '../core/surfaces/input.ts';

const event = { track: 2, status: 144, note: 60, pitch: 60, value: 90, shift: 0, row: -1, full: 0 };
const writes = [];
const input = new SurfaceInput((key, value) => { writes.push([key, value]); return true; }, 100);
for (const field of Object.keys(event)) {
    const malformed = { ...event }; delete malformed[field];
    assert.equal(input.enqueue(1, malformed), false, `missing ${field}`);
}
assert.equal(input.enqueue(1, null), false);
assert.equal(input.enqueue(3, event), false);
input.release(3);
assert(input.flush()); assert.equal(writes.length, 0);
for (const track of [-1, 16, 1.5, NaN, undefined]) assert.equal(fm1NoteEvent([145, 60, 90], 1, track), null);
assert.equal(fm1NoteEvent([145, , 90], 1, 2), null);
assert.equal(fm1NoteEvent([145, 60, 0], 1, 2).status, 128);
assert(input.enqueue(1, event));
assert(input.enqueue(1, { ...event, status: 128, value: 0 }));
assert(input.flush());
assert.equal(writes[0][1], '101,2,144,188,60,90,0,-1,0;102,2,128,188,60,0,0,-1,0');
console.log('Surface input: malformed events cannot poison valid note/release batches');
