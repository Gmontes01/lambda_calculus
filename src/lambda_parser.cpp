#include "../include/lambda_parser.h"
#include <unordered_set>
#include <stdexcept>
#include <iostream>

using namespace std;

// ==========================================
// STEP 1: THE LEXER (Tokenization)
// ==========================================

/**
 * @brief Converts a continuous string of text into discrete, actionable tokens.
 * * The Lexer scans byte-by-byte. It ignores whitespace, treats special characters 
 * as standalone delimiters, and continuously accumulates alphanumeric characters 
 * into complete variable names (e.g., separating "var_x" from ".").
 * * @param str The raw input string from the user or file.
 * @return A vector of clean strings representing syntax tokens.
 */
vector<string> tokenize(const string& str) {
    vector<string> tokens;
    string curr_word = "";
    
    // The exact characters that trigger a split in Lambda Calculus grammar
    unordered_set<char> delimiters = {' ', '.', '(', ')', ';', '\\'};

    for (char c : str) {
        if (delimiters.count(c)) {
            // If we were building a variable name, save it before processing the delimiter
            if (!curr_word.empty()) {
                tokens.push_back(curr_word);
                curr_word = "";
            }
            // Discard spaces, but keep mathematical symbols
            if (c != ' ') tokens.push_back(string(1, c));
        } else {
            // Not a delimiter; accumulate into the current variable name
            curr_word += c;
        }
    }
    
    // Flush any remaining characters in the buffer
    if (!curr_word.empty()) tokens.push_back(curr_word);
    
    return tokens;
}

// ==========================================
// STEP 2: THE PARSER (Recursive Descent)
// ==========================================

/**
 * @brief The engine that translates a linear list of tokens into a hierarchical AST.
 * * This class uses Recursive Descent. It consumes tokens from left to right and 
 * relies on the C++ call stack (recursion) to handle nested parenthesis and scope, 
 * eliminating the need to manually manage data stacks and booleans.
 */
class LambdaParser {
private:
    vector<string> tokens;
    size_t pos;

    /** @brief Looks at the current token without consuming it. */
    string peek() { return (pos >= tokens.size()) ? "" : tokens[pos]; }
    
    /** @brief Consumes the current token and moves the internal cursor forward. */
    string advance() { return (pos >= tokens.size()) ? "" : tokens[pos++]; }

    /**
     * @brief Parses a single self-contained unit (Atom).
     * * An atom is either a variable, an entire parenthesized expression, or a lambda.
     * This function contains the logic for Greedy Lambda Scope and Automatic Currying.
     */
    lambda_expression* parse_atom() {
        string t = peek();

        // Rule 1: Parentheses override scope. Recurse entirely until the matching ')'
        if (t == "(") {
            advance();
            lambda_expression* expr = parse_expression();
            if (advance() != ")") throw runtime_error("Parse Error: Missing closing ')'");
            return expr;
        } 
        
        // Rule 2: Lambda functions
        else if (t == "lambda" || t == "λ" || t == "\\") {
            advance();
            
            // Automatic Currying: Collect all variables (e.g., \x y z .)
            vector<string> bound_vars;
            while (peek() != "." && peek() != "") bound_vars.push_back(advance());
            
            if (bound_vars.empty()) throw runtime_error("Parse Error: Lambda missing variable");
            if (advance() != ".") throw runtime_error("Parse Error: Expected '.' after variables");

            // Greedy Scope: The body consumes everything else remaining in the expression
            lambda_expression* body = parse_expression();
            
            // Unpack the curried variables inside-out to build the nested tree
            for (int i = bound_vars.size() - 1; i >= 0; --i) {
                body = make_lambda(bound_vars[i], body);
            }
            return body;
        } 
        
        // Rule 3: End of statement marker
        else if (t == ";") {
            advance();
            return nullptr;
        }
        
        // Rule 4: If it's none of the above, it must be a standard variable
        else {
            advance();
            return make_var(t);
        }
    }

public:
    LambdaParser(const vector<string>& t) : tokens(t), pos(0) {}

    /**
     * @brief Parses a full expression, enforcing Left-Associativity.
     * * This function continuously applies new arguments to the existing expression
     * from left to right. E.g., reading "x y z" results in ((x y) z).
     */
    lambda_expression* parse_expression() {
        if (peek() == "") return nullptr;
        
        // Obtain the base function
        lambda_expression* expr = parse_atom();

        // Continuously consume remaining atoms and apply them to the base
        while (peek() != "" && peek() != ")" && peek() != ";") {
            lambda_expression* next_arg = parse_atom();
            if (next_arg) expr = make_app(expr, next_arg);
        }
        
        return expr;
    }
};

// ==========================================
// STEP 3: THE WRAPPER (Public API)
// ==========================================

/**
 * @brief Safely orchestrates the Lexing and Parsing processes.
 * * Provides a unified entry point for the rest of the program to convert strings 
 * into ASTs. Wraps the engine in a try-catch block to prevent syntax errors 
 * from causing fatal C++ segmentation faults.
 */
lambda_expression* parse_string(const string& str) {
    try {
        // Pass 1: Lexing
        vector<string> tokens = tokenize(str);
        if (tokens.empty()) return nullptr;

        // Pass 2: Parsing
        LambdaParser parser(tokens);
        return parser.parse_expression();
        
    } catch (const exception& e) {
        // Log syntax errors gracefully
        cout << e.what() << endl;
        return nullptr;
    }
}