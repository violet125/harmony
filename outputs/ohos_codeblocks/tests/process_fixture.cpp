#include <QCoreApplication>
#include <QFile>
#include <QThread>
#include <QTimer>
#include <cstdio>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto mode = app.arguments().value(1);
    if (mode == "hang") { QTimer::singleShot(30000, &app, &QCoreApplication::quit); return app.exec(); }
    if (mode == "flood") { const QByteArray bytes(65536, 'x'); for (int i = 0; i < 64; ++i) std::fwrite(bytes.data(), 1, bytes.size(), stdout); return 0; }
    if (mode == "fail") { std::fprintf(stderr, "intentional failure\n"); return 7; }
    if (mode == "echo") { char buffer[256]; if (std::fgets(buffer, sizeof(buffer), stdin)) std::fputs(buffer, stdout); return 0; }
    if (mode == "unicode") {
        const auto bytes = QStringLiteral("鸿蒙 C 运行\n").toUtf8();
        for (char byte : bytes) { std::fputc(static_cast<unsigned char>(byte), stdout); std::fflush(stdout); QThread::msleep(3); }
        return 0;
    }
    std::puts("fixture ok"); return 0;
}
