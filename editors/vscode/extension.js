'use strict';

const childProcess = require('child_process');
const vscode = require('vscode');

const maximumHeaderBytes = 16 * 1024;
const maximumMessageBytes = 16 * 1024 * 1024;
const requestTimeoutMilliseconds = 10_000;

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
    const initialization = await this.request('initialize', {
      processId: process.pid,
      clientInfo: { name: 'rocket-vscode', version: '1.7.0' },
      rootUri,
      capabilities: {
        general: { positionEncodings: ['utf-16'] },
        textDocument: { publishDiagnostics: { versionSupport: true } }
      }
    });
    this.serverCapabilities = initialization.capabilities ?? {};
    this.notify('initialized', {});
    this.sendConfiguration();
    this.ready = true;
    for (const document of vscode.workspace.textDocuments) this.open(document);

    this.context.subscriptions.push(
      vscode.workspace.onDidOpenTextDocument(document => this.open(document)),
      vscode.workspace.onDidChangeTextDocument(event => this.change(event)),
      vscode.workspace.onDidSaveTextDocument(document => this.save(document)),
      vscode.workspace.onDidCloseTextDocument(document => this.close(document)),
      vscode.workspace.onDidChangeConfiguration(event => {
        if (event.affectsConfiguration('rocket.languageServer')) this.sendConfiguration();
      })
    );
    this.registerProviders();
    this.registerWorkspaceWatcher();
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

  change(event) {
    const document = event.document;
    if (!this.ready || !this.isRocket(document)) return;
    this.notify('textDocument/didChange', {
      textDocument: { uri: document.uri.toString(), version: document.version },
      contentChanges: event.contentChanges.map(change => ({
        range: {
          start: { line: change.range.start.line, character: change.range.start.character },
          end: { line: change.range.end.line, character: change.range.end.character }
        },
        rangeLength: change.rangeLength,
        text: change.text
      }))
    });
  }

  sendConfiguration() {
    const configuration = vscode.workspace.getConfiguration('rocket.languageServer');
    this.notify('workspace/didChangeConfiguration', {
      settings: {
        rocket: {
          languageServer: {
            maximumProjectFiles: configuration.get('maximumProjectFiles', 4096),
            maximumProjectBytes: configuration.get('maximumProjectBytes', 64 * 1024 * 1024),
            telemetry: configuration.get('telemetry', true)
          }
        }
      }
    });
  }

  textDocumentPosition(document, position) {
    return {
      textDocument: { uri: document.uri.toString() },
      position: { line: position.line, character: position.character }
    };
  }

  toPosition(position) {
    return new vscode.Position(position.line, position.character);
  }

  toRange(range) {
    return new vscode.Range(this.toPosition(range.start), this.toPosition(range.end));
  }

  toLocation(location) {
    return new vscode.Location(vscode.Uri.parse(location.uri), this.toRange(location.range));
  }

  toWorkspaceEdit(edit) {
    const result = new vscode.WorkspaceEdit();
    for (const [uri, edits] of Object.entries(edit?.changes ?? {})) {
      const target = vscode.Uri.parse(uri);
      for (const item of edits) result.replace(target, this.toRange(item.range), item.newText);
    }
    return result;
  }

  requestWithCancellation(method, params, token) {
    const request = this.request(method, params);
    const id = this.sequence - 1;
    const subscription = token?.onCancellationRequested(() => {
      this.notify('$/cancelRequest', { id });
    });
    return request.finally(() => subscription?.dispose());
  }

  registerProviders() {
    const selector = { language: 'rocket', scheme: 'file' };
    const semanticLegend = new vscode.SemanticTokensLegend(
      ['namespace', 'type', 'struct', 'enum', 'interface', 'typeParameter',
       'parameter', 'variable', 'property', 'function', 'method', 'keyword',
       'string', 'number', 'comment', 'decorator'],
      ['declaration', 'definition', 'readonly', 'static', 'deprecated', 'abstract',
       'async', 'modification', 'documentation', 'defaultLibrary']
    );
    this.context.subscriptions.push(
      vscode.languages.registerCompletionItemProvider(selector, {
        provideCompletionItems: async (document, position, token, context) => {
          const result = await this.requestWithCancellation('textDocument/completion', {
            ...this.textDocumentPosition(document, position), context
          }, token);
          const items = (result.items ?? result).map(raw => {
            const item = new vscode.CompletionItem(raw.label, raw.kind);
            item.detail = raw.detail;
            item.sortText = raw.sortText;
            item.filterText = raw.filterText;
            if (raw.documentation) item.documentation = new vscode.MarkdownString(raw.documentation.value ?? raw.documentation);
            if (raw.additionalTextEdits) {
              item.additionalTextEdits = raw.additionalTextEdits.map(edit =>
                vscode.TextEdit.replace(this.toRange(edit.range), edit.newText));
            }
            return item;
          });
          return new vscode.CompletionList(items, Boolean(result.isIncomplete));
        }
      }, '.'),
      vscode.languages.registerHoverProvider(selector, {
        provideHover: async (document, position, token) => {
          const raw = await this.requestWithCancellation('textDocument/hover',
            this.textDocumentPosition(document, position), token);
          if (!raw) return undefined;
          return new vscode.Hover(new vscode.MarkdownString(raw.contents.value), this.toRange(raw.range));
        }
      }),
      vscode.languages.registerDefinitionProvider(selector, {
        provideDefinition: async (document, position, token) => {
          const raw = await this.requestWithCancellation('textDocument/definition',
            this.textDocumentPosition(document, position), token);
          return raw.map(location => this.toLocation(location));
        }
      }),
      vscode.languages.registerReferenceProvider(selector, {
        provideReferences: async (document, position, context, token) => {
          const raw = await this.requestWithCancellation('textDocument/references', {
            ...this.textDocumentPosition(document, position), context
          }, token);
          return raw.map(location => this.toLocation(location));
        }
      }),
      vscode.languages.registerRenameProvider(selector, {
        prepareRename: async (document, position, token) => {
          const raw = await this.requestWithCancellation('textDocument/prepareRename',
            this.textDocumentPosition(document, position), token);
          return { range: this.toRange(raw.range), placeholder: raw.placeholder };
        },
        provideRenameEdits: async (document, position, newName, token) => {
          const raw = await this.requestWithCancellation('textDocument/rename', {
            ...this.textDocumentPosition(document, position), newName
          }, token);
          return this.toWorkspaceEdit(raw);
        }
      }),
      vscode.languages.registerSignatureHelpProvider(selector, {
        provideSignatureHelp: async (document, position, token, context) => {
          const raw = await this.requestWithCancellation('textDocument/signatureHelp', {
            ...this.textDocumentPosition(document, position), context
          }, token);
          const help = new vscode.SignatureHelp();
          help.activeParameter = raw.activeParameter;
          help.activeSignature = raw.activeSignature;
          help.signatures = raw.signatures.map(signature => {
            const item = new vscode.SignatureInformation(signature.label, signature.documentation);
            item.parameters = signature.parameters.map(parameter =>
              new vscode.ParameterInformation(parameter.label, parameter.documentation));
            return item;
          });
          return help;
        }
      }, '(', ','),
      vscode.languages.registerDocumentSemanticTokensProvider(selector, {
        provideDocumentSemanticTokens: async (document, token) => {
          const raw = await this.requestWithCancellation('textDocument/semanticTokens/full',
            { textDocument: { uri: document.uri.toString() } }, token);
          return new vscode.SemanticTokens(new Uint32Array(raw.data), raw.resultId);
        }
      }, semanticLegend),
      vscode.languages.registerCodeActionsProvider(selector, {
        provideCodeActions: async (document, range, context, token) => {
          const raw = await this.requestWithCancellation('textDocument/codeAction', {
            textDocument: { uri: document.uri.toString() },
            range: {
              start: { line: range.start.line, character: range.start.character },
              end: { line: range.end.line, character: range.end.character }
            },
            context: {
              diagnostics: context.diagnostics.map(diagnostic => ({
                range: {
                  start: { line: diagnostic.range.start.line, character: diagnostic.range.start.character },
                  end: { line: diagnostic.range.end.line, character: diagnostic.range.end.character }
                },
                code: diagnostic.code,
                message: diagnostic.message,
                source: diagnostic.source
              }))
            }
          }, token);
          return raw.map(action => {
            const item = new vscode.CodeAction(action.title, action.kind);
            item.isPreferred = action.isPreferred;
            item.edit = this.toWorkspaceEdit(action.edit);
            return item;
          });
        }
      }, { providedCodeActionKinds: [vscode.CodeActionKind.QuickFix,
                                     vscode.CodeActionKind.Source] })
    );
  }

  registerWorkspaceWatcher() {
    const watcher = vscode.workspace.createFileSystemWatcher('**/*.rocket');
    const changed = (uri, type) => this.notify('workspace/didChangeWatchedFiles', {
      changes: [{ uri: uri.toString(), type }]
    });
    this.context.subscriptions.push(
      watcher,
      watcher.onDidCreate(uri => changed(uri, 1)),
      watcher.onDidChange(uri => changed(uri, 2)),
      watcher.onDidDelete(uri => changed(uri, 3))
    );
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

  request(method, params, timeoutMilliseconds = requestTimeoutMilliseconds) {
    const id = this.sequence++;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        if (!this.pending.delete(id)) return;
        reject(new Error(`rocket-lsp did not answer ${method} within ${timeoutMilliseconds} ms`));
      }, timeoutMilliseconds);
      this.pending.set(id, { resolve, reject, timer });
      if (!this.send({ jsonrpc: '2.0', id, method, params })) {
        clearTimeout(timer);
        this.pending.delete(id);
        reject(new Error(`rocket-lsp is not writable while sending ${method}`));
      }
    });
  }

  notify(method, params) {
    this.send({ jsonrpc: '2.0', method, params });
  }

  send(message) {
    if (!this.process?.stdin.writable) return false;
    const body = Buffer.from(JSON.stringify(message), 'utf8');
    if (this.trace) this.output.appendLine(`--> ${message.method ?? `response ${message.id}`}`);
    this.process.stdin.write(`Content-Length: ${body.length}\r\n\r\n`);
    this.process.stdin.write(body);
    return true;
  }

  receive(data) {
    this.buffer = Buffer.concat([this.buffer, data]);
    while (true) {
      const headerEnd = this.buffer.indexOf('\r\n\r\n');
      if (headerEnd < 0) {
        if (this.buffer.length > maximumHeaderBytes) {
          this.output.appendLine('rocket-lsp sent an oversized protocol header');
          this.process.kill();
        }
        return;
      }
      const bodyStart = headerEnd + 4;
      if (bodyStart > maximumHeaderBytes) {
        this.output.appendLine('rocket-lsp sent an oversized protocol header');
        this.process.kill();
        return;
      }
      const header = this.buffer.subarray(0, headerEnd).toString('ascii');
      const match = /(?:^|\r\n)Content-Length:\s*(\d+)(?:\r\n|$)/i.exec(header);
      if (!match) {
        this.output.appendLine('rocket-lsp sent a malformed protocol header');
        this.process.kill();
        return;
      }
      const length = Number(match[1]);
      if (!Number.isSafeInteger(length) || length > maximumMessageBytes) {
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
      clearTimeout(pending.timer);
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
    for (const pending of this.pending.values()) {
      clearTimeout(pending.timer);
      pending.reject(error);
    }
    this.pending.clear();
  }

  async stop() {
    if (!this.process || this.stopping) return;
    this.stopping = true;
    try {
      if (this.ready) await this.request('shutdown', null, 1000);
    } catch (error) {
      this.output.appendLine(`Language-server shutdown failed: ${error.message}`);
    }
    this.notify('exit');
    this.process.stdin.end();
    const processHandle = this.process;
    setTimeout(() => {
      if (processHandle.exitCode === null && processHandle.signalCode === null) {
        processHandle.kill();
      }
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

module.exports = { activate, deactivate, RocketLanguageClient };
