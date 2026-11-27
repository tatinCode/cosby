#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

// Exposed to the page as `host` through QWebChannel.
//   page -> C++ : the notify_* slots
//   C++ -> page : the *_requested signals (the page connects to them)
// Data crosses as signal/slot arguments, never pasted into JavaScript source.
//
// `generation` identifies which set_text_requested() a text belongs to, so
// C++ can drop updates that were in flight while a new text was loaded.
// (A double, because that is what a JS number is; exact up to 2^53.)
class MonacoBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString type_definitions READ type_definitions CONSTANT)

    public:
        explicit MonacoBridge(const QString& type_definitions, QObject* parent = nullptr);

        QString type_definitions() const;

    public slots:
        void notify_ready();
        void notify_content_changed(const QString& text, double generation);
        void notify_save();
        void notify_run();
        void notify_failure(const QString& reason);

    signals:
        // to the page
        void set_text_requested(const QString& text, double generation);
        void markers_requested(const QVariantList& errors);
        void go_to_requested(int line, int column);

        // to MonacoEditor
        void ready();
        void content_changed(const QString& text, double generation);
        void save();
        void run();
        void failure(const QString& reason);

    private:
        QString m_type_definitions;
};
