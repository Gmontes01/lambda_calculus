#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "../include/environment.h"

using namespace std;

namespace EnvironmentTests {

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

    // ---- File IO Helper ----
    string load_file(const string& filepath) {
        ifstream file(filepath);
        if (!file.is_open()) return "";
        stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // ==========================================
    // ENVIRONMENT TEST SUITE
    // ==========================================

    struct FileTestCase {
        string filepath;
        bool should_pass;
        string expected_error_substring;
    };

    void test_all_files() {
        vector<FileTestCase> test_files = {
            {"tests/test_envs/valid_basic.lc", true, ""},
            {"tests/test_envs/valid_infinite_uncalled_variable.lc", true, ""},
            {"tests/test_envs/valid_comments.lc", true, ""}, // <--- Add this
            {"tests/test_envs/err_missing_equals.lc", false, "Missing '='"},
            {"tests/test_envs/err_invalid_varname.lc", false, "Invalid variable name"},
            {"tests/test_envs/err_missing_expr.lc", false, "Missing expression"},
            {"tests/test_envs/err_missing_semi.lc", false, "Missing semicolon ';'"},
            {"tests/test_envs/err_parser_paren.lc", false, "Failed to build AST"},
            {"tests/test_envs/err_parser_var.lc", false, "Failed to build AST"},
            {"tests/test_envs/err_parser_dot.lc", false, "Failed to build AST"},
            {"tests/test_envs/err_eval_block.lc", false, "Failed to build AST for the final evaluation block"},
            {"tests/test_envs/err_unclosed_comment.lc", false, "Unclosed comment detected"},
            {"tests/test_envs/err_variable_collision.lc", false, "Syntax Error: Naming collision. The variable 'TRUE' is already defined."},
            {"tests/test_envs/valid_currying.lc", true, ""}
        };
        
        // ... remainder of the function ...

        for (const FileTestCase& test : test_files) {
            string code = load_file(test.filepath);
            
            if (code.empty()) {
                report(false, "File read: " + test.filepath, test.filepath, "load_file()", "File contents", "File not found or empty.");
                continue;
            }

            // Suppress terminal output so the parser's internal couts don't spam our test results
            stringstream dummy_buffer;
            streambuf* original_cout = cout.rdbuf();
            if (verbosity == Verbosity::QUIET || !test.should_pass) {
                cout.rdbuf(dummy_buffer.rdbuf());
            }

            bool threw_error = false;
            string error_msg = "";
            ProgramEnvironment env;

            try {
                env = parse_program(code);
            } catch (const runtime_error& e) {
                threw_error = true;
                error_msg = e.what();
            }

            // Restore terminal output
            if (verbosity == Verbosity::QUIET || !test.should_pass) {
                cout.rdbuf(original_cout);
            }

            if (test.should_pass) {
                report(!threw_error, "parse valid: " + test.filepath, test.filepath, "parse_program()", "Successful parse", threw_error ? string("Exception: ") + error_msg : "Parsed without exceptions");
            } else {
                bool expected_msg = (error_msg.find(test.expected_error_substring) != string::npos);
                string expected_desc = "Throws error containing '" + test.expected_error_substring + "'";
                string actual_desc = threw_error ? "Threw: " + error_msg : "Did not throw exception";
                
                report(threw_error && expected_msg, "parse invalid: " + test.filepath, test.filepath, "parse_program()", expected_desc, actual_desc);
            }

            // Memory Cleanup (safely handles incomplete envs)
            for (auto& pair : env.variables) delete pair.second;
            delete env.evaluation_target;
        }
    }

    void test_combine_environments_success() {
        ProgramEnvironment base_env;
        base_env.variables["VAR_A"] = make_var("a");

        ProgramEnvironment incoming_env;
        incoming_env.variables["VAR_B"] = make_var("b");

        try {
            base_env.combine_environments(&incoming_env);
            
            bool passed = (base_env.variables.size() == 2) && (incoming_env.variables.size() == 0);
            report(passed, "combine: successful merge", "Base(VAR_A) + Incoming(VAR_B)", "combine_environments()", "Base has 2, Incoming empty", "Base has " + to_string(base_env.variables.size()) + ", Incoming has " + to_string(incoming_env.variables.size()));

            for (auto& pair : base_env.variables) delete pair.second;

        } catch (const exception& e) {
            report(false, "combine: successful merge", "Base + Incoming", "combine_environments()", "No exception", e.what());
        }
    }

    void test_combine_environments_collision() {
        ProgramEnvironment base_env;
        base_env.variables["VAR_A"] = make_var("a");

        ProgramEnvironment incoming_env;
        incoming_env.variables["VAR_A"] = make_var("a_prime");

        bool threw_error = false;
        string error_msg = "";

        try {
            base_env.combine_environments(&incoming_env);
        } catch (const runtime_error& e) {
            threw_error = true;
            error_msg = e.what();
        }

        bool expected_msg = (error_msg.find("VAR_A") != string::npos);
        report(threw_error && expected_msg, "combine: collision detection", "Base(VAR_A) + Incoming(VAR_A)", "combine_environments()", "Throws runtime_error mentioning VAR_A", threw_error ? "Threw: " + error_msg : "Did not throw");

        for (auto& pair : base_env.variables) delete pair.second;
        for (auto& pair : incoming_env.variables) delete pair.second;
    }

    void test_express_function() {
        ProgramEnvironment env;
        
        // Setup mock environment with standard combinators
        env.variables["ID"] = make_lambda("x", make_var("x"));
        env.variables["TRUE"] = make_lambda("t", make_lambda("f", make_var("t")));
        env.variables["FALSE"] = make_lambda("t", make_lambda("f", make_var("f")));

        // 1. Edge Case: Null Pointer
        lambda_expression* null_res = env.express(nullptr);
        report(null_res == nullptr, "express: nullptr safety", "nullptr", "express()", "nullptr", null_res == nullptr ? "nullptr" : "Not nullptr");

        // 2. Edge Case: Variable Node (Should remain completely unchanged)
        lambda_expression* var_target = make_var("z");
        lambda_expression* var_res = env.express(var_target);
        report(var_res->to_string() == "z", "express: variable pass-through", "z", "express()", "z", var_res->to_string());
        delete var_res; // Clean up because express didn't replace/delete it

        // 3. Success: Direct Alpha-Equivalent Lambda Match
        lambda_expression* id_target = make_lambda("a", make_var("a")); // \a.a
        lambda_expression* id_res = env.express(id_target);
        report(id_res->type == NodeType::VARIABLE && id_res->name == "ID", 
               "express: exact lambda match", "\\a.a", "express()", "ID", id_res->to_string());
        delete id_res; // Clean up the newly generated variable node

        // 4. Failure Case: No Match in Environment
        lambda_expression* no_match_target = make_lambda("x", make_var("y")); // \x.y
        lambda_expression* no_match_res = env.express(no_match_target);
        bool no_match_success = (no_match_res->type == NodeType::LAMBDA);
        report(no_match_success, "express: no match found", "\\x.y", "express()", "Unchanged AST", no_match_res->type == NodeType::LAMBDA ? "Unchanged AST" : "Changed AST");
        delete no_match_res;

        // 5. Success: Nested Application Recursive Match
        // Target: (\a.\b.a) (\c.\d.d) -> Should evaluate to TRUE FALSE
        lambda_expression* app_target = make_app(
            make_lambda("a", make_lambda("b", make_var("a"))), 
            make_lambda("c", make_lambda("d", make_var("d")))
        );
        lambda_expression* app_res = env.express(app_target);
        
        bool app_success = (app_res->type == NodeType::APPLICATION && 
                            app_res->expression_1->type == NodeType::VARIABLE && app_res->expression_1->name == "TRUE" && 
                            app_res->expression_2->type == NodeType::VARIABLE && app_res->expression_2->name == "FALSE");
        
        report(app_success, "express: nested recursion match", "(\\a.\\b.a) (\\c.\\d.d)", "express()", "Application(TRUE, FALSE)", app_res->to_string());
        delete app_res;

        // Clean up environment
        for (auto& pair : env.variables) {
            delete pair.second;
        }
    }

    // ---- NAMESPACE ENTRY POINT ----
    pair<int,int> run_tests(bool verbose_flag, bool quiet_flag) {
        if (verbose_flag) verbosity = Verbosity::VERBOSE;
        else if (quiet_flag) verbosity = Verbosity::QUIET;
        else verbosity = Verbosity::NORMAL;

        tests_passed = 0;
        tests_failed = 0;

        if (verbosity != Verbosity::QUIET) {
            cout << "\n=== ENVIRONMENT TEST SUITE ===\n";
            string mode = (verbosity == Verbosity::VERBOSE) ? "VERBOSE" : "NORMAL";
            cout << "Running in " << mode << " mode\n\n";
        }

        // Run Tests
        test_all_files();
        test_combine_environments_success();
        test_combine_environments_collision();
        test_express_function();

        if (verbosity != Verbosity::QUIET) cout << "\n=== RESULTS ===\n";
        else cout << "[Environment Tests] ";

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