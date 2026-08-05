#include "minidb/Wal.h"
#include <iostream>
#include <utility>

namespace minidb 
{

    Wal::Wal(std::string log_filename) 
        : log_filename_(std::move(log_filename)) 
    {
        // Open in append-only mode
        log_stream_.open(log_filename_, std::ios::out | std::ios::app | std::ios::binary);
    }

    Wal::~Wal() 
    {
        if (log_stream_.is_open()) 
        {
            log_stream_.close();
        }
    }

    void Wal::append_set(const std::string& key, const std::string& value) 
    {
        write_record(LogRecordType::SET, key, value);
    }

    void Wal::append_delete(const std::string& key) 
    {
        write_record(LogRecordType::DELETE, key, "");
    }

    void Wal::flush() 
    {
        if (log_stream_.is_open()) 
        {
            log_stream_.flush();
        }
    }

    void Wal::clear() 
    {
        if (log_stream_.is_open()) 
        {
            log_stream_.close();
        }
        // Opening with trunc flag wipes the file contents completely
        log_stream_.open(log_filename_, std::ios::out | std::ios::trunc | std::ios::binary);
        log_stream_.close();
        
        // Return to append mode
        log_stream_.open(log_filename_, std::ios::out | std::ios::app | std::ios::binary);
    }

    void Wal::write_record(LogRecordType type, const std::string& key, const std::string& value) 
    {
        if (!log_stream_.is_open()) 
            return;

        uint32_t checksum = calculate_checksum(type, key, value);
        uint8_t t = static_cast<uint8_t>(type);
        uint16_t k_len = static_cast<uint16_t>(key.size());
        uint32_t v_len = static_cast<uint32_t>(value.size());

        log_stream_.write(reinterpret_cast<const char*>(&t), sizeof(t));
        log_stream_.write(reinterpret_cast<const char*>(&k_len), sizeof(k_len));
        log_stream_.write(reinterpret_cast<const char*>(&v_len), sizeof(v_len));
        log_stream_.write(key.data(), k_len);
        
        if (v_len > 0) 
        {
            log_stream_.write(value.data(), v_len);
        }
        
        log_stream_.write(reinterpret_cast<const char*>(&checksum), sizeof(checksum));
    }

    std::vector<WalRecord> Wal::recover() 
    {
        std::vector<WalRecord> records;
        
        // Open a separate stream for reading from the start of the file
        std::ifstream in(log_filename_, std::ios::in | std::ios::binary);
        if (!in.is_open()) 
            return records;

        while (true) 
        {
            uint8_t t;
            if (!in.read(reinterpret_cast<char*>(&t), sizeof(t))) 
                break; // End of file

            uint16_t k_len;
            if (!in.read(reinterpret_cast<char*>(&k_len), sizeof(k_len))) 
                break;

            uint32_t v_len;
            if (!in.read(reinterpret_cast<char*>(&v_len), sizeof(v_len))) 
                break;

            std::string key(k_len, '\0');
            if (!in.read(key.data(), k_len)) 
                break;

            std::string value(v_len, '\0');
            if (v_len > 0) 
            {
                if (!in.read(value.data(), v_len)) 
                    break;
            }

            uint32_t checksum;
            if (!in.read(reinterpret_cast<char*>(&checksum), sizeof(checksum))) 
                break;

            LogRecordType type = static_cast<LogRecordType>(t);
            uint32_t expected_checksum = calculate_checksum(type, key, value);
            
            // Guard against corrupted records (if the system crashed during write)
            if (checksum != expected_checksum) 
            {
                std::cerr << "Warning: WAL corruption detected at end of file. Stopping recovery.\n";
                break;
            }

            records.push_back({type, std::move(key), std::move(value)});
        }

        return records;
    }

    uint32_t Wal::calculate_checksum(LogRecordType type, const std::string& key, const std::string& value) const 
    {
        // Simple FNV-1a checksum algorithm
        uint32_t hash = 2166136261u;
        
        auto hash_combine = [&hash](unsigned char c) 
            {
                hash ^= c;
                hash *= 16777619u;
            };
        
        hash_combine(static_cast<unsigned char>(type));
        
        for (char c : key) 
            hash_combine(static_cast<unsigned char>(c));
            
        for (char c : value) 
            hash_combine(static_cast<unsigned char>(c));
            
        return hash;
    }

} // namespace minidb