#include "minidb/Database.h"
#include "minidb/Repl.h"

int main() 
{
    minidb::Database db("database.db");
    minidb::Repl repl(db);
    
    repl.run();

    return 0;
}