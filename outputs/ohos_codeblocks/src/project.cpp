#include "project.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

namespace {
QString portable(QString path) { return path.replace('\\', '/'); }
bool fail(QString *error, const QString &message) {
    if (error) *error = message;
    return false;
}
void readFlags(const QDomElement &parent, BuildTarget &target) {
    const auto compiler = parent.firstChildElement("Compiler");
    for (auto e = compiler.firstChildElement("Add"); !e.isNull(); e = e.nextSiblingElement("Add")) {
        if (e.hasAttribute("option")) target.options << e.attribute("option");
        if (e.hasAttribute("directory")) target.includeDirs << portable(e.attribute("directory"));
    }
    const auto linker = parent.firstChildElement("Linker");
    for (auto e = linker.firstChildElement("Add"); !e.isNull(); e = e.nextSiblingElement("Add")) {
        if (e.hasAttribute("option")) target.linkOptions << e.attribute("option");
        if (e.hasAttribute("directory")) target.libDirs << portable(e.attribute("directory"));
        if (e.hasAttribute("library")) target.libraries << e.attribute("library");
    }
}
}

QString Project::directory() const { return QFileInfo(m_fileName).absolutePath(); }
QString Project::absolutePath(const QString &path) const {
    return QDir::cleanPath(QDir(directory()).absoluteFilePath(portable(path)));
}
QDomElement Project::projectElement() const {
    return m_document.documentElement().firstChildElement("Project");
}

bool Project::load(const QString &fileName, QString *error) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, file.errorString());
    if (file.size() > 8 * 1024 * 1024) return fail(error, QStringLiteral("工程文件超过 8 MiB 限制。"));
    const QByteArray bytes = file.readAll();
    // No DTD / entities: project files have no reason to define them.
    if (bytes.toUpper().contains("<!DOCTYPE") || bytes.toUpper().contains("<!ENTITY"))
        return fail(error, QStringLiteral("不支持包含 DTD 或实体声明的工程。"));
    QDomDocument document;
    QString xmlError;
    int line = 0, column = 0;
    if (!document.setContent(bytes, &xmlError, &line, &column))
        return fail(error, QStringLiteral("XML %1:%2 — %3").arg(line).arg(column).arg(xmlError));
    if (document.documentElement().tagName() != "CodeBlocks_project_file" ||
        document.documentElement().firstChildElement("Project").isNull())
        return fail(error, QStringLiteral("文件不是 Code::Blocks .cbp 工程。"));
    // Commit only after validation, preserving the currently open project on failure.
    m_document = document;
    m_fileName = QFileInfo(fileName).absoluteFilePath();
    parse();
    return true;
}

void Project::parse() {
    m_units.clear();
    m_targets.clear();
    const auto p = projectElement();
    m_title = QFileInfo(m_fileName).completeBaseName();
    for (auto e = p.firstChildElement("Option"); !e.isNull(); e = e.nextSiblingElement("Option"))
        if (e.hasAttribute("title")) m_title = e.attribute("title");
    const auto build = p.firstChildElement("Build");
    for (auto e = build.firstChildElement("Target"); !e.isNull(); e = e.nextSiblingElement("Target")) {
        BuildTarget target;
        target.name = e.attribute("title");
        readFlags(p, target);
        for (auto o = e.firstChildElement("Option"); !o.isNull(); o = o.nextSiblingElement("Option")) {
            if (o.hasAttribute("output")) target.output = portable(o.attribute("output"));
            if (o.hasAttribute("working_dir")) target.workingDirectory = portable(o.attribute("working_dir"));
            if (o.hasAttribute("type")) target.type = o.attribute("type").toInt();
        }
        readFlags(e, target);
        m_targets << target;
    }
    for (auto e = p.firstChildElement("Unit"); !e.isNull(); e = e.nextSiblingElement("Unit")) {
        ProjectUnit unit;
        unit.path = portable(e.attribute("filename"));
        for (auto o = e.firstChildElement("Option"); !o.isNull(); o = o.nextSiblingElement("Option")) {
            if (o.hasAttribute("target")) unit.targets << o.attribute("target");
            if (o.attribute("compile") == "0") unit.compile = false;
        }
        m_units << unit;
    }
}

bool Project::save(QString *error) {
    if (!isOpen()) return fail(error, QStringLiteral("没有打开的工程。"));
    QSaveFile file(m_fileName);
    if (!file.open(QIODevice::WriteOnly)) return fail(error, file.errorString());
    const auto bytes = m_document.toByteArray(2);
    if (file.write(bytes) != bytes.size() || !file.commit()) return fail(error, file.errorString());
    return true;
}

bool Project::create(const QString &directory, const QString &name, QString *error) {
    if (!QRegularExpression("^[A-Za-z_][A-Za-z0-9_-]{0,63}$").match(name).hasMatch())
        return fail(error, QStringLiteral("工程名须以英文字母或下划线开头，只含字母、数字、下划线或连字符，最多 64 字符。"));
    QDir dir(directory);
    if (dir.exists(name)) return fail(error, QStringLiteral("同名目录已存在，请换一个工程名。"));
    if (!dir.mkpath(name)) return fail(error, QStringLiteral("无法创建工程目录。"));
    dir.cd(name);
    QDomDocument doc;
    auto root = doc.createElement("CodeBlocks_project_file"); doc.appendChild(root);
    auto version = doc.createElement("FileVersion");
    version.setAttribute("major", 1); version.setAttribute("minor", 6); root.appendChild(version);
    auto p = doc.createElement("Project"); root.appendChild(p);
    auto title = doc.createElement("Option"); title.setAttribute("title", name); p.appendChild(title);
    auto build = doc.createElement("Build"); p.appendChild(build);
    for (const auto &targetName : {QStringLiteral("Debug"), QStringLiteral("Release")}) {
        auto target = doc.createElement("Target"); target.setAttribute("title", targetName); build.appendChild(target);
        auto output = doc.createElement("Option");
        output.setAttribute("output", "bin/" + targetName + "/" + name);
        output.setAttribute("prefix_auto", 1); output.setAttribute("extension_auto", 1); target.appendChild(output);
        auto type = doc.createElement("Option"); type.setAttribute("type", 1); target.appendChild(type);
        auto compiler = doc.createElement("Compiler"); target.appendChild(compiler);
        for (const auto &flag : QStringList{"-Wall", "-Wextra", targetName == "Debug" ? "-g" : "-O2"}) {
            auto add = doc.createElement("Add"); add.setAttribute("option", flag); compiler.appendChild(add);
        }
    }
    auto unit = doc.createElement("Unit"); unit.setAttribute("filename", "main.c"); p.appendChild(unit);
    QSaveFile source(dir.filePath("main.c"));
    const QByteArray initial = "#include <stdio.h>\n\nint main(void)\n{\n    printf(\"Hello from ohos_codeblocks!\\n\");\n    return 0;\n}\n";
    if (!source.open(QIODevice::WriteOnly) || source.write(initial) != initial.size() || !source.commit())
        return fail(error, source.errorString());
    Project candidate;
    candidate.m_document = doc;
    candidate.m_fileName = dir.filePath(name + ".cbp");
    candidate.parse();
    if (!candidate.save(error)) return false;
    *this = candidate;
    return true;
}

bool Project::addFile(const QString &fileName, QString *error) {
    if (!isOpen()) return fail(error, QStringLiteral("请先打开工程。"));
    if (!QFileInfo(fileName).isFile()) return fail(error, QStringLiteral("源文件不存在。"));
    const auto relative = QDir(directory()).relativeFilePath(QFileInfo(fileName).absoluteFilePath());
    for (const auto &unit : m_units) if (absolutePath(unit.path) == absolutePath(relative)) return true;
    const auto previous = m_document.cloneNode(true).toDocument();
    auto e = m_document.createElement("Unit"); e.setAttribute("filename", relative);
    projectElement().appendChild(e);
    if (!save(error)) { m_document = previous; return false; }
    parse();
    return true;
}

bool Project::removeFile(const QString &fileName, QString *error) {
    const auto previous = m_document.cloneNode(true).toDocument();
    auto p = projectElement();
    for (auto e = p.firstChildElement("Unit"); !e.isNull();) {
        auto next = e.nextSiblingElement("Unit");
        if (absolutePath(e.attribute("filename")) == QFileInfo(fileName).absoluteFilePath()) p.removeChild(e);
        e = next;
    }
    if (!save(error)) { m_document = previous; return false; }
    parse();
    return true;
}

BuildPlan Project::buildPlan(int index, const QString &cc, const QString &cxx) const {
    BuildPlan plan;
    auto reject = [&plan](const QString &message) { plan.error = message; return plan; };
    if (index < 0 || index >= m_targets.size()) return reject(QStringLiteral("请选择有效构建目标。"));
    const auto target = m_targets.at(index);
    if (target.type != 1) return reject(QStringLiteral("原型仅构建控制台可执行目标（Code::Blocks type=1）。"));
    const auto p = projectElement();
    if (!p.elementsByTagName("ExtraCommands").isEmpty() || !p.elementsByTagName("MakeCommands").isEmpty())
        return reject(QStringLiteral("此工程含自定义命令；为避免错误构建，原型拒绝执行，请在 Code::Blocks 中构建。"));
    for (auto o = p.firstChildElement("Option"); !o.isNull(); o = o.nextSiblingElement("Option"))
        if (o.attribute("makefile_is_custom") == "1") return reject(QStringLiteral("暂不支持自定义 Makefile。"));
    QStringList sources;
    bool cpp = false;
    for (const auto &unit : m_units) {
        if (!unit.compile || (!unit.targets.isEmpty() && !unit.targets.contains(target.name))) continue;
        const auto suffix = QFileInfo(unit.path).suffix();
        if (QStringList{"h", "hpp", "hh", "hxx", "md", "txt"}.contains(suffix.toLower())) continue;
        if (!QStringList{"c", "C", "cpp", "cc", "cxx"}.contains(suffix))
            return reject(QStringLiteral("不支持编译此文件类型：%1").arg(unit.path));
        if (!QFileInfo::exists(absolutePath(unit.path))) return reject(QStringLiteral("文件不存在：%1").arg(unit.path));
        sources << absolutePath(unit.path);
        cpp = cpp || suffix != "c";
    }
    if (sources.isEmpty()) return reject(QStringLiteral("目标没有可编译的 C/C++ 文件。"));
    bool hasC = false, hasCpp = false;
    for (const auto &source : sources) {
        if (QFileInfo(source).suffix() == "c") hasC = true; else hasCpp = true;
    }
    if (hasC && hasCpp) return reject(QStringLiteral("原型暂不支持混合 C/C++ 目标，请拆分目标或使用 Code::Blocks 构建。"));
    plan.program = cpp ? cxx : cc;
    if (plan.program.isEmpty()) return reject(QStringLiteral("请在工具链设置中指定 GCC/Clang 编译器。"));
    plan.workingDirectory = directory();
    if (target.output.isEmpty()) return reject(QStringLiteral("目标缺少 output 配置。"));
    plan.output = absolutePath(target.output);
#ifdef Q_OS_WIN
    if (!plan.output.endsWith(".exe", Qt::CaseInsensitive)) plan.output += ".exe";
#endif
    plan.arguments = target.options;
    for (const auto &include : target.includeDirs) plan.arguments << "-I" << absolutePath(include);
    plan.arguments << sources << "-o" << plan.output;
    plan.arguments << target.linkOptions;
    for (const auto &libDir : target.libDirs) plan.arguments << "-L" << absolutePath(libDir);
    for (const auto &lib : target.libraries)
        plan.arguments << ((lib.contains('/') || lib.contains('\\') || lib.endsWith(".a") || lib.endsWith(".lib")) ? absolutePath(lib) : "-l" + lib);
    const QString all = plan.arguments.join('\n');
    if (all.contains("$(") || all.contains("${") || all.contains('%'))
        return reject(QStringLiteral("构建参数含未支持的 Code::Blocks 宏；请使用明确的相对路径或绝对路径。"));
    return plan;
}
