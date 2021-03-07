#include <stdio.h>
#include "compiler.h"
#include "scanner.h"
#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

typedef enum {
    VAR_UNINITIALIZED,
    VAR_READABLE,
    VAR_WRITEABLE
} VarState;

typedef struct {
    Token name;
    int depth;
    VarState state;
} Local;

typedef struct CompilationContext {
    struct CompilationContext* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
    VarState globalStates[UINT8_COUNT];
} CompilationContext;

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
    Obj** objectRoot;
    Scanner* scanner;
    CompilationContext* context;
} Compiler;

static void initCompilationContext(Obj** objectRoot, struct CompilationContext* context, FunctionType type) {
    context->enclosing = NULL;
    context->type = type;
    context->function = NULL;
    context->localCount = 0;
    context->scopeDepth = 0;
    context->function = newFunction(objectRoot);

    Local* local = &context->locals[context->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

static void initCompiler(Compiler* compiler) {
    compiler->hadError = false;
    compiler->panicMode = false;
    compiler->scanner = NULL;
    compiler->context = NULL;
}

static void errorAt(Compiler* compiler, Token* token, const char* message) {
    if (compiler->panicMode) return;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    compiler->hadError = true;
}

static void errorAtCurrent(Compiler* compiler) {
    errorAt(compiler, &compiler->current, compiler->current.start);
}

//static void error(Compiler* compiler, const char* message) {
//    errorAt(compiler, &compiler->previous, message);
//}

static void advance(Compiler* compiler) {
    compiler->previous = compiler->current;

    for (;;) {
        compiler->current = scanToken(compiler->scanner);
        if (compiler->current.type != TOKEN_ERROR) break;

        errorAtCurrent(compiler);
    }
}

static bool check(Compiler* compiler, TokenType type) {
    return compiler->current.type == type;
}

static bool match(Compiler* compiler, TokenType type) {
    if (!check(compiler, type)) return false;
    advance(compiler);
    return true;
}


static Chunk* currentChunk(Compiler* compiler) {
    return &compiler->context->function->chunk;
}

static void emitByte(Compiler* compiler, uint8_t byte) {
    writeChunk(currentChunk(compiler), byte, compiler->previous.line);
}

static void emitReturn(Compiler* compiler) {
    emitByte(compiler, OP_NIL);
    emitByte(compiler, OP_RETURN);
}

static ObjFunction* endCompiler(Compiler* compiler) {
    emitReturn(compiler);
    ObjFunction* function = compiler->context->function;
#ifdef DEBUG_PRINT_CODE
    if (!compiler->hadError) {
        disassembleChunk(stdout, currentChunk(compiler), function->name != NULL ? function->name->chars : "<script>");
    }
#endif
    compiler->context = compiler->context->enclosing;
    return function;
}

ObjFunction* compile(Obj** objectRoot, const char* source) {
    Scanner scanner;
    initScanner(&scanner, source);

    CompilationContext scriptContext;
    initCompilationContext(objectRoot, &scriptContext, TYPE_SCRIPT);

    Compiler compiler;
    initCompiler(&compiler);
    compiler.objectRoot = objectRoot;
    compiler.scanner = &scanner;
    compiler.context = &scriptContext;

    advance(&compiler);

    while (!match(&compiler, TOKEN_EOF)) {
        advance(&compiler);
//        declaration(&compiler);
    }

    ObjFunction* function = endCompiler(&compiler);
    return compiler.hadError ? NULL : function;
}
