#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "../include/environment.h"
#include "../include/lambda_parser.h"

using namespace std;

// ==========================================
// REPL HELPERS
// ==========================================

static string repl_trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool repl_is_valid_var_name(const string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!isalpha(c) && c != '_') return false;
    }
    return true;
}

static void print_help() {
    cout << "========================================================\n";
    cout << "   Lambda Calculus Interactive REPL\n";
    cout << "--------------------------------------------------------\n";
    cout << "   Assign variable :  NAME = \\x. x\n";
    cout << "   Evaluate expr   :  (\\x. x) y\n";
    cout << "   Built-in cmds   :\n";
    cout << "     env           - List all saved variables\n";
    cout << "     load <file>   - Load definitions from a .lc file\n";
    cout << "     clear         - Clear all variables in memory\n";
    cout << "     help          - Show this help message\n";
    cout << "     exit / quit   - Exit the REPL\n";
    cout << "========================================================\n";
}

// ==========================================
// STANDALONE REPL MAIN
// ==========================================

int main(int argc, char* argv[]) {
    ProgramEnvironment session_env;

    print_help();

    // Optional: If a file was passed on startup (e.g., ./repl.exe preload.lc), load it first
    if (argc >= 2) {
        string filepath = argv[1];
        ifstream file(filepath);
        if (file.is_open()) {
            stringstream buffer;
            buffer << file.rdbuf();
            file.close();
            try {
                ProgramEnvironment file_env = parse_program(buffer.str());
                file_env.reduce_variables();
                session_env.combine_environments(&file_env);
                file_env.clear();
                cout << "\n[Preloaded environment from '" << filepath << "']\n";
            } catch (const exception& e) {
                cerr << "\n[Startup Load Error] " << e.what() << "\n";
            }
        } else {
            cerr << "\n[Warning] Could not open startup file '" << filepath << "'\n";
        }
    }

    string line;
    while (true) {
        cout << "\n>>> ";
        if (!getline(cin, line)) break; // Exit on EOF (Ctrl+Z / Ctrl+D)

        string input = repl_trim(line);
        if (input.empty()) continue;

        // Strip trailing semicolon if typed
        if (input.back() == ';') {
            input.pop_back();
            input = repl_trim(input);
        }

        // ------------------------------------------
        // 1. BUILT-IN COMMANDS
        // ------------------------------------------
        if (input == "exit" || input == "quit") {
            break;
        } 
        else if (input == "help") {
            print_help();
            continue;
        } 
        else if (input == "clear") {
            session_env.clear();
            cout << "[Environment cleared]" << endl;
            continue;
        } 
        else if (input == "env") {
            if (session_env.variables.empty()) {
                cout << "[Environment is empty]" << endl;
            } else {
                for (const string& name : session_env.declaration_order) {
                    cout << "  " << name << " = " << session_env.variables[name]->to_string() << endl;
                }
            }
            continue;
        } 
        else if (input.rfind("load ", 0) == 0) {
            string filepath = repl_trim(input.substr(5));
            ifstream file(filepath);
            if (!file.is_open()) {
                cerr << "[Error] Could not open file '" << filepath << "'" << endl;
                continue;
            }
            stringstream buffer;
            buffer << file.rdbuf();
            file.close();

            try {
                ProgramEnvironment file_env = parse_program(buffer.str());
                file_env.reduce_variables();
                session_env.combine_environments(&file_env);
                file_env.clear();
                cout << "[Loaded variables from '" << filepath << "']" << endl;
            } catch (const exception& e) {
                cerr << "[Load Error] " << e.what() << endl;
            }
            continue;
        }

        // ------------------------------------------
        // 2. ASSIGNMENT OR EVALUATION
        // ------------------------------------------
        try {
            size_t eq_pos = input.find('=');

            // CASE A: Variable Assignment (e.g., TRUE = \t f. t)
            if (eq_pos != string::npos) {
                string var_name = repl_trim(input.substr(0, eq_pos));
                string expr_str = repl_trim(input.substr(eq_pos + 1));

                if (!repl_is_valid_var_name(var_name)) {
                    cout << "[Syntax Error] Invalid variable name '" << var_name << "' (letters and '_' only)." << endl;
                    continue;
                }
                if (expr_str.empty()) {
                    cout << "[Syntax Error] Missing expression after '='." << endl;
                    continue;
                }

                lambda_expression* ast = parse_string(expr_str);
                if (!ast) continue;

                // Substitute existing variables in declaration order
                for (const string& prev_name : session_env.declaration_order) {
                    if (prev_name != var_name) {
                        ast->substitute(prev_name, session_env.variables[prev_name]);
                    }
                }

                // Pre-reduce the variable to Normal Form if possible
                lambda_expression* copy = lambda_expression::deepcopy(ast);
                try {
                    copy->reduce_total();
                    delete ast;
                    ast = copy;
                } catch (...) {
                    delete copy;
                    // Keep unreduced AST if it diverges (like Omega)
                }

                // Overwrite if it already exists, otherwise record in declaration_order
                if (session_env.variables.find(var_name) != session_env.variables.end()) {
                    delete session_env.variables[var_name];
                } else {
                    session_env.declaration_order.push_back(var_name);
                }

                session_env.variables[var_name] = ast;
                cout << var_name << " = " << ast->to_string() << endl;
            }
            // CASE B: Immediate Expression Evaluation (e.g., NOT TRUE)
            else {
                lambda_expression* ast = parse_string(input);
                if (!ast) continue;

                // Substitute all known session variables into the expression
                for (const string& prev_name : session_env.declaration_order) {
                    ast->substitute(prev_name, session_env.variables[prev_name]);
                }

                try {
                    ast->reduce_total();
                    lambda_expression* simplified = lambda_expression::deepcopy(ast);
                    lambda_expression* expressed = session_env.express(simplified);

                    string raw_str = ast->to_string();
                    string exp_str = expressed->to_string();

                    if (exp_str != raw_str) {
                        cout << exp_str << "   [raw: " << raw_str << "]" << endl;
                    } else {
                        cout << exp_str << endl;
                    }

                    delete expressed;
                } catch (const exception& e) {
                    cerr << "[Evaluation Error] " << e.what() << endl;
                }

                delete ast;
            }
        } catch (const exception& e) {
            cerr << "[Error] " << e.what() << endl;
        }
    }

    session_env.clear();
    return 0;
}