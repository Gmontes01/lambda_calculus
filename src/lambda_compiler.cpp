#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "../include/environment.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <filename.lc>" << endl;
        return 1;
    }

    string filepath = argv[1];
    ifstream file(filepath);
    
    if (!file.is_open()) {
        cerr << "Fatal Error: Could not open file '" << filepath << "'" << endl;
        return 1;
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string source_code = buffer.str();
    file.close();

    try {
        ProgramEnvironment env = parse_program(source_code);

        cout << "=== Environment Loaded ===" << endl;
        for (const string& name : env.declaration_order) {
            cout << name << " = " << env.variables[name]->to_string() << endl;
        }
        cout << "==========================\n" << endl;

        if (env.evaluation_target != nullptr) {
            if (!env.target_name.empty()) {
                cout << "Evaluating: " << env.target_name << endl;
            } else {
                cout << "Evaluating: " << env.evaluation_target->to_string() << endl;
            }
            
            env.evaluation_target->reduce_total();
            lambda_expression* simplified_lambda_ex = lambda_expression::deepcopy(env.evaluation_target);
            env.reduce_variables();
            
            lambda_expression* expressed_result = env.express(simplified_lambda_ex);
            cout << "Result:     " << expressed_result->to_string() << endl;
            delete expressed_result; // Frees the resugared tree or variable node
        } else {
            cout << "No execution block found. Variables loaded successfully." << endl;
        }

    } catch (const exception& e) {
        cerr << "\n[COMPILER ERROR]\n" << e.what() << endl;
        return 1;
    }

    // env and its internal AST pointers are safely reclaimed by ~ProgramEnvironment()
    return 0;
}