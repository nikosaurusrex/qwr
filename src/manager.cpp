#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#define TIMER_NOW std::chrono::steady_clock::now()
#define TIMER_DIFF(a, b) std::chrono::duration_cast<std::chrono::microseconds>(b - a).count()

#include "manager.h"
#include "parser.h"
#include "arena.h"
#include "gen.h"

size_t read_entire_file(const char *file_name, const char **contents);

static Options *options;
static CodeGenerator code_gen;
static Messenger messenger;
static Parser parser;
static Typer typer;

static long long parse_time = 0;
static long long llvm_time = 0;
static long long link_time = 0;
static long long loc = 0;

#ifdef _WIN32
dasdas
#else
const char *LIB_PATH = "/usr/local/bin/qwrstd/";
#endif

void manager_init(Options *_options) {
    options = _options;

	parser.init(&typer, &messenger);
	code_gen.init(&typer);
	typer.init(&code_gen.llvm_context, &messenger);

	arena_init();
}

void manager_run() {
	auto total_start = TIMER_NOW;

    const char *code;
    auto code_len = read_entire_file(options->src_file, &code);

    messenger.init(code);
    parser.lexer.init(code, code_len);

    manager_add_library("qwr");

    Stmt *stmt;

    while (true) {
        auto start = TIMER_NOW;

        stmt = parser.parse_top_level_stmt();
        if (parser.has_reached_end) {
            break;
        }

        auto end = TIMER_NOW;
        auto diff = TIMER_DIFF(start, end);
        parse_time += diff;

        start = TIMER_NOW;

        code_gen.gen_stmt(stmt);

        end = TIMER_NOW;
        diff = TIMER_DIFF(start, end);

        llvm_time += diff;
	}

	auto start = TIMER_NOW;

	code_gen.output(options);
    code_gen.dump(options);

	auto end = TIMER_NOW;
	auto diff = TIMER_DIFF(start, end);

	llvm_time += diff;

	start = TIMER_NOW;

    if ((options->flags & COMPILE_ONLY) == 0) {
	    code_gen.link(options);
    }

	end = TIMER_NOW;
	diff = TIMER_DIFF(start, end);
	link_time = diff;

    auto total_end = std::chrono::steady_clock::now();
    auto total_time = TIMER_DIFF(total_start, total_end);

    std::cout << "Program LOC: " << loc << "\n";
    std::cout << "Compilation time: " << (total_time / 1000) << " ms\n";
	std::cout << "Parse time: " << (parse_time / 1000) << " ms\n";
	std::cout << "LLVM time: " << (llvm_time / 1000) << " ms\n";
	std::cout << "Link time: " << (link_time / 1000) << " ms\n";
}

void manager_add_library(const char *lib_name) {
    // + 5 = ".qwr" + \0
    char *full_lib_path = new char[strlen(LIB_PATH) + strlen(lib_name) + 5];
    strcpy(full_lib_path, LIB_PATH);
    strcat(full_lib_path, lib_name);
    strcat(full_lib_path, ".qwr");

    const char *code;
    auto code_len = read_entire_file(full_lib_path, &code);

    parser.lexer.backup();
    parser.lexer.init(code, code_len);

    Stmt *stmt;

    while (true) {
        auto start = TIMER_NOW;

        stmt = parser.parse_top_level_stmt();
        if (parser.has_reached_end) {
            break;
        }

        auto end = TIMER_NOW;
        auto diff = TIMER_DIFF(start, end);
        parse_time += diff;

        start = TIMER_NOW;

        code_gen.gen_stmt(stmt);

        end = TIMER_NOW;
        diff = TIMER_DIFF(start, end);

        llvm_time += diff;
	}

    parser.has_reached_end = false;
	parser.lexer.restore();
}

void manager_add_flags(const char *flags) {
    options->linker_flags.push_back(flags);
}

size_t read_entire_file(const char *file_name, const char **contents) {
    FILE *f = fopen(file_name, "rb");
    if (!f) {
        std::cout << "Failed to open file " << file_name << "\n";
        std::exit(1);
    }

	fseek(f, 0, SEEK_END);
	size_t len = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *buffer = new char[len + 1];
	if (!fread(buffer, 1, len, f)) {
		fclose(f);
		return 0;
	}

    fclose(f);

	buffer[len] = '\0';
	*contents = buffer;

    std::ifstream file_stream(file_name); 
    loc += std::count(std::istreambuf_iterator<char>(file_stream), 
             std::istreambuf_iterator<char>(), '\n');

	return len;
}
