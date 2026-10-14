#include "project.h"
#include "runner.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

class CoreTests : public QObject {
    Q_OBJECT
private:
    static void write(const QString &path, const QByteArray &data) {
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(data), data.size());
    }
    static QByteArray read(const QString &path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
    static QString output(const QSignalSpy &spy) { QString text; for (const auto &row : spy) text += row.at(0).toString(); return text; }
    static QString fixturePath() {
        auto path = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("process_fixture"));
#ifdef Q_OS_WIN
        if (!QFileInfo::exists(path)) path += QStringLiteral(".exe");
#endif
        return path;
    }
    static RunCommand fixture(const QString &mode) { return {fixturePath(), QDir::tempPath(), {mode}, {}}; }
private slots:
    void createRoundTrip() {
        QTemporaryDir temp; QVERIFY(temp.isValid()); Project project; QString error;
        QVERIFY2(project.create(temp.path(), "hello_c", &error), qPrintable(error));
        QCOMPARE(project.targets().size(), 2); QCOMPARE(project.units().size(), 1);
        Project restored; QVERIFY(restored.load(project.fileName(), &error));
        QCOMPARE(restored.title(), QString("hello_c"));
        const auto plan = restored.buildPlan(0, "gcc", "g++");
        QVERIFY2(plan.error.isEmpty(), qPrintable(plan.error)); QCOMPARE(plan.program, QString("gcc"));
        QVERIFY(plan.arguments.contains("-Wall")); QVERIFY(plan.arguments.contains(restored.absolutePath("main.c")));
        QVERIFY(!project.create(temp.path(), "hello_c", &error));
        QVERIFY(!project.create(temp.path(), "../outside", &error));
    }
    void preserveExtensionsAndNormalizePaths() {
        QTemporaryDir temp; QDir(temp.path()).mkpath("src");
        write(temp.filePath("src/main.c"), "int main(void){return 0;}\n");
        const auto file = temp.filePath("round trip.cbp");
        write(file, R"(<CodeBlocks_project_file><Project><Option title="跨平台"/><Build><Target title="Debug"><Option output="bin\hello" type="1"/><Compiler><Add directory="include path"/></Compiler></Target></Build><Unit filename="src\main.c"/><Extensions><future keep="yes"><data>untouched</data></future></Extensions></Project></CodeBlocks_project_file>)");
        Project project; QString error; QVERIFY(project.load(file, &error));
        QCOMPARE(project.units().first().path, QString("src/main.c"));
        write(temp.filePath("extra.c"), "int extra(void){return 1;}\n");
        QVERIFY(project.addFile(temp.filePath("extra.c"), &error));
        QVERIFY(project.addFile(temp.filePath("extra.c"), &error)); QCOMPARE(project.units().size(), 2);
        QVERIFY(read(file).contains("keep=\"yes\"")); QVERIFY(read(file).contains("untouched"));
        const auto plan = project.buildPlan(0, "gcc", "g++"); QVERIFY(plan.error.isEmpty());
        QVERIFY(plan.arguments.contains(temp.filePath("include path")));
        QVERIFY(project.removeFile(temp.filePath("extra.c"), &error));
        QVERIFY(QFileInfo::exists(temp.filePath("extra.c"))); QCOMPARE(project.units().size(), 1);
    }
    void invalidLoadDoesNotReplaceProject() {
        QTemporaryDir temp; Project project; QString error;
        QVERIFY(project.create(temp.path(), "valid", &error));
        const auto original = project.fileName();
        write(temp.filePath("bad.cbp"), "<not-project/>");
        QVERIFY(!project.load(temp.filePath("bad.cbp"), &error)); QCOMPARE(project.fileName(), original);
        write(temp.filePath("bad.cbp"), "<!DOCTYPE x [<!ENTITY x 'bad'>]><CodeBlocks_project_file><Project/></CodeBlocks_project_file>");
        QVERIFY(!project.load(temp.filePath("bad.cbp"), &error)); QCOMPARE(project.fileName(), original);
    }
    void targetMembershipAndUnsafeCommands() {
        QTemporaryDir temp; const auto file = temp.filePath("test.cbp");
        write(temp.filePath("main.c"), "int main(void){return 0;}");
        write(file, R"(<CodeBlocks_project_file><Project><Build><Target title="Debug"><Option output="bin/test" type="1"/></Target></Build><Unit filename="main.c"/><Unit filename="missing.cpp"><Option target="Release"/></Unit></Project></CodeBlocks_project_file>)");
        Project project; QString error; QVERIFY(project.load(file, &error)); QVERIFY(project.buildPlan(0, "gcc", "g++").error.isEmpty());
        auto xml = read(file); xml.replace("</Project>", "<ExtraCommands><Add before=\"untrusted\"/></ExtraCommands></Project>"); write(file, xml);
        QVERIFY(project.load(file, &error)); QVERIFY(!project.buildPlan(0, "gcc", "g++").error.isEmpty());
        QVERIFY(!project.buildPlan(9, "gcc", "g++").error.isEmpty());
    }
    void unsupportedMacroAndMixedLanguage() {
        QTemporaryDir temp; Project project; QString error;
        QVERIFY(project.create(temp.path(), "sample", &error));
        const auto file = project.fileName(); auto xml = read(file);
        xml.replace("bin/Debug/sample", "$(BAD)/sample"); write(file, xml);
        QVERIFY(project.load(file, &error)); QVERIFY(!project.buildPlan(0, "gcc", "g++").error.isEmpty());
        write(project.absolutePath("extra.cpp"), "int extra(){return 0;}");
        QVERIFY(project.addFile(project.absolutePath("extra.cpp"), &error));
        QVERIFY(!project.buildPlan(1, "gcc", "g++").error.isEmpty());
    }
    void successAndUnicode() {
        ProcessRunner runner; QSignalSpy done(&runner, &ProcessRunner::completed); QSignalSpy log(&runner, &ProcessRunner::output);
        QVERIFY(runner.start(fixture("unicode"), 5000));
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 6000);
        QVERIFY(done.first().at(1).toBool()); QCOMPARE(output(log), QStringLiteral("鸿蒙 C 运行\n")); QVERIFY(!runner.isRunning());
    }
    void failureAndRestart() {
        ProcessRunner runner; QSignalSpy done(&runner, &ProcessRunner::completed);
        QVERIFY(runner.start(fixture("fail"), 5000)); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 6000);
        QCOMPARE(done.first().at(0).toInt(), 7); QVERIFY(!done.first().at(1).toBool());
        QVERIFY(runner.start(fixture("ok"), 5000)); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 2, 6000); QVERIFY(done.last().at(1).toBool());
    }
    void missingExecutable() {
        ProcessRunner runner; QSignalSpy done(&runner, &ProcessRunner::completed);
        QVERIFY(runner.start({QDir::tempPath() + "/nonexistent-ohos-tool-732", {}, {}, {}}, 500));
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 2000); QVERIFY(!done.first().at(1).toBool()); QVERIFY(!runner.isRunning());
    }
    void timeoutAndStop() {
        ProcessRunner runner; QSignalSpy done(&runner, &ProcessRunner::completed);
        QVERIFY(runner.start(fixture("hang"), 200)); QVERIFY(!runner.start(fixture("ok")));
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 3000); QVERIFY(!done.first().at(1).toBool());
        QVERIFY(done.first().at(2).toString().contains(QStringLiteral("时间")));
        QVERIFY(runner.start(fixture("hang"), 5000)); QTimer::singleShot(100, &runner, &ProcessRunner::stop);
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 2, 3000); QVERIFY(done.last().at(2).toString().contains(QStringLiteral("用户")));
    }
    void outputLimit() {
        ProcessRunner runner; QSignalSpy done(&runner, &ProcessRunner::completed); QSignalSpy log(&runner, &ProcessRunner::output);
        QVERIFY(runner.start(fixture("flood"), 5000, 1024)); QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 6000);
        QVERIFY(!done.first().at(1).toBool()); QVERIFY(output(log).size() <= 1024);
        QVERIFY(done.first().at(2).toString().contains(QStringLiteral("输出")));
    }
    void standardInput() {
        ProcessRunner runner; QSignalSpy done(&runner, &ProcessRunner::completed); QSignalSpy log(&runner, &ProcessRunner::output);
        QVERIFY(runner.start(fixture("echo"), 5000));
        QTimer::singleShot(150, &runner, [&runner] { runner.sendInput("hello input\n"); runner.closeInput(); });
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 6000); QVERIFY(done.first().at(1).toBool());
        QVERIFY(output(log).contains("hello input"));
    }
    void sandboxNeverSilentlyFallsBack() {
        QTemporaryDir temp;
        const auto command = picocCommand(PicocMode::OhosSandbox, fixturePath(), temp.path());
        QVERIFY(!command.error.isEmpty()); QVERIFY(command.program.isEmpty());
        const auto trusted = picocCommand(PicocMode::TrustedHost, fixturePath(), temp.path());
        QVERIFY(trusted.error.isEmpty()); QCOMPARE(trusted.arguments, QStringList{temp.filePath("main.c")});
#ifndef Q_OS_LINUX
        const auto linuxMode = picocCommand(PicocMode::LinuxSandbox, fixturePath(), temp.path());
        QVERIFY(!linuxMode.error.isEmpty()); QVERIFY(linuxMode.program.isEmpty());
#endif
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
