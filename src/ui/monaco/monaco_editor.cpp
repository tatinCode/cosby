#include "ui/monaco/monaco_editor.h"
#include "ui/monaco/monaco_bridge.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QPointer>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantMap>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>
#include <QWebEngineUrlSchemeHandler>
#include <QWebEngineView>

Q_LOGGING_CATEGORY(lc_monaco, "cosby.monaco")

namespace {
    const QByteArray scheme_name = QByteArrayLiteral("cosby-editor");
    const QString page_host = QStringLiteral("monaco");
    const QUrl page_url(QStringLiteral("cosby-editor://monaco/index.html"));

    constexpr int startup_timeout_ms = 30000;
    constexpr int sync_timeout_ms = 1000;

    QByteArray mime_type_for(const QString& path){
        const QString suffix = QFileInfo(path).suffix().toLower();

        if(suffix == "html"){ return "text/html"; }
        if(suffix == "js"){ return "text/javascript"; }
        if(suffix == "css"){ return "text/css"; }
        if(suffix == "json"){ return "application/json"; }
        if(suffix == "ttf"){ return "font/ttf"; }
        if(suffix == "svg"){ return "image/svg+xml"; }
        if(suffix == "ts"){ return "text/plain"; }

        return "application/octet-stream";
    }

    // Serves cosby-editor://monaco/<path>. Only a fixed set of resource files
    // and regular files inside the Monaco folder are reachable.
    class MonacoSchemeHandler : public QWebEngineUrlSchemeHandler {
        public:
            MonacoSchemeHandler(const QString& monaco_directory, QObject* parent) :
                QWebEngineUrlSchemeHandler(parent),
                m_root(QFileInfo(monaco_directory).canonicalFilePath())
            {
            }

            void requestStarted(QWebEngineUrlRequestJob* job) override{
                const QUrl url = job->requestUrl();

                if(job->requestMethod() != "GET" || url.host() != page_host){
                    job->fail(QWebEngineUrlRequestJob::RequestDenied);
                    return;
                }

                const QString file_path = resolve(QDir::cleanPath(url.path(QUrl::FullyDecoded)));

                if(file_path.isEmpty()){
                    qCDebug(lc_monaco) << "not found:" << url.toString();
                    job->fail(QWebEngineUrlRequestJob::UrlNotFound);
                    return;
                }

                auto* file = new QFile(file_path);

                if(!file->open(QIODevice::ReadOnly)){
                    delete file;
                    job->fail(QWebEngineUrlRequestJob::UrlNotFound);
                    return;
                }

                // WebEngine reads the device until the job is gone, and does not own it.
                QObject::connect(job, &QObject::destroyed, file, &QObject::deleteLater);
                job->reply(mime_type_for(file_path), file);
            }

        private:
            QString resolve(const QString& path) const{
                if(path == "/index.html"){ return QStringLiteral(":/monaco_host/index.html"); }
                if(path == "/host.js"){ return QStringLiteral(":/monaco_host/host.js"); }
                if(path == "/qwebchannel.js"){ return QStringLiteral(":/qtwebchannel/qwebchannel.js"); }

                if(m_root.isEmpty() || !path.startsWith("/vs/")){
                    return {};
                }

                // Canonicalising resolves ".." and symlinks; the result must stay inside the root.
                const QFileInfo info(QDir(m_root).filePath(path.mid(1)));
                const QString canonical = info.canonicalFilePath();

                if(canonical.isEmpty() || !canonical.startsWith(m_root + '/') || !QFileInfo(canonical).isFile()){
                    return {};
                }

                return canonical;
            }

            QString m_root;
    };

    class MonacoPage : public QWebEnginePage {
        public:
            using QWebEnginePage::QWebEnginePage;

        protected:
            void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message,
                    int line, const QString& source) override{
                if(level == ErrorMessageLevel){
                    qCWarning(lc_monaco).noquote() << message << QString("(%1:%2)").arg(source).arg(line);
                }
                else{
                    qCDebug(lc_monaco).noquote() << message << QString("(%1:%2)").arg(source).arg(line);
                }
            }

            // Stay on the editor page: e.g. a file dropped onto the view would
            // otherwise navigate away and take the editor with it.
            bool acceptNavigationRequest(const QUrl& url, NavigationType, bool is_main_frame) override{
                if(!is_main_frame){
                    return false;
                }

                return url == page_url;
            }
    };

    QString read_resource(const QString& path){
        QFile file(path);

        if(!file.open(QIODevice::ReadOnly)){
            return {};
        }

        return QString::fromUtf8(file.readAll());
    }
}

void MonacoEditor::register_url_scheme(){
    static bool registered = false;

    if(registered){
        return;
    }

    registered = true;

    QWebEngineUrlScheme scheme(scheme_name);
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Host);
    // A real (secure) origin, so Monaco's web workers load same-origin.
    // Inside workers Monaco's loader uses fetch(), which custom schemes
    // only get with FetchApiAllowed; without it autocomplete is dead.
    QWebEngineUrlScheme::Flags flags = QWebEngineUrlScheme::SecureScheme | QWebEngineUrlScheme::CorsEnabled;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    flags |= QWebEngineUrlScheme::FetchApiAllowed;
#endif
    scheme.setFlags(flags);

    QWebEngineUrlScheme::registerScheme(scheme);
}

QString MonacoEditor::monaco_directory(){
    const QString overridden = qEnvironmentVariable("COSBY_MONACO_DIR");

    if(!overridden.isEmpty()){
        return QDir(overridden).absolutePath();
    }

    return QDir(QCoreApplication::applicationDirPath()).filePath("monaco");
}

bool MonacoEditor::is_available(QString* reason){
    const QDir dir(monaco_directory());

    for(const char* required : {"vs/loader.js", "vs/editor/editor.main.js"}){
        if(!QFileInfo(dir.filePath(required)).isFile()){
            if(reason){
                *reason = QString("Monaco not found in %1, using the plain editor").arg(dir.path());
            }

            return false;
        }
    }

    return true;
}

MonacoEditor::MonacoEditor(QWidget* parent) :
    ScriptEditor(parent),
    m_profile(new QWebEngineProfile(this)),     // off-the-record: nothing written to disk
    m_view(new QWebEngineView(this)),
    m_channel(new QWebChannel(this)),
    m_bridge(new MonacoBridge(read_resource(":/monaco_host/cosby.d.ts"), this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    setFocusProxy(m_view);

    m_profile->installUrlSchemeHandler(scheme_name, new MonacoSchemeHandler(monaco_directory(), m_profile));

    auto* page = new MonacoPage(m_profile, m_view);
    page->setBackgroundColor(QColor(0x1e, 0x1e, 0x1e));     // no white flash before Monaco paints

    QWebEngineSettings* settings = page->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
    settings->setAttribute(QWebEngineSettings::JavascriptCanPaste, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);

    m_channel->registerObject(QStringLiteral("host"), m_bridge);
    page->setWebChannel(m_channel);

    m_view->setPage(page);
    m_view->setContextMenuPolicy(Qt::NoContextMenu);    // Monaco draws its own

    connect(m_bridge, &MonacoBridge::ready, this, &MonacoEditor::on_page_ready);
    connect(m_bridge, &MonacoBridge::content_changed, this, [this](const QString& text, double generation){
        if(is_current_generation(generation)){
            editor_text_changed(text);
        }
    });
    connect(m_bridge, &MonacoBridge::save, this, &ScriptEditor::save_requested);
    connect(m_bridge, &MonacoBridge::run, this, &ScriptEditor::run_requested);
    connect(m_bridge, &MonacoBridge::failure, this, &MonacoEditor::fail);

    connect(page, &QWebEnginePage::loadFinished, this, [this](bool ok){
        if(!ok){
            fail("The editor page could not be loaded");
        }
    });

    connect(page, &QWebEnginePage::renderProcessTerminated, this,
            [this](QWebEnginePage::RenderProcessTerminationStatus status, int){
        if(status != QWebEnginePage::NormalTerminationStatus){
            fail("The editor process stopped unexpectedly");
        }
    });

    QTimer::singleShot(startup_timeout_ms, this, [this]{
        if(!m_ready){
            fail("Monaco did not start in time");
        }
    });

    page->load(page_url);
}

MonacoEditor::~MonacoEditor(){
    *m_alive = false;

    // The page must go before its profile (both are children of this widget,
    // and the profile was created first).
    delete m_view;
    m_view = nullptr;
}

bool MonacoEditor::is_ready() const{
    return m_ready;
}

QWebEnginePage* MonacoEditor::web_page() const{
    return m_view ? m_view->page() : nullptr;
}

void MonacoEditor::on_page_ready(){
    if(m_ready || m_failed){
        return;
    }

    m_ready = true;
    qCDebug(lc_monaco) << "Monaco ready";

    // Always push: it carries text set before the page was ready (e.g. an
    // opened project) and the current generation.
    emit m_bridge->set_text_requested(text(), static_cast<double>(text_generation()));

    push_errors();

    emit ready();
}

void MonacoEditor::fail(const QString& reason){
    if(m_failed){
        return;
    }

    m_failed = true;
    qCWarning(lc_monaco).noquote() << "Monaco failed:" << reason;

    emit failed(reason);
}

void MonacoEditor::sync(std::function<void()> done){
    if(!m_ready || m_failed){
        if(done){
            done();
        }

        return;
    }

    // Whichever comes first: the page's answer, or the timeout (then the
    // cached text, at most one keystroke behind, is used).
    auto finished = std::make_shared<bool>(false);
    auto alive = m_alive;

    auto finish = [finished, alive, done]{
        if(*finished || !*alive){
            return;
        }

        *finished = true;

        if(done){
            done();
        }
    };

    QPointer<MonacoEditor> self(this);

    // Returns {text, generation}; the generation check drops an answer that
    // predates a set_text() the page has not processed yet.
    m_view->page()->runJavaScript(
            QStringLiteral("window.cosbyGetText ? window.cosbyGetText() : null"),
            [self, alive, finish](const QVariant& result){
                const QVariantMap answer = result.toMap();

                if(*alive && self && answer.value("text").typeId() == QMetaType::QString &&
                        self->is_current_generation(answer.value("generation").toDouble())){
                    self->editor_text_changed(answer.value("text").toString());
                }

                finish();
            });

    QTimer::singleShot(sync_timeout_ms, this, finish);
}

void MonacoEditor::show_errors(const QList<ScriptError>& errors){
    m_errors = errors;
    push_errors();
}

void MonacoEditor::push_errors(){
    if(!m_ready){
        return;
    }

    QVariantList list;

    for(const ScriptError& error : m_errors){
        list.append(QVariantMap{
                {"line", error.line},
                {"column", error.column},
                {"message", error.message},
                });
    }

    emit m_bridge->markers_requested(list);
}

void MonacoEditor::go_to_line(int line, int column){
    if(m_ready){
        emit m_bridge->go_to_requested(line, column);
    }

    m_view->setFocus();
}

QString MonacoEditor::backend_name() const{
    return QStringLiteral("Monaco");
}

void MonacoEditor::load_into_widget(const QString& text){
    // A new model also clears undo history and markers.
    m_errors.clear();

    if(m_ready){
        emit m_bridge->set_text_requested(text, static_cast<double>(text_generation()));
    }
}

bool MonacoEditor::is_current_generation(double generation) const{
    return generation >= 0 && static_cast<quint64>(generation) == text_generation();
}
