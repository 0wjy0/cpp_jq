// cpp_jq - SPDX-License-Identifier: MIT
// Phase 2 minimal main: supports -f filter.jq + stdin (replaced in Phase 3).
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <exception>
#include "cpp_jq/version.hpp"
#include "cpp_jq/ast.hpp"
#include "cpp_jq/error.hpp"
#include "lexer.hpp"
#include "parser.hpp"

using namespace cpp_jq;

namespace {
std::string slurp(std::istream& is) {
    std::ostringstream ss;
    ss << is.rdbuf();
    return ss.str();
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "cpp_jq " << CPP_JQ_VERSION << "\n";
        return 0;
    }
    std::string filter_src = ".";
    bool from_file = false;
    std::string filter_path;
    for (int k = 1; k < argc; ++k) {
        std::string a = argv[k];
        if (a == "-f" || a == "--filter") {
            if (k + 1 >= argc) return 1;
            filter_path = argv[++k];
            from_file = true;
        } else if (k == 1 && !from_file && filter_src == ".") {
            filter_src = a;
        }
    }
    if (from_file) {
        std::ifstream fs(filter_path);
        if (!fs) {
            std::cerr << "cpp_jq: error: cannot open filter file\n";
            return 3;
        }
        filter_src = slurp(fs);
    }
    try {
        auto toks = lex(filter_src);
        NodePtr ast = parse(toks);
        J v = J::parse(std::cin);
        Values out = ast->eval(v);
        for (auto& x : out) std::cout << x.dump() << "\n";
        return 0;
    } catch (const CppJqError& e) {
        std::cerr << "cpp_jq: error";
        if (e.pos().line > 0) std::cerr << " at " << e.pos().line << ":" << e.pos().col;
        std::cerr << ": " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "cpp_jq: error: " << e.what() << "\n";
        return 2;
    }
}