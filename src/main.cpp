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
#include <iostream>
#include <cstring>


int main() 
{
    std::cout << "Testing Pager...\n";

    minidb::Pager pager("test_data.db");

    if (pager.num_pages() == 0) 
    {
        std::cout << "Allocating Page 0...\n";
        pager.allocate_page();

        minidb::PageData data{};
        const char* message = "Hello, Page Storage!";
        std::memcpy(data.data(), message, std::strlen(message));

        pager.write_page(0, data);
        std::cout << "Written to Page 0: " << message << "\n";
    } 
    else 
    {
        std::cout << "Total pages in file: " << pager.num_pages() << "\n";
        minidb::PageData read_data{};
        if (pager.read_page(0, read_data)) 
        {
            std::cout << "Read from Page 0: " << reinterpret_cast<const char*>(read_data.data()) << "\n";
        }
    }

    return 0;
}