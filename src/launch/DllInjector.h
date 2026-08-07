#pragma once

#include <QString>

/// Result of launching a game with the emulator DLL injected. The handle is a
/// Windows HANDLE (null on non-Windows builds).
struct LaunchResult
{
    bool success = false;
    quint32 pid = 0;
    void *processHandle = nullptr;
    QString error;
};

/// Windows-only process launcher: CreateProcessW suspended, payload injected
/// with CreateRemoteThread(LoadLibraryW) or the static import rebound, then the
/// process resumed. Non-Windows builds fail with a clear message.
namespace DllInjector
{
LaunchResult launchAndInject(const QString &exePath,
                             const QString &dllPath,
                             const QString &arguments,
                             const QString &workingDir,
                             bool rebindStaticImport,
                             const QString &importModuleName);
}
