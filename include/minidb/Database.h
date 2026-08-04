#pragma once

#include "minidb/Pager.h"

#include <string>
#include <optional>
#include <memory>


namespace minidb 
{
    class Database 
    {
    public:
        explicit Database(std::string filename);

        void set(const std::string& key, const std::string& value);
        std::optional<std::string> get(const std::string& key);
        void remove(const std::string& key);

    private:
        std::string filename_;
        std::unique_ptr<Pager> pager_;
    };

} // namespace minidb