#include "../include/lambda_expression.h"

#include <unordered_map>
#include <string>

#include <stdexcept>
#include <iostream>

#include <new> // Required to catch std::bad_alloc

using namespace std;

/**
 * @brief Creates a deep, independent copy of an Abstract Syntax Tree (AST).
 * * In C++, if you assign one pointer to another, they both point to the same memory.
 * If one is deleted, the other becomes a dangling pointer. This function recursively
 * allocates new memory for every node in the tree to ensure complete independence.
 * * @param node A pointer to the root of the tree you want to copy.
 * @return A pointer to the root of the newly allocated tree.
 */
lambda_expression* lambda_expression::deepcopy(const lambda_expression* node) {
    if (node == nullptr) return nullptr;
    
    lambda_expression* copy = new lambda_expression();
    copy->type = node->type;
    copy->name = node->name;
    copy->expression_1 = deepcopy(node->expression_1);
    copy->expression_2 = deepcopy(node->expression_2);
    
    return copy;
}

/**
 * @brief Validates the structural integrity of the AST node.
 * * Ensures that the memory structure matches the rules of Lambda Calculus.
 * For example, a VARIABLE cannot have children, and an APPLICATION must have both.
 * * @return True if the node is mathematically sound, false if memory is corrupted or malformed.
 */
bool lambda_expression::is_valid() const {
    switch (type) {
        case NodeType::VARIABLE:
            return !name.empty() && expression_1 == nullptr && expression_2 == nullptr;
        case NodeType::LAMBDA:
            return !name.empty() && expression_1 != nullptr && expression_2 == nullptr && expression_1->is_valid();
        case NodeType::APPLICATION:
            return name.empty() && expression_1 != nullptr && expression_2 != nullptr && expression_1->is_valid() && expression_2->is_valid();
        default:
            return false;
    }
}

/**
 * @brief Checks if the current node is a Reducible Expression (Redex).
 * * A redex only exists when a function (LAMBDA) is applied to an argument.
 * Therefore, the current node must be an APPLICATION, and its left child must be a LAMBDA.
 * * @return True if the node can be beta-reduced.
 */
bool lambda_expression::is_redux() {
    return type == NodeType::APPLICATION && expression_1->type == NodeType::LAMBDA;
}

/**
 * @brief Public interface to rename free variables.
 * * Aliases to rename_free to keep the public API clean and intuitive.
 */
void lambda_expression::rename(const string& old_name, const string& new_name) {
    this->rename_free(old_name, new_name);
}

/**
 * @brief Recursively renames variables that are "free" (not bound by a local lambda).
 * * If it encounters a LAMBDA that binds the `old_name` (e.g., renaming 'x' inside \x. x),
 * it stops recursing, because the inner 'x' is shadowed and belongs to the inner lambda, 
 * not the outer scope.
 */
void lambda_expression::rename_free(const string& old_name, const string& new_name) {
    switch (type) {
        case NodeType::VARIABLE:
            if (this->name == old_name) this->name = new_name;
            break;
        case NodeType::APPLICATION:
            this->expression_1->rename_free(old_name, new_name);
            this->expression_2->rename_free(old_name, new_name);
            break;
        case NodeType::LAMBDA:
            if (this->name == old_name) {
                // Shadowing detected. Rename the binder and switch to rename_bound 
                // to force all instances in the body to match the new binder name.
                this->name = new_name;
                this->expression_1->rename_bound(old_name, new_name);
            } else {
                // Not shadowed, keep traversing normally.
                this->expression_1->rename_free(old_name, new_name);
            }
    }
}

/**
 * @brief Force-renames all occurrences of a variable name regardless of standard shadowing rules.
 * * This is primarily an internal helper used during alpha-conversion to aggressively 
 * update a lambda's bound variable and all its internal references at once.
 */
void lambda_expression::rename_bound(const string& old_name, const string& new_name) {
    switch (type) {
        case NodeType::VARIABLE:
            if (this->name == old_name) this->name = new_name;
            break;
        case NodeType::APPLICATION:
            this->expression_1->rename_bound(old_name, new_name);
            this->expression_2->rename_bound(old_name, new_name);
            break;
        case NodeType::LAMBDA:
            if (this->name != old_name) this->expression_1->rename_bound(old_name, new_name);
    }
}

/**
 * @brief Scans the tree to see if a specific variable name exists "freely".
 * * A variable is free if it is used without being wrapped in a lambda that binds it.
 * This is a crucial prerequisite check for capture-avoiding substitution.
 */
bool lambda_expression::occurs_free_in(const string& var) const {
    switch (type) {
        case NodeType::VARIABLE: return name == var;
        case NodeType::APPLICATION: return expression_1->occurs_free_in(var) || expression_2->occurs_free_in(var);
        case NodeType::LAMBDA:
            // If the lambda binds the variable we are looking for, it cannot occur free inside.
            if (name == var) return false;
            return expression_1->occurs_free_in(var);
        default: return false;
    }
}

/**
 * @brief Generates a mathematically unique variable name (Alpha-Conversion).
 * * If substituting `y` into `\y. x` causes a naming collision, this function appends 
 * apostrophes (e.g., `y'`) until it finds a name that does not exist anywhere in the 
 * current scope or the replacement term.
 */
string lambda_expression::generate_fresh_name(const string& base, const lambda_expression* replacement) const {
    string candidate = base;
    while (true) {
        bool fresh = true;
        if (expression_1 && expression_1->occurs_free_in(candidate)) fresh = false;
        if (replacement && replacement->occurs_free_in(candidate)) fresh = false;
        
        if (fresh) break;
        candidate += "'"; // Mutate and retry
    }
    return candidate;
}

/**
 * @brief The core substitution engine: replaces instances of `var` with the `replacement` tree.
 * * Features automatic capture avoidance. If a naming collision is detected, it pauses 
 * the substitution to alpha-convert (rename) the conflicting bound variables, ensuring 
 * mathematical meaning is preserved.
 */
void lambda_expression::substitute(const string& var, const lambda_expression* replacement) {
    switch (type) {
        case NodeType::VARIABLE:
            if (name == var) {
                // Deepcopy the replacement to prevent double-free memory corruption later.
                lambda_expression* temp = deepcopy(replacement);
                
                // Inherit the replacement's properties (in-place mutation)
                this->type = temp->type;
                this->name = temp->name;
                this->expression_1 = temp->expression_1;
                this->expression_2 = temp->expression_2;
                
                // Nullify the temp pointers so its destructor doesn't delete our new children.
                temp->expression_1 = nullptr;
                temp->expression_2 = nullptr;
                delete temp;
            }
            break;
        case NodeType::APPLICATION:
            expression_1->substitute(var, replacement);
            expression_2->substitute(var, replacement);
            break;
        case NodeType::LAMBDA:
            // Stop if the variable we are replacing is shadowed by this lambda.
            if (name == var) return; 
            
            // Capture Avoidance Check
            if (replacement->occurs_free_in(name)) {
                string fresh = this->generate_fresh_name(name, replacement);
                expression_1->rename_bound(name, fresh);
                name = fresh;
            }
            expression_1->substitute(var, replacement);
    }
}

/**
 * @brief Drives the reduction process until a Normal Form is reached.
 * * Uses a MAX_STEPS circuit breaker. If the evaluation loops infinitely 
 * (e.g., the Omega combinator), it catches the divergence and throws an error 
 * rather than hanging the system or overflowing the stack.
 */
void lambda_expression::reduce_total(int MAX_STEPS) {
    int steps = 0;
    try {
        while (this->reduce()) {
            if (steps >= MAX_STEPS) {
                throw runtime_error("DIVERGENCE_ERROR: Maximum reduction steps exceeded");
            }
            steps++;
        }
    } catch (const std::bad_alloc& e) {
        // C++ throws bad_alloc when the heap runs out of RAM.
        // We intercept it and throw it as a standard runtime error so the 
        // environment parser can safely swallow it!
        throw runtime_error("MEMORY_ERROR: The expression experienced exponential blowup and exhausted system memory.");
        
    } catch (const runtime_error& e) {
        // Standard timeout errors fall through here
        cerr << "Reduction Failed: " << e.what() << endl;
        throw; 
    }
}

/**
 * @brief Performs a single Normal Order (Lazy) beta-reduction step.
 * * Normal Order means we always attempt to evaluate the leftmost-outermost redex first.
 * This strategy guarantees we find a normal form if one exists, allowing us to safely 
 * drop divergent (infinite) terms if they are passed as unused arguments.
 * * @return True if a reduction occurred, false if no redexes remain.
 */
bool lambda_expression::reduce() {
    // Top-Down Check: Attempt root reduction first
    if (this->is_redux()) {
        lambda_expression* lambda = this->expression_1;
        lambda_expression* body = lambda->expression_1;
        lambda_expression* argument = this->expression_2;

        // Perform the mathematical substitution
        body->substitute(lambda->name, argument);

        // Splice the reduced body into the current node's location
        this->type         = body->type;
        this->name         = body->name;
        this->expression_1 = body->expression_1;
        this->expression_2 = body->expression_2;

        // Clean up consumed nodes safely
        body->expression_1 = nullptr;
        body->expression_2 = nullptr;
        delete body;                    
        lambda->expression_1 = nullptr; 
        delete lambda;                  
        delete argument;
        
        return true;
    } else {
        // If root is not a redex, recurse left-to-right
        switch (type) {
            case NodeType::VARIABLE: return false;
            case NodeType::LAMBDA: return this->expression_1->reduce();
            case NodeType::APPLICATION:
                // Short-circuiting: If left reduces, stop. Ensures lazy evaluation.
                if (this->expression_1->reduce()) return true;
                return this->expression_2->reduce();
        }
    }
    return false;
}

/**
 * @brief Destructor.
 * Recursively deletes child nodes. If children are set to nullptr, `delete nullptr` 
 * does nothing, making it safe.
 */
lambda_expression::~lambda_expression() {
    delete expression_1;
    delete expression_2;
}


// ==========================================
// ALPHA EQUIVALENCE HELPER
// ==========================================
static bool is_alpha_equiv(const lambda_expression* a, 
                           const lambda_expression* b, 
                           std::unordered_map<std::string, std::string>& env_A_to_B,
                           std::unordered_map<std::string, std::string>& env_B_to_A) {
                           
    // 1. Safety & Type Checks
    if (a == nullptr && b == nullptr) return true;
    if (a == nullptr || b == nullptr) return false;
    if (a->type != b->type) return false;

    switch (a->type) {
        case NodeType::VARIABLE: {
            // Explicitly defining the iterator types instead of using 'auto'
            std::unordered_map<std::string, std::string>::iterator it_A = env_A_to_B.find(a->name);
            std::unordered_map<std::string, std::string>::iterator it_B = env_B_to_A.find(b->name);
            
            bool a_is_bound = (it_A != env_A_to_B.end());
            bool b_is_bound = (it_B != env_B_to_A.end());

            if (a_is_bound && b_is_bound) {
                // Both are bound variables: they must map directly to each other
                return it_A->second == b->name;
            } else if (!a_is_bound && !b_is_bound) {
                // Neither are bound: they are free variables and must have the exact same name
                return a->name == b->name;
            } else {
                // One is bound and the other is free: automatic mismatch
                return false;
            }
        }

        case NodeType::APPLICATION:
            return is_alpha_equiv(a->expression_1, b->expression_1, env_A_to_B, env_B_to_A) &&
                   is_alpha_equiv(a->expression_2, b->expression_2, env_A_to_B, env_B_to_A);

        case NodeType::LAMBDA: {
            // STEP 1: SAVE PREVIOUS STATE (Handles variable shadowing)
            bool a_had_prev = env_A_to_B.count(a->name);
            std::string prev_A = a_had_prev ? env_A_to_B[a->name] : "";
            
            bool b_had_prev = env_B_to_A.count(b->name);
            std::string prev_B = b_had_prev ? env_B_to_A[b->name] : "";

            // STEP 2: APPLY NEW BINDINGS FOR THIS SCOPE
            env_A_to_B[a->name] = b->name;
            env_B_to_A[b->name] = a->name;
            
            // STEP 3: RECURSE
            bool bodies_match = is_alpha_equiv(a->expression_1, b->expression_1, env_A_to_B, env_B_to_A);
            
            // STEP 4: RESTORE PREVIOUS STATE (Pops the scope safely)
            if (a_had_prev) env_A_to_B[a->name] = prev_A;
            else env_A_to_B.erase(a->name);
            
            if (b_had_prev) env_B_to_A[b->name] = prev_B;
            else env_B_to_A.erase(b->name);
            
            return bodies_match;
        }
    }
    return false;
}

// ==========================================
// EQUALITY OPERATOR OVERLOAD
// ==========================================
bool lambda_expression::operator==(const lambda_expression& other) const {
    // Instantiate the dual maps for O(1) bijection lookups
    std::unordered_map<std::string, std::string> env_A_to_B;
    std::unordered_map<std::string, std::string> env_B_to_A;
    
    // Fire the recursive helper
    return is_alpha_equiv(this, &other, env_A_to_B, env_B_to_A);
}

/**
 * @brief Serializes the AST back into a human-readable mathematical string.
 */
string lambda_expression::to_string() const {
    switch (type) {
        case NodeType::VARIABLE: return name;
        case NodeType::LAMBDA: return "(λ" + name + ". " + expression_1->to_string() + ")";
        case NodeType::APPLICATION: return "(" + expression_1->to_string() + " " + expression_2->to_string() + ")";
        default: return "<malformed>";
    }
}
bool lambda_expression::beta_equivalence(const lambda_expression& other, int max_depth) const {
    // 1. Pass the pointers to your static deepcopy function
    lambda_expression* le_0 = deepcopy(this);
    lambda_expression* le_1 = deepcopy(&other);

    bool is_equiv = false;

    // 2. Wrap the reduction in a try-catch to ensure memory safety
    try {
        // Pass the max_depth safeguard down the chain
        le_0->reduce_total(max_depth);
        le_1->reduce_total(max_depth);

        // 3. Dereference the pointers to trigger your overloaded Alpha equivalence operator
        is_equiv = (*le_0 == *le_1);
        
    } catch (...) {
        // If reduce_total hits an infinite loop and throws an error,
        // we MUST clean up the copied trees before passing the error up!
        delete le_0;
        delete le_1;
        throw; // Re-throw the original error to the user
    }

    // 4. Standard memory cleanup for a successful run
    delete le_0;
    delete le_1;

    return is_equiv;
}

// ---- Factories ----

/** @brief Instantiates a basic variable node on the heap. */
lambda_expression* make_var(const string& name) {
    lambda_expression* node = new lambda_expression();
    node->type = NodeType::VARIABLE;
    node->name = name;
    node->expression_1 = nullptr;
    node->expression_2 = nullptr;
    return node;
}

/** @brief Instantiates a lambda abstraction, capturing the given body tree. */
lambda_expression* make_lambda(const string& bound_var, lambda_expression* body) {
    lambda_expression* node = new lambda_expression();
    node->type = NodeType::LAMBDA;
    node->name = bound_var;
    node->expression_1 = body;
    node->expression_2 = nullptr;
    return node;
}

/** @brief Instantiates an application of a function to an argument. */
lambda_expression* make_app(lambda_expression* func, lambda_expression* arg) {
    lambda_expression* node = new lambda_expression();
    node->type = NodeType::APPLICATION;
    node->name = "";
    node->expression_1 = func;
    node->expression_2 = arg;
    return node;
}


