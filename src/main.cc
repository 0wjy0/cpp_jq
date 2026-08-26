// cpp_jq - SPDX-License-Identifier: MIT
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "cpp_jq/version.hpp"
#include "cpp_jq/ast.hpp"
#include "cpp_jq/error.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "printer.hpp"
#include "diag.hpp"
#include "builtin.hpp"

using namespace cpp_jq;

namespace {

void print_usage(std::ostream& os) {
    os << "Usage: cpp_jq [OPTIONS] [FILTER] [INPUT]\n"
       << "  -f FILE       read filter from FILE\n"
       << "  --compact     compact output\n"
       << "  -h, --help    show this help\n"
       << "  -V, --version show version\n";
}

std::string slurp(std::istream& is) {
    std::ostringstream ss; ss << is.rdbuf(); return ss.str();
}

int run(const std::string& filter_src, std::istream& input, bool compact) {
    try {
        auto toks = lex(filter_src);
        NodePtr ast = parse(toks);
        std::string line;
        while (std::getline(input, line)) {
            if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
            try {
                J v = J::parse(line);
                Values out = ast->eval(v);
                for (auto& x : out) print_json(std::cout, x, compact);
            } catch (const CppJqError& e) {
                print_diag(std::cerr, e.pos(), e.what()); return 2;
            } catch (const std::exception& e) {
                print_diag(std::cerr, {}, e.what()); return 2;
            }
        }
        return 0;
    } catch (const CppJqError& e) {
        print_diag(std::cerr, e.pos(), e.what()); return 1;
    } catch (const std::exception& e) {
        print_diag(std::cerr, {}, e.what()); return 1;
    }
}

}

int main(int argc, char** argv) {
    register_builtins();
    std::vector<std::string> args(argv+1, argv+argc);
    bool compact = false;
    std::string filter_file;
    std::vector<std::string> positional;
    for (size_t k=0; k<args.size(); ++k) {
        const std::string& a = args[k];
        if (a == "-h" || a == "--help")  { print_usage(std::cout); return 0; }
        if (a == "-V" || a == "--version"){ std::cout << "cpp_jq " << CPP_JQ_VERSION << "\n"; return 0; }
        if (a == "--compact") { compact = true; continue; }
        if (a == "-f" || a == "--filter") { if (++k >= args.size()) { print_usage(std::cerr); return 1; } filter_file = args[k]; continue; }
        positional.push_back(a);
    }
    if (positional.empty() && filter_file.empty()) { print_usage(std::cerr); return 1; }
    std::string filter_src;
    if (!filter_file.empty()) {
        std::ifstream fs(filter_file);
        if (!fs) { print_diag(std::cerr, {}, "cannot open filter file"); return 3; }
        filter_src = slurp(fs);
    } else {
        filter_src = positional[0];
        positional.erase(positional.begin());
    }

    int rc;
    if (positional.empty()) {
        rc = run(filter_src, std::cin, compact);
    } else {
        std::ifstream fs(positional[0]);
        if (!fs) { print_diag(std::cerr, {}, "cannot open input file"); return 3; }
        rc = run(filter_src, fs, compact);
    }
    return rc;
}