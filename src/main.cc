// cpp_jq - SPDX-License-Identifier: MIT
#include <iostream>
#include <string>
#include "cpp_jq/version.hpp"
#include "cpp_jq/ast.hpp"

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "cpp_jq " << CPP_JQ_VERSION << "\n";
        return 0;
    }
    cpp_jq::Node n;
    n.kind = cpp_jq::Identity{};
    cpp_jq::Values vs = n.eval(cpp_jq::J::parse(std::cin));
    for (auto& v : vs) std::cout << v.dump() << "\n";
    return 0;
}