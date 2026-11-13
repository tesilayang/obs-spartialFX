#include "project_archive.hpp"
#include "vendor/miniz/miniz.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtEndian>
#include <algorithm>
#include <limits>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace spatial::projects {
namespace {
constexpr qint64 manifestLimit = 64 * 1024 * 1024;
[[noreturn]] void fail(const QString &message) { throw message; }
void checkCancel(std::atomic_bool &cancel) { if (cancel.load(std::memory_order_relaxed)) fail(QStringLiteral("Operation cancelled.")); }
QString key(const QString &path) { return path.normalized(QString::NormalizationForm_C).toCaseFolded(); }
QString safeRelative(const QString &path, bool directory = false)
{
    QString name = path;
    if (directory && name.endsWith('/')) name.chop(1);
    if (name.isEmpty() || name.contains('\\') || name.startsWith('/') || name.contains(QChar(0)) || name.size() >= 500)
        fail(QStringLiteral("Unsafe package path: %1").arg(path));
    static const QRegularExpression reserved(QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\\..*)?$"), QRegularExpression::CaseInsensitiveOption);
    for (const QString &part : name.split('/')) {
        if (part.isEmpty() || part == "." || part == ".." || part.endsWith('.') || part.endsWith(' ') || reserved.match(part).hasMatch())
            fail(QStringLiteral("Unsafe package path: %1").arg(path));
        for (QChar c : part) if (c.unicode() < 32 || QStringLiteral(":<>\"|?*").contains(c)) fail(QStringLiteral("Unsafe package path: %1").arg(path));
    }
    return name;
}
bool isLink(const QString &path)
{
    if (QFileInfo(path).isSymLink()) return true;
#ifdef Q_OS_WIN
    const DWORD attrs = GetFileAttributesW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()));
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT);
#else
    return false;
#endif
}
void safeAncestors(const QString &path)
{
    QString current = QFileInfo(path).absoluteFilePath();
    while (true) {
        if (isLink(current)) fail(QStringLiteral("Symbolic links and reparse points are not allowed: %1").arg(current));
        const QString parent = QFileInfo(current).absolutePath(); if (parent == current) break; current = parent;
    }
}
void requireAbsent(const QString &path)
{
    safeAncestors(path);
    if (QFileInfo::exists(path) || isLink(path)) fail(QStringLiteral("Destination already exists: %1").arg(path));
}
void publish(const QString &from, const QString &to)
{
    requireAbsent(to);
#ifdef Q_OS_WIN
    if (!MoveFileExW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(from).utf16()), reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(to).utf16()), 0))
        fail(QStringLiteral("Cannot publish destination (Windows error %1): %2").arg(GetLastError()).arg(to));
#else
    if (!QDir().rename(from, to)) fail(QStringLiteral("Cannot publish destination: %1").arg(to));
#endif
}
void addPath(QSet<QString> &files, QSet<QString> &directories, QSet<QString> &ancestors, const QString &name, bool directory)
{
    const QString folded = key(name);
    if (files.contains(folded) || directories.contains(folded)) fail(QStringLiteral("Duplicate or conflicting package path: %1").arg(name));
    QString parent = name;
    while (parent.contains('/')) { parent = parent.left(parent.lastIndexOf('/')); const QString foldedParent = key(parent); if (files.contains(foldedParent)) fail(QStringLiteral("File/directory path conflict: %1").arg(name)); ancestors.insert(foldedParent); }
    if (!directory) {
        if (ancestors.contains(folded)) fail(QStringLiteral("File/directory path conflict: %1").arg(name));
        files.insert(folded);
    } else directories.insert(folded);
}
QString pointerToken(QString token)
{
    for (qsizetype i = 0; i < token.size(); ++i) if (token[i] == '~') { if (i + 1 >= token.size() || (token[i+1] != '0' && token[i+1] != '1')) fail(QStringLiteral("Invalid JSON pointer escape.")); ++i; }
    return token.replace("~1", "/").replace("~0", "~");
}
QJsonValue replacePointer(QJsonValue node, const QStringList &parts, qsizetype depth, const QString &value)
{
    if (depth == parts.size()) { if (!node.isString()) fail(QStringLiteral("Asset JSON pointer must refer to an existing string.")); return value; }
    const QString token = pointerToken(parts[depth]);
    if (node.isObject()) { auto object = node.toObject(); if (!object.contains(token)) fail(QStringLiteral("Asset JSON pointer does not exist.")); object[token] = replacePointer(object[token], parts, depth + 1, value); return object; }
    if (node.isArray()) { bool ok = false; const int index = token.toInt(&ok); auto array = node.toArray(); if (!ok || index < 0 || index >= array.size() || QString::number(index) != token) fail(QStringLiteral("Invalid asset JSON pointer array index.")); array[index] = replacePointer(array[index], parts, depth + 1, value); return array; }
    fail(QStringLiteral("Asset JSON pointer crosses a scalar value."));
}
void rewritePointers(QJsonObject &document, const QJsonObject &entry, const QString &path, QSet<QString> &used)
{
    if (!entry.value("pointers").isArray()) fail(QStringLiteral("Asset pointers must be an array."));
    for (const auto &p : entry.value("pointers").toArray()) {
        const QString pointer = p.toString();
        if (!p.isString() || !pointer.startsWith("/sources/") || used.contains(pointer)) fail(QStringLiteral("Invalid or duplicate asset JSON pointer: %1").arg(pointer));
        used.insert(pointer); document = replacePointer(document, pointer.mid(1).split('/'), 0, path).toObject();
    }
}
void validateDocument(const QJsonObject &document)
{
    if (document.value("format") != "obs-spatial-fx-project" || document.value("version").toInt(-1) != 1 || !document.value("sources").isArray() || !document.value("assets").isArray() || (document.contains("directories") && !document.value("directories").isArray())) fail(QStringLiteral("Unsupported or invalid project manifest."));
}
void addTotal(qint64 &total, qint64 size) { if (size < 0 || size > std::numeric_limits<qint64>::max() - total) fail(QStringLiteral("Invalid or overflowing package size.")); total += size; }
struct Io {
    QFile *file = nullptr; std::atomic_bool *cancel = nullptr; const Progress *progress = nullptr; QCryptographicHash *hash = nullptr;
    qint64 completed = 0, base = 0, total = 0, limit = std::numeric_limits<qint64>::max(); QString detail;
};
size_t readAt(void *opaque, mz_uint64 offset, void *buffer, size_t size)
{
    auto &io = *static_cast<Io *>(opaque);
    if (io.cancel->load(std::memory_order_relaxed) || offset > static_cast<mz_uint64>(std::numeric_limits<qint64>::max()) || size > static_cast<size_t>(std::numeric_limits<qint64>::max())) return 0;
    if (io.hash && offset != static_cast<mz_uint64>(io.completed)) return 0;
    if (!io.file->seek(static_cast<qint64>(offset))) return 0;
    const qint64 n = io.file->read(static_cast<char *>(buffer), static_cast<qint64>(size)); if (n <= 0) return 0;
    if (io.hash) { io.hash->addData(QByteArrayView(static_cast<const char *>(buffer), n)); io.completed += n; if (io.progress && *io.progress) (*io.progress)(io.base + io.completed, io.total, io.detail); }
    return static_cast<size_t>(n);
}
size_t writeAt(void *opaque, mz_uint64 offset, const void *buffer, size_t size)
{
    auto &io = *static_cast<Io *>(opaque);
    if (io.cancel->load(std::memory_order_relaxed) || offset > static_cast<mz_uint64>(io.limit) || size > static_cast<mz_uint64>(io.limit) - offset) return 0;
    if (!io.file->seek(static_cast<qint64>(offset))) return 0;
    const qint64 n = io.file->write(static_cast<const char *>(buffer), static_cast<qint64>(size)); if (n <= 0) return 0;
    io.completed += n; if (io.progress && *io.progress) (*io.progress)(io.base + io.completed, io.total, io.detail); return static_cast<size_t>(n);
}
struct Zip {
    mz_zip_archive archive{}; bool writer = false, active = false;
    ~Zip() { if (active) { if (writer) mz_zip_writer_end(&archive); else mz_zip_reader_end(&archive); } }
    QString error() { return QString::fromLatin1(mz_zip_get_error_string(mz_zip_get_last_error(&archive))); }
};
quint16 le16(const QByteArray &data, qsizetype offset) { return qFromLittleEndian<quint16>(data.constData() + offset); }
quint32 le32(const QByteArray &data, qsizetype offset) { return qFromLittleEndian<quint32>(data.constData() + offset); }
quint64 le64(const QByteArray &data, qsizetype offset) { return qFromLittleEndian<quint64>(data.constData() + offset); }
// miniz's header validator guesses descriptor width from the global ZIP64 flag.
// ZIP permits 32-bit descriptors for small members of a ZIP64 archive; verify
// both legal widths here and leave full streamed CRC checks to the extractor.
quint64 validateLocal(QFile &file, const mz_zip_archive_file_stat &stat, const QByteArray &name, quint64 centralOffset)
{
    const quint64 start = stat.m_local_header_ofs;
    if (start > centralOffset || centralOffset - start < 30 || !file.seek(static_cast<qint64>(start))) fail(QStringLiteral("Invalid ZIP local header offset."));
    const QByteArray header = file.read(30);
    if (header.size() != 30 || le32(header,0) != 0x04034b50 || le16(header,6) != stat.m_bit_flag || le16(header,8) != stat.m_method || le16(header,26) != name.size()) fail(QStringLiteral("ZIP local and central headers disagree."));
    const quint64 payload = start + 30 + le16(header,26) + le16(header,28);
    if (payload > centralOffset || stat.m_comp_size > centralOffset - payload) fail(QStringLiteral("ZIP member exceeds data bounds."));
    if (file.read(name.size()) != name) fail(QStringLiteral("ZIP local and central filenames disagree."));
    const QByteArray extra = file.read(le16(header,28));
    if (extra.size() != le16(header,28)) fail(QStringLiteral("Truncated ZIP extra data."));
    quint64 uncompressed = le32(header,22), compressed = le32(header,18);
    const bool hasDescriptor = (stat.m_bit_flag & 8) != 0;
    if (!hasDescriptor && (uncompressed == 0xffffffff || compressed == 0xffffffff)) {
        bool found = false;
        for (qsizetype at = 0; at + 4 <= extra.size();) {
            const quint16 id = le16(extra,at), length = le16(extra,at+2); at += 4;
            if (at + length > extra.size()) fail(QStringLiteral("Invalid ZIP extra field."));
            if (id == 1) {
                qsizetype valueAt = at;
                if (uncompressed == 0xffffffff) { if (valueAt + 8 > at + length) fail(QStringLiteral("Missing ZIP64 size.")); uncompressed = le64(extra,valueAt); valueAt += 8; }
                if (compressed == 0xffffffff) { if (valueAt + 8 > at + length) fail(QStringLiteral("Missing ZIP64 size.")); compressed = le64(extra,valueAt); }
                found = true; break;
            }
            at += length;
        }
        if (!found) fail(QStringLiteral("Missing ZIP64 extended sizes."));
    }
    const quint64 end = payload + stat.m_comp_size;
    if (!hasDescriptor) {
        if (le32(header,14) != stat.m_crc32 || compressed != stat.m_comp_size || uncompressed != stat.m_uncomp_size) fail(QStringLiteral("ZIP local and central sizes/CRC disagree."));
        return end;
    }
    if (!file.seek(static_cast<qint64>(end))) fail(QStringLiteral("Invalid ZIP descriptor offset."));
    const QByteArray descriptor = file.read(static_cast<qint64>(std::min<quint64>(24,centralOffset-end)));
    const qsizetype at = descriptor.size() >= 4 && le32(descriptor,0) == 0x08074b50 ? 4 : 0;
    if (descriptor.size() < at + 12 || le32(descriptor,at) != stat.m_crc32) fail(QStringLiteral("Invalid ZIP descriptor CRC."));
    if (descriptor.size() >= at + 20 && le64(descriptor,at+4) == stat.m_comp_size && le64(descriptor,at+12) == stat.m_uncomp_size) return end + static_cast<quint64>(at) + 20;
    if (le32(descriptor,at+4) == stat.m_comp_size && le32(descriptor,at+8) == stat.m_uncomp_size) return end + static_cast<quint64>(at) + 12;
    fail(QStringLiteral("Invalid ZIP descriptor sizes."));
}
QString hashFile(const QString &path, std::atomic_bool &cancel, const Progress &progress, qint64 base, qint64 total)
{
    safeAncestors(path); QFile file(path); if (!QFileInfo(path).isFile() || !file.open(QIODevice::ReadOnly)) fail(QStringLiteral("Cannot read asset: %1").arg(path));
    QCryptographicHash hash(QCryptographicHash::Sha256); qint64 done = 0;
    while (!file.atEnd()) { checkCancel(cancel); const QByteArray bytes = file.read(1024 * 1024); if (bytes.isEmpty() && file.error() != QFileDevice::NoError) fail(QStringLiteral("Cannot read asset: %1").arg(path)); hash.addData(bytes); done += bytes.size(); if (progress) progress(base + done, total, path); }
    return QString::fromLatin1(hash.result().toHex());
}
}

bool writeProjectPackage(QJsonObject document, const QString &zipPath, std::atomic_bool &cancel, const Progress &progress, QString &error)
{
    error.clear();
    try {
        checkCancel(cancel); validateDocument(document); const QString output = QFileInfo(zipPath).absoluteFilePath(); requireAbsent(output);
        const QString parent = QFileInfo(output).absolutePath(); if (!QFileInfo(parent).isDir()) fail(QStringLiteral("Destination parent does not exist."));
        auto assets = document.value("assets").toArray(); auto directories = document.value("directories").toArray();
        QSet<QString> files{key("project.json")}, dirs, ancestors, pointers, ids; qint64 total = 0;
        for (const auto &value : directories) {
            if (!value.isObject()) fail(QStringLiteral("Invalid directory record.")); const auto entry = value.toObject(); const QString relative = safeRelative(entry.value("relative_path").toString(), true);
            const QString source = entry.value("original_path").toString();
            safeAncestors(source);
            if (source.isEmpty() || !QFileInfo(source).isDir()) fail(QStringLiteral("Missing source directory: %1").arg(source));
            addPath(files, dirs, ancestors, relative, true); rewritePointers(document, entry, relative, pointers);
        }
        for (const auto &value : assets) {
            if (!value.isObject()) fail(QStringLiteral("Invalid asset record.")); const auto entry = value.toObject(); const QString relative = safeRelative(entry.value("relative_path").toString());
            const QString id = entry.value("id").toString(); if (id.isEmpty() || ids.contains(id)) fail(QStringLiteral("Missing or duplicate asset identity.")); ids.insert(id);
            addPath(files, dirs, ancestors, relative, false); rewritePointers(document, entry, relative, pointers);
            const QString source = entry.value("original_path").toString(); safeAncestors(source); if (!QFileInfo(source).isFile()) fail(QStringLiteral("Missing asset: %1").arg(source)); addTotal(total, QFileInfo(source).size());
        }
        QTemporaryDir staging(parent + "/.spatial-package-XXXXXX");
        if (!staging.isValid()) fail(QStringLiteral("Cannot create package staging directory."));
        QFile temporary(staging.path() + "/package.zip");
        if (!temporary.open(QIODevice::WriteOnly | QIODevice::NewOnly)) fail(QStringLiteral("Cannot create package staging file."));
        Io outputIo; outputIo.file = &temporary; outputIo.cancel = &cancel;
        Zip zip; zip.writer = true; zip.archive.m_pWrite = writeAt; zip.archive.m_pIO_opaque = &outputIo;
        if (!mz_zip_writer_init_v2(&zip.archive, 0, MZ_ZIP_FLAG_WRITE_ZIP64)) fail(zip.error()); zip.active = true;
        for (const auto &value : directories) { checkCancel(cancel); const auto entry = value.toObject(); const QByteArray name = (safeRelative(entry.value("relative_path").toString(), true) + '/').toUtf8(); if (!mz_zip_writer_add_mem(&zip.archive, name.constData(), nullptr, 0, 0)) fail(zip.error()); }
        qint64 done = 0;
        for (qsizetype i = 0; i < assets.size(); ++i) {
            checkCancel(cancel); auto entry = assets[i].toObject(); const QString source = entry.value("original_path").toString(); safeAncestors(source); QFile file(source); if (!file.open(QIODevice::ReadOnly)) fail(QStringLiteral("Cannot read asset: %1").arg(source));
            const qint64 size = file.size(); QCryptographicHash hash(QCryptographicHash::Sha256);
            Io input; input.file = &file; input.cancel = &cancel; input.progress = &progress; input.hash = &hash; input.base = done; input.total = total; input.detail = source;
            const QByteArray name = entry.value("relative_path").toString().toUtf8();
            if (!mz_zip_writer_add_read_buf_callback(&zip.archive, name.constData(), readAt, &input, static_cast<mz_uint64>(size), nullptr, nullptr, 0, MZ_ZIP_FLAG_WRITE_HEADER_SET_SIZE, nullptr, 0, nullptr, 0)) { checkCancel(cancel); fail(zip.error()); }
            if (input.completed != size || file.size() != size) fail(QStringLiteral("Asset changed during export: %1").arg(source));
            entry["size"] = size; entry["sha256"] = QString::fromLatin1(hash.result().toHex()); assets[i] = entry; done += size;
        }
        document["assets"] = assets; const QByteArray json = QJsonDocument(document).toJson(QJsonDocument::Indented); if (json.size() > manifestLimit) fail(QStringLiteral("Project manifest is too large.")); checkCancel(cancel);
        if (!mz_zip_writer_add_mem(&zip.archive, "project.json", json.constData(), static_cast<size_t>(json.size()), MZ_ZIP_FLAG_WRITE_HEADER_SET_SIZE) || !mz_zip_writer_finalize_archive(&zip.archive)) fail(zip.error());
        if (!mz_zip_writer_end(&zip.archive)) fail(zip.error()); zip.active = false;
        if (!temporary.flush()) fail(QStringLiteral("Cannot flush package file.")); temporary.close(); checkCancel(cancel); publish(temporary.fileName(), output); if (progress) progress(total, total, output); return true;
    } catch (const QString &message) { error = message; } catch (const std::exception &e) { error = QString::fromUtf8(e.what()); }
    return false;
}

QJsonObject readProjectFolder(const QString &folder, std::atomic_bool &cancel, const Progress &progress, QString &error)
{
    error.clear();
    try {
        checkCancel(cancel); const QString root = QFileInfo(folder).absoluteFilePath(); safeAncestors(root); if (!QFileInfo(root).isDir()) fail(QStringLiteral("Project folder does not exist."));
        const QString manifest = root + "/project.json"; safeAncestors(manifest); QFile file(manifest); if (!file.open(QIODevice::ReadOnly) || file.size() > manifestLimit) fail(QStringLiteral("Cannot read project.json or manifest too large."));
        QJsonParseError parse; const auto json = QJsonDocument::fromJson(file.readAll(), &parse); if (parse.error != QJsonParseError::NoError || !json.isObject()) fail(QStringLiteral("Invalid project.json: %1").arg(parse.errorString()));
        QJsonObject document = json.object(); validateDocument(document); auto assets = document["assets"].toArray(); auto directories = document["directories"].toArray();
        QSet<QString> files{key("project.json")}, dirs, ancestors, pointers, ids; qint64 total = 0;
        for (const auto &value : assets) {
            if (!value.isObject()) fail(QStringLiteral("Invalid asset record.")); const auto entry = value.toObject(); const QString relative = safeRelative(entry["relative_path"].toString()); addPath(files, dirs, ancestors, relative, false);
            const QString id = entry["id"].toString(); if (id.isEmpty() || ids.contains(id)) fail(QStringLiteral("Missing or duplicate asset identity.")); ids.insert(id);
            const auto declared = entry["size"]; const qint64 size = declared.toInteger(-1); if (!declared.isDouble() || declared.toDouble() != static_cast<double>(size)) fail(QStringLiteral("Invalid asset size.")); addTotal(total, size);
        }
        qint64 done = 0;
        for (qsizetype i = 0; i < directories.size(); ++i) {
            checkCancel(cancel); if (!directories[i].isObject()) fail(QStringLiteral("Invalid directory record.")); auto entry = directories[i].toObject(); const QString relative = safeRelative(entry["relative_path"].toString(), true); addPath(files, dirs, ancestors, relative, true);
            const QString path = QDir(root).absoluteFilePath(relative); safeAncestors(path); if (!QFileInfo(path).isDir()) fail(QStringLiteral("Missing project directory: %1").arg(relative));
            rewritePointers(document, entry, path, pointers); entry["original_path"] = path; directories[i] = entry;
        }
        static const QRegularExpression sha(QStringLiteral("^[0-9a-fA-F]{64}$"));
        for (qsizetype i = 0; i < assets.size(); ++i) {
            checkCancel(cancel); auto entry = assets[i].toObject(); const QString path = QDir(root).absoluteFilePath(entry["relative_path"].toString()); safeAncestors(path);
            if (!QFileInfo(path).isFile() || QFileInfo(path).size() != entry["size"].toInteger(-1)) fail(QStringLiteral("Asset missing or size mismatch: %1").arg(path));
            const QString expected = entry["sha256"].toString(); if (!sha.match(expected).hasMatch() || hashFile(path, cancel, progress, done, total).compare(expected, Qt::CaseInsensitive) != 0) fail(QStringLiteral("Asset SHA256 mismatch: %1").arg(path));
            done += entry["size"].toInteger(); rewritePointers(document, entry, path, pointers); entry["original_path"] = path; assets[i] = entry;
        }
        checkCancel(cancel); document["assets"] = assets; if (document.contains("directories")) document["directories"] = directories; return document;
    } catch (const QString &message) { error = message; } catch (const std::exception &e) { error = QString::fromUtf8(e.what()); }
    return {};
}

bool extractProjectPackage(const QString &zipPath, const QString &destination, std::atomic_bool &cancel, const Progress &progress, QString &error)
{
    error.clear();
    try {
        checkCancel(cancel); const QString output = QFileInfo(destination).absoluteFilePath(); requireAbsent(output); const QString parent = QFileInfo(output).absolutePath(); if (!QFileInfo(parent).isDir()) fail(QStringLiteral("Destination parent does not exist."));
        safeAncestors(zipPath); QFile input(zipPath); if (!input.open(QIODevice::ReadOnly)) fail(QStringLiteral("Cannot open package.")); Io inputIo; inputIo.file = &input; inputIo.cancel = &cancel;
        Zip zip; zip.archive.m_pRead = readAt; zip.archive.m_pIO_opaque = &inputIo; if (!mz_zip_reader_init(&zip.archive, static_cast<mz_uint64>(input.size()), 0)) fail(zip.error()); zip.active = true;
        const mz_uint count = mz_zip_reader_get_num_files(&zip.archive); if (count > 1000000) fail(QStringLiteral("Package has too many entries."));
        struct Entry { mz_uint index; QString path; qint64 size; bool directory; }; QList<Entry> entries; QSet<QString> files, dirs, ancestors; qint64 total = 0; QList<QPair<quint64,quint64>> ranges;
        for (mz_uint i = 0; i < count; ++i) {
            checkCancel(cancel); mz_zip_archive_file_stat stat{}; if (!mz_zip_reader_file_stat(&zip.archive, i, &stat)) fail(zip.error());
            const mz_uint nameSize = mz_zip_reader_get_filename(&zip.archive, i, nullptr, 0); if (nameSize < 2 || nameSize > 2000) fail(QStringLiteral("Invalid package filename length."));
            QByteArray raw(static_cast<qsizetype>(nameSize), 0); mz_zip_reader_get_filename(&zip.archive, i, raw.data(), nameSize); raw.chop(1); const QString name = QString::fromUtf8(raw);
            if (name.toUtf8() != raw || raw.contains('\0')) fail(QStringLiteral("Package filename must be valid UTF-8.")); const bool directory = stat.m_is_directory != 0;
            const QString relative = safeRelative(name, directory); addPath(files, dirs, ancestors, relative, directory); const mz_uint mode = (stat.m_external_attr >> 16) & 0170000;
            if ((mode && mode != 0100000 && mode != 0040000) || (stat.m_external_attr & 0x400) || stat.m_is_encrypted || !stat.m_is_supported) fail(QStringLiteral("Unsupported link, reparse point, encrypted or special ZIP entry: %1").arg(name));
            if (stat.m_uncomp_size > static_cast<mz_uint64>(std::numeric_limits<qint64>::max()) || (directory && (stat.m_uncomp_size != 0 || stat.m_comp_size != 0))) fail(QStringLiteral("Invalid ZIP entry size."));
            if (!directory) { addTotal(total, static_cast<qint64>(stat.m_uncomp_size)); if (key(relative) == "project.json" && stat.m_uncomp_size > manifestLimit) fail(QStringLiteral("Project manifest too large.")); }
            if ((stat.m_method == 0 && stat.m_comp_size != stat.m_uncomp_size) || (stat.m_uncomp_size == 0 && stat.m_crc32 != 0)) fail(QStringLiteral("Invalid ZIP stored size or empty CRC."));
            const quint64 end = validateLocal(input, stat, raw, zip.archive.m_central_directory_file_ofs);
            ranges.append({stat.m_local_header_ofs,end}); entries.append({i, relative, static_cast<qint64>(stat.m_uncomp_size), directory});
        }
        std::sort(ranges.begin(),ranges.end());
        for (qsizetype i = 1; i < ranges.size(); ++i) if (ranges[i].first < ranges[i-1].second) fail(QStringLiteral("Overlapping ZIP members."));
        if (!files.contains(key("project.json"))) fail(QStringLiteral("Package has no project.json."));
        QTemporaryDir staging(parent + "/.spatial-import-XXXXXX"); if (!staging.isValid()) fail(QStringLiteral("Cannot create extraction staging directory.")); qint64 done = 0;
        for (const auto &entry : entries) {
            checkCancel(cancel); const QString path = QDir(staging.path()).absoluteFilePath(entry.path); safeAncestors(path); if (!QDir().mkpath(entry.directory ? path : QFileInfo(path).absolutePath())) fail(QStringLiteral("Cannot create package directory.")); if (entry.directory) continue;
            QFile file(path); if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) fail(QStringLiteral("Cannot create extracted asset: %1").arg(entry.path));
            Io outputIo; outputIo.file = &file; outputIo.cancel = &cancel; outputIo.progress = &progress; outputIo.base = done; outputIo.total = total; outputIo.limit = entry.size; outputIo.detail = entry.path;
            if (!mz_zip_reader_extract_to_callback(&zip.archive, entry.index, writeAt, &outputIo, 0)) { checkCancel(cancel); fail(QStringLiteral("Corrupt ZIP entry %1: %2").arg(entry.path, zip.error())); }
            if (file.size() != entry.size || outputIo.completed != entry.size || !file.flush()) fail(QStringLiteral("Extracted size or disk write mismatch: %1").arg(entry.path)); done += entry.size;
        }
        QString validationError; if (readProjectFolder(staging.path(), cancel, {}, validationError).isEmpty()) fail(validationError); checkCancel(cancel); publish(staging.path(), output); staging.setAutoRemove(false); if (progress) progress(total, total, output); return true;
    } catch (const QString &message) { error = message; } catch (const std::exception &e) { error = QString::fromUtf8(e.what()); }
    return false;
}
}


