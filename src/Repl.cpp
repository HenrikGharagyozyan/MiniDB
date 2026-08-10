#include "minidb/Repl.h"
#include <iostream>
#include <sstream>

namespace minidb 
{

    Repl::Repl(Database& db) 
        : db_(db), current_txn_(nullptr)
    {
        db_.set_lock_manager(&lock_manager_);
        txn_manager_.set_lock_manager(&lock_manager_);
    }

    void Repl::run() 
    {
        std::cout << "MiniDB v0.2.0 (REPL with Transactions)\n";
        std::cout << "Commands: SET <k> <v>, GET <k>, DELETE <k>, BEGIN, COMMIT, ROLLBACK, EXIT\n";

        std::string line;
        while (true) 
        {
            // Change the prompt when we are inside a transaction
            if (current_txn_) 
            {
                std::cout << "db (txn:" << current_txn_->get_transaction_id() << ")> ";
            } 
            else 
            {
                std::cout << "db> ";
            }

            if (!std::getline(std::cin, line)) 
            {
                break;
            }

            if (!executeCommand(line)) 
            {
                break; // If executeCommand returned false, exit the loop
            }
        }
    }

    bool Repl::executeCommand(const std::string& line) 
    {
        std::istringstream iss(line);
        std::string command;
        iss >> command;

        for (auto& c : command) 
            c = toupper(c);

        if (command.empty()) 
        {
            return true;
        }

        if (command == "EXIT" || command == "QUIT") 
        {
            std::cout << "Bye!\n";
            return false; // Signal to terminate the REPL
        }
        
        try
        {
            if (command == "BEGIN") 
            {
                if (current_txn_) 
                {
                    std::cout << "Error: Transaction already active.\n";
                } 
                else 
                {
                    current_txn_ = txn_manager_.begin();
                    std::cout << "Transaction " << current_txn_->get_transaction_id() << " started.\n";
                }
            }
            else if (command == "COMMIT") 
            {
                if (!current_txn_) 
                {
                    std::cout << "Error: No active transaction.\n";
                } 
                else 
                {
                    txn_manager_.commit(current_txn_.get());
                    std::cout << "Transaction committed.\n";
                    current_txn_ = nullptr;
                }
            }
            else if (command == "ROLLBACK") 
            {
                if (!current_txn_) 
                {
                    std::cout << "Error: No active transaction.\n";
                } 
                else 
                {
                    txn_manager_.abort(current_txn_.get(), &db_);
                    std::cout << "Transaction rolled back.\n";
                    current_txn_ = nullptr;
                }
            }
            else if (command == "SET") 
            {
                std::string key, value;
                iss >> key;
                std::getline(iss >> std::ws, value); 
                
                if (key.empty() || value.empty()) 
                {
                    std::cout << "Error: Usage: SET <k> <v>\n";
                } 
                else 
                {
                    // Pass the current transaction
                    db_.set(key, value, current_txn_.get());
                    std::cout << "OK\n";
                }
            } 
            else if (command == "GET") 
            {
                std::string key;
                iss >> key;
                
                if (key.empty()) 
                {
                    std::cout << "Error: Usage: GET <k>\n";
                } 
                else 
                {
                    auto result = db_.get(key, current_txn_.get());
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
                    std::cout << "Error: Usage: DELETE <k>\n";
                } 
                else 
                {
                    // Pass the current transaction
                    db_.remove(key, current_txn_.get());
                    std::cout << "OK\n";
                }
            } 
            else 
            {
                std::cout << "Error: Unknown command '" << command << "'\n";
            }
        }
        catch (const std::runtime_error& e) 
        {
            std::cout << "Error: " << e.what() << "\n";
            
            // Automatic ROLLBACK on deadlock/timeout
            if (current_txn_) 
            {
                std::cout << "Automatically aborting transaction " << current_txn_->get_transaction_id() << "...\n";
                txn_manager_.abort(current_txn_.get(), &db_);
                current_txn_ = nullptr;
            }
        }
        
        return true;
    }

} // namespace minidb