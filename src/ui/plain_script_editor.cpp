#include "ui/plain_script_editor.h"

#include <QFontDatabase>
#include <QFontMetricsF>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QVBoxLayout>

#include <algorithm>

PlainScriptEditor::PlainScriptEditor(QWidget* parent) :
    ScriptEditor(parent),
    m_edit(new QPlainTextEdit(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_edit);

    setFocusProxy(m_edit);

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSizeF(std::max(font.pointSizeF(), 12.0));
    m_edit->setFont(font);
    m_edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_edit->setTabStopDistance(4 * QFontMetricsF(m_edit->font()).horizontalAdvance(QLatin1Char(' ')));

    connect(m_edit, &QPlainTextEdit::textChanged, this, [this]{
        if(!m_loading){
            editor_text_changed(m_edit->toPlainText());
        }
    });
}

void PlainScriptEditor::load_into_widget(const QString& text){
    m_loading = true;
    m_edit->setPlainText(text);     // also clears the undo history
    m_edit->setExtraSelections({});
    m_loading = false;
}

void PlainScriptEditor::show_errors(const QList<ScriptError>& errors){
    QList<QTextEdit::ExtraSelection> selections;

    for(const ScriptError& error : errors){
        const QTextBlock block = m_edit->document()->findBlockByNumber(error.line - 1);

        if(!block.isValid()){
            continue;
        }

        QTextEdit::ExtraSelection selection;
        selection.cursor = QTextCursor(block);
        selection.format.setBackground(QColor(120, 30, 30));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.format.setToolTip(error.message);

        selections.append(selection);
    }

    m_edit->setExtraSelections(selections);
}

void PlainScriptEditor::go_to_line(int line, int column){
    const QTextBlock block = m_edit->document()->findBlockByNumber(line - 1);

    if(!block.isValid()){
        return;
    }

    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
            qBound(0, column - 1, block.length() - 1));

    m_edit->setTextCursor(cursor);
    m_edit->centerCursor();
    m_edit->setFocus();
}

QString PlainScriptEditor::backend_name() const{
    return QStringLiteral("plain text");
}

QPlainTextEdit* PlainScriptEditor::text_edit() const{
    return m_edit;
}
