#include "minidb/Database.h"
#include <iostream>

int main() 
{
    std::cout << "MiniDB v0.1.0\n";
    
    minidb::Database db("database.db");

    auto old_name = db.get("name");
    if (old_name) 
    {
        std::cout << "Loaded from disk: name = " << *old_name << "\n";
    } 
    else 
    {
        std::cout << "Database is empty. Setting initial values.\n";
        db.set("name", "Henrik");
        db.set("project", "MiniDB");
    }

    return 0;
}