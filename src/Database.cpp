#include "minidb/Database.h"
#include <fstream>
#include <iostream>
#include <utility>

namespace minidb 
{

    Database::Database(std::string filename) 
        : filename_(std::move(filename)) 
    {
        loadFromFile();
    }

    void Database::set(const std::string& key, const std::string& value) 
    {
        data_[key] = value;
        saveToFile(); 
    }

    std::optional<std::string> Database::get(const std::string& key) const 
    {
        auto it = data_.find(key);
        if (it != data_.end()) 
            return it->second;

        return std::nullopt;
    }

    void Database::remove(const std::string& key) 
    {
        if (data_.erase(key) > 0) 
            saveToFile(); 
    }

    void Database::loadFromFile() 
    {
        std::ifstream file(filename_);
        if (!file.is_open()) 
            return;

        std::string line;
        while (std::getline(file, line)) 
        {
            size_t delimiter_pos = line.find('=');
            if (delimiter_pos != std::string::npos) 
            {
                std::string key = line.substr(0, delimiter_pos);
                std::string value = line.substr(delimiter_pos + 1);
                data_[key] = value;
            }
        }
    }

    void Database::saveToFile() const 
    {
        std::ofstream file(filename_); 
        if (!file.is_open()) 
        {
            std::cerr << "Error: Could not open file for writing: " << filename_ << "\n";
            return;
        }

        for (const auto& [key, value] : data_) 
            file << key << "=" << value << "\n";
    }

} // namespace minidb   