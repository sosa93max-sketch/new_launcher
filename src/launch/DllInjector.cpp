#include "DllInjector.h"

#include "PeUtils.h"

#include "../util/Log.h"

#include <QDir>
#include <QFileInfo>

#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
#ifdef Q_OS_WIN

struct ProcessBasicInformation
{
    PVOID reserved1;
    PVOID pebBaseAddress;
    PVOID reserved2[2];
    ULONG_PTR uniqueProcessId;
    PVOID reserved3;
};

using NtQueryInformationProcessFn = LONG(WINAPI *)(HANDLE, ULONG, PVOID, ULONG, PULONG);

constexpr ULONG ProcessBasicInformationClass = 0;
constexpr ULONG ProcessWow64InformationClass = 26;
constexpr DWORD MemCommitReserve = 0x3000;
constexpr DWORD MemRelease = 0x8000;
constexpr DWORD PageReadWrite = 0x04;

QString winError(DWORD code)
{
    wchar_t *buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<wchar_t *>(&buffer), 0, nullptr);
    QString message;
    if (length > 0 && buffer != nullptr)
    {
        message = QString::fromWCharArray(buffer, static_cast<int>(length)).trimmed();
        LocalFree(buffer);
    }
    if (message.isEmpty())
        message = QStringLiteral("error 0x%1").arg(code, 8, 16, QLatin1Char('0'));
    return QStringLiteral("error %1 (%2)").arg(code).arg(message);
}

bool readImageBase(HANDLE process, PeUtils::Arch arch, quintptr &imageBase, QString &error)
{
    static NtQueryInformationProcessFn ntQuery = reinterpret_cast<NtQueryInformationProcessFn>(
        reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                                                "NtQueryInformationProcess")));
    if (ntQuery == nullptr)
    {
        error = QStringLiteral("NtQueryInformationProcess no disponible");
        return false;
    }

    if (arch == PeUtils::Arch::X86 && sizeof(void *) == 8)
    {
        ULONG_PTR peb = 0;
        ULONG returned = 0;
        const LONG status = ntQuery(process, ProcessWow64InformationClass, &peb,
                                    sizeof(peb), &returned);
        if (status != 0 || peb == 0)
        {
            error = QStringLiteral("NtQueryInformationProcess (wow64) falló 0x%1")
                        .arg(static_cast<quint32>(status), 8, 16, QLatin1Char('0'));
            return false;
        }
        DWORD base = 0;
        SIZE_T readBytes = 0;
        if (!ReadProcessMemory(process, reinterpret_cast<LPCVOID>(peb + 0x08),
                               &base, sizeof(base), &readBytes)
            || readBytes != sizeof(base))
        {
            error = QStringLiteral("ReadProcessMemory (imagen base x86) falló: %1")
                        .arg(winError(GetLastError()));
            return false;
        }
        imageBase = base;
        return true;
    }

    ProcessBasicInformation basic{};
    ULONG returned = 0;
    const LONG status = ntQuery(process, ProcessBasicInformationClass, &basic,
                                sizeof(basic), &returned);
    if (status != 0 || basic.pebBaseAddress == nullptr)
    {
        error = QStringLiteral("NtQueryInformationProcess (peb) falló 0x%1")
                    .arg(static_cast<quint32>(status), 8, 16, QLatin1Char('0'));
        return false;
    }

    SIZE_T readBytes = 0;
    if (sizeof(void *) == 8)
    {
        quint64 base = 0;
        if (!ReadProcessMemory(process,
                               reinterpret_cast<LPCVOID>(
                                   reinterpret_cast<quintptr>(basic.pebBaseAddress) + 0x10),
                               &base, sizeof(base), &readBytes)
            || readBytes != sizeof(base))
        {
            error = QStringLiteral("ReadProcessMemory (imagen base x64) falló: %1")
                        .arg(winError(GetLastError()));
            return false;
        }
        imageBase = static_cast<quintptr>(base);
        return true;
    }

    DWORD base = 0;
    if (!ReadProcessMemory(process,
                           reinterpret_cast<LPCVOID>(
                               reinterpret_cast<quintptr>(basic.pebBaseAddress) + 0x08),
                           &base, sizeof(base), &readBytes)
        || readBytes != sizeof(base))
    {
        error = QStringLiteral("ReadProcessMemory (imagen base x86) falló: %1")
                    .arg(winError(GetLastError()));
        return false;
    }
    imageBase = base;
    return true;
}

bool writeBytes(HANDLE process, LPVOID address, const QByteArray &bytes,
                const QString &operation, QString &error)
{
    SIZE_T written = 0;
    if (!WriteProcessMemory(process, address, bytes.constData(),
                            static_cast<SIZE_T>(bytes.size()), &written)
        || written != static_cast<SIZE_T>(bytes.size()))
    {
        error = QStringLiteral("WriteProcessMemory falló para %1: %2")
                    .arg(operation, winError(GetLastError()));
        return false;
    }
    return true;
}

bool writeProtectedBytes(HANDLE process, LPVOID address, const QByteArray &bytes,
                         const QString &operation, QString &error)
{
    DWORD previousProtect = 0;
    if (!VirtualProtectEx(process, address, bytes.size(), PageReadWrite, &previousProtect))
    {
        error = QStringLiteral("VirtualProtectEx falló para %1: %2")
                    .arg(operation, winError(GetLastError()));
        return false;
    }
    const bool ok = writeBytes(process, address, bytes, operation, error);
    VirtualProtectEx(process, address, bytes.size(), previousProtect, nullptr);
    return ok;
}

LPVOID allocateWithinImageRva(HANDLE process, quintptr imageBase, int byteCount,
                              QString &error)
{
    constexpr quintptr InitialOffset = 0x01000000;
    constexpr quintptr RetryStride = 0x01000000;
    for (int attempt = 0; attempt < 64; ++attempt)
    {
        const auto hint = reinterpret_cast<LPVOID>(
            imageBase + InitialOffset + RetryStride * attempt);
        LPVOID allocation = VirtualAllocEx(process, hint, static_cast<SIZE_T>(byteCount),
                                           MemCommitReserve, PageReadWrite);
        if (allocation == nullptr)
            continue;
        const auto rva = reinterpret_cast<quintptr>(allocation) - imageBase;
        if (rva <= 0xFFFFFFFFu)
            return allocation;
        VirtualFreeEx(process, allocation, 0, MemRelease);
    }
    error = QStringLiteral("No se pudo asignar un RVA válido para el import estático");
    return nullptr;
}

bool rebindStaticImportInProcess(HANDLE process, const QString &exePath,
                                 const QString &payloadPath,
                                 const QString &importModuleName, QString &error)
{
    const auto nameFieldRvas = PeUtils::findImportNameFieldRvas(exePath, importModuleName);
    if (nameFieldRvas.isEmpty())
    {
        error = QStringLiteral("No se encontró el import estático de %1 en el ejecutable")
                    .arg(importModuleName);
        return false;
    }

    quintptr imageBase = 0;
    if (!readImageBase(process, PeUtils::detectArch(exePath), imageBase, error))
        return false;

    const auto fullPath = QDir::toNativeSeparators(QFileInfo(payloadPath).absoluteFilePath());
    const QByteArray pathBytes = fullPath.toUtf8() + '\0';
    LPVOID remotePath = allocateWithinImageRva(process, imageBase, pathBytes.size(), error);
    if (remotePath == nullptr)
        return false;

    bool ok = writeBytes(process, remotePath, pathBytes,
                         QStringLiteral("payload import path"), error);
    if (ok)
    {
        const quint32 pathRva = static_cast<quint32>(
            reinterpret_cast<quintptr>(remotePath) - imageBase);
        QByteArray pathRvaBytes(4, '\0');
        pathRvaBytes[0] = static_cast<char>(pathRva & 0xFF);
        pathRvaBytes[1] = static_cast<char>((pathRva >> 8) & 0xFF);
        pathRvaBytes[2] = static_cast<char>((pathRva >> 16) & 0xFF);
        pathRvaBytes[3] = static_cast<char>((pathRva >> 24) & 0xFF);

        for (const auto rva : nameFieldRvas)
        {
            const auto address = reinterpret_cast<LPVOID>(imageBase + rva);
            if (!writeProtectedBytes(process, address, pathRvaBytes,
                                     QStringLiteral("descriptor de import"), error))
            {
                ok = false;
                break;
            }
        }
    }

    if (!ok)
        VirtualFreeEx(process, remotePath, 0, MemRelease);
    return ok;
}

bool injectInto(HANDLE process, const QString &dllPath, QString &error)
{
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    const auto loadLibrary = reinterpret_cast<FARPROC>(
        GetProcAddress(kernel, "LoadLibraryW"));
    if (loadLibrary == nullptr)
    {
        error = QStringLiteral("GetProcAddress(LoadLibraryW) falló");
        return false;
    }

    // LoadLibraryW expects a UTF-16 path; writing UTF-8 bytes made it read a
    // garbage wide string and return NULL.
    const auto nativePath = QDir::toNativeSeparators(QFileInfo(dllPath).absoluteFilePath());
    const std::wstring widePath = nativePath.toStdWString();
    const int pathByteCount = static_cast<int>((widePath.size() + 1) * sizeof(wchar_t));
    QByteArray pathBytes(pathByteCount, '\0');
    std::memcpy(pathBytes.data(), widePath.data(), widePath.size() * sizeof(wchar_t));
    LPVOID remoteMemory = VirtualAllocEx(process, nullptr, pathBytes.size(),
                                         MemCommitReserve, PageReadWrite);
    if (remoteMemory == nullptr)
    {
        error = QStringLiteral("VirtualAllocEx falló: %1").arg(winError(GetLastError()));
        return false;
    }

    bool ok = writeBytes(process, remoteMemory, pathBytes,
                         QStringLiteral("ruta del payload"), error);
    if (ok)
    {
        const auto startRoutine = reinterpret_cast<LPTHREAD_START_ROUTINE>(loadLibrary);
        HANDLE thread = CreateRemoteThread(process, nullptr, 0, startRoutine,
                                           remoteMemory, 0, nullptr);
        if (thread == nullptr)
        {
            error = QStringLiteral("CreateRemoteThread falló: %1").arg(winError(GetLastError()));
            ok = false;
        }
        else
        {
            const DWORD wait = WaitForSingleObject(thread, 15000);
            if (wait == WAIT_TIMEOUT)
            {
                error = QStringLiteral("Timeout cargando el payload en el proceso");
                ok = false;
            }
            else if (wait != WAIT_OBJECT_0)
            {
                error = QStringLiteral("WaitForSingleObject falló: %1").arg(winError(GetLastError()));
                ok = false;
            }
            else
            {
                DWORD exitCode = 0;
                GetExitCodeThread(thread, &exitCode);
                if (exitCode == 0)
                {
                    error = QStringLiteral("LoadLibraryW devolvió NULL en el proceso destino");
                    ok = false;
                }
            }
            CloseHandle(thread);
        }
    }

    VirtualFreeEx(process, remoteMemory, 0, MemRelease);
    return ok;
}

#endif // Q_OS_WIN
}

namespace DllInjector
{
LaunchResult launchAndInject(const QString &exePath,
                             const QString &dllPath,
                             const QString &arguments,
                             const QString &workingDir,
                             bool rebindStaticImport,
                             const QString &importModuleName)
{
#ifdef Q_OS_WIN
    if (!QFileInfo::exists(exePath))
        return {false, 0, nullptr, QStringLiteral("Ejecutable no encontrado: %1").arg(exePath)};
    if (!QFileInfo::exists(dllPath))
        return {false, 0, nullptr, QStringLiteral("Payload no encontrado: %1").arg(dllPath)};

    const auto nativeExe = QDir::toNativeSeparators(QFileInfo(exePath).absoluteFilePath());
    const auto nativeWork = workingDir.isEmpty()
        ? QFileInfo(exePath).absolutePath()
        : QDir::toNativeSeparators(QFileInfo(workingDir).absoluteFilePath());

    QString commandLine = QStringLiteral("\"%1\"").arg(nativeExe);
    if (!arguments.trimmed().isEmpty())
        commandLine += QLatin1Char(' ') + arguments.trimmed();

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_SHOWNORMAL;
    PROCESS_INFORMATION processInfo{};

    std::wstring mutableCommandLine = commandLine.toStdWString();
    const BOOL created = CreateProcessW(
        nativeExe.toStdWString().c_str(),
        mutableCommandLine.data(),
        nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr,
        nativeWork.toStdWString().c_str(), &startup, &processInfo);
    if (!created)
    {
        return {false, 0, nullptr,
                QStringLiteral("CreateProcess falló: %1").arg(winError(GetLastError()))};
    }

    QString error;
    bool injected = rebindStaticImport
        ? rebindStaticImportInProcess(processInfo.hProcess, exePath, dllPath,
                                      importModuleName, error)
        : injectInto(processInfo.hProcess, dllPath, error);

    if (injected)
    {
        AllowSetForegroundWindow(processInfo.dwProcessId);
        if (ResumeThread(processInfo.hThread) == static_cast<DWORD>(-1))
        {
            error = QStringLiteral("ResumeThread falló: %1").arg(winError(GetLastError()));
            injected = false;
        }
    }

    if (!injected)
    {
        TerminateProcess(processInfo.hProcess, 1);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        Log::line(QStringLiteral("LAUNCH falló: %1").arg(error));
        return {false, 0, nullptr, error};
    }

    Log::line(QStringLiteral("LAUNCH ok pid=%1 (rebind=%2)")
                  .arg(processInfo.dwProcessId)
                  .arg(rebindStaticImport));
    CloseHandle(processInfo.hThread);
    return {true, processInfo.dwProcessId, processInfo.hProcess, QString()};
#else
    Q_UNUSED(exePath)
    Q_UNUSED(dllPath)
    Q_UNUSED(arguments)
    Q_UNUSED(workingDir)
    Q_UNUSED(rebindStaticImport)
    Q_UNUSED(importModuleName)
    return {false, 0, nullptr, QStringLiteral("El lanzamiento con inyección solo está disponible en Windows")};
#endif
}
}
