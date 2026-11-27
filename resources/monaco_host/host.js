// Glue between Monaco and MonacoEditor (C++), see monaco_bridge.h.
(function () {
    'use strict';

    var MARKER_OWNER = 'cosby';

    var host = null;
    var editor = null;
    var generation = 0;
    var model_serial = 0;

    // Every model gets its own URI: the TypeScript worker caches files by
    // name + version, and a new model restarts at version 1, so reusing a
    // name would keep autocomplete/diagnostics on the previous script.
    function create_model(text) {
        model_serial += 1;
        var uri = monaco.Uri.parse('file:///cosby/script-' + model_serial + '.js');

        return monaco.editor.createModel(text, 'javascript', uri);
    }

    // For custom schemes URL.origin is "null", so Monaco would think its own
    // files are cross-origin and wrap workers in a blob: that cannot load
    // them. Point it at the (same-origin) worker script directly.
    window.MonacoEnvironment = {
        getWorkerUrl: function () {
            return new URL('vs/base/worker/workerMain.js', document.baseURI).href;
        }
    };

    function report_failure(message) {
        console.error('cosby: ' + message);

        if (host) {
            host.notify_failure(String(message));
        }
    }

    // Monaco rejects pending work with a "Canceled" error when a model is
    // replaced; that is expected, not a failure.
    window.addEventListener('unhandledrejection', function (event) {
        if (event.reason && event.reason.name === 'Canceled') {
            event.preventDefault();
        }
    });

    // Errors before the editor exists mean it will never start.
    window.addEventListener('error', function (event) {
        if (!editor) {
            report_failure(event.message || 'script error');
        }
    });

    function set_text(text, new_generation) {
        generation = new_generation;

        // A fresh model: clears undo history and markers of the old script.
        var old_model = editor.getModel();
        editor.setModel(null);

        if (old_model) {
            old_model.dispose();
        }

        editor.setModel(create_model(text));
    }

    function set_markers(errors) {
        var model = editor.getModel();

        if (!model) {
            return;
        }

        var markers = errors.map(function (error) {
            var line = Math.min(Math.max(1, error.line | 0), model.getLineCount());
            var start = error.column > 0
                ? error.column
                : Math.max(1, model.getLineFirstNonWhitespaceColumn(line));
            var end = Math.max(start + 1, model.getLineMaxColumn(line));

            return {
                severity: monaco.MarkerSeverity.Error,
                message: String(error.message),
                startLineNumber: line,
                startColumn: start,
                endLineNumber: line,
                endColumn: end
            };
        });

        monaco.editor.setModelMarkers(model, MARKER_OWNER, markers);
    }

    function go_to(line, column) {
        var position = { lineNumber: Math.max(1, line), column: Math.max(1, column) };

        editor.setPosition(position);
        editor.revealPositionInCenter(position);
        editor.focus();
    }

    function start() {
        var ts = monaco.languages.typescript;

        // Scripts run in QJSEngine: plain ECMAScript, no DOM.
        ts.javascriptDefaults.setCompilerOptions({
            target: ts.ScriptTarget.ES2020,
            allowNonTsExtensions: true,
            allowJs: true,
            lib: ['es2020']
        });
        ts.javascriptDefaults.addExtraLib(host.type_definitions, 'file:///cosby.d.ts');

        editor = monaco.editor.create(document.getElementById('editor'), {
            model: create_model(''),
            theme: 'vs-dark',
            automaticLayout: true,
            fontSize: 15,
            lineHeight: 22,
            padding: { top: 8 },
            smoothScrolling: true,
            cursorBlinking: 'smooth',
            renderLineHighlight: 'all',
            bracketPairColorization: { enabled: true },
            tabSize: 4,
            insertSpaces: true,
            minimap: { enabled: false },
            scrollBeyondLastLine: false,
            fixedOverflowWidgets: true
        });

        // Editor-level event, so it follows model replacement.
        editor.onDidChangeModelContent(function () {
            host.notify_content_changed(editor.getValue(), generation);
        });

        // Fallbacks in case a key reaches the page instead of the Qt shortcut.
        editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, function () {
            host.notify_save();
        });
        editor.addCommand(monaco.KeyCode.F5, function () {
            host.notify_run();
        });

        host.set_text_requested.connect(set_text);
        host.markers_requested.connect(set_markers);
        host.go_to_requested.connect(go_to);

        // Used by MonacoEditor::sync() through runJavaScript().
        window.cosbyGetText = function () {
            return { text: editor.getValue(), generation: generation };
        };

        host.notify_ready();
    }

    if (typeof qt === 'undefined' || !qt.webChannelTransport) {
        console.error('cosby: no web channel transport');
        return;
    }

    new QWebChannel(qt.webChannelTransport, function (channel) {
        host = channel.objects.host;

        // Absolute, so workers resolve modules the same way as the page.
        require.config({ paths: { vs: new URL('vs', document.baseURI).href } });

        require(['vs/editor/editor.main'], function () {
            try {
                start();
            } catch (error) {
                report_failure((error && error.stack) || error);
            }
        }, function (error) {
            report_failure('could not load Monaco: ' + error);
        });
    });
})();
