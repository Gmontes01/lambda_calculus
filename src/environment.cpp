#include "../include/environment.h"
#include "../include/lambda_parser.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <new> // Required for std::bad_alloc

using namespace std;

// ==========================================
// STRING PARSING HELPERS
// ==========================================

string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool is_valid_var_name(const string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!isalpha(c) && c != '_') return false;
    }
    return true;
}

bool is_separator(const string& line) {
    string t = trim(line);
    if (t.length() < 3) return false;
    for (char c : t) {
        if (c != '*') return false;
    }
    return true;
}

string remove_comments(const string& source) {
    string result = "";
    bool in_comment = false;
    
    for (size_t i = 0; i < source.length(); ++i) {
        if (i + 2 < source.length() && source[i] == '"' && source[i+1] == '"' && source[i+2] == '"') {
            in_comment = !in_comment;
            i += 2;
        } else if (!in_comment) {
            result += source[i]; 
        }
    }
    
    if (in_comment) {
        throw runtime_error("Syntax Error: Unclosed comment detected. Missing a closing '\"\"\"'.");
    }
    
    return result;
}

// ==========================================
// THE ORCHESTRATOR
// ==========================================

ProgramEnvironment parse_program(const string& source_code) {
    string clean_code = remove_comments(source_code);
    
    istringstream stream(clean_code);
    string line;

    string assignment_block = "";
    string eval_block = "";
    bool in_eval_section = false;
    bool first_eval_line_checked = false;

    ProgramEnvironment env;
    env.evaluation_target = nullptr;
    env.target_name = "";

    // 1. Separate the text into Assignments, Title, and Evaluation Code
    while (getline(stream, line)) {
        if (!in_eval_section && is_separator(line)) {
            in_eval_section = true;
            continue;
        }
        
        if (in_eval_section) {
            string trimmed = trim(line);
            if (trimmed.empty()) continue; 

            if (!first_eval_line_checked) {
                first_eval_line_checked = true;
                
                if (trimmed.find('\\') == string::npos && 
                    trimmed.find("λ") == string::npos && 
                    trimmed.find('(') == string::npos &&
                    trimmed.find('.') == string::npos &&
                    trimmed.find("lambda") == string::npos) {
                    
                    env.target_name = trimmed; 
                    continue; 
                }
            }
            
            eval_block += line + " ";
        } else {
            assignment_block += line + " ";
        }
    }

    // 2. Parse the Assignments (Statements ending in ';')
    size_t start = 0;
    size_t end = assignment_block.find(';');
    
    while (end != string::npos) {
        string stmt = trim(assignment_block.substr(start, end - start));
        
        if (!stmt.empty()) {
            size_t eq_pos = stmt.find('=');
            if (eq_pos == string::npos) throw runtime_error("Syntax Error: Missing '=' -> " + stmt);

            string var_name = trim(stmt.substr(0, eq_pos));
            string expr_str = trim(stmt.substr(eq_pos + 1));

            if (!is_valid_var_name(var_name)) throw runtime_error("Syntax Error: Invalid variable name '" + var_name + "'");
            if (expr_str.empty()) throw runtime_error("Syntax Error: Missing expression for '" + var_name + "'");

            if (env.variables.find(var_name) != env.variables.end()) {
                throw runtime_error("Syntax Error: Naming collision. The variable '" + var_name + "' is already defined.");
            }

            lambda_expression* ast = parse_string(expr_str);
            if (!ast) throw runtime_error("Parse Error: Failed to build AST for '" + var_name + "'.");

            // Substitute in exact declaration order using const auto&
            for (const string& prev_name : env.declaration_order) {
                ast->substitute(prev_name, env.variables[prev_name]);
            }
            env.variables[var_name] = ast;
            env.declaration_order.push_back(var_name);
        }
        
        start = end + 1;
        end = assignment_block.find(';', start);
    }

    string dangling = trim(assignment_block.substr(start));
    if (!dangling.empty()) throw runtime_error("Syntax Error: Missing semicolon ';' after -> " + dangling);

    // 3. Parse the Final Evaluation Block
    eval_block = trim(eval_block);
    if (!eval_block.empty()) {
        env.evaluation_target = parse_string(eval_block);
        if (!env.evaluation_target) {
            throw runtime_error("Parse Error: Failed to build AST for the final evaluation block.");
        }
        
        for (const string& prev_name : env.declaration_order) {
            env.evaluation_target->substitute(prev_name, env.variables[prev_name]);
        }
    }

    return env;
}

void ProgramEnvironment::clear() {
    for (auto& pair : variables) {
        delete pair.second;
    }
    variables.clear();
    declaration_order.clear();
    delete evaluation_target;
    evaluation_target = nullptr;
}

void ProgramEnvironment::combine_environments(ProgramEnvironment* pe) {
    if (pe == nullptr) return;

    // Use const auto& to prevent -Wrange-loop-construct temporary copies
    for (const auto& pair : pe->variables) {
        const string& var_name = pair.first;
        lambda_expression* incoming_ast = pair.second;

        if (this->variables.find(var_name) != this->variables.end()) {
            throw runtime_error("Environment Merge Error: Naming collision detected. The variable '" + 
                                var_name + "' already exists.");
        }

        this->variables[var_name] = incoming_ast;
        this->declaration_order.push_back(var_name);
    }

    pe->variables.clear();
    pe->declaration_order.clear();
}

void ProgramEnvironment::reduce_variables() {
    // Use auto& to directly modify the map values without temporary copies
    for (auto& env_var : this->variables) {
        const string& var_name = env_var.first;
        lambda_expression* ast = env_var.second;

        if (ast == nullptr) continue; 

        lambda_expression* copy = lambda_expression::deepcopy(ast);

        try {
            copy->reduce_total();
            delete ast;
            env_var.second = copy;
            
        } catch (const std::bad_alloc& e) {
            delete copy;
            cerr << "[Warning] Variable '" << var_name 
                 << "' triggered Exponential Blowup (ran out of RAM) during pre-computation. "
                 << "It has been safely stored unreduced." << endl;
                 
        } catch (const runtime_error& e) {
            delete copy;
            cerr << "[Warning] Variable '" << var_name 
                 << "' failed to reduce during pre-computation (" << e.what() << "). "
                 << "It has been safely stored unreduced." << endl;
        }
    }
}

lambda_expression* ProgramEnvironment::express(lambda_expression* target) {
    if (target == nullptr) return nullptr;

    switch (target->type) {
        case NodeType::VARIABLE: 
            return target;
            
        case NodeType::LAMBDA: 
        case NodeType::APPLICATION: {
            // Use const auto& to avoid -Wrange-loop-construct warnings
            for (const auto& env_var : this->variables) {
                if (*target == *(env_var.second)) {
                    delete target;
                    return make_var(env_var.first);
                }
            }
            if (target->type == NodeType::LAMBDA) {
                target->expression_1 = express(target->expression_1);
            } else { // APPLICATION
                target->expression_1 = express(target->expression_1);
                target->expression_2 = express(target->expression_2);
            }
            return target;
        }
        default: 
            return target;
    }
}