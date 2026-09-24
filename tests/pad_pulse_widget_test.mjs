import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const sandbox = {};
vm.createContext(sandbox);
vm.runInContext(fs.readFileSync(new URL('../modules/harmonybus/canvas.js', import.meta.url), 'utf8'), sandbox);
const widget = sandbox.canvas_overlay;
assert.equal(widget.widgetKind, 'custom:hb-pulse-shape');
for (const [width,height] of [[32,15],[24,12]]) {
    const signatures=[];
    for (const shape of ['Smooth','Triangle','Square','None',3]) {
        const marks=[];
        const ctx={width,height,
            fillRect(x,y,w,h){assert(x>=0&&y>=0&&x+w<=width&&y+h<=height);marks.push(['rect',x,y,w,h]);},
            line(x,y,u,v){assert(Math.min(x,u)>=0&&Math.max(x,u)<width&&Math.min(y,v)>=0&&Math.max(y,v)<height);marks.push(['line',x,y,u,v]);}};
        widget.drawCell(ctx,{group:{keys:['pad_pulse_shape']},values:{pad_pulse_shape:shape}});
        if(shape==='None'||shape===3){assert.equal(marks.length,1);assert.equal(marks[0][0],'rect');assert.equal(marks[0][4],1);}
        signatures.push(JSON.stringify(marks));
    }
    assert.equal(new Set(signatures.slice(0,4)).size,4);
    assert.equal(signatures[3],signatures[4]);
}
console.log('Pulse shape graphics: four distinct bounded glyphs; None is a steady flat line');
