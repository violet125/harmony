#pragma once
#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>

class CodeEditor;
class LineNumbers : public QWidget {
public:
    explicit LineNumbers(CodeEditor *editor);
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    CodeEditor *m_editor;
};

class CppHighlighter : public QSyntaxHighlighter {
public:
    explicit CppHighlighter(QTextDocument *document);
protected:
    void highlightBlock(const QString &text) override;
private:
    struct Rule { QRegularExpression pattern; QTextCharFormat format; };
    QVector<Rule> m_rules;
    QTextCharFormat m_comment, m_string;
};

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CodeEditor(QWidget *parent = nullptr);
    int numberAreaWidth() const;
    void paintNumbers(QPaintEvent *event);
    QString filePath;
protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
private:
    void updateMargin();
    void highlightLine();
    LineNumbers *m_numbers;
};
