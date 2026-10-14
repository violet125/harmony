#include "mainwindow.h"
#include "codeeditor.h"
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_runner(this) {
    setWindowTitle(QStringLiteral("ohos_codeblocks · C / C++ 工作台"));
    resize(1240, 800);
    createUi(); createActions();
    connect(&m_runner, &ProcessRunner::output, this, &MainWindow::appendOutput);
    connect(&m_runner, &ProcessRunner::completed, this, &MainWindow::finishJob);
    QSettings config;
    restoreGeometry(config.value("window/geometry").toByteArray());
    setBusy(false);
    appendOutput(QStringLiteral("ohos_codeblocks 0.1 · Qt Widgets 原型\n打开 .cbp 工程，或通过「文件 → 新建工程」开始。\npicoc 默认要求沙箱；鸿蒙执行服务尚待平台接入。\n"));
}

void MainWindow::createUi() {
    m_tabs = new QTabWidget(this);
    m_tabs->setTabsClosable(true); m_tabs->setMovable(true); m_tabs->setDocumentMode(true);
    setCentralWidget(m_tabs);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) { closeTab(index); });
    auto projectDock = new QDockWidget(QStringLiteral("工程 / WORKSPACE"), this);
    projectDock->setObjectName("projectDock");
    m_tree = new QTreeWidget(projectDock);
    m_tree->setHeaderHidden(true); m_tree->setMinimumWidth(225);
    projectDock->setWidget(m_tree); addDockWidget(Qt::LeftDockWidgetArea, projectDock);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item, int) {
        const auto path = item->data(0, Qt::UserRole).toString();
        if (!path.isEmpty()) openFile(path);
    });
    auto outputDock = new QDockWidget(QStringLiteral("构建与运行 / OUTPUT"), this);
    outputDock->setObjectName("outputDock");
    auto output = new QWidget(outputDock);
    auto layout = new QVBoxLayout(output); layout->setContentsMargins(6, 4, 6, 6);
    m_log = new QPlainTextEdit(output); m_log->setReadOnly(true); m_log->setMaximumBlockCount(10000);
    m_log->setMinimumHeight(150); layout->addWidget(m_log);
    auto row = new QHBoxLayout;
    m_input = new QLineEdit(output); m_input->setPlaceholderText(QStringLiteral("标准输入：输入后按 Enter 发送"));
    m_input->setMaxLength(4096);
    auto send = new QPushButton(QStringLiteral("发送"), output);
    auto eof = new QPushButton(QStringLiteral("结束输入"), output);
    auto clear = new QPushButton(QStringLiteral("清空"), output);
    row->addWidget(m_input); row->addWidget(send); row->addWidget(eof); row->addWidget(clear);
    layout->addLayout(row); outputDock->setWidget(output); addDockWidget(Qt::BottomDockWidgetArea, outputDock);
    auto sendInput = [this] {
        if (!m_runner.isRunning() || (m_job != Job::Native && m_job != Job::Picoc)) return;
        m_runner.sendInput(m_input->text() + '\n'); m_input->clear();
    };
    connect(m_input, &QLineEdit::returnPressed, this, sendInput);
    connect(send, &QPushButton::clicked, this, sendInput);
    connect(eof, &QPushButton::clicked, &m_runner, &ProcessRunner::closeInput);
    connect(clear, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);
    m_mode = new QLabel(QStringLiteral("就绪"), this);
    m_cursor = new QLabel(QStringLiteral("UTF-8  |  C / C++"), this);
    statusBar()->addWidget(m_mode); statusBar()->addPermanentWidget(m_cursor);
}

void MainWindow::createActions() {
    auto file = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    auto edit = menuBar()->addMenu(QStringLiteral("编辑(&E)"));
    auto project = menuBar()->addMenu(QStringLiteral("工程(&P)"));
    auto buildMenu = menuBar()->addMenu(QStringLiteral("构建(&B)"));
    auto tools = menuBar()->addMenu(QStringLiteral("工具(&T)"));
    auto help = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    auto bar = addToolBar(QStringLiteral("主工具栏")); bar->setObjectName("mainToolbar"); bar->setMovable(false);
    auto action = [this](QMenu *menu, const QString &name, const QKeySequence &shortcut, auto callback, bool idle = false) {
        auto a = menu->addAction(name); a->setShortcut(shortcut);
        connect(a, &QAction::triggered, this, callback);
        if (idle) m_idleActions << a;
        return a;
    };
    action(file, QStringLiteral("新建工程…"), QKeySequence("Ctrl+Shift+N"), [this] { newProject(); }, true);
    bar->addAction(action(file, QStringLiteral("打开工程…"), QKeySequence("Ctrl+O"), [this] {
        const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("打开工程"), {}, "Code::Blocks (*.cbp)");
        if (!path.isEmpty()) openProject(path);
    }, true));
    action(file, QStringLiteral("打开源文件…"), QKeySequence("Ctrl+Shift+O"), [this] {
        const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("打开源文件"), {}, "C/C++ (*.c *.cpp *.cc *.cxx *.h *.hpp);;所有文件 (*)");
        if (!path.isEmpty()) openFile(path);
    }, true);
    bar->addAction(action(file, QStringLiteral("保存"), QKeySequence::Save, [this] { if (editor()) saveEditor(editor()); }, true));
    action(file, QStringLiteral("全部保存"), QKeySequence("Ctrl+Shift+S"), [this] { saveAll(); }, true);
    action(file, QStringLiteral("关闭标签"), QKeySequence::Close, [this] { closeTab(m_tabs->currentIndex()); }, true);
    file->addSeparator(); action(file, QStringLiteral("退出"), QKeySequence::Quit, [this] { close(); });
    action(edit, QStringLiteral("撤销"), QKeySequence::Undo, [this] { if (editor()) editor()->undo(); });
    action(edit, QStringLiteral("重做"), QKeySequence::Redo, [this] { if (editor()) editor()->redo(); });
    action(edit, QStringLiteral("查找…"), QKeySequence::Find, [this] { find(); });
    action(project, QStringLiteral("添加已有文件…"), {}, [this] { addFile(); }, true);
    action(project, QStringLiteral("从工程移除选中文件"), {}, [this] { removeFile(); }, true);
    action(project, QStringLiteral("保存工程"), {}, [this] {
        QString error; if (!m_project.save(&error)) showError(error);
    }, true);
    bar->addSeparator();
    m_targets = new QComboBox(bar); m_targets->setMinimumWidth(130); bar->addWidget(m_targets);
    bar->addAction(action(buildMenu, QStringLiteral("构建"), QKeySequence("F7"), [this] { build(false); }, true));
    bar->addAction(action(buildMenu, QStringLiteral("构建并运行"), QKeySequence("F5"), [this] { build(true); }, true));
    bar->addAction(action(buildMenu, QStringLiteral("C 检查 + picoc 运行"), QKeySequence("F6"), [this] { runPicoc(); }, true));
    m_stop = action(buildMenu, QStringLiteral("停止"), QKeySequence("Shift+F5"), [this] { m_runner.stop(); });
    bar->addAction(m_stop);
    action(tools, QStringLiteral("工具链与执行设置…"), {}, [this] { settings(); }, true);
    action(help, QStringLiteral("关于"), {}, [this] {
        QMessageBox::about(this, QStringLiteral("关于 ohos_codeblocks"), QStringLiteral(
            "ohos_codeblocks 0.1\n\nQt Widgets / C++17 独立原型。\n兼容 Code::Blocks .cbp 工程的受限子集。\n"
            "未复制 Code::Blocks 源码；不是官方移植版。\n鸿蒙部署与执行沙箱需要目标平台接入。"));
    });
}

CodeEditor *MainWindow::editor() const { return qobject_cast<CodeEditor *>(m_tabs->currentWidget()); }
void MainWindow::showError(const QString &message) { QMessageBox::warning(this, QStringLiteral("操作未完成"), message); }
void MainWindow::appendOutput(const QString &text) {
    auto cursor = m_log->textCursor(); cursor.movePosition(QTextCursor::End); cursor.insertText(text);
    m_log->setTextCursor(cursor); m_log->ensureCursorVisible();
}
bool MainWindow::openProject(const QString &fileName) {
    if (m_runner.isRunning()) return false;
    Project candidate; QString error;
    if (!candidate.load(fileName, &error)) { showError(error); return false; }
    if (!confirmCloseEditors()) return false;
    while (m_tabs->count()) { auto widget = m_tabs->widget(0); m_tabs->removeTab(0); delete widget; }
    m_project = candidate; refreshProject();
    if (!m_project.units().isEmpty()) openFile(m_project.absolutePath(m_project.units().first().path));
    return true;
}
void MainWindow::refreshProject() {
    const auto selected = m_targets->currentText();
    m_tree->clear(); m_targets->clear();
    if (!m_project.isOpen()) return;
    auto root = new QTreeWidgetItem(m_tree, {m_project.title()});
    root->setToolTip(0, m_project.fileName());
    for (const auto &unit : m_project.units()) {
        auto item = new QTreeWidgetItem(root, {unit.path});
        const auto path = m_project.absolutePath(unit.path);
        item->setData(0, Qt::UserRole, path); item->setToolTip(0, path);
        if (!QFileInfo::exists(path)) item->setForeground(0, QColor("#e58d8d"));
    }
    root->setExpanded(true);
    for (const auto &target : m_project.targets()) m_targets->addItem(target.name);
    if (m_targets->findText(selected) >= 0) m_targets->setCurrentText(selected);
    setWindowTitle(m_project.title() + QStringLiteral(" — ohos_codeblocks"));
}
void MainWindow::openFile(const QString &fileName) {
    const auto absolute = QFileInfo(fileName).absoluteFilePath();
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto e = qobject_cast<CodeEditor *>(m_tabs->widget(i));
        if (e->filePath == absolute) { m_tabs->setCurrentIndex(i); return; }
    }
    QFile file(absolute);
    if (!file.open(QIODevice::ReadOnly)) { showError(file.errorString()); return; }
    if (file.size() > 4 * 1024 * 1024) { showError(QStringLiteral("编辑器仅打开 4 MiB 以内的文本文件。")); return; }
    const auto bytes = file.readAll();
    QStringDecoder decoder(QStringDecoder::Utf8); const QString content = decoder(bytes);
    if (decoder.hasError() || bytes.contains('\0')) { showError(QStringLiteral("仅支持 UTF-8 文本，请先转换编码。")); return; }
    auto e = new CodeEditor(m_tabs); e->filePath = absolute; e->setPlainText(content);
    e->document()->setModified(false); e->setReadOnly(m_runner.isRunning());
    m_tabs->addTab(e, QFileInfo(absolute).fileName()); m_tabs->setCurrentWidget(e);
    connect(e->document(), &QTextDocument::modificationChanged, this, [this, e] { updateTab(e); });
    connect(e, &QPlainTextEdit::cursorPositionChanged, this, [this, e] {
        m_cursor->setText(QStringLiteral("行 %1  列 %2  |  UTF-8").arg(e->textCursor().blockNumber() + 1).arg(e->textCursor().positionInBlock() + 1));
    });
    updateTab(e);
}
void MainWindow::updateTab(CodeEditor *e) {
    const int index = m_tabs->indexOf(e);
    if (index >= 0) { m_tabs->setTabText(index, QFileInfo(e->filePath).fileName() + (e->document()->isModified() ? " *" : "")); m_tabs->setTabToolTip(index, e->filePath); }
}
bool MainWindow::saveEditor(CodeEditor *e) {
    if (!e || !e->document()->isModified()) return true;
    QSaveFile file(e->filePath); const auto bytes = e->toPlainText().toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) { showError(file.errorString()); return false; }
    e->document()->setModified(false); return true;
}
bool MainWindow::saveAll() {
    for (int i = 0; i < m_tabs->count(); ++i) if (!saveEditor(qobject_cast<CodeEditor *>(m_tabs->widget(i)))) return false;
    return true;
}
bool MainWindow::confirmCloseEditors() {
    bool dirty = false;
    for (int i = 0; i < m_tabs->count(); ++i) dirty |= qobject_cast<CodeEditor *>(m_tabs->widget(i))->document()->isModified();
    if (!dirty) return true;
    const auto result = QMessageBox::question(this, QStringLiteral("未保存的修改"), QStringLiteral("保存所有已修改文件？"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    return result == QMessageBox::Discard || (result == QMessageBox::Save && saveAll());
}
bool MainWindow::closeTab(int index) {
    if (index < 0 || m_runner.isRunning()) return false;
    auto e = qobject_cast<CodeEditor *>(m_tabs->widget(index));
    if (e->document()->isModified()) {
        const auto choice = QMessageBox::question(this, QStringLiteral("保存文件"), QFileInfo(e->filePath).fileName() + QStringLiteral(" 有未保存修改。是否保存？"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (choice == QMessageBox::Cancel || (choice == QMessageBox::Save && !saveEditor(e))) return false;
    }
    m_tabs->removeTab(index); delete e; return true;
}
void MainWindow::newProject() {
    if (!confirmCloseEditors()) return;
    const auto parent = QFileDialog::getExistingDirectory(this, QStringLiteral("选择新工程的父目录"));
    if (parent.isEmpty()) return;
    bool ok = false;
    const auto name = QInputDialog::getText(this, QStringLiteral("新建 C 控制台工程"), QStringLiteral("工程名（英文字母、数字、下划线、连字符）"), QLineEdit::Normal, "hello_c", &ok);
    if (!ok || name.isEmpty()) return;
    Project candidate; QString error;
    if (!candidate.create(parent, name, &error)) { showError(error); return; }
    // The close decision above also covers discarded buffers: do not prompt a second time.
    while (m_tabs->count()) { auto widget = m_tabs->widget(0); m_tabs->removeTab(0); delete widget; }
    m_project = candidate; refreshProject(); openFile(m_project.absolutePath("main.c"));
}
void MainWindow::addFile() {
    if (!m_project.isOpen()) { showError(QStringLiteral("请先打开工程。")); return; }
    const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("添加已有文件"), m_project.directory());
    if (path.isEmpty()) return;
    QString error; if (!m_project.addFile(path, &error)) { showError(error); return; }
    refreshProject(); openFile(path);
}
void MainWindow::removeFile() {
    const auto item = m_tree->currentItem(); if (!item) return;
    const auto path = item->data(0, Qt::UserRole).toString(); if (path.isEmpty()) return;
    QString error; if (!m_project.removeFile(path, &error)) { showError(error); return; }
    refreshProject(); statusBar()->showMessage(QStringLiteral("已移除工程引用，磁盘文件仍保留。"), 4000);
}
void MainWindow::find() {
    auto e = editor(); if (!e) return;
    bool ok = false;
    const auto text = QInputDialog::getText(this, QStringLiteral("查找"), QStringLiteral("查找文本"), QLineEdit::Normal, m_findText, &ok);
    if (!ok || text.isEmpty()) return;
    m_findText = text;
    if (!e->find(text)) { auto cursor = e->textCursor(); cursor.movePosition(QTextCursor::Start); e->setTextCursor(cursor); if (!e->find(text)) statusBar()->showMessage(QStringLiteral("未找到匹配文本。"), 3000); }
}

void MainWindow::settings() {
    QSettings config; QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("工具链与执行设置")); dialog.resize(700, 400);
    auto form = new QFormLayout(&dialog);
    auto field = [&](const QString &label, const QString &key, const QString &fallback) {
        auto entry = new QLineEdit(config.value(key, fallback).toString(), &dialog); form->addRow(label, entry); return entry;
    };
    auto cc = field(QStringLiteral("C 编译器（GCC / Clang）"), "tools/cc", "gcc");
    auto cxx = field(QStringLiteral("C++ 编译器（G++ / Clang++）"), "tools/cxx", "g++");
    auto picoc = field(QStringLiteral("picoc 可执行文件"), "tools/picoc", "picoc");
    auto mode = new QComboBox(&dialog);
    mode->addItems({QStringLiteral("Linux bubblewrap 沙箱（默认）"), QStringLiteral("本机执行：仅可信代码，无安全隔离"), QStringLiteral("鸿蒙沙箱服务（待接入，禁止执行）")});
    mode->setCurrentIndex(qBound(0, config.value("tools/picocMode", 0).toInt(), 2)); form->addRow(QStringLiteral("picoc 执行方式"), mode);
    auto note = new QLabel(QStringLiteral("填写可执行文件路径，不附带参数，不加引号。\n原生编译、语法检查与原生程序运行使用本机工具链。\nF6 将当前 C 文件保存后暂存为 main.c：先做 C99 语法检查，再交给 picoc。\npicoc 模式仅支持单个 C 源文件与其内置头文件。\n运行时限 10 秒，输出上限 1 MiB；原生构建时限 120 秒。"), &dialog);
    note->setWordWrap(true); form->addRow(note);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog); form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    config.setValue("tools/cc", cc->text().trimmed()); config.setValue("tools/cxx", cxx->text().trimmed());
    config.setValue("tools/picoc", picoc->text().trimmed()); config.setValue("tools/picocMode", mode->currentIndex());
}
void MainWindow::setBusy(bool busy) {
    for (auto a : m_idleActions) a->setEnabled(!busy);
    m_stop->setEnabled(busy); m_targets->setEnabled(!busy);
    m_input->setEnabled(busy && (m_job == Job::Native || m_job == Job::Picoc));
    for (int i = 0; i < m_tabs->count(); ++i) qobject_cast<CodeEditor *>(m_tabs->widget(i))->setReadOnly(busy);
    m_mode->setText(busy ? QStringLiteral("执行中…") : QStringLiteral("就绪"));
}
bool MainWindow::startJob(Job job, const RunCommand &command, int timeoutMs) {
    if (!command.error.isEmpty()) { showError(command.error); return false; }
    if (command.program.isEmpty()) { showError(QStringLiteral("工具路径为空，请先配置工具链。")); return false; }
    m_job = job; setBusy(true);
    appendOutput("\n> " + command.program + '\n');
    for (const auto &arg : command.arguments) appendOutput("    " + arg + '\n');
    if (!m_runner.start(command, timeoutMs)) { m_job = Job::None; setBusy(false); return false; }
    return true;
}
void MainWindow::build(bool runAfter) {
    if (m_runner.isRunning() || !saveAll()) return;
    QSettings config;
    const auto plan = m_project.buildPlan(m_targets->currentIndex(), config.value("tools/cc", "gcc").toString(), config.value("tools/cxx", "g++").toString());
    if (!plan.error.isEmpty()) { showError(plan.error); return; }
    if (!QDir().mkpath(QFileInfo(plan.output).absolutePath())) { showError(QStringLiteral("无法创建构建输出目录。")); return; }
    m_nativeOutput = plan.output;
    const auto target = m_project.targets().at(m_targets->currentIndex());
    m_nativeDirectory = target.workingDirectory.isEmpty() ? m_project.directory() : m_project.absolutePath(target.workingDirectory);
    m_runAfterBuild = runAfter;
    startJob(Job::Build, {plan.program, plan.workingDirectory, plan.arguments, {}}, 120000);
}
void MainWindow::runPicoc() {
    if (m_runner.isRunning()) return;
    auto e = editor();
    if (!e || QFileInfo(e->filePath).suffix() != "c") { showError(QStringLiteral("请打开并选中一个 .c 文件。picoc 不支持 C++。")); return; }
    if (!saveAll()) return;
    m_stage = std::make_unique<QTemporaryDir>();
    if (!m_stage->isValid()) { showError(QStringLiteral("无法创建运行暂存目录。")); m_stage.reset(); return; }
    QFile staged(m_stage->filePath("main.c")); const auto bytes = e->toPlainText().toUtf8();
    if (!staged.open(QIODevice::WriteOnly) || staged.write(bytes) != bytes.size()) { showError(staged.errorString()); staged.close(); m_stage.reset(); return; }
    staged.close();
    QSettings config;
    m_picocCommand = picocCommand(static_cast<PicocMode>(qBound(0, config.value("tools/picocMode", 0).toInt(), 2)), config.value("tools/picoc", "picoc").toString(), m_stage->path());
    if (!m_picocCommand.error.isEmpty()) { showError(m_picocCommand.error); m_stage.reset(); return; }
    appendOutput(QStringLiteral("\n[C 检查] GCC/Clang -std=c99 -fsyntax-only；检查成功不代表 picoc 支持全部语法。\n"));
    RunCommand check{config.value("tools/cc", "gcc").toString(), m_stage->path(), {"-std=c99", "-Wall", "-Wextra", "-fsyntax-only", m_stage->filePath("main.c")}, {}};
    if (!startJob(Job::Check, check, 10000)) m_stage.reset();
}
void MainWindow::finishJob(int code, bool success, const QString &reason) {
    Q_UNUSED(code);
    appendOutput(QStringLiteral("\n[%1] %2\n").arg(success ? QStringLiteral("完成") : QStringLiteral("失败"), reason));
    const auto previous = m_job; m_job = Job::None;
    if (previous == Job::Build && success && m_runAfterBuild) {
        m_runAfterBuild = false;
        appendOutput(QStringLiteral("[本机运行] 标准输出通过管道回显；程序可调用 fflush(stdout) 即时刷新提示。\n"));
        if (startJob(Job::Native, {m_nativeOutput, m_nativeDirectory, {}, {}}, 10000)) return;
    }
    if (previous == Job::Check && success) {
        appendOutput(QStringLiteral("[picoc] 开始解释执行当前 C 文件。\n"));
        if (startJob(Job::Picoc, m_picocCommand, 10000)) return;
    }
    m_runAfterBuild = false; m_stage.reset(); setBusy(false);
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_runner.isRunning()) { showError(QStringLiteral("请先停止当前任务，再关闭窗口。")); event->ignore(); return; }
    if (!confirmCloseEditors()) { event->ignore(); return; }
    QSettings config; config.setValue("window/geometry", saveGeometry()); event->accept();
}
