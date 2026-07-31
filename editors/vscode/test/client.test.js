'use strict';

const assert = require('assert');
const Module = require('module');

const outputLines = [];
const vscodeMock = {
  languages: {
    createDiagnosticCollection: () => ({
      clear() {},
      delete() {},
      dispose() {},
      set() {}
    })
  },
  window: {
    createOutputChannel: () => ({
      append() {},
      appendLine(line) { outputLines.push(line); },
      dispose() {}
    })
  }
};

const originalLoad = Module._load;
Module._load = function load(request, parent, isMain) {
  if (request === 'vscode') return vscodeMock;
  return originalLoad.call(this, request, parent, isMain);
};
const { RocketLanguageClient } = require('../extension.js');
Module._load = originalLoad;

function createClient() {
  return new RocketLanguageClient({ subscriptions: [] });
}

async function main() {
  const unwritable = createClient();
  unwritable.process = { stdin: { writable: false } };
  await assert.rejects(
    unwritable.request('initialize', {}, 50),
    /not writable/
  );
  assert.strictEqual(unwritable.pending.size, 0);

  const unresponsive = createClient();
  unresponsive.process = { stdin: { writable: true, write() {} } };
  await assert.rejects(
    unresponsive.request('initialize', {}, 10),
    /did not answer initialize/
  );
  assert.strictEqual(unresponsive.pending.size, 0);

  let killed = false;
  const oversized = createClient();
  oversized.process = { kill() { killed = true; } };
  oversized.receive(Buffer.alloc(16 * 1024 + 1, 0x41));
  assert.strictEqual(killed, true);
  assert(outputLines.includes('rocket-lsp sent an oversized protocol header'));

  console.log('VS Code language client tests passed');
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
