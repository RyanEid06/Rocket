'use strict';

const assert = require('assert');
const childProcess = require('child_process');
const fs = require('fs');
const path = require('path');
const {pathToFileURL} = require('url');

const root = path.resolve(__dirname, '..', '..', '..');
const compiler = path.resolve(process.argv[2] || path.join(root, 'out', 'build', 'windows-debug', 'rocketc.exe'));
const server = path.resolve(process.argv[3] || path.join(root, 'out', 'build', 'windows-debug', 'rocket-lsp.exe'));
const work = path.join(root, 'out', 'lsp-conformance');
fs.rmSync(work, {recursive: true, force: true});
fs.cpSync(path.join(root, 'tests', 'fixtures', 'phase16_packages'), work, {recursive: true});
const app = path.join(work, 'app');
const resolved = childProcess.spawnSync(compiler, ['resolve', app], {encoding: 'utf8'});
assert.strictEqual(resolved.status, 0, resolved.stderr || resolved.stdout);

const processHandle = childProcess.spawn(server, [], {stdio: ['pipe', 'pipe', 'pipe']});
let buffered = Buffer.alloc(0);
let nextId = 1;
const pending = new Map();
let stderr = '';
processHandle.stderr.on('data', chunk => { stderr += chunk.toString('utf8'); });

function frame(message) {
  const body = Buffer.from(JSON.stringify(message), 'utf8');
  return Buffer.concat([Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'), body]);
}

function send(message) {
  processHandle.stdin.write(frame(message));
}

function request(method, params) {
  const id = nextId++;
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      pending.delete(id);
      reject(new Error(`timed out waiting for ${method}; stderr=${stderr}`));
    }, 10000);
    pending.set(id, message => {
      clearTimeout(timer);
      if (message.error) reject(new Error(JSON.stringify(message.error)));
      else resolve(message.result);
    });
    send({jsonrpc: '2.0', id, method, params});
  });
}

processHandle.stdout.on('data', chunk => {
  buffered = Buffer.concat([buffered, chunk]);
  while (true) {
    const boundary = buffered.indexOf('\r\n\r\n');
    if (boundary < 0) return;
    const header = buffered.subarray(0, boundary).toString('ascii');
    const match = /^Content-Length:\s*(\d+)$/im.exec(header);
    assert(match, `invalid LSP header: ${header}`);
    const length = Number(match[1]);
    const bodyStart = boundary + 4;
    if (buffered.length < bodyStart + length) return;
    const message = JSON.parse(buffered.subarray(bodyStart, bodyStart + length).toString('utf8'));
    buffered = buffered.subarray(bodyStart + length);
    if (pending.has(message.id)) {
      const resolve = pending.get(message.id);
      pending.delete(message.id);
      resolve(message);
    }
  }
});

(async () => {
  const initialized = await request('initialize', {
    rootUri: pathToFileURL(app).href,
    capabilities: {},
  });
  assert.strictEqual(initialized.serverInfo.version, '1.0.0');
  send({jsonrpc: '2.0', method: 'initialized', params: {}});
  const source = path.join(app, 'src', 'main.rocket');
  const uri = pathToFileURL(source).href;
  send({jsonrpc: '2.0', method: 'textDocument/didOpen', params: {textDocument: {
    uri, languageId: 'rocket', version: 1,
    text: 'import math\n\nfn main() -> Int:\n    return math.answer()\n',
  }}});
  const started = Date.now();
  const completion = await request('textDocument/completion', {
    textDocument: {uri}, position: {line: 3, character: 16},
  });
  const elapsed = Date.now() - started;
  const status = await request('rocket/projectStatus', {});
  assert(completion.items.some(item => item.label === 'answer'),
    `locked dependency completion missing: ${JSON.stringify(completion)} status=${JSON.stringify(status)} stderr=${stderr}`);
  assert(elapsed < 5000, `completion exceeded latency gate: ${elapsed}ms`);
  assert(status.files >= 2 && status.files <= status.maximumProjectFiles);
  assert(status.bytes > 0 && status.bytes <= status.maximumProjectBytes);
  await request('shutdown', null);
  send({jsonrpc: '2.0', method: 'exit'});
  processHandle.stdin.end();
  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error('rocket-lsp did not exit')), 5000);
    processHandle.once('exit', code => {
      clearTimeout(timer);
      if (code === 0) resolve();
      else reject(new Error(`rocket-lsp exited ${code}; ${stderr}`));
    });
  });
  const report = {
    schema: 'rocket-lsp-conformance-1',
    client: 'Node.js editor-neutral protocol client',
    protocol: '1.0.0',
    files: status.files,
    bytes: status.bytes,
    completion_ms: elapsed,
    dependency_completion: 'answer',
    unsaved_overlay: true,
  };
  fs.writeFileSync(path.join(work, 'report.json'), `${JSON.stringify(report, null, 2)}\n`);
  console.log(`Rocket editor-neutral multi-package LSP conformance passed: ${path.join(work, 'report.json')}`);
})().catch(error => {
  processHandle.kill();
  console.error(error.stack || error);
  process.exitCode = 1;
});
