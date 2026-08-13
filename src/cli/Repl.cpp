#include "Repl.h"
#include "minidb/sql/Tokenizer.h"
#include "minidb/sql/Parser.h"

#include <iostream>
#include <sstream>


namespace minidb 
{

    Repl::Repl(Database& db) 
        : db_(db)
        , current_txn_(nullptr)
        , executor_(db_, catalog_)
    {
        db_.set_lock_manager(&lock_manager_);
        txn_manager_.set_lock_manager(&lock_manager_);
    }

    void Repl::run() 
    {
        std::cout << "MiniDB v0.3.0 (SQL Engine & Transactions)\n";
        std::cout << "SQL:  CREATE TABLE ..., INSERT INTO ..., SELECT * FROM ...\n";
        std::cout << "KV:   SET <k> <v>, GET <k>, DELETE <k>\n";
        std::cout << "Txn:  BEGIN, COMMIT, ROLLBACK, VACUUM, EXIT\n";

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

        if (command.empty()) 
        {
            return true;
        }

        if (!command.empty() && command.back() == ';') 
        {
            command.pop_back();
        }

        std::string upper_cmd = command;
        for (auto& c : upper_cmd) 
            c = toupper(c);

        if (upper_cmd == "EXIT" || upper_cmd == "QUIT") 
        {
            std::cout << "Bye!\n";
            return false; 
        }

        try
        {
            // --- SQL COMMANDS HANDLER ---
            if (upper_cmd == "CREATE" || upper_cmd == "INSERT" || upper_cmd == "SELECT") 
            {
                Tokenizer tokenizer(line);
                auto tokens = tokenizer.tokenize();

                Parser parser(tokens);
                auto stmt = parser.parse();

                // Pass the active transaction to the SQL executor!
                executor_.execute(stmt.get(), current_txn_.get());
                return true;
            }

            // --- TRANSACTION & KV HANDLERS ---
            if (upper_cmd == "BEGIN") 
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
            else if (upper_cmd == "COMMIT") 
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
            else if (upper_cmd == "ROLLBACK") 
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
            else if (upper_cmd == "VACUUM") 
            {
                if (current_txn_) 
                {
                    std::cout << "Error: Cannot run VACUUM inside a transaction.\n";
                }
                else
                {
                    txn_manager_.vacuum();
                    std::cout << "Vacuum completed. Old MVCC versions cleaned up.\n";
                }
            }
            else if (upper_cmd == "SET") 
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
                    db_.set(key, value, current_txn_.get());
                    std::cout << "OK\n";
                }
            } 
            else if (upper_cmd == "GET") 
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
            else if (upper_cmd == "DELETE") 
            {
                std::string key;
                iss >> key;
                
                if (key.empty()) 
                {
                    std::cout << "Error: Usage: DELETE <k>\n";
                } 
                else 
                {
                    db_.remove(key, current_txn_.get());
                    std::cout << "OK\n";
                }
            } 
            else 
            {
                std::cout << "Error: Unknown command '" << command << "'\n";
            }
        }
        catch (const std::exception& e) 
        {
            std::cout << "Error: " << e.what() << "\n";
            
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