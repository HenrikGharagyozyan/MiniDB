#include "minidb/core/Database.h"
#include "Repl.h"

int main() 
{
    minidb::Database db("database.db");
    minidb::Repl repl(db);
    
    repl.run();

    return 0;
}