#pragma once
#include <string>
#include <vector>
#include "lambda_expression.h"

// Public API for the parser
std::vector<std::string> tokenize(const std::string& str);
lambda_expression* parse_string(const std::string& str);