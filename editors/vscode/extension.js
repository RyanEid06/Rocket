'use strict';

const childProcess = require('child_process');
const vscode = require('vscode');

let client;

class RocketLanguageClient {
  constructor(context) {
    this.context = context;
    this.diagnostics = vscode.languages.createDiagnosticCollection('rocket');
    this.output = vscode.window.createOutputChannel('Rocket Language Server');
    this.pending = new Map();
    this.sequence = 1;
    this.buffer = Buffer.alloc(0);
    this.ready = false;
    this.stopping = false;
    context.subscriptions.push(this.diagnostics, this.output);
  }

  async start() {
    const configuration = vscode.workspace.getConfiguration('rocket.languageServer');
    const executable = configuration.get('path', 'rocket-lsp');
    this.trace = configuration.get('trace', false);
    this.process = childProcess.spawn(executable, [], {
      cwd: vscode.workspace.workspaceFolders?.[0]?.uri.fsPath,
      stdio: ['pipe', 'pipe', 'pipe'],
      windowsHide: true
    });
    this.process.stdout.on('data', data => this.receive(data));
    this.process.stderr.on('data', data => this.output.append(data.toString('utf8')));
    this.process.on('error', error => {
      this.output.appendLine(`Could not start ${executable}: ${error.message}`);
      this.failPending(error);
      vscode.window.showErrorMessage(
        `Rocket language server could not start. Configure rocket.languageServer.path. ${error.message}`
      );
    });
    this.process.on('exit', code => {
      this.failPending(new Error(`rocket-lsp exited with code ${code}`));
      if (!this.stopping && code !== 0) {
        this.output.appendLine(`rocket-lsp exited with code ${code}`);
      }
      this.ready = false;
    });

    const rootUri = vscode.workspace.workspaceFolders?.[0]?.uri.toString() ?? null;
    await this.request('initialize', {
      processId: process.pid,
      clientInfo: { name: 'rocket-vscode', version: '1.7.0' },
      rootUri,
      capabilities: {
        general: { positionEncodings: ['utf-16'] },
        textDocument: { publishDiagnostics: { versionSupport: true } }
      }
    });
    this.notify('initialized', {});
    this.ready = true;
    for (const document of vscode.workspace.textDocuments) this.open(document);

    this.context.subscriptions.push(
      vscode.workspace.onDidOpenTextDocument(document => this.open(document)),
      vscode.workspace.onDidChangeTextDocument(event => this.change(event.document)),
      vscode.workspace.onDidSaveTextDocument(document => this.save(document)),
      vscode.workspace.onDidCloseTextDocument(document => this.close(document))
    );
  }

  isRocket(document) {
    return document.languageId === 'rocket';
  }

  open(document) {
    if (!this.ready || !this.isRocket(document)) return;
    this.notify('textDocument/didOpen', {
      textDocument: {
        uri: document.uri.toString(),
        languageId: 'rocket',
        version: document.version,
        text: document.getText()
      }
    });
  }

  change(document) {
    if (!this.ready || !this.isRocket(document)) return;
    this.notify('textDocument/didChange', {
      textDocument: { uri: document.uri.toString(), version: document.version },
      contentChanges: [{ text: document.getText() }]
    });
  }

  save(document) {
    if (!this.ready || !this.isRocket(document)) return;
    this.notify('textDocument/didSave', {
      textDocument: { uri: document.uri.toString() },
      text: document.getText()
    });
  }

  close(document) {
    if (!this.ready || !this.isRocket(document)) return;
    this.notify('textDocument/didClose', {
      textDocument: { uri: document.uri.toString() }
    });
    this.diagnostics.delete(document.uri);
  }

  request(method, params) {
    const id = this.sequence++;
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      this.send({ jsonrpc: '2.0', id, method, params });
    });
  }

  notify(method, params) {
    this.send({ jsonrpc: '2.0', method, params });
  }

  send(message) {
    if (!this.process?.stdin.writable) return;
    const body = Buffer.from(JSON.stringify(message), 'utf8');
    if (this.trace) this.output.appendLine(`--> ${message.method ?? `response ${message.id}`}`);
    this.process.stdin.write(`Content-Length: ${body.length}\r\n\r\n`);
    this.process.stdin.write(body);
  }

  receive(data) {
    this.buffer = Buffer.concat([this.buffer, data]);
    while (true) {
      const headerEnd = this.buffer.indexOf('\r\n\r\n');
      if (headerEnd < 0) return;
      const header = this.buffer.subarray(0, headerEnd).toString('ascii');
      const match = /(?:^|\r\n)Content-Length:\s*(\d+)(?:\r\n|$)/i.exec(header);
      if (!match) {
        this.output.appendLine('rocket-lsp sent a malformed protocol header');
        this.process.kill();
        return;
      }
      const length = Number(match[1]);
      const bodyStart = headerEnd + 4;
      if (!Number.isSafeInteger(length) || length > 16 * 1024 * 1024) {
        this.output.appendLine('rocket-lsp sent an oversized protocol message');
        this.process.kill();
        return;
      }
      if (this.buffer.length < bodyStart + length) return;
      const body = this.buffer.subarray(bodyStart, bodyStart + length).toString('utf8');
      this.buffer = this.buffer.subarray(bodyStart + length);
      try {
        this.handle(JSON.parse(body));
      } catch (error) {
        this.output.appendLine(`Could not parse rocket-lsp response: ${error.message}`);
      }
    }
  }

  handle(message) {
    if (this.trace) this.output.appendLine(`<-- ${message.method ?? `response ${message.id}`}`);
    if (Object.prototype.hasOwnProperty.call(message, 'id')) {
      const pending = this.pending.get(message.id);
      if (!pending) return;
      this.pending.delete(message.id);
      if (message.error) pending.reject(new Error(message.error.message));
      else pending.resolve(message.result);
      return;
    }
    if (message.method !== 'textDocument/publishDiagnostics') return;
    const uri = vscode.Uri.parse(message.params.uri);
    const diagnostics = message.params.diagnostics.map(item => {
      const start = new vscode.Position(item.range.start.line, item.range.start.character);
      const end = new vscode.Position(item.range.end.line, item.range.end.character);
      const diagnostic = new vscode.Diagnostic(
        new vscode.Range(start, end), item.message, vscode.DiagnosticSeverity.Error
      );
      diagnostic.code = item.code;
      diagnostic.source = item.source ?? 'rocketc';
      return diagnostic;
    });
    this.diagnostics.set(uri, diagnostics);
  }

  failPending(error) {
    for (const pending of this.pending.values()) pending.reject(error);
    this.pending.clear();
  }

  async stop() {
    if (!this.process || this.stopping) return;
    this.stopping = true;
    try {
      if (this.ready) await this.request('shutdown', null);
      this.notify('exit');
    } catch (error) {
      this.output.appendLine(`Language-server shutdown failed: ${error.message}`);
    }
    this.process.stdin.end();
    const processHandle = this.process;
    setTimeout(() => {
      if (!processHandle.killed) processHandle.kill();
    }, 1000).unref();
    this.diagnostics.clear();
  }
}

async function activate(context) {
  client = new RocketLanguageClient(context);
  try {
    await client.start();
  } catch (error) {
    client.output.appendLine(`Language-server initialization failed: ${error.message}`);
    vscode.window.showErrorMessage(`Rocket language-server initialization failed: ${error.message}`);
  }
}

async function deactivate() {
  await client?.stop();
}

module.exports = { activate, deactivate };
