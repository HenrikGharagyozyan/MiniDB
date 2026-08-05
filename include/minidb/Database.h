#pragma once

#include "minidb/Pager.h"
#include "minidb/Wal.h"

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
        std::unique_ptr<Wal> wal_;

        // Вспомогательный метод для физической записи на страницу
        void write_to_page(const std::string& key, const std::string& value);
    };

} // namespace minidb