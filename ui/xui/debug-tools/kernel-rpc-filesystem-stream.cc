//
// xemu Guest Kernel RPC filesystem host-stream ownership
//
#include "kernel-rpc-filesystem.hh"
#include "kernel-rpc-filesystem-internal.hh"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>

namespace XemuKernelFs {

struct ImportHostStream::Impl {
    std::ifstream stream;
    std::string path;
};

ImportHostStream::ImportHostStream()
    : m_impl(std::make_unique<Impl>())
{
}

ImportHostStream::~ImportHostStream() = default;
ImportHostStream::ImportHostStream(ImportHostStream &&) noexcept = default;
ImportHostStream &ImportHostStream::operator=(ImportHostStream &&) noexcept = default;

void ImportHostStream::Reset()
{
    if (!m_impl) {
        m_impl = std::make_unique<Impl>();
        return;
    }
    if (m_impl->stream.is_open()) {
        m_impl->stream.close();
    }
    m_impl->stream.clear();
    m_impl->path.clear();
}

bool LoadImportFileChunk(ImportHostStream &cache,
                         const TransferEntry &item,
                         uint64_t file_offset,
                         std::vector<uint8_t> &chunk,
                         uint32_t &chunk_bytes,
                         uint64_t &expected_file_size,
                         std::string &error)
{
    namespace fs = std::filesystem;
    error.clear();
    chunk.clear();
    chunk_bytes = 0;
    expected_file_size = 0;

    if (item.directory) {
        return true;
    }
    if (file_offset > item.file_size) {
        error = "Kernel Import file offset moved beyond the planned host file size.";
        return false;
    }

    const uint64_t remaining = item.file_size - file_offset;
    const size_t amount = static_cast<size_t>(
        std::min<uint64_t>(remaining, kImportChunkBytes));
    if (amount != 0u) {
        std::error_code status_error;
        const fs::path host = fs::u8path(item.host_path);
        const fs::file_status status = fs::symlink_status(host, status_error);
        if (status_error || !fs::is_regular_file(status) || fs::is_symlink(status)) {
            error = "Host file changed or became unavailable during Kernel Import: " +
                    item.host_path;
            return false;
        }
        const uint64_t live_size = static_cast<uint64_t>(fs::file_size(host, status_error));
        int64_t live_write_time = 0;
        if (status_error || live_size != item.file_size ||
            !ReadHostWriteTime(host, live_write_time, error) ||
            live_write_time != item.host_write_time) {
            if (error.empty()) {
                error = "Host file changed or became unavailable during Kernel Import: " +
                        item.host_path;
            }
            return false;
        }

        if (!cache.m_impl) {
            cache.m_impl = std::make_unique<ImportHostStream::Impl>();
        }
        auto &cached = *cache.m_impl;
        if (!cached.stream.is_open() || cached.path != item.host_path) {
            cache.Reset();
            cached.stream.open(host, std::ios::binary);
            cached.path = item.host_path;
        }
        if (!cached.stream) {
            error = "Could not open host file during Kernel Import: " + item.host_path;
            cache.Reset();
            return false;
        }
        cached.stream.clear();
        cached.stream.seekg(static_cast<std::streamoff>(file_offset), std::ios::beg);
        if (!cached.stream) {
            error = "Could not seek host file during Kernel Import: " + item.host_path;
            cache.Reset();
            return false;
        }
        chunk.resize(amount);
        cached.stream.read(reinterpret_cast<char *>(chunk.data()),
                           static_cast<std::streamsize>(amount));
        if (static_cast<size_t>(cached.stream.gcount()) != amount) {
            chunk.clear();
            error = "Could not read the complete host file chunk during Kernel Import: " +
                    item.host_path;
            cache.Reset();
            return false;
        }
    }

    chunk_bytes = static_cast<uint32_t>(amount);
    expected_file_size = file_offset + amount;
    return true;
}

bool LoadImportFileChunk(const TransferEntry &item,
                         uint64_t file_offset,
                         std::vector<uint8_t> &chunk,
                         uint32_t &chunk_bytes,
                         uint64_t &expected_file_size,
                         std::string &error)
{
    ImportHostStream stream;
    return LoadImportFileChunk(stream, item, file_offset, chunk, chunk_bytes,
                               expected_file_size, error);
}

} // namespace XemuKernelFs
