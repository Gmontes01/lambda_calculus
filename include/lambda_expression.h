#pragma once
#include <string>

// 1. Types
enum class NodeType { VARIABLE, LAMBDA, APPLICATION };

// 2. Struct Declaration (Signatures Only)
struct lambda_expression {
    NodeType type;
    std::string name;

    lambda_expression* expression_1;
    lambda_expression* expression_2;

    static lambda_expression* deepcopy(const lambda_expression* node);
    bool is_valid() const;
    bool is_redux();
    
    void rename(const std::string& old_name, const std::string& new_name);
    void rename_free(const std::string& old_name, const std::string& new_name);
    void rename_bound(const std::string& old_name, const std::string& new_name);
    
    bool occurs_free_in(const std::string& var) const;
    std::string generate_fresh_name(const std::string& base, const lambda_expression* replacement) const;
    void substitute(const std::string& var, const lambda_expression* replacement);
    
    void reduce_total(int MAX_DEPTH = 1000);


    bool reduce();
    std::string to_string() const;
    
    ~lambda_expression();

    bool operator==(const lambda_expression& other) const;//alpha equivalence up to combinator variable names
    bool beta_equivalence(const lambda_expression& other, int max_depth) const;//beta equivalence
};

// 3. Factory Function Signatures
lambda_expression* make_var(const std::string& name);
lambda_expression* make_lambda(const std::string& bound_var, lambda_expression* body);
lambda_expression* make_app(lambda_expression* func, lambda_expression* arg);