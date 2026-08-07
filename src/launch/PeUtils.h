#pragma once

#include <QString>
#include <QVector>

#include <cstdint>

/// Managed PE reader: architecture detection and import-table inspection.
/// Pure file parsing, so it compiles anywhere; only used on Windows at runtime.
namespace PeUtils
{
enum class Arch
{
    Unknown,
    X86,
    X64
};

Arch detectArch(const QString &exePath);

/// True when the image imports the given module by name.
bool importsModule(const QString &path, const QString &moduleName);

/// RVAs of the import-descriptor Name fields for the given module (used by the
/// static-import rebinder to point steam_api64.dll at the injected payload).
QVector<quint32> findImportNameFieldRvas(const QString &path, const QString &moduleName);
}
