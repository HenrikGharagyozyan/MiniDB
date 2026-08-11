#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>

namespace minidb 
{
    // Types of operations in the log
    enum class LogRecordType : uint8_t 
    {
        SET = 0x01,
        DELETE = 0x02
    };

    // In-memory log record structure
    struct WalRecord 
    {
        LogRecordType type;
        std::string key;
        std::string value;
    };

    class Wal 
    {
    public:
        explicit Wal(std::string log_filename);
        ~Wal();

        // Disable copying to avoid duplicating file descriptors
        Wal(const Wal&) = delete;
        Wal& operator=(const Wal&) = delete;

        // Allow moving
        Wal(Wal&&) noexcept = default;
        Wal& operator=(Wal&&) noexcept = default;

        // Append operations to the log
        void append_set(const std::string& key, const std::string& value);
        void append_delete(const std::string& key);

        // Force flush buffers to disk (fflush + fsync)
        void flush();

        // Read the log on startup to recover data
        std::vector<WalRecord> recover();

        // Clear the log (after pages have been successfully flushed to disk)
        void clear();

    private:
        std::string log_filename_;
        std::fstream log_stream_;

        void write_record(LogRecordType type, const std::string& key, const std::string& value);
        
        // Helper method to compute CRC32 / checksum
        uint32_t calculate_checksum(LogRecordType type, const std::string& key, const std::string& value) const;
    };

} // namespace minidb