#pragma once

#include "minidb/Pager.h"
#include <cstdint>
#include <string>
#include <optional>
#include <utility>

namespace minidb 
{

    #pragma pack(push, 1) // Disable compiler padding so the structure is exactly 6 bytes
    struct PageHeader 
    {
        uint16_t page_type{0};          // 0 = Leaf/Data Page
        uint16_t num_records{0};        // Number of records
        uint16_t free_space_offset{6};  // Offset from where a new record can be written (starts immediately after the header)
    };
    #pragma pack(pop)

    class Page 
    {
    public:
        explicit Page(PageData& raw_data);

    // Initialize a new empty page
    void init();

    // Insert a key-value pair. Returns false if there is no room on the page
    bool insert_record(const std::string& key, const std::string& value);

    // Get a record by its sequential index
    std::optional<std::pair<std::string, std::string>> get_record(uint16_t index) const;

    // Helper methods

    private:
        PageData& data_;

        PageHeader* header();
        const PageHeader* header() const;
    };

} // namespace minidb