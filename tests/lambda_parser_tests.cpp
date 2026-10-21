#include <string>
#include <iostream>
#include <sstream>
#include "../include/lambda_parser.h"

using namespace std;

namespace ParserTests {

    // ---- test configuration ----
    enum class Verbosity { QUIET, NORMAL, VERBOSE };
    static Verbosity verbosity = Verbosity::NORMAL;  

    int tests_passed = 0;
    int tests_failed = 0;

    // ---- Unified Reporter ----
    void report(bool pass, const string& name, const string& input, 
                const string& op, const string& expected, const string& actual) {
        if (pass) {
            ++tests_passed;
            if (verbosity != Verbosity::QUIET) cout << "PASS " << name << "\n";
        } else {
            ++tests_failed;
            if (verbosity != Verbosity::QUIET) cout << "FAIL " << name << "\n";
        }

        if (verbosity == Verbosity::VERBOSE || (!pass && verbosity == Verbosity::NORMAL)) {
            cout << "   Input:     " << input << "\n";
            cout << "   Operation: " << op << "\n";
            cout << "   Expected:  " << expected << "\n";
            cout << "   Actual:    " << actual << "\n";
            cout << "   ---------------------------------------\n";
        }
    }

    // ---- Parser Test Helper ----
    void check_parse(const string& input, const string& expected, const string& test_name) {
        
        // 1. Setup the dummy buffer and save the original terminal output
        stringstream dummy_buffer;
        streambuf* original_cout = cout.rdbuf();

        // 2. If in quiet mode, redirect all 'cout' to the black hole
        if (verbosity == Verbosity::QUIET) {
            cout.rdbuf(dummy_buffer.rdbuf());
        }

        // 3. Run the engine
        lambda_expression* expr = parse_string(input); 
        
        // 4. Instantly restore terminal output so our test reporter can print
        if (verbosity == Verbosity::QUIET) {
            cout.rdbuf(original_cout);
        }
        
        if (expr == nullptr) {
            report(expected == "ERROR", test_name, input, "parse_string()", expected, "nullptr (Caught Error)");
        } else {
            string actual = expr->to_string();
            report(actual == expected, test_name, input, "parse_string()", expected, actual);
            delete expr;
        }
    }

    // ==========================================
    // PARSER TEST SUITE
    // ==========================================

    void test_parser_basics() {
        check_parse("x", "x", "parse: single variable");
        check_parse("\\x. x", "(λx. x)", "parse: simple lambda (backslash)");
        check_parse("lambda y . y", "(λy. y)", "parse: simple lambda (keyword)");
        check_parse("x y", "(x y)", "parse: simple application");
    }

    void test_parser_associativity() {
        check_parse("x y z", "((x y) z)", "parse: left-associative application");
        check_parse("a b c d", "(((a b) c) d)", "parse: deep left-associativity");
        check_parse("x (y z)", "(x (y z))", "parse: parentheses grouping right");
        check_parse("(x y) z", "((x y) z)", "parse: parentheses grouping left");
    }

    void test_parser_greedy_scope() {
        check_parse("\\x. a b", "(λx. (a b))", "parse: greedy lambda scope");
        check_parse("(\\x. a) b", "((λx. a) b)", "parse: parenthesis bounds scope");
    }

    void test_parser_currying() {
        check_parse("lambda x y . x", "(λx. (λy. x))", "parse: double curry (K combinator)");
        check_parse("\\x y z . (x z) (y z)", "(λx. (λy. (λz. ((x z) (y z)))))", "parse: triple curry (S combinator)");
    }

    void test_parser_combinators() {
        check_parse("(\\x. x x) (\\x. x x)", "((λx. (x x)) (λx. (x x)))", "parse: Omega combinator");
        check_parse("\\f. (\\x. f (x x)) (\\x. f (x x))", 
                    "(λf. ((λx. (f (x x))) (λx. (f (x x)))))", 
                    "parse: Y combinator");
    }

    void test_parser_errors() {
        check_parse("(x y", "ERROR", "error: missing closing parenthesis");
        check_parse("\\x y z", "ERROR", "error: missing dot in lambda");
        check_parse("\\ . x", "ERROR", "error: lambda missing bound variable");
        check_parse("()", "ERROR", "error: empty parenthesis");
    }

    // ---- NAMESPACE ENTRY POINT ----
    pair<int,int> run_tests(bool verbose_flag, bool quiet_flag) {
        if (verbose_flag) verbosity = Verbosity::VERBOSE;
        else if (quiet_flag) verbosity = Verbosity::QUIET;
        else verbosity = Verbosity::NORMAL;

        tests_passed = 0;
        tests_failed = 0;

        if (verbosity != Verbosity::QUIET) {
            cout << "\n=== PARSER TEST SUITE ===\n";
            string mode = (verbosity == Verbosity::VERBOSE) ? "VERBOSE" : "NORMAL";
            cout << "Running in " << mode << " mode\n\n";

            cout << "-- Basics --\n";
        }
        test_parser_basics();

        if (verbosity != Verbosity::QUIET) cout << "\n-- Associativity & Scope --\n";
        test_parser_associativity();
        test_parser_greedy_scope();

        if (verbosity != Verbosity::QUIET) cout << "\n-- Currying & Combinators --\n";
        test_parser_currying();
        test_parser_combinators();

        if (verbosity != Verbosity::QUIET) cout << "\n-- Error Handling (Should not crash) --\n";
        test_parser_errors();

        if (verbosity != Verbosity::QUIET) cout << "\n=== RESULTS ===\n";
        else cout << "[Parser Tests] ";

        const string RESET = "\033[0m";
        const string BOLD_GREEN = "\033[1;32m";
        const string BOLD_RED = "\033[1;31m";

        if (tests_failed == 0) {
            cout << BOLD_GREEN << tests_passed << " passed, " << tests_failed << " failed" << RESET << "\n";
        } else {
            cout << BOLD_RED << tests_passed << " passed, " << tests_failed << " failed" << RESET << "\n";
        }

        return {tests_passed, tests_failed};
    }
}