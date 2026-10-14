#pragma once
#include <QDomDocument>
#include <QStringList>
#include <QVector>

struct BuildTarget {
    QString name, output, workingDirectory;
    int type = 1;
    QStringList options, includeDirs, linkOptions, libDirs, libraries;
};

struct ProjectUnit {
    QString path;
    QStringList targets;
    bool compile = true;
};

struct BuildPlan {
    QString program, workingDirectory, output;
    QStringList arguments;
    QString error;
};

class Project {
public:
    bool load(const QString &fileName, QString *error);
    bool create(const QString &directory, const QString &name, QString *error);
    bool save(QString *error);
    bool addFile(const QString &fileName, QString *error);
    bool removeFile(const QString &fileName, QString *error);
    bool isOpen() const { return !m_fileName.isEmpty(); }
    QString fileName() const { return m_fileName; }
    QString directory() const;
    QString title() const { return m_title; }
    QVector<ProjectUnit> units() const { return m_units; }
    QVector<BuildTarget> targets() const { return m_targets; }
    QString absolutePath(const QString &path) const;
    BuildPlan buildPlan(int target, const QString &cc, const QString &cxx) const;
private:
    void parse();
    QDomElement projectElement() const;
    QString m_fileName, m_title;
    QDomDocument m_document;
    QVector<ProjectUnit> m_units;
    QVector<BuildTarget> m_targets;
};
