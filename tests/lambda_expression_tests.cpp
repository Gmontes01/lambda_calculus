#include <string>
#include <iostream>
#include "../include/lambda_expression.h"

using namespace std;

namespace ExprTests {

    // ---- test configuration ----
    enum class Verbosity { QUIET, NORMAL, VERBOSE };
    static Verbosity verbosity = Verbosity::NORMAL;  

    int tests_passed = 0;
    int tests_failed = 0;

    // ---- helper functions ----
    lambda_expression* make_I() { return make_lambda("x", make_var("x")); }
    lambda_expression* make_K() { return make_lambda("x", make_lambda("y", make_var("x"))); }
    lambda_expression* make_KI() { return make_lambda("x", make_lambda("y", make_var("y"))); }
    lambda_expression* make_omega() {
        lambda_expression* self_app = make_lambda("x", make_app(make_var("x"), make_var("x")));
        return make_app(self_app, lambda_expression::deepcopy(self_app));
    }

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

        // Print details if verbose, or if the test failed in normal mode
        if (verbosity == Verbosity::VERBOSE || (!pass && verbosity == Verbosity::NORMAL)) {
            cout << "   Input:     " << input << "\n";
            cout << "   Operation: " << op << "\n";
            cout << "   Expected:  " << expected << "\n";
            cout << "   Actual:    " << actual << "\n";
            cout << "   ---------------------------------------\n";
        }
    }


    // ==========================================
    //                 TESTS
    // ==========================================

    // ---- to_string tests ----
    void test_to_string_variable() {
        lambda_expression* v = make_var("x");
        string actual = v->to_string();
        report(actual == "x", "to_string: variable", "make_var(x)", "to_string()", "x", actual);
        delete v;
    }

    void test_to_string_lambda() {
        lambda_expression* expr = make_lambda("x", make_var("x"));
        string actual = expr->to_string();
        report(actual == "(λx. x)", "to_string: lambda", "make_lambda(x, x)", "to_string()", "(λx. x)", actual);
        delete expr;
    }

    void test_to_string_application() {
        lambda_expression* expr = make_app(make_lambda("x", make_var("x")), make_var("y"));
        string actual = expr->to_string();
        report(actual == "((λx. x) y)", "to_string: application", "make_app(λx.x, y)", "to_string()", "((λx. x) y)", actual);
        delete expr;
    }

    void test_to_string_nested_lambda() {
        lambda_expression* expr = make_lambda("x", make_lambda("y", make_var("x")));
        string actual = expr->to_string();
        report(actual == "(λx. (λy. x))", "to_string: nested lambda", "make_lambda(x, λy.x)", "to_string()", "(λx. (λy. x))", actual);
        delete expr;
    }

    // ---- rename_free tests ----
    void test_rename_free_variable_match() {
        lambda_expression* v = make_var("x");
        v->rename_free("x", "x'");
        string actual = v->to_string();
        report(actual == "x'", "rename_free: variable match", "x", "rename_free(x, x')", "x'", actual);
        delete v;
    }

    void test_rename_free_variable_no_match() {
        lambda_expression* v = make_var("y");
        v->rename_free("x", "x'");
        string actual = v->to_string();
        report(actual == "y", "rename_free: variable no match", "y", "rename_free(x, x')", "y", actual);
        delete v;
    }

    void test_rename_free_application() {
        lambda_expression* expr = make_app(make_var("x"), make_var("y"));
        expr->rename_free("x", "z");
        string actual = expr->to_string();
        report(actual == "(z y)", "rename_free: application", "(x y)", "rename_free(x, z)", "(z y)", actual);
        delete expr;
    }

    void test_rename_free_lambda_shadow() {
        lambda_expression* expr = make_lambda("x", make_var("x"));
        expr->rename_free("x", "y");
        string actual = expr->to_string();
        report(actual == "(λy. y)", "rename_free: lambda shadow", "(λx. x)", "rename_free(x, y)", "(λy. y)", actual);
        delete expr;
    }

    void test_rename_free_lambda_no_shadow() {
        lambda_expression* expr = make_lambda("y", make_var("x"));
        expr->rename_free("x", "z");
        string actual = expr->to_string();
        report(actual == "(λy. z)", "rename_free: lambda no shadow", "(λy. x)", "rename_free(x, z)", "(λy. z)", actual);
        delete expr;
    }

    // ---- rename_bound tests ----
    void test_rename_bound_simple() {
        lambda_expression* expr = make_lambda("x", make_var("x"));
        expr->rename_bound("x", "y");
        string actual = expr->to_string();
        report(actual == "(λx. x)", "rename_bound: simple rename", "(λx. x)", "rename_bound(x, y)", "(λx. x)", actual);
        delete expr;
    }

    void test_rename_bound_stops_at_inner_shadow() {
        lambda_expression* inner = make_lambda("x", make_var("x"));
        lambda_expression* outer = make_lambda("x", inner);
        outer->rename("x", "y");
        string actual = outer->to_string();
        report(actual == "(λy. (λx. x))", "rename_bound: stops at inner shadow", "(λx. (λx. x))", "rename(x, y)", "(λy. (λx. x))", actual);
        delete outer;
    }

    // ---- substitute tests ----
    void test_substitute_variable_match() {
        lambda_expression* v = make_var("x");
        lambda_expression* repl = make_var("y");
        v->substitute("x", repl);
        string actual = v->to_string();
        report(actual == "y", "substitute: variable match", "x", "substitute(x, y)", "y", actual);
        delete v;
        delete repl;
    }

    void test_substitute_variable_no_match() {
        lambda_expression* v = make_var("z");
        lambda_expression* repl = make_var("y");
        v->substitute("x", repl);
        string actual = v->to_string();
        report(actual == "z", "substitute: variable no match", "z", "substitute(x, y)", "z", actual);
        delete v;
        delete repl;
    }

    void test_substitute_application() {
        lambda_expression* expr = make_app(make_var("x"), make_var("x"));
        lambda_expression* repl = make_var("y");
        expr->substitute("x", repl);
        string actual = expr->to_string();
        report(actual == "(y y)", "substitute: application", "(x x)", "substitute(x, y)", "(y y)", actual);
        delete expr;
        delete repl;
    }

    void test_substitute_under_lambda_no_capture() {
        lambda_expression* expr = make_lambda("y", make_var("x"));
        lambda_expression* repl = make_var("y");
        expr->substitute("x", repl);
        string actual = expr->to_string();
        report(actual == "(λy'. y)", "substitute: under lambda (no capture)", "(λy. x)", "substitute(x, y)", "(λy'. y)", actual);
        delete expr;
        delete repl;
    }

    void test_substitute_avoids_capture() {
        lambda_expression* inner = make_lambda("y", make_var("x"));
        lambda_expression* outer = make_lambda("x", inner);
        lambda_expression* repl = make_var("y");
        outer->substitute("x", repl);
        string actual = outer->to_string();
        bool is_avoided = (actual != "(λy. y)") && (actual.find("λy") != string::npos);
        report(is_avoided, "substitute: avoids capture", "(λx. (λy. x))", "substitute(x, y)", "(λy'. y)", actual);
        delete outer;
        delete repl;
    }

    void test_substitute_shadowing() {
        lambda_expression* expr = make_lambda("x", make_var("x"));
        lambda_expression* repl = make_var("y");
        expr->substitute("x", repl);
        string actual = expr->to_string();
        report(actual == "(λx. x)", "substitute: shadowing stops substitution", "(λx. x)", "substitute(x, y)", "(λx. x)", actual);
        delete expr;
        delete repl;
    }

    // ---- is_valid tests ----
    void test_is_valid_variable() {
        lambda_expression* v = make_var("x");
        bool valid = v->is_valid();
        report(valid, "is_valid: variable", "make_var(x)", "is_valid()", "true", valid ? "true" : "false");
        delete v;
    }

    void test_is_valid_lambda() {
        lambda_expression* expr = make_lambda("x", make_var("x"));
        bool valid = expr->is_valid();
        report(valid, "is_valid: lambda", "make_lambda(x, x)", "is_valid()", "true", valid ? "true" : "false");
        delete expr;
    }

    void test_is_valid_application() {
        lambda_expression* expr = make_app(make_var("x"), make_var("y"));
        bool valid = expr->is_valid();
        report(valid, "is_valid: application", "make_app(x, y)", "is_valid()", "true", valid ? "true" : "false");
        delete expr;
    }

    void test_is_valid_rejects_variable_with_children() {
        lambda_expression* v = make_var("x");
        v->expression_1 = make_var("y");
        bool valid = v->is_valid();
        report(!valid, "is_valid: rejects variable with child", "Variable with expression_1", "is_valid()", "false", valid ? "true" : "false");
        v->expression_1 = nullptr; // prevent double delete
        delete v;
    }

    void test_is_valid_rejects_application_with_name() {
        lambda_expression* expr = make_app(make_var("x"), make_var("y"));
        expr->name = "bad";
        bool valid = expr->is_valid();
        report(!valid, "is_valid: rejects application with name", "Application with name='bad'", "is_valid()", "false", valid ? "true" : "false");
        expr->name = "";
        delete expr;
    }

    void test_is_valid_rejects_lambda_without_body() {
        lambda_expression* expr = new lambda_expression();
        expr->type = NodeType::LAMBDA;
        expr->name = "x";
        expr->expression_1 = nullptr;
        expr->expression_2 = nullptr;
        bool valid = expr->is_valid();
        report(!valid, "is_valid: rejects lambda without body", "Lambda without expression_1", "is_valid()", "false", valid ? "true" : "false");
        delete expr;
    }

    // ---- is_redux tests ----
    void test_is_redux_true() {
        lambda_expression* expr = make_app(make_lambda("x", make_var("x")), make_var("y"));
        bool redux = expr->is_redux();
        report(redux, "is_redux: true", "((λx. x) y)", "is_redux()", "true", redux ? "true" : "false");
        delete expr;
    }

    void test_is_redux_false_not_lambda() {
        lambda_expression* expr = make_app(make_var("x"), make_var("y"));
        bool redux = expr->is_redux();
        report(!redux, "is_redux: false (not lambda)", "(x y)", "is_redux()", "false", redux ? "true" : "false");
        delete expr;
    }

    void test_is_redux_false_not_application() {
        lambda_expression* expr = make_lambda("x", make_var("x"));
        bool redux = expr->is_redux();
        report(!redux, "is_redux: false (not application)", "(λx. x)", "is_redux()", "false", redux ? "true" : "false");
        delete expr;
    }

    // ---- reduce tests ----
    void test_reduce_identity() {
        lambda_expression* expr = make_app(make_lambda("x", make_var("x")), make_var("y"));
        string input = expr->to_string();
        expr->reduce();
        string actual = expr->to_string();
        report(actual == "y", "reduce: identity", input, "reduce()", "y", actual);
        delete expr;
    }

    void test_reduce_const() {
        lambda_expression* expr = make_app(make_lambda("x", make_var("z")), make_var("y"));
        string input = expr->to_string();
        expr->reduce();
        string actual = expr->to_string();
        report(actual == "z", "reduce: constant function", input, "reduce()", "z", actual);
        delete expr;
    }

    void test_reduce_no_redex() {
        lambda_expression* expr = make_app(make_var("x"), make_var("y"));
        string input = expr->to_string();
        expr->reduce();
        string actual = expr->to_string();
        report(actual == input, "reduce: no redex (unchanged)", input, "reduce()", input, actual);
        delete expr;
    }

    void test_reduce_nested() {
        lambda_expression* expr = make_app(
            make_lambda("x", make_lambda("y", make_var("x"))),
            make_var("z")
        );
        string input = expr->to_string();
        expr->reduce();
        string actual = expr->to_string();
        report(actual == "(λy. z)", "reduce: nested lambda result", input, "reduce()", "(λy. z)", actual);
        delete expr;
    }

    void test_reduce_with_capture_avoidance() {
        lambda_expression* expr = make_app(
            make_lambda("x", make_lambda("y", make_var("x"))),
            make_var("y")
        );
        string input = expr->to_string();
        expr->reduce();
        string actual = expr->to_string();
        bool avoided = (actual != "(λy. y)") && (actual.find("λy") != string::npos);
        report(avoided, "reduce: capture avoidance", input, "reduce()", "(λy'. y)", actual);
        delete expr;
    }

    // ---- deepcopy tests ----
    void test_deepcopy_complex_shadowing() {
        lambda_expression* original = make_lambda("x", 
                                        make_lambda("x", 
                                            make_lambda("x", 
                                                make_lambda("x", make_var("x")))));
        
        lambda_expression* copy = lambda_expression::deepcopy(original);
        string original_str = original->to_string();
        string copy_str = copy->to_string();
        
        report(original_str == copy_str, "deepcopy: complex equality", original_str, "deepcopy()", copy_str, original_str);
        report(copy_str == "(λx. (λx. (λx. (λx. x))))", "deepcopy: complex format", "(λx. (λx. (λx. (λx. x))))", "to_string()", "(λx. (λx. (λx. (λx. x))))", copy_str);
        
        delete original;
        string after_delete = copy->to_string();
        report(after_delete == copy_str, "deepcopy: complex independence", copy_str, "delete original", copy_str, after_delete);
        delete copy;
    }

    void test_deepcopy_identity() {
        lambda_expression* original = make_lambda("x", make_var("x"));
        lambda_expression* copy = lambda_expression::deepcopy(original);
        string original_str = original->to_string();
        string copy_str = copy->to_string();
        
        report(original_str == copy_str, "deepcopy: identity equality", original_str, "deepcopy()", copy_str, original_str);
        report(copy_str == "(λx. x)", "deepcopy: identity format", "(λx. x)", "to_string()", "(λx. x)", copy_str);
        
        delete original;
        string after_delete = copy->to_string();
        report(after_delete == "(λx. x)", "deepcopy: identity independence", "(λx. x)", "delete original", "(λx. x)", after_delete);
        delete copy;
    }

    void test_deepcopy_single_var() {
        lambda_expression* original = make_var("x");
        lambda_expression* copy = lambda_expression::deepcopy(original);
        string original_str = original->to_string();
        string copy_str = copy->to_string();
        
        report(original_str == copy_str, "deepcopy: single var equality", original_str, "deepcopy()", copy_str, original_str);
        report(copy->name == "x", "deepcopy: single var name", "x", "check name", "x", copy->name);
        
        delete original;
        report(copy->name == "x", "deepcopy: single var independence", "x", "delete original", "x", copy->name);
        delete copy;
    }

    // ---- normal order reduction tests ----
    void test_normal_order_reduction() {
        lambda_expression* kite = make_KI();
        lambda_expression* omega_1 = make_omega();
        lambda_expression* x_var_1 = make_var("x");
        lambda_expression* test1 = make_app(make_app(kite, omega_1), x_var_1);
        string input1 = test1->to_string();
        
        for(int i = 0; i < 20; ++i) test1->reduce(); 
        string actual1 = test1->to_string();
        report(actual1 == "x", "test_lazy_KI_Omega_x", input1, "reduce() x20", "x", actual1);

        lambda_expression* kestrel = make_K();
        lambda_expression* identity = make_I();
        lambda_expression* omega_2 = make_omega();
        lambda_expression* x_var_2 = make_var("x");
        lambda_expression* test2 = make_app(make_app(make_app(kestrel, identity), omega_2), x_var_2);
        string input2 = test2->to_string();
        
        for(int i = 0; i < 20; ++i) test2->reduce();
        string actual2 = test2->to_string();
        report(actual2 == "x", "test_lazy_K_I_Omega_x", input2, "reduce() x20", "x", actual2);

        delete test1;
        delete test2;
    }

    void test_eval_divergent_omega_KI() {
        lambda_expression* kite = make_KI();
        lambda_expression* omega = make_omega();
        lambda_expression* x_var = make_var("x");
        lambda_expression* test = make_app(make_app(kite, omega), x_var);
        string input = test->to_string();
        
        try {
            for(int i = 0; i < 20; ++i) test->reduce(); 
        } catch (...) {}

        string actual = test->to_string();
        report(actual == "x", "eval: explicit KI Omega x", input, "Normal Order Reduce (x20)", "x", actual);
        delete test;
    }

    void test_eval_deep_redex() {
        lambda_expression* identity = make_I();
        lambda_expression* expr = make_lambda("x", make_app(identity, make_var("x")));
        string input = expr->to_string();
        
        expr->reduce(); 
        string actual = expr->to_string();
        
        report(actual == "(λx. x)", "eval: deep redex", input, "recursive reduce()", "(λx. x)", actual);
        delete expr;
    }

    void test_eval_shadowing_reduction() {
        lambda_expression* inner = make_lambda("x", make_var("x"));
        lambda_expression* outer = make_lambda("x", inner);
        lambda_expression* test = make_app(outer, make_var("y"));
        string input = test->to_string();
        
        test->reduce();
        string actual = test->to_string();
        
        report(actual == "(λx. x)", "eval: shadowing reduction", input, "reduce()", "(λx. x)", actual);
        delete test;
    }

    // ---- NAMESPACE ENTRY POINT ----
    pair<int,int> run_tests(bool verbose_flag, bool quiet_flag) {
        if (verbose_flag) verbosity = Verbosity::VERBOSE;
        else if (quiet_flag) verbosity = Verbosity::QUIET;
        else verbosity = Verbosity::NORMAL;

        tests_passed = 0;
        tests_failed = 0;

        if (verbosity != Verbosity::QUIET) {
            cout << "\n=== LAMBDA ENGINE TEST SUITE ===\n";
            string mode = (verbosity == Verbosity::VERBOSE) ? "VERBOSE" : "NORMAL";
            cout << "Running in " << mode << " mode\n\n";

            cout << "-- to_string --\n";
        }
        test_to_string_variable();
        test_to_string_lambda();
        test_to_string_application();
        test_to_string_nested_lambda();

        if (verbosity != Verbosity::QUIET) cout << "\n-- rename_free --\n";
        test_rename_free_variable_match();
        test_rename_free_variable_no_match();
        test_rename_free_application();
        test_rename_free_lambda_shadow();
        test_rename_free_lambda_no_shadow();

        if (verbosity != Verbosity::QUIET) cout << "\n-- rename_bound --\n";
        test_rename_bound_simple();
        test_rename_bound_stops_at_inner_shadow();

        if (verbosity != Verbosity::QUIET) cout << "\n-- substitute --\n";
        test_substitute_variable_match();
        test_substitute_variable_no_match();
        test_substitute_application();
        test_substitute_under_lambda_no_capture();
        test_substitute_avoids_capture();
        test_substitute_shadowing();

        if (verbosity != Verbosity::QUIET) cout << "\n-- is_valid --\n";
        test_is_valid_variable();
        test_is_valid_lambda();
        test_is_valid_application();
        test_is_valid_rejects_variable_with_children();
        test_is_valid_rejects_application_with_name();
        test_is_valid_rejects_lambda_without_body();

        if (verbosity != Verbosity::QUIET) cout << "\n-- is_redux --\n";
        test_is_redux_true();
        test_is_redux_false_not_lambda();
        test_is_redux_false_not_application();

        if (verbosity != Verbosity::QUIET) cout << "\n-- reduce --\n";
        test_reduce_identity();
        test_reduce_const();
        test_reduce_no_redex();
        test_reduce_nested();
        test_reduce_with_capture_avoidance();

        if (verbosity != Verbosity::QUIET) cout << "\n-- deepcopy --\n";
        test_deepcopy_complex_shadowing();
        test_deepcopy_identity();
        test_deepcopy_single_var();

        if (verbosity != Verbosity::QUIET) cout << "\n-- evaluations --\n";
        test_normal_order_reduction();
        test_eval_divergent_omega_KI();
        test_eval_deep_redex();
        test_eval_shadowing_reduction();

        // Final output is ALWAYS printed, even in QUIET mode
        if (verbosity != Verbosity::QUIET) cout << "\n=== RESULTS ===\n";
        else cout << "[AST Engine Tests] ";
        
        // Add these constants near the top of your function (or globally)
        const string RESET = "\033[0m";
        const string BOLD_GREEN = "\033[1;32m";
        const string BOLD_RED = "\033[1;31m";

        // Replace your final cout with this logic:
        if (tests_failed == 0) {
            // 100% Passed: Bold Green
            cout << BOLD_GREEN << tests_passed << " passed, " << tests_failed << " failed" << RESET << "\n";
        } else {
            // 1 or more Failed: Bold Red
            cout << BOLD_RED << tests_passed << " passed, " << tests_failed << " failed" << RESET << "\n";
        }

        return {tests_passed, tests_failed};
    }
}