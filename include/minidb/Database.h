#pragma once

#include "minidb/Pager.h"
#include "minidb/BufferPoolManager.h"
#include "minidb/Wal.h"
#include "minidb/BTree.h"

#include <string>
#include <optional>
#include <memory>


namespace minidb 
{

    class Database 
    {
    public:
        explicit Database(std::string filename);
        ~Database() = default;

        void set(const std::string& key, const std::string& value);
        std::optional<std::string> get(const std::string& key);
        void remove(const std::string& key);

    private:
        std::string filename_;
       
        std::unique_ptr<Pager> pager_;
        std::unique_ptr<BufferPoolManager> bpm_;
        std::unique_ptr<Wal> wal_;
        std::unique_ptr<BTree> btree_;
    };

} // namespace minidb