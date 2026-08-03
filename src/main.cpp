#include "minidb/Database.h"
#include "minidb/Repl.h"
#include <iostream>
#include <sstream>

int main() 
{
    std::cout << "MiniDB v0.1.0\n";
    std::cout << "Commands: SET  , GET , DELETE , EXIT\n";
    
    minidb::Database db("database.db");

    std::string line;
    while (true) 
    {
        std::cout << "db> ";
        
        // Read the entire input line
        if (!std::getline(std::cin, line)) 
        {
            break; // Exit on Ctrl+D (EOF)
        }

        // Use stringstream to split the line into words
        std::istringstream iss(line);
        std::string command;
        iss >> command; // Read the first word into `command`

        if (command.empty()) 
        {
            continue; // Ignore empty input (just Enter)
        }

        // Process commands
        if (command == "EXIT" || command == "QUIT") 
        {
            std::cout << "Bye!\n";
            break;
        } 
        else if (command == "SET") 
        {
            std::string key, value;
            iss >> key;
            // std::ws consumes leading whitespace, write the rest of the line to `value`
            // This allows storing values with spaces, e.g.: SET role C++ Developer
            std::getline(iss >> std::ws, value); 
            
            if (key.empty() || value.empty()) 
            {
                std::cout << "Error: Usage: SET  \n";
            } 
            else 
            {
                db.set(key, value);
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
                auto result = db.get(key);
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
                db.remove(key);
                std::cout << "OK\n";
            }
        } 
        else 
        {
            std::cout << "Error: Unknown command '" << command << "'\n";
        }
    }

    return 0;
}