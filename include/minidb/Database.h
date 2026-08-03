#pragma once

#include <string>
#include <unordered_map>
#include <optional>

namespace minidb 
{

    class Database 
    {
    public:
        explicit Database(std::string filename);

        void set(const std::string& key, const std::string& value);
        std::optional<std::string> get(const std::string& key) const;
        void remove(const std::string& key);

    private:
        std::unordered_map<std::string, std::string> data_;
        std::string filename_;

        void loadFromFile();
        void saveToFile() const;
    };

} // namespace minidb