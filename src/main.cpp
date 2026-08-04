// #include "minidb/Database.h"
// #include "minidb/Repl.h"

// int main() 
// {
//     minidb::Database db("database.db");
//     minidb::Repl repl(db);
    
//     repl.run();

//     return 0;
// }




// ======================
// TEMP TEST
// ======================
#include "minidb/Pager.h"
#include "minidb/Page.h"
#include <iostream>

int main() 
{
    std::cout << "Testing Page Storage Engine...\n";

    minidb::Pager pager("page_test.db");

    if (pager.num_pages() == 0) 
    {
        std::cout << "Allocating Page 0 and inserting records...\n";
        pager.allocate_page();

        minidb::PageData raw_page{};
        minidb::Page page(raw_page);
        page.init();

        page.insert_record("name", "Henrik");
        page.insert_record("role", "C++ Developer");
        page.insert_record("city", "Vardenis");

        pager.write_page(0, raw_page);
        std::cout << "Saved 3 records to Page 0.\n";
    } 
    else 
    {
        std::cout << "Reading Page 0 from disk...\n";
        minidb::PageData raw_page{};
        pager.read_page(0, raw_page);

        minidb::Page page(raw_page);
        std::cout << "Records count on page: " << page.num_records() << "\n";

        for (uint16_t i = 0; i < page.num_records(); ++i) 
        {
            auto rec = page.get_record(i);
            if (rec) 
            {
                std::cout << "  [" << i << "] " << rec->first << " => " << rec->second << "\n";
            }
        }
    }

    return 0;
}