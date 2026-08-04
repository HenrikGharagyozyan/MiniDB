#include "minidb/Database.h"
#include "minidb/Page.h"
#include <utility>


namespace minidb 
{

    // Специальный маркер для удаленных ключей
    constexpr const char* TOMBSTONE = "__DELETED__";


    Database::Database(std::string filename) 
        : filename_(std::move(filename)) 
    {
        pager_ = std::make_unique<Pager>(filename_);

        // Если файл новый/пустой, сразу инициализируем первую страницу (root)
        if (pager_->num_pages() == 0) 
        {
            PageId root_page_id = pager_->allocate_page();
            PageData raw_page{};
            Page page(raw_page);
            page.init();
            pager_->write_page(root_page_id, raw_page);
        }
    }

    void Database::set(const std::string& key, const std::string& value)
    {
        if (pager_->num_pages() == 0) 
            return;

        PageId last_page_id = pager_->num_pages() - 1;
        PageData raw_page{};
        pager_->read_page(last_page_id, raw_page);

        Page page(raw_page);
        
        // Если на текущей странице нет места для записи, выделяем новую
        if (!page.insert_record(key, value)) 
        {
            last_page_id = pager_->allocate_page();
            PageData new_raw_page{};
            Page new_page(new_raw_page);
            new_page.init();
            
            // Вставляем в новую страницу
            new_page.insert_record(key, value);
            pager_->write_page(last_page_id, new_raw_page);
        } 
        else 
        {
            // Записываем обновленную страницу обратно на диск
            pager_->write_page(last_page_id, raw_page);
        }
    }

    std::optional<std::string> Database::get(const std::string& key) 
    {
        if (pager_->num_pages() == 0) 
            return std::nullopt;

        // Ищем с конца к началу (Append-Only: последняя запись - самая свежая)
        for (int32_t page_id = pager_->num_pages() - 1; page_id >= 0; --page_id) 
        {
            PageData raw_page{};
            pager_->read_page(static_cast<PageId>(page_id), raw_page);
            
            Page page(raw_page);
            
            // Сканируем записи на странице тоже с конца
            for (int32_t rec_idx = page.num_records() - 1; rec_idx >= 0; --rec_idx) 
            {
                auto rec = page.get_record(static_cast<uint16_t>(rec_idx));
                if (rec && rec->first == key) 
                {
                    if (rec->second == TOMBSTONE) 
                    {
                        return std::nullopt; // Ключ был удален
                    }
                    return rec->second; // Нашли актуальное значение!
                }
            }
        }
        
        return std::nullopt;
    }

    void Database::remove(const std::string& key) 
    {
        // Вставляем запись с маркером удаления
        set(key, TOMBSTONE);
    }

} // namespace minidb   