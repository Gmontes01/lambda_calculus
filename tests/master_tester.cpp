#include <iostream>
#include <string>
#include <vector>
#include <utility>

using namespace std;

// 1. Declare the namespaces and functions 
// (Updated to match the pair<int, int> return type)
namespace ExprTests { pair<int,int> run_tests(bool verbose, bool quiet); }
namespace ParserTests { pair<int,int> run_tests(bool verbose, bool quiet); }
namespace EnvironmentTests { pair<int,int> run_tests(bool verbose, bool quiet); }

// 2. Define a structure to hold the test metadata and function pointer
struct TestModule {
    string name;
    pair<int,int> (*run)(bool, bool);
};

int main(int argc, char* argv[]) {
    bool verbose = false;
    bool quiet = false;

    // Check for verbosity flags in the terminal
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-verbose" || arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "-quiet" || arg == "--quiet" || arg == "-q") {
            quiet = true;
        }
    }

    // ==========================================
    // THE TEST REGISTRY
    // Add any future test modules to this vector!
    // ==========================================
    vector<TestModule> test_suite = {
        {"AST Engine", ExprTests::run_tests},
        {"Parser Frontend", ParserTests::run_tests},
        {"Environment/File IO", EnvironmentTests::run_tests} // <--- Add this
    };

    if (!quiet) {
        cout << "#############################################\n";
        cout << "     FULL LAMBDA COMPILER TEST SUITE         \n";
        cout << "#############################################\n";
    }

    int total_failures = 0;
    int total_passed = 0;

    // 3. Dynamically loop over all registered tests
    // 3. Dynamically loop over all registered tests
    for (const TestModule& module : test_suite) {
        if (!quiet) cout << "\n>>> Running " << module.name << " Tests...\n";
        
        // Execute the test and unpack the pair
        pair<int, int> results = module.run(verbose, quiet);
        total_passed += results.first;
        total_failures += results.second;
    }

    // ANSI Color Codes
    const string RESET = "\033[0m";
    const string BOLD_GREEN = "\033[1;32m";
    const string BOLD_RED = "\033[1;31m";

    if (!quiet) cout << "\n#############################################\n";
    
    // 4. Print the final aggregated summary in color
    if (total_failures == 0) {
        cout << BOLD_GREEN << "   ALL MODULES PASSED! SYSTEM IS STABLE.   \n";
        cout << "   (Total: " << total_passed << " tests passed)\n" << RESET;
    } else {
        cout << BOLD_RED << "   SYSTEM FAILED WITH " << total_failures << " ERRORS.\n";
        cout << "   (" << total_passed << " tests passed)\n" << RESET;
    }
    
    if (!quiet) cout << "#############################################\n";

    return total_failures > 0 ? 1 : 0;
}