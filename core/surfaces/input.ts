/** Transport-only commands. Harmony, mapping and note ownership stay in the engine. */
export type SurfaceSource = 0 | 1 | 2; // Launchpad, FM1, Remote UI
export type SurfaceEvent = {
    track: number; status: number; note: number; pitch: number; value: number;
    shift: number; row: number; full: number;
};
type Pending = SurfaceEvent & { source: SurfaceSource; serial: number };
export type SurfaceTransport = (key: string, value: string, timeout: number) => boolean;

/** One bounded ordered queue shared by every external adapter. No timer or MIDI I/O. */
export class SurfaceInput {
    private serial: number;
    private attemptedThrough = 0;
    private queue: Pending[] = [];
    private fences: number[] = [0, 0, 0];
    constructor(private readonly send: SurfaceTransport, initialSerial: number) {
        if (!Number.isSafeInteger(initialSerial) || initialSerial < 0) throw new Error('Invalid serial');
        this.serial = initialSerial;
    }
    enqueue(source: SurfaceSource, event: SurfaceEvent): boolean {
        if (![0,1,2].includes(source) || !Object.values(event).every(Number.isInteger) ||
            event.track < 0 || event.track > 15 || ![128,144,160].includes(event.status) ||
            event.note < 0 || event.note > 127 || event.pitch < 0 || event.pitch > 127 ||
            event.value < 0 || event.value > 127 || event.row < -1 || event.row > 3 ||
            event.shift < -127 || event.shift > 127 || ![0,1].includes(event.full) ||
            (event.row >= 0 && (event.pitch + event.shift < 0 || event.pitch + event.shift > 127))) return false;
        const previous = this.queue[this.queue.length - 1];
        if (event.status === 160 && previous?.status === 160 && previous.source === source &&
            previous.note === event.note && previous.serial > this.attemptedThrough) {
            previous.value = event.value; return true;
        }
        // Never perform IPC from a MIDI callback. A stalled engine fails closed.
        if (this.queue.length >= 256) { this.release(source); return false; }
        this.queue.push({ ...event, source, serial: ++this.serial }); return true;
    }
    release(source: SurfaceSource): void {
        this.queue = this.queue.filter(event => event.source !== source);
        this.fences[source] = ++this.serial;
    }
    flush(): boolean {
        for (let source = 0; source < 3; source++) {
            if (!this.fences[source]) continue;
            if (!this.send('surface_release_source', `${source},${this.fences[source]}`, 8)) return false;
            this.fences[source] = 0;
        }
        if (!this.queue.length) return true;
        const batch = this.queue.slice(0,32);
        this.attemptedThrough = batch[batch.length-1].serial;
        const payload = batch.map(event => [event.serial,event.track,event.status,
            event.source*128+event.note,event.pitch,event.value,event.shift,event.row,event.full].join(',')).join(';');
        if (!this.send('surface_events', payload, 8)) return false;
        this.queue.splice(0,batch.length); return this.queue.length === 0;
    }
}

/** Stock-firmware MIDI keyboard adapter. No guessed FM1 CC, LED or SysEx protocol. */
export function fm1NoteEvent(data: readonly number[], channel: number, track: number): SurfaceEvent | null {
    if (data.length !== 3 || !data.every(Number.isInteger) || !Number.isInteger(channel) || channel < 0 || channel > 15) return null;
    const [status,note,value] = data, kind = status & 240;
    if (status < 128 || status > 175 || (status & 15) !== channel || note < 0 || note > 127 ||
        value < 0 || value > 127 || ![128,144,160].includes(kind)) return null;
    return { track, status: kind === 144 && value === 0 ? 128 : kind, note, pitch: note,
        value, shift: 0, row: -1, full: 0 };
}
