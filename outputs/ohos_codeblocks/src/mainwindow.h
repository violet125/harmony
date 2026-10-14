#pragma once
#include "project.h"
#include "runner.h"
#include <QMainWindow>
#include <QTemporaryDir>
#include <memory>

class CodeEditor;
class QTreeWidget;
class QTabWidget;
class QPlainTextEdit;
class QLineEdit;
class QComboBox;
class QLabel;
class QAction;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    bool openProject(const QString &fileName);
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    enum class Job { None, Build, Native, Check, Picoc };
    CodeEditor *editor() const;
    void createUi();
    void createActions();
    void refreshProject();
    void openFile(const QString &fileName);
    bool saveEditor(CodeEditor *editor);
    bool saveAll();
    bool confirmCloseEditors();
    bool closeTab(int index);
    void updateTab(CodeEditor *editor);
    void newProject();
    void addFile();
    void removeFile();
    void settings();
    void find();
    void build(bool runAfter);
    void runPicoc();
    bool startJob(Job job, const RunCommand &command, int timeoutMs);
    void finishJob(int code, bool success, const QString &reason);
    void setBusy(bool busy);
    void appendOutput(const QString &text);
    void showError(const QString &message);
    Project m_project;
    ProcessRunner m_runner;
    QTreeWidget *m_tree = nullptr;
    QTabWidget *m_tabs = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QLineEdit *m_input = nullptr;
    QComboBox *m_targets = nullptr;
    QLabel *m_cursor = nullptr;
    QLabel *m_mode = nullptr;
    QList<QAction *> m_idleActions;
    QAction *m_stop = nullptr;
    Job m_job = Job::None;
    bool m_runAfterBuild = false;
    QString m_nativeOutput, m_nativeDirectory, m_findText;
    std::unique_ptr<QTemporaryDir> m_stage;
    RunCommand m_picocCommand;
};
