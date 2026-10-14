#include "runner.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

RunCommand picocCommand(PicocMode mode, const QString &picoc, const QString &stagedDirectory) {
    RunCommand command;
    command.workingDirectory = stagedDirectory;
    QString executable = picoc;
    if (!QFileInfo(executable).isAbsolute()) executable = QStandardPaths::findExecutable(executable);
    if (executable.isEmpty() || !QFileInfo(executable).isExecutable()) {
        command.error = QStringLiteral("找不到 picoc 可执行文件，请先安装并配置路径。");
        return command;
    }
    if (mode == PicocMode::OhosSandbox) {
        command.error = QStringLiteral("鸿蒙沙箱桥接尚未实现。需要设备侧受限执行服务与 Qt 平台适配后才能启用；不会回退到本机执行。");
        return command;
    }
    if (mode == PicocMode::TrustedHost) {
        command.program = executable;
        command.arguments << QDir(stagedDirectory).filePath("main.c");
        return command;
    }
#if defined(Q_OS_LINUX) && !defined(OHOS_CODEBLOCKS_OHOS)
    command.program = QStandardPaths::findExecutable("bwrap");
    if (command.program.isEmpty()) {
        command.error = QStringLiteral("Linux 沙箱需要 bubblewrap（bwrap）；未安装时禁止执行。");
        return command;
    }
    command.arguments = {"--unshare-all", "--die-with-parent", "--new-session", "--cap-drop", "ALL",
                         "--clearenv", "--setenv", "PATH", "/usr/bin:/bin", "--setenv", "HOME", "/tmp",
                         "--setenv", "LANG", "C.UTF-8"};
    for (const auto &path : {"/usr", "/bin", "/lib", "/lib64"})
        if (QFileInfo::exists(path)) command.arguments << "--ro-bind" << path << path;
    command.arguments << "--dir" << "/etc";
    if (QFileInfo::exists("/etc/ld.so.cache"))
        command.arguments << "--ro-bind" << "/etc/ld.so.cache" << "/etc/ld.so.cache";
    command.arguments << "--proc" << "/proc" << "--dev" << "/dev" << "--tmpfs" << "/tmp"
                      << "--dir" << "/workspace" << "--ro-bind" << stagedDirectory << "/workspace"
                      << "--dir" << "/runner" << "--ro-bind" << executable << "/runner/picoc"
                      << "--chdir" << "/workspace" << "--" << "/runner/picoc" << "/workspace/main.c";
#else
    command.error = QStringLiteral("此平台不提供 Linux bubblewrap 沙箱。可接入鸿蒙执行服务，或在设置中明确启用仅供可信代码使用的本机模式。");
#endif
    return command;
}

ProcessRunner::ProcessRunner(QObject *parent) : QObject(parent) {
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    m_timer.setSingleShot(true);
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &ProcessRunner::drain);
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
        drain();
        const bool success = m_reason.isEmpty() && code == 0 && status == QProcess::NormalExit;
        const auto reason = m_reason.isEmpty()
                ? (status == QProcess::CrashExit ? QStringLiteral("进程异常终止") : QStringLiteral("退出码 %1").arg(code))
                : m_reason;
        finish(code, success, reason);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) finish(-1, false, m_process.errorString());
    });
    connect(&m_timer, &QTimer::timeout, this, [this] {
        m_reason = QStringLiteral("超过执行时间限制，已终止");
        m_process.kill();
    });
}

bool ProcessRunner::start(const RunCommand &command, int timeoutMs, qint64 outputLimit) {
    if (m_active || !command.error.isEmpty() || command.program.isEmpty()) return false;
    m_active = true;
    m_reason.clear();
    m_received = 0;
    m_limit = qMax<qint64>(1, outputLimit);
    m_decoder = QStringDecoder(QStringDecoder::Utf8);
    m_process.setWorkingDirectory(command.workingDirectory);
    m_process.setProgram(command.program);
    m_process.setArguments(command.arguments);
    m_timer.start(qMax(1, timeoutMs));
    m_process.start();
    return true;
}

void ProcessRunner::drain() {
    const auto bytes = m_process.readAllStandardOutput();
    const auto available = qMax<qint64>(0, m_limit - m_received);
    const auto accepted = bytes.left(available);
    m_received += bytes.size();
    if (!accepted.isEmpty()) emit output(m_decoder(accepted));
    if (m_received > m_limit && m_reason.isEmpty()) {
        m_reason = QStringLiteral("输出超过限制，已终止");
        m_process.kill();
    }
}

void ProcessRunner::finish(int code, bool success, const QString &reason) {
    if (!m_active) return;
    m_timer.stop();
    m_active = false;
    emit completed(code, success, reason);
}
void ProcessRunner::sendInput(const QString &text) {
    if (m_active && m_process.state() == QProcess::Running && m_process.bytesToWrite() < 65536)
        m_process.write(text.toUtf8());
}
void ProcessRunner::closeInput() { if (m_active) m_process.closeWriteChannel(); }
void ProcessRunner::stop() {
    if (!m_active) return;
    m_reason = QStringLiteral("用户停止执行");
    m_process.kill();
}
