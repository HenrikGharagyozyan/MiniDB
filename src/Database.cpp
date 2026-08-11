#include "minidb/Database.h"
#include "minidb/Page.h"
#include "minidb/LogRecord.h"
#include "minidb/TransactionManager.h"
#include "minidb/TupleMeta.h"

#include <utility>
#include <cstring>


namespace minidb 
{

    // Special marker for deleted keys
    constexpr const char* TOMBSTONE = "__DELETED__";


    Database::Database(std::string filename) 
        : filename_(std::move(filename)) 
    {
        pager_ = std::make_unique<Pager>(filename_);

        // Create a 10-page pool in memory
        bpm_ = std::make_unique<BufferPoolManager>(10, *pager_);

        // The WAL file is named after the database plus ".log"
        wal_ = std::make_unique<Wal>(filename_ + ".log");

        PageId root_page_id = 0;

        // If the file is new/empty, initialize the Meta Page and the first root page
        if (pager_->num_pages() == 0) 
        {
            PageId meta_page_id;
            // allocate_page() is hidden inside new_page()
            PageData* meta_raw = bpm_->new_page(&meta_page_id); 
            
            PageData* root_raw = bpm_->new_page(&root_page_id); 
            
            Page root_page(*root_raw);
            root_page.init(NodeType::LEAF, true);
            
            // Write the root ID into page zero
            std::memcpy(meta_raw->data(), &root_page_id, sizeof(PageId));

            // Unpin the pages (is_dirty = true because we modified them)
            bpm_->unpin_page(root_page_id, true);
            bpm_->unpin_page(meta_page_id, true);
        }
        else 
        {
            // The database already exists. Read the Meta Page (Page 0) to find the root
            // Read the meta page through the BPM
            PageData* meta_raw = bpm_->fetch_page(0);
            std::memcpy(&root_page_id, meta_raw->data(), sizeof(PageId));
            
            // Only read, no changes were made
            bpm_->unpin_page(0, false);
        }

        // Initialize the B-tree with the correct root
        btree_ = std::make_unique<BTree>(*bpm_, root_page_id);

        // --- Crash Recovery Stage ---
        auto uncommitted_records = wal_->recover();
        if (!uncommitted_records.empty())
        {
            for (const auto& record : uncommitted_records)
            {
                if (record.type == LogRecordType::SET) 
                {
                    TupleMeta meta(0, 0, false);
                    btree_->insert(record.key, encode_value(meta, record.value));
                } 
                else if (record.type == LogRecordType::DELETE) 
                {
                    TupleMeta meta(0, 0, true);
                    btree_->insert(record.key, encode_value(meta, TOMBSTONE));
                }
            }
            // After successfully applying all records to pages, clear the WAL
            wal_->clear();
        }
    }

    void Database::set(const std::string& key, const std::string& value, Transaction* txn)
    {
        // 2PL: Acquire Exclusive lock (X-Lock)
        if (txn != nullptr && lock_mgr_ != nullptr) 
        {
            if (!txn->holds_exclusive_lock(key)) 
            {
                if (!lock_mgr_->lock_exclusive(key)) 
                {
                    throw std::runtime_error("Lock wait timeout exceeded");
                }
                txn->add_exclusive_lock(key);
            }
        }

        lsn_t undo_lsn = 0;

        if (txn != nullptr && txn_mgr_ != nullptr) 
        {
            // Берем самую последнюю ФИЗИЧЕСКУЮ версию, чтобы привязать к ней новую
            auto raw_opt = btree_->get(key);
            if (raw_opt) 
            {
                auto [old_meta, old_val] = decode_value(*raw_opt);
                
                // Генерируем LSN и сохраняем старую версию в лог
                undo_lsn = txn_mgr_->get_next_lsn();
                auto log_record = std::make_shared<LogRecord>(
                    txn->get_transaction_id(), undo_lsn, UndoLogType::UPDATE, key, old_val, old_meta
                );
                
                txn->add_log_record(log_record);           // Для ROLLBACK
                txn_mgr_->add_undo_record(log_record);     // Для MVCC Version Chain
            }
            else 
            {
                // Версии еще нет (INSERT)
                txn->add_log_record(std::make_shared<LogRecord>(
                    txn->get_transaction_id(), 0, UndoLogType::INSERT, key
                ));
            }
        }

        // Создаем метаданные для НОВОЙ версии
        TupleMeta new_meta;
        if (txn != nullptr) 
        {
            new_meta.txn_id = txn->get_transaction_id();
            new_meta.undo_lsn = undo_lsn; // ССЫЛКА НА СТАРУЮ ВЕРСИЮ!
            new_meta.is_deleted = false;
        }

        // Упаковываем и сохраняем!
        std::string encoded_value = encode_value(new_meta, value);

        // First write strictly to the WAL and flush to disk
        wal_->append_set(key, value);
        wal_->flush();

        // Only after that modify the BTree
        btree_->insert(key, encoded_value);
    }

    std::optional<std::string> Database::get(const std::string& key, Transaction* txn) 
    {
        // В MVCC ЧИТАТЕЛИ БОЛЬШЕ НЕ БЕРУТ S-Lock!
        // Девиз MVCC в действии: Readers do not block writers.

        // Получаем самую свежую физическую версию из дерева
        auto raw_opt = btree_->get(key);
        if (!raw_opt) 
        {
            return std::nullopt; // Ключа вообще нет в базе
        }

        auto [meta, val] = decode_value(*raw_opt);

        // Если транзакции нет (системный вызов), возвращаем как есть
        if (txn == nullptr) 
        {
            if (meta.is_deleted || val == TOMBSTONE) return std::nullopt;
            return val;
        }

        // --- Visibility Engine: Идем по Version Chain ---
        auto current_meta = meta;
        auto current_val = val;
        
        // Пока текущая версия нам НЕ видна
        while (!txn->get_read_view().is_visible(current_meta.txn_id)) 
        {
            if (current_meta.undo_lsn == 0) 
            {
                // Старых версий больше нет, значит для нашей транзакции ключа не существует
                return std::nullopt;
            }

            // Идем в глобальный Undo Log за предыдущей версией!
            auto undo_record = txn_mgr_->get_undo_record(current_meta.undo_lsn);
            if (!undo_record) 
            {
                return std::nullopt; // Лог был очищен Vacuum'ом
            }

            current_meta = undo_record->get_old_meta();
            current_val = undo_record->get_old_value();
        }

        // Нашли видимую версию
        if (current_meta.is_deleted || current_val == TOMBSTONE) 
        {
            return std::nullopt;
        }
        
        return current_val;
    }

    void Database::remove(const std::string& key, Transaction* txn) 
    {
        // 2PL: Acquire Exclusive lock (X-Lock)
        if (txn != nullptr && lock_mgr_ != nullptr) 
        {
            if (!txn->holds_exclusive_lock(key)) 
            {
                if (txn->holds_shared_lock(key)) 
                {
                    lock_mgr_->unlock_shared(key);
                    txn->remove_shared_lock(key);
                }
                
                if (!lock_mgr_->lock_exclusive(key)) 
                {
                    throw std::runtime_error("Lock wait timeout exceeded");
                }
                txn->add_exclusive_lock(key);
            }
        }

        lsn_t undo_lsn = 0;

        if (txn != nullptr && txn_mgr_ != nullptr) 
        {
            auto raw_opt = btree_->get(key);
            if (raw_opt) 
            {
                auto [old_meta, old_val] = decode_value(*raw_opt);
                
                undo_lsn = txn_mgr_->get_next_lsn();
                auto log_record = std::make_shared<LogRecord>(
                    txn->get_transaction_id(), undo_lsn, UndoLogType::DELETE, key, old_val, old_meta
                );
                
                txn->add_log_record(log_record);
                txn_mgr_->add_undo_record(log_record);
            }
        }

        // Формируем метаданные для удаленной записи (Tombstone)
        TupleMeta new_meta;
        if (txn != nullptr) 
        {
            new_meta.txn_id = txn->get_transaction_id();
            new_meta.undo_lsn = undo_lsn;
            new_meta.is_deleted = true; // <-- Флаг удаления
        }

        std::string encoded_value = encode_value(new_meta, TOMBSTONE);

        wal_->append_delete(key);
        wal_->flush();

        btree_->insert(key, encoded_value);
    }

    std::string Database::encode_value(const TupleMeta& meta, const std::string& val) 
    {
        std::string res;
        // Резервируем память под размер структуры + размер строки
        res.resize(sizeof(TupleMeta) + val.size());
        
        // Копируем байты структуры в начало
        std::memcpy(res.data(), &meta, sizeof(TupleMeta));
        // Копируем саму строку после структуры
        std::memcpy(res.data() + sizeof(TupleMeta), val.data(), val.size());
        
        return res;
    }

    std::pair<TupleMeta, std::string> Database::decode_value(const std::string& raw_val) 
    {
        TupleMeta meta;
        // Читаем структуру из начала строки
        std::memcpy(&meta, raw_val.data(), sizeof(TupleMeta));
        
        // Отрезаем всё остальное — это наше реальное значение
        std::string val = raw_val.substr(sizeof(TupleMeta));
        
        return {meta, val};
    }

} // namespace minidb   