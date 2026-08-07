#include "PeUtils.h"

#include <QFile>

#include <QByteArray>

#include <algorithm>

namespace
{
struct Section
{
    quint32 virtualSize = 0;
    quint32 virtualAddress = 0;
    quint32 rawSize = 0;
    quint32 rawOffset = 0;
};

bool readU16(QFile &file, qint64 offset, quint16 &out)
{
    if (!file.seek(offset))
        return false;
    QByteArray bytes = file.read(2);
    if (bytes.size() != 2)
        return false;
    out = static_cast<quint16>(static_cast<unsigned char>(bytes.at(0))
                               | (static_cast<unsigned char>(bytes.at(1)) << 8));
    return true;
}

bool readU32(QFile &file, qint64 offset, quint32 &out)
{
    if (!file.seek(offset))
        return false;
    QByteArray bytes = file.read(4);
    if (bytes.size() != 4)
        return false;
    out = 0;
    for (int i = 0; i < 4; ++i)
        out |= static_cast<quint32>(static_cast<unsigned char>(bytes.at(i))) << (8 * i);
    return true;
}

bool readU64(QFile &file, qint64 offset, quint64 &out)
{
    quint32 low = 0;
    quint32 high = 0;
    if (!readU32(file, offset, low) || !readU32(file, offset + 4, high))
        return false;
    out = (static_cast<quint64>(high) << 32) | low;
    return true;
}

QString readAnsiZ(QFile &file, qint64 offset)
{
    if (!file.seek(offset))
        return QString();
    QByteArray bytes;
    bytes.reserve(256);
    for (int i = 0; i < 4096; ++i)
    {
        char byte = 0;
        if (file.read(&byte, 1) != 1)
            break;
        if (byte == '\0')
            return QString::fromLatin1(bytes);
        bytes.append(byte);
    }
    return QString();
}

class PeImage
{
public:
    explicit PeImage(const QString &path)
        : m_file(path)
    {
        m_ok = parse();
    }

    bool ok() const { return m_ok; }
    Arch arch() const { return m_arch; }
    int pointerSize() const { return m_pointerSize; }

    QVector<QString> importedModules() const
    {
        QVector<QString> result;
        if (!m_ok || m_importRva == 0)
            return result;

        qint64 descriptorOffset = rvaToOffset(m_importRva);
        if (descriptorOffset < 0)
            return result;

        for (int i = 0;; ++i)
        {
            const qint64 offset = descriptorOffset + static_cast<qint64>(i) * 20;
            quint32 originalFirstThunk = 0;
            quint32 nameRva = 0;
            quint32 firstThunk = 0;
            if (!readU32(m_file, offset, originalFirstThunk)
                || !readU32(m_file, offset + 12, nameRva)
                || !readU32(m_file, offset + 16, firstThunk))
                break;
            if (originalFirstThunk == 0 && nameRva == 0 && firstThunk == 0)
                break;
            if (nameRva == 0)
                continue;
            result.append(readAnsiZ(m_file, rvaToOffset(nameRva)));
        }
        return result;
    }

    QVector<quint32> importNameFieldRvas(const QString &moduleName) const
    {
        QVector<quint32> result;
        if (!m_ok || m_importRva == 0)
            return result;

        qint64 descriptorOffset = rvaToOffset(m_importRva);
        if (descriptorOffset < 0)
            return result;

        for (int i = 0;; ++i)
        {
            const quint32 descriptorRva = m_importRva + static_cast<quint32>(i) * 20;
            const qint64 offset = descriptorOffset + static_cast<qint64>(i) * 20;
            quint32 originalFirstThunk = 0;
            quint32 nameRva = 0;
            quint32 firstThunk = 0;
            if (!readU32(m_file, offset, originalFirstThunk)
                || !readU32(m_file, offset + 12, nameRva)
                || !readU32(m_file, offset + 16, firstThunk))
                break;
            if (originalFirstThunk == 0 && nameRva == 0 && firstThunk == 0)
                break;
            if (nameRva != 0
                && QString::compare(readAnsiZ(m_file, rvaToOffset(nameRva)),
                                    moduleName, Qt::CaseInsensitive) == 0)
                result.append(descriptorRva + 12);
        }
        return result;
    }

private:
    bool parse()
    {
        if (!m_file.open(QIODevice::ReadOnly))
            return false;

        quint16 dosSignature = 0;
        if (!readU16(m_file, 0, dosSignature) || dosSignature != 0x5A4D)
            return false;

        quint32 peOffset = 0;
        if (!readU32(m_file, 0x3C, peOffset))
            return false;

        quint32 peSignature = 0;
        if (!readU32(m_file, peOffset, peSignature) || peSignature != 0x00004550)
            return false;

        quint16 machine = 0;
        quint16 sectionCount = 0;
        quint16 optionalHeaderSize = 0;
        if (!readU16(m_file, peOffset + 4, machine)
            || !readU16(m_file, peOffset + 6, sectionCount)
            || !readU16(m_file, peOffset + 20, optionalHeaderSize))
            return false;

        m_arch = (machine == 0x8664) ? PeUtils::Arch::X64
               : (machine == 0x014C) ? PeUtils::Arch::X86
                                     : PeUtils::Arch::Unknown;

        const quint32 optionalHeader = peOffset + 24;
        quint16 magic = 0;
        if (!readU16(m_file, optionalHeader, magic))
            return false;
        if (magic == 0x20B)
            m_pointerSize = 8;
        else if (magic == 0x10B)
            m_pointerSize = 4;
        else
            return false;

        const quint32 dataDirectories = optionalHeader + (m_pointerSize == 8 ? 112u : 96u);
        if (!readU32(m_file, dataDirectories + 8, m_importRva))
            return false;

        const qint64 sectionOffset = optionalHeader + optionalHeaderSize;
        for (int i = 0; i < sectionCount; ++i)
        {
            const qint64 offset = sectionOffset + static_cast<qint64>(i) * 40;
            Section section;
            if (!readU32(m_file, offset + 8, section.virtualSize)
                || !readU32(m_file, offset + 12, section.virtualAddress)
                || !readU32(m_file, offset + 16, section.rawSize)
                || !readU32(m_file, offset + 20, section.rawOffset))
                return false;
            m_sections.append(section);
        }
        return true;
    }

    qint64 rvaToOffset(quint32 rva) const
    {
        for (const auto &section : m_sections)
        {
            const quint32 size = std::max(section.virtualSize, section.rawSize);
            if (rva >= section.virtualAddress && rva < section.virtualAddress + size)
                return static_cast<qint64>(section.rawOffset) + rva - section.virtualAddress;
        }
        return -1;
    }

    QFile m_file;
    bool m_ok = false;
    PeUtils::Arch m_arch = PeUtils::Arch::Unknown;
    int m_pointerSize = 0;
    quint32 m_importRva = 0;
    QVector<Section> m_sections;
};
}

namespace PeUtils
{
Arch detectArch(const QString &exePath)
{
    PeImage image(exePath);
    if (!image.ok())
        return Arch::Unknown;
    return image.arch();
}

bool importsModule(const QString &path, const QString &moduleName)
{
    PeImage image(path);
    if (!image.ok())
        return false;
    const auto modules = image.importedModules();
    return std::any_of(modules.cbegin(), modules.cend(), [&moduleName](const QString &name) {
        return QString::compare(name, moduleName, Qt::CaseInsensitive) == 0;
    });
}

QVector<quint32> findImportNameFieldRvas(const QString &path, const QString &moduleName)
{
    PeImage image(path);
    if (!image.ok())
        return {};
    return image.importNameFieldRvas(moduleName);
}
}
