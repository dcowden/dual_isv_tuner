// Execute the actual embedded browser script with minimal DOM/network doubles.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');

const source = fs.readFileSync(path.join(__dirname, '../src/debug_page.h'), 'utf8');
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];
const elements = Object.fromEntries(['status', 'capture', 'pause', 'save'].map(id => [id, {textContent: ''}]));
const timers = new Map();
let nextTimer = 0, fetchCount = 0, fail = false, saved;
const capture = 'PC RX: 3\nA discarded 00 80 FF\n';
const context = {
    document: {
        getElementById: id => elements[id],
        createElement: () => ({click() { saved = this.download; }})
    },
    fetch: async url => {
        assert.equal(url, '/capture.txt');
        ++fetchCount;
        if (fail) throw new Error('disconnected');
        return {ok: true, text: async () => capture};
    },
    setTimeout: (fn, delay) => { const id = ++nextTimer; timers.set(id, {fn, delay}); return id; },
    clearTimeout: id => timers.delete(id),
    AbortController, Blob, Date,
    URL: {createObjectURL(blob) { assert.equal(blob.size, capture.length); return 'blob:capture'; }, revokeObjectURL() {}}
};
const settle = () => new Promise(resolve => setImmediate(resolve));
const nextPoll = () => {
    const [id, timer] = [...timers].find(([, timer]) => timer.delay === 750);
    timers.delete(id); timer.fn();
};

(async () => {
    vm.runInNewContext(script, context);
    await settle();
    assert.equal(elements.capture.textContent, capture);
    assert.match(elements.status.textContent, /^Live/);
    elements.pause.onclick();
    assert.match(elements.status.textContent, /paused/);
    nextPoll(); await settle();
    assert.equal(fetchCount, 1);
    elements.save.onclick();
    assert.match(saved, /^servo-capture-.*\.txt$/);
    elements.pause.onclick();
    fail = true; nextPoll(); await settle();
    assert.match(elements.status.textContent, /Disconnected/);
    assert.equal(elements.capture.textContent, capture); // Preserve last known evidence.
    fail = false; nextPoll(); await settle();
    assert.match(elements.status.textContent, /^Live/);
    assert.equal(fetchCount, 3);
    console.log('Debug page polling, pause, export, disconnect, and reconnect tests passed');
})().catch(error => { console.error(error); process.exitCode = 1; });
