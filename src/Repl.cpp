#include "minidb/Repl.h"
#include <iostream>
#include <sstream>

namespace minidb 
{

    Repl::Repl(Database& db) 
        : db_(db) 
    {
    }

    void Repl::run() 
    {
        std::cout << "MiniDB v0.2.0 (REPL)\n";
        std::cout << "Commands: SET  , GET , DELETE , EXIT\n";

        std::string line;
        while (true) 
        {
            std::cout << "db> ";
            if (!std::getline(std::cin, line)) 
            {
                break;
            }

            if (!executeCommand(line)) 
            {
                break; // Если executeCommand вернул false, выходим из цикла
            }
        }
    }

    bool Repl::executeCommand(const std::string& line) 
    {
        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command.empty()) 
        {
            return true;
        }

        if (command == "EXIT" || command == "QUIT") 
        {
            std::cout << "Bye!\n";
            return false; // Сигнал к завершению REPL
        } 
        else if (command == "SET") 
        {
            std::string key, value;
            iss >> key;
            std::getline(iss >> std::ws, value); 
            
            if (key.empty() || value.empty()) 
            {
                std::cout << "Error: Usage: SET  \n";
            } 
            else 
            {
                db_.set(key, value);
                std::cout << "OK\n";
            }
        } 
        else if (command == "GET") 
        {
            std::string key;
            iss >> key;
            
            if (key.empty()) 
            {
                std::cout << "Error: Usage: GET \n";
            } 
            else 
            {
                auto result = db_.get(key);
                if (result) 
                {
                    std::cout << *result << "\n";
                } 
                else 
                {
                    std::cout << "(nil)\n";
                }
            }
        } 
        else if (command == "DELETE") 
        {
            std::string key;
            iss >> key;
            
            if (key.empty()) 
            {
                std::cout << "Error: Usage: DELETE \n";
            } 
            else 
            {
                db_.remove(key);
                std::cout << "OK\n";
            }
        } 
        else 
        {
            std::cout << "Error: Unknown command '" << command << "'\n";
        }
        
        return true;
    }

} // namespace minidb