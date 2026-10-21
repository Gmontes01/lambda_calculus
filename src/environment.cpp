#include "../include/environment.h"
#include "../include/lambda_parser.h"


#include <iostream>
#include <sstream>
#include <stdexcept>
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
        // Look ahead to see if the next three characters are quotes
        if (i + 2 < source.length() && source[i] == '"' && source[i+1] == '"' && source[i+2] == '"') {
            in_comment = !in_comment; // Toggle the state (Open -> Close, or Close -> Open)
            i += 2; // Skip the next two quotes so we don't process them again
        } 
        // Only keep characters if we are NOT inside a comment
        else if (!in_comment) {
            result += source[i]; 
        }
    }
    
    if (in_comment) {
        // We use '\"' to safely print quotation marks in the error message
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

            // ==========================================
            // COLLISION CHECK ADDED HERE
            // ==========================================
            if (env.variables.find(var_name) != env.variables.end()) {
                throw runtime_error("Syntax Error: Naming collision. The variable '" + var_name + "' is already defined.");
            }

            lambda_expression* ast = parse_string(expr_str);
            if (!ast) throw runtime_error("Parse Error: Failed to build AST for '" + var_name + "'.");

            for (const pair<string, lambda_expression*>& pair : env.variables) {
                ast->substitute(pair.first, pair.second);
            }
            env.variables[var_name] = ast;
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
        
        for (const pair<string, lambda_expression*>& pair : env.variables) {
            env.evaluation_target->substitute(pair.first, pair.second);
        }
    }

    return env;
}


void ProgramEnvironment::combine_environments(ProgramEnvironment* pe) {
    if (pe == nullptr) return;

    for (const pair<string, lambda_expression*>& pair : pe->variables) {
        const string& var_name = pair.first;
        lambda_expression* incoming_ast = pair.second;

        // Check if the variable name already exists in OUR environment
        if (this->variables.find(var_name) != this->variables.end()) {
            throw runtime_error("Environment Merge Error: Naming collision detected. The variable '" + 
                                var_name + "' already exists.");
        }

        // If no collision, add it to our environment
        this->variables[var_name] = incoming_ast;
    }

    // Safety Step: Clear the incoming environment's variables so it doesn't 
    // accidentally double-delete the AST pointers when it gets destroyed later.
    pe->variables.clear();
}

/**
 * @brief Iterates through all stored variables in the environment and 
 * fully beta-reduces them to their Normal Form.
 * * This is a critical optimization pass. By ensuring all variables are in 
 * normal form, the compiler can use O(1) Alpha Equivalence structural 
 * checks during the resugaring (express) phase, rather than O(n^2) tree reductions.
 */
void ProgramEnvironment::reduce_variables() {
    // Iterate over every variable bound in the current environment
    for (pair<const string, lambda_expression*>& env_var : this->variables) {
        const string& var_name = env_var.first;
        lambda_expression* ast = env_var.second;

        lambda_expression* copy = lambda_expression::deepcopy(ast);

        if (ast == nullptr) continue; 

        try {
            // Attempt to pre-evaluate the AST completely.
            
            copy->reduce_total();
            delete ast;
            env_var.second = copy;
            
        } catch (const std::bad_alloc& e) {
            // SWALLOW MEMORY EXPLOSION: Catches heap exhaustion anywhere in the AST.
            cerr << "[Warning] Variable '" << var_name 
                 << "' triggered Exponential Blowup (ran out of RAM) during pre-computation. "
                 << "It has been safely stored unreduced." << endl;
                 
        } catch (const runtime_error& e) {
            // SWALLOW DIVERGENCE: Preserves lazy evaluation by allowing infinite 
            // combinators to exist in the environment as long as they aren't executed.
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
            // Variables are already fully folded, no need to check or recurse
            return target;
            
        case NodeType::LAMBDA: 
        case NodeType::APPLICATION: {
            // 1. GREEDY PATTERN MATCHING (Maximal Munch)
            // Check if this entire subtree is beta-equivalent to a saved variable.
            for (const pair<string, lambda_expression*>& env_var : this->variables) {
                
                // Dereference the stored pointer to trigger your alpha_equivalence overload
                if (*target == *(env_var.second)) {
                    
                    // Memory Safety: Since we are replacing this entire complex subtree,
                    // we must delete it to free the heap memory (which triggers its 
                    // recursive destructor), and return a brand new, clean variable node.
                    delete target;
                    return make_var(env_var.first);
                }
            }
            // 2. RECURSION
            // If the whole block didn't match anything, step one level deeper.
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