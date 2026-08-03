#pragma once

#include "minidb/Database.h"
#include <string>

namespace minidb 
{

    class Repl 
    {
    public:
        // Принимаем ссылку на базу данных, чтобы Repl не владел ею, а только использовал
        explicit Repl(Database& db);

        // Главный цикл
        void run();

    private:
        Database& db_;
        
        // Обработчик одной строки ввода
        bool executeCommand(const std::string& line);
    };

} // namespace minidb