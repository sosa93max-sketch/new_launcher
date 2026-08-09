#pragma once

#include <QString>

/// Common install locations for the Dota 2 7.22g build the server targets.
namespace DotaPathDetector
{
/// Returns a validated path to dota2.exe. The optional configured path is
/// checked first and may be either the executable or a Dota/Steam directory.
QString detect(const QString &preferredPath = {});

/// Resolves an executable, Dota install directory or Steam library root to a
/// real dota2.exe path. Returns an empty string when the path is not usable.
QString resolve(const QString &path);

bool isValid(const QString &path);
}
