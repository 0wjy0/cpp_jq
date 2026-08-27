# cpp_jq

C++17 implementation of a jq-filter command-line JSON processor.

Header-only nlohmann/json dependency. Zero runtime dependencies.

## Build

```bash
make            # debug build -> build/cpp_jq
make release    # -O2 -DNDEBUG
make clean
make test       # run all e2e fixtures
```

## Usage

```bash
echo '{"a":1}' | cpp_jq '.a'                  # -> 1
echo '{"a":1}' | cpp_jq --compact '.a'         # -> 1
echo '{"a":1}' | cpp_jq -f filter.jq
cpp_jq -f filter.jq input.json
echo '{"a":1,"b":2}' | cpp_jq '.a, .b'        # -> 1, 2 on separate lines
cpp_jq --version
cpp_jq --help
```

## Exit codes

- `0` success
- `1` parse error (filter)
- `2` runtime error (per-input)
- `3` I/O error (cannot open file)

## Supported filter subset

- identity: `.`
- field / index / slice: `.foo`, `.[0]`, `.[2:5]`
- iterate: `.[]`, `.foo[]`
- recursion: `..`
- optional: `?` after accessor (suppresses error)
- pipe: `|`
- comma: `,`
- group: `(...)`
- if / then / else / end
- select: `select(cond)`
- arithmetic: `+ - * / %`
- comparison: `== != < <= > >=`
- logical: `and or not`
- constructors: `[...]`, `{k:v,...}`
- literals: number, string, `true`, `false`, `null`

## Builtins

`length keys type has contains in map add min max sort unique group_by tostring tonumber`

## Tests

```bash
make test
```

34 end-to-end fixtures under `tests/fixtures/`, including positive cases and negative
cases that assert specific exit codes and stderr substrings.

## Architecture

- `src/lexer.cc` -- hand-written tokenizer
- `src/parser.cc` -- recursive-descent parser -> std::variant AST
- `src/evaluator.cc` -- tree-walking evaluator
- `src/builtin.cc` -- 15 builtins registered in a function map
- `src/printer.cc` -- pretty/compact JSON output
- `src/diag.cc` -- stderr diagnostic with position
- `src/main.cc` -- CLI driver (NDJSON stdin, NDJSON file, filter from arg or `-f FILE`)