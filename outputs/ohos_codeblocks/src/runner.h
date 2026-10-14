#pragma once
#include <QObject>
#include <QProcess>
#include <QStringConverter>
#include <QTimer>

struct RunCommand {
    QString program, workingDirectory;
    QStringList arguments;
    QString error;
};

enum class PicocMode { LinuxSandbox, TrustedHost, OhosSandbox };
RunCommand picocCommand(PicocMode mode, const QString &picoc, const QString &stagedDirectory);

class ProcessRunner : public QObject {
    Q_OBJECT
public:
    explicit ProcessRunner(QObject *parent = nullptr);
    bool start(const RunCommand &command, int timeoutMs = 10000, qint64 outputLimit = 1024 * 1024);
    bool isRunning() const { return m_active; }
    void sendInput(const QString &text);
    void closeInput();
    void stop();
signals:
    void output(const QString &text);
    void completed(int exitCode, bool success, const QString &reason);
private:
    void drain();
    void finish(int exitCode, bool success, const QString &reason);
    QProcess m_process;
    QTimer m_timer;
    QStringDecoder m_decoder{QStringDecoder::Utf8};
    qint64 m_received = 0, m_limit = 1024 * 1024;
    bool m_active = false;
    QString m_reason;
};
