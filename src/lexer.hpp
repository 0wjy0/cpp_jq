// cpp_jq - SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "cpp_jq/error.hpp"

namespace cpp_jq {

enum class TokKind {
    IDENT, NUMBER, STRING, TRUE, FALSE, NULL_T,
    DOT, LBRACKET, RBRACKET, LBRACE, RBRACE,
    LPAREN, RPAREN, PIPE, COMMA, QUESTION, COLON,
    IF, THEN, ELSE, END, SELECT,
    AND, OR, NOT,
    EQ, NEQ, LT, LE, GT, GE,
    PLUS, MINUS, STAR, SLASH, PERCENT,
    EOF_T,
};

struct Tok {
    TokKind kind;
    std::string text;
    int64_t num = 0;
    Pos pos;
};

std::vector<Tok> lex(const std::string& src);

}