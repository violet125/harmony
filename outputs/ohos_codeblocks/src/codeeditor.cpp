#include "codeeditor.h"
#include <QFontDatabase>
#include <QKeyEvent>
#include <QPainter>
#include <QTextBlock>

LineNumbers::LineNumbers(CodeEditor *editor) : QWidget(editor), m_editor(editor) {}
QSize LineNumbers::sizeHint() const { return QSize(m_editor->numberAreaWidth(), 0); }
void LineNumbers::paintEvent(QPaintEvent *event) { m_editor->paintNumbers(event); }

CppHighlighter::CppHighlighter(QTextDocument *document) : QSyntaxHighlighter(document) {
    auto add = [this](const QString &pattern, const QColor &color, bool bold = false) {
        QTextCharFormat format; format.setForeground(color);
        if (bold) format.setFontWeight(QFont::DemiBold);
        m_rules.push_back({QRegularExpression(pattern), format});
    };
    add(QStringLiteral("\\b(auto|bool|break|case|char|class|const|constexpr|continue|default|delete|do|double|else|enum|extern|false|float|for|if|inline|int|long|namespace|new|nullptr|private|protected|public|return|short|signed|sizeof|static|struct|switch|template|this|true|typedef|typename|union|unsigned|using|virtual|void|volatile|while)\\b"), QColor("#c8a0ef"), true);
    add(QStringLiteral("\\b[0-9]+(?:\\.[0-9]+)?(?:[uUlLfF]+)?\\b"), QColor("#e4b779"));
    add(QStringLiteral("\\b[A-Za-z_]\\w*(?=\\s*\\()"), QColor("#83c9e6"));
    add(QStringLiteral("^\\s*#[^\n]*"), QColor("#e4b779"));
    m_comment.setForeground(QColor("#73899a"));
    m_string.setForeground(QColor("#a5c98d"));
}

void CppHighlighter::highlightBlock(const QString &text) {
    for (const auto &rule : m_rules) {
        auto it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next(); setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
    // Scan strings and comments together so comment markers inside strings are harmless.
    bool inComment = previousBlockState() == 1;
    int i = 0;
    while (i < text.size()) {
        if (inComment || text.mid(i, 2) == "/*") {
            const int end = text.indexOf("*/", i + (inComment ? 0 : 2));
            if (end < 0) { setFormat(i, text.size() - i, m_comment); inComment = true; break; }
            setFormat(i, end + 2 - i, m_comment); i = end + 2; inComment = false;
        } else if (text.mid(i, 2) == "//") {
            setFormat(i, text.size() - i, m_comment); break;
        } else if (text.at(i) == '"' || text.at(i) == '\'') {
            const auto quote = text.at(i);
            const int start = i++;
            while (i < text.size()) {
                if (text.at(i) == '\\') { i = qMin(i + 2, int(text.size())); continue; }
                if (text.at(i++) == quote) break;
            }
            setFormat(start, i - start, m_string);
        } else ++i;
    }
    setCurrentBlockState(inComment ? 1 : 0);
}

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent), m_numbers(new LineNumbers(this)) {
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    auto editorFont = font(); editorFont.setPointSize(11); setFont(editorFont);
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * 4);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    new CppHighlighter(document());
    connect(this, &QPlainTextEdit::blockCountChanged, this, &CodeEditor::updateMargin);
    connect(this, &QPlainTextEdit::cursorPositionChanged, this, &CodeEditor::highlightLine);
    connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect &rect, int dy) {
        if (dy) m_numbers->scroll(0, dy);
        else m_numbers->update(0, rect.y(), m_numbers->width(), rect.height());
        if (rect.contains(viewport()->rect())) updateMargin();
    });
    updateMargin(); highlightLine();
}
int CodeEditor::numberAreaWidth() const {
    return 22 + fontMetrics().horizontalAdvance('9') * QString::number(qMax(1, blockCount())).size();
}
void CodeEditor::updateMargin() { setViewportMargins(numberAreaWidth(), 0, 0, 0); }
void CodeEditor::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);
    const auto rect = contentsRect();
    m_numbers->setGeometry(rect.left(), rect.top(), numberAreaWidth(), rect.height());
}
void CodeEditor::highlightLine() {
    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(QColor("#202f3e"));
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor(); selection.cursor.clearSelection();
    setExtraSelections({selection});
}
void CodeEditor::paintNumbers(QPaintEvent *event) {
    QPainter painter(m_numbers);
    painter.fillRect(event->rect(), QColor("#17222e"));
    auto block = firstVisibleBlock();
    int number = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            painter.setPen(number == textCursor().blockNumber() ? QColor("#e0ebf2") : QColor("#627889"));
            painter.drawText(0, top, m_numbers->width() - 10, fontMetrics().height(), Qt::AlignRight, QString::number(number + 1));
        }
        block = block.next(); top = bottom; bottom = top + qRound(blockBoundingRect(block).height()); ++number;
    }
}
void CodeEditor::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Tab && !textCursor().hasSelection()) { insertPlainText("    "); return; }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && !textCursor().hasSelection()) {
        const auto before = textCursor().block().text().left(textCursor().positionInBlock());
        QString indent = QRegularExpression("^\\s*").match(before).captured();
        if (before.trimmed().endsWith('{')) indent += "    ";
        QPlainTextEdit::keyPressEvent(event); insertPlainText(indent); return;
    }
    QPlainTextEdit::keyPressEvent(event);
}
