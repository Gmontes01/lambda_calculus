#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "lambda_expression.h"

/**
 * @brief Represents the complete state of a parsed Lambda Calculus program.
 */
struct ProgramEnvironment {
    std::unordered_map<std::string, lambda_expression*> variables;
    std::vector<std::string> declaration_order; 
    lambda_expression* evaluation_target = nullptr; 
    std::string target_name = "";

    void clear();
    void combine_environments(ProgramEnvironment* pe);
    void reduce_variables();
    lambda_expression* express(lambda_expression* target);
};

/**
 * @brief Parses a complete program containing variable assignments, an optional title, 
 * and a final multi-line expression.
 */
ProgramEnvironment parse_program(const std::string& source_code);