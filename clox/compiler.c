#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "scanner.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
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
    // Parsing State
    Scanner* scanner;
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;

    CompilationContext* context;

    // Injected VM State
    Obj** objectRoot;
    Table* internedStrings;
    Globals* globals;
} Compiler;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,
    PREC_OR,
    PREC_AND,
    PREC_EQUALITY,
    PREC_COMPARISON,
    PREC_TERM,
    PREC_FACTOR,
    PREC_UNARY,
    PREC_CALL,
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(Compiler*, bool);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

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

static void error(Compiler* compiler, const char* message) {
    errorAt(compiler, &compiler->previous, message);
}

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

static void consume(Compiler* compiler, TokenType type, const char* message) {
    if (compiler->current.type == type) {
        advance(compiler);
        return;
    }

    error(compiler, message);
}

static Chunk* currentChunk(Compiler* compiler) {
    return &compiler->context->function->chunk;
}

static void emitByte(Compiler* compiler, uint8_t byte) {
    writeChunk(currentChunk(compiler), byte, compiler->previous.line);
}

static void emitBytes(Compiler* compiler, uint8_t byte1, uint8_t byte2) {
    emitByte(compiler, byte1);
    emitByte(compiler, byte2);
}

__unused static void emitLoop(Compiler* compiler, int loopStart) {
    emitByte(compiler, OP_LOOP);

    int offset = currentChunk(compiler)->count - loopStart + 2;
    if (offset > UINT16_MAX) error(compiler, "Loop body too large.");

    emitByte(compiler, (offset >> 8) & 0xff);
    emitByte(compiler, offset & 0xff);
}

static int emitJump(Compiler* compiler, uint8_t instruction) {
    emitByte(compiler, instruction);
    emitByte(compiler, 0xff);
    emitByte(compiler, 0xff);
    return currentChunk(compiler)->count - 2;
}

static void patchJump(Compiler* compiler, int offset) {
    int jump = currentChunk(compiler)->count - offset - 2;
    if (jump > UINT16_MAX) {
        error(compiler, "Too much code to jump over.");
    }

    currentChunk(compiler)->code[offset] = (jump >> 8) & 0xff;
    currentChunk(compiler)->code[offset + 1] = jump & 0xff;
}

static uint8_t makeConstant(Compiler* compiler, Value value) {
    int constant = addConstant(currentChunk(compiler), value);
    if (constant > UINT8_MAX) {
        error(compiler, "Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(Compiler* compiler, Value value) {
    emitBytes(compiler, OP_CONSTANT, makeConstant(compiler, value));
}

static void emitReturn(Compiler* compiler) {
    emitByte(compiler, OP_NIL);
    emitByte(compiler, OP_RETURN);
}



static void synchronize(Compiler* compiler) {
    compiler->panicMode = false;

    while (compiler->current.type != TOKEN_EOF) {
        if (compiler->previous.type == TOKEN_SEMICOLON) return;

        switch (compiler->current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                //NO-OP
                ;
        }

        advance(compiler);
    }
}

static void expression(Compiler* compiler);
static void statement(Compiler* compiler);
static void declaration(Compiler* compiler);
static ParseRule* getRule(TokenType type);

static void printStatement(Compiler* compiler) {
    expression(compiler);
    consume(compiler, TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(compiler, OP_PRINT);
}

static void beginScope(Compiler* compiler) {
    compiler->context->scopeDepth++;
}

static void endScope(Compiler* compiler) {
    compiler->context->scopeDepth--;

    while (compiler->context->localCount > 0 &&
           compiler->context->locals[compiler->context->localCount - 1].depth > compiler->context->scopeDepth) {
        emitByte(compiler, OP_POP); //TODO(kjaa): add an OP_POPN, instead of many at a time.
        compiler->context->localCount--;
    }
}

static void block(Compiler* compiler) {
    while (!check(compiler, TOKEN_RIGHT_BRACE) && !check(compiler, TOKEN_EOF)) {
        declaration(compiler);
    }

    consume(compiler, TOKEN_RIGHT_BRACE, "Expect '}' after block");
}

static void whileStatement(Compiler* compiler) {
    int loopStart = currentChunk(compiler)->count;

    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(compiler, OP_JUMP_IF_FALSE);

    emitByte(compiler, OP_POP);
    statement(compiler);

    emitLoop(compiler, loopStart);

    patchJump(compiler, exitJump);
    emitByte(compiler, OP_POP);
}

static void returnStatement(Compiler* compiler) {
    if (compiler->current.type == TYPE_SCRIPT) {
        error(compiler, "Can't return from top-level code.");
    }

    if (match(compiler, TOKEN_SEMICOLON)) {
        emitReturn(compiler);
    } else {
        expression(compiler);
        consume(compiler, TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(compiler, OP_RETURN);
    }
}

static void expressionStatement(Compiler* compiler) {
    expression(compiler);
    consume(compiler, TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(compiler, OP_POP);
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static void addLocal(Compiler* compiler, Token name) {
    if (compiler->context->localCount == UINT8_COUNT) {
        error(compiler, "Too many local variables in function.");
        return;
    }

    Local* local = &compiler->context->locals[compiler->context->localCount++];
    local->name = name;
    local->depth = compiler->context->scopeDepth;
    local->state = VAR_UNINITIALIZED;
}


static void declareVariable(Compiler* compiler) {
    if (compiler->context->scopeDepth == 0) return;

    Token* name = &compiler->previous;
    for (int i = compiler->context->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->context->locals[i];
        if (local->depth != -1 && local->depth < compiler->context->scopeDepth) {
            break;
        }
        if (identifiersEqual(name, &local->name)) {
            error(compiler, "Already variable with this name in this scope.");
        }
    }

    addLocal(compiler, *name);
}

static uint8_t identifierConstant(Compiler* compiler, Token* name) {
    Value index;
    ObjString* identifier = copyString(compiler->internedStrings, compiler->objectRoot, name->start, name->length);
    if (tableGet(&compiler->globals->names, identifier, &index)) {
        return (uint8_t)AS_NUMBER(index);
    }

    uint8_t newIndex = (uint8_t)compiler->globals->count++;
    compiler->globals->values[newIndex] = UNDEFINED_VAL;
    compiler->globals->identifiers[newIndex] = identifier;

    tableSet(&compiler->globals->names, identifier, NUMBER_VAL((double)newIndex));
    return newIndex;
}


static uint8_t parseVariable(Compiler* compiler, const char* errorMessage) {
    consume(compiler, TOKEN_IDENTIFIER, errorMessage);

    declareVariable(compiler);
    if (compiler->context->scopeDepth > 0) return 0;

    return identifierConstant(compiler, &compiler->previous);
}

static void markInitialized(CompilationContext* context, VarState varState) {
    if (context->scopeDepth == 0) return;
    context->locals[context->localCount - 1].state = varState;
}

static void defineVariable(Compiler* compiler, uint8_t global, VarState varState) {
    // If Local Variable
    if (compiler->context->scopeDepth > 0) {
        markInitialized(compiler->context, varState);
        return;
    }
    // Else, global variable
    compiler->context->globalStates[global] = varState;
    emitBytes(compiler, OP_DEFINE_GLOBAL, global);
}

static void varDeclaration(Compiler* compiler, VarState varState) {
    uint8_t global = parseVariable(compiler, "Expect variable name.");

    if (match(compiler, TOKEN_EQUAL)) {
        expression(compiler);
    } else {
        emitByte(compiler, OP_NIL);
    }
    consume(compiler, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(compiler, global, varState);
}

static void forStatement(Compiler* compiler) {
    beginScope(compiler);

    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(compiler, TOKEN_SEMICOLON)) {
        // No initializer;
    } else if (match(compiler, TOKEN_VAR)) {
        varDeclaration(compiler, VAR_WRITEABLE);
    } else {
        expressionStatement(compiler);
    }
    consume(compiler, TOKEN_SEMICOLON, "Expect ';'.");

    int loopStart = currentChunk(compiler)->count;

    int exitJump = -1;
    if (!match(compiler, TOKEN_SEMICOLON)) {
        expression(compiler);
        consume(compiler, TOKEN_SEMICOLON, "Expect ';' after loop condition");

        exitJump = emitJump(compiler, OP_JUMP_IF_FALSE);
        emitByte(compiler, OP_POP);
    }

    consume(compiler, TOKEN_SEMICOLON, "Expect ';'.");

    if (!match(compiler, TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(compiler, OP_JUMP);

        int incrementStart = currentChunk(compiler)->count;
        expression(compiler);
        emitByte(compiler, OP_POP);
        consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(compiler, loopStart);
        loopStart = incrementStart;
        patchJump(compiler, bodyJump);
    }

    statement(compiler);

    emitLoop(compiler, loopStart);

    if (exitJump != -1) {
        patchJump(compiler, exitJump);
        emitByte(compiler, OP_POP);
    }

    endScope(compiler);
}

static void ifStatement(Compiler* compiler) {
    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "expect ')' after condition.");

    int thenJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);
    statement(compiler);

    int elseJump = emitJump(compiler, OP_JUMP);
    patchJump(compiler, thenJump);
    emitByte(compiler, OP_POP);

    if (match(compiler, TOKEN_ELSE)) statement(compiler);
    patchJump(compiler, elseJump);
}

static void statement(Compiler* compiler) {
    if (match(compiler, TOKEN_PRINT)) {
        printStatement(compiler);
    } else if (match(compiler, TOKEN_FOR)) {
        forStatement(compiler);
    } else if (match(compiler, TOKEN_IF)) {
        ifStatement(compiler);
    } else if (match(compiler, TOKEN_RETURN)) {
        returnStatement(compiler);
    } else if (match(compiler, TOKEN_WHILE)) {
        whileStatement(compiler);
    } else if (match(compiler, TOKEN_LEFT_BRACE)) {
        beginScope(compiler);
        block(compiler);
        endScope(compiler);
    } else {
        expressionStatement(compiler);
    }
}

static ObjFunction* endCompilation(Compiler* compiler) {
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

static void function(Compiler* compiler, uint8_t globalSlotForName, FunctionType type) {
    CompilationContext context;
    initCompilationContext(compiler->objectRoot, &context, type);
    context.enclosing = compiler->context;
    compiler->context = &context;
    context.function->name = compiler->globals->identifiers[globalSlotForName];

    beginScope(compiler);

    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(compiler, TOKEN_RIGHT_PAREN)) {
        do {
            context.function->arity++;
            if (context.function->arity > 255) {
                error(compiler, "Can't have more than 255 parameters.");
            }

            uint8_t paramConstant = parseVariable(compiler, "Expect parameter name.");
            defineVariable(compiler, paramConstant, VAR_READABLE);
        } while (match(compiler, TOKEN_COMMA));
    }
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

    consume(compiler, TOKEN_LEFT_BRACE, "Expect '{' before function body");
    block(compiler);

    ObjFunction* function = endCompilation(compiler);
    emitBytes(compiler, OP_CONSTANT, makeConstant(compiler, OBJ_VAL(function)));
}

static void funDeclaration(Compiler* compiler) {
    uint8_t global = parseVariable(compiler, "Expect function name.");
    markInitialized(compiler->context, VAR_READABLE);
    function(compiler, global, TYPE_FUNCTION);
    defineVariable(compiler, global, VAR_WRITEABLE);
}

static void declaration(Compiler* compiler) {
    if (match(compiler, TOKEN_FUN)) {
        funDeclaration(compiler);
    } else if (match(compiler, TOKEN_VAR)) {
        varDeclaration(compiler, VAR_WRITEABLE);
    } else if (match(compiler, TOKEN_CONST)) {
        varDeclaration(compiler, VAR_READABLE);
    } else {
        statement(compiler);
    }

    if (compiler->panicMode) synchronize(compiler);
}

static void grouping(Compiler* compiler, bool canAssign) {
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void parsePrecendence(Compiler* compiler, Precedence precedence) {
    advance(compiler);
    ParseFn prefixRule = getRule(compiler->previous.type)->prefix;
    if (prefixRule == NULL) {
        error(compiler, "Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(compiler, canAssign);

    while (precedence <= getRule(compiler->current.type)->precedence) {
        advance(compiler);
        ParseFn infixRule = getRule(compiler->previous.type)->infix;
        infixRule(compiler, true); // TODO(kjaa): NO IDEA if this is right, it might not need canAssign.
    }

    if (canAssign && match(compiler, TOKEN_EQUAL)) {
        error(compiler, "Invalid assignment target.");
    }
}

static void unary(Compiler* compiler, bool canAssign) {
    //TODO(kjaa): See note on error reporting in 17.4.3
    TokenType operatorType = compiler->previous.type;
    parsePrecendence(compiler, PREC_UNARY);

    switch (operatorType) {
        case TOKEN_BANG: emitByte(compiler, OP_NOT); break;
        case TOKEN_MINUS: emitByte(compiler, OP_NEGATE); break;
        default:
            return; // Unreachable
    }
}

static void binary(Compiler* compiler, bool canAssign) {
    TokenType operatorType = compiler->previous.type;

    ParseRule* rule = getRule(operatorType);
    parsePrecendence(compiler, (Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_PLUS: emitByte(compiler, OP_ADD); break;
        case TOKEN_MINUS: emitByte(compiler, OP_SUBTRACT); break;
        case TOKEN_STAR: emitByte(compiler, OP_MULTIPLY); break;
        case TOKEN_SLASH: emitByte(compiler, OP_DIVIDE); break;
        case TOKEN_BANG_EQUAL: emitBytes(compiler, OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL: emitByte(compiler, OP_EQUAL); break;
        case TOKEN_LESS: emitByte(compiler, OP_LESS); break;
        case TOKEN_GREATER: emitByte(compiler, OP_GREATER); break;
        case TOKEN_LESS_EQUAL: emitBytes(compiler, OP_GREATER, OP_NOT); break;
        case TOKEN_GREATER_EQUAL: emitBytes(compiler, OP_LESS, OP_NOT); break;
        default:
            return; // Unreachable
    }
}

static uint8_t argumentList(Compiler* compiler) {
    uint8_t argCount = 0;
    if (!check(compiler, TOKEN_RIGHT_PAREN)) {
        do {
            expression(compiler);

            if (argCount == 255) {
                error(compiler, "Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(compiler, TOKEN_COMMA));
    }

    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static void call(Compiler* compiler, bool canAssign) {
    uint8_t argCount = argumentList(compiler);
    emitBytes(compiler, OP_CALL, argCount);
}

static void literal(Compiler* compiler, bool canAssign) {
    switch (compiler->previous.type) {
        case TOKEN_NIL: emitByte(compiler, OP_NIL); break;
        case TOKEN_TRUE: emitByte(compiler, OP_TRUE); break;
        case TOKEN_FALSE: emitByte(compiler, OP_FALSE); break;
        default:
            return; // Unreachable
    }
}


static int resolveLocal(Compiler* compiler, Token* name) {
    CompilationContext* context = compiler->context;
    for (int i = context->localCount - 1; i >= 0; i--) {
        Local* local = &context->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->state == VAR_UNINITIALIZED) {
                error(compiler, "Cant read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static void namedVariable(Compiler* compiler, Token name, bool canAssign) {
    uint8_t getOp, setOp;
    VarState varState;
    int arg = resolveLocal(compiler, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
        varState = compiler->context->locals[arg].state;
    } else {
        arg = identifierConstant(compiler, &name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
        varState = compiler->context->globalStates[arg];
    }

    if (canAssign && match(compiler, TOKEN_EQUAL)) {
        if (varState != VAR_WRITEABLE) {
            error(compiler, "Writing to const variable.");
        }
        expression(compiler);
        emitBytes(compiler, setOp, (uint8_t) arg);
    } else {
        emitBytes(compiler, getOp, (uint8_t) arg);
    }
}

static void variable(Compiler* compiler, bool canAssign) {
    namedVariable(compiler, compiler->previous, canAssign);
}


static void and_(Compiler* compiler, bool canAssign) {
    int endJump = emitJump(compiler, OP_JUMP_IF_FALSE);

    emitByte(compiler, OP_POP);
    parsePrecendence(compiler, PREC_AND);

    patchJump(compiler, endJump);
}

static void or_(Compiler* compiler, bool canAssign) {
    int elseJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    int endJump = emitJump(compiler, OP_JUMP);

    patchJump(compiler, elseJump);
    emitByte(compiler, OP_POP);

    parsePrecendence(compiler, PREC_OR);
    patchJump(compiler, endJump);
}



static void number(Compiler* compiler, bool canAssign) {
    double value = strtod(compiler->previous.start, NULL);
    emitConstant(compiler, NUMBER_VAL(value));
}

static void string(Compiler* compiler, bool canAssign) {
    emitConstant(compiler, OBJ_VAL(copyString(compiler->internedStrings, compiler->objectRoot, compiler->previous.start + 1, compiler->previous.length - 2)));
}


ParseRule rules[] = {
        [TOKEN_LEFT_PAREN]    = { grouping, call,   PREC_CALL },
        [TOKEN_RIGHT_PAREN]   = { NULL,     NULL,   PREC_NONE },
        [TOKEN_LEFT_BRACE]    = { NULL,     NULL,   PREC_NONE },
        [TOKEN_RIGHT_BRACE]   = { NULL,     NULL,   PREC_NONE },
        [TOKEN_COMMA]         = { NULL,     NULL,   PREC_NONE },
        [TOKEN_DOT]           = { NULL,     NULL,   PREC_NONE },
        [TOKEN_MINUS]         = { unary,    binary, PREC_TERM },
        [TOKEN_PLUS]          = { NULL,     binary, PREC_TERM },
        [TOKEN_SEMICOLON]     = { NULL,     NULL,   PREC_NONE },
        [TOKEN_SLASH]         = { NULL,     binary, PREC_FACTOR },
        [TOKEN_STAR]          = { NULL,     binary, PREC_FACTOR },
        [TOKEN_BANG]          = { unary,    NULL,   PREC_NONE },
        [TOKEN_BANG_EQUAL]    = { NULL,     binary, PREC_EQUALITY },
        [TOKEN_EQUAL]         = { NULL,     NULL,   PREC_NONE },
        [TOKEN_EQUAL_EQUAL]   = { NULL,     binary, PREC_EQUALITY },
        [TOKEN_GREATER]       = { NULL,     binary, PREC_EQUALITY },
        [TOKEN_GREATER_EQUAL] = { NULL,     binary, PREC_EQUALITY },
        [TOKEN_LESS]          = { NULL,     binary, PREC_EQUALITY },
        [TOKEN_LESS_EQUAL]    = { NULL,     binary, PREC_EQUALITY },
        [TOKEN_IDENTIFIER]    = { variable,     NULL,   PREC_NONE },
        [TOKEN_STRING]        = { string,   NULL,   PREC_NONE },
        [TOKEN_NUMBER]        = { number,   NULL,   PREC_NONE },
        [TOKEN_AND]           = { NULL,     and_,   PREC_AND },
        [TOKEN_CLASS]         = { NULL,     NULL,   PREC_NONE },
        [TOKEN_ELSE]          = { NULL,     NULL,   PREC_NONE },
        [TOKEN_FALSE]         = { literal,  NULL,   PREC_NONE },
        [TOKEN_FOR]           = { NULL,     NULL,   PREC_NONE },
        [TOKEN_FUN]           = { NULL,     NULL,   PREC_NONE },
        [TOKEN_IF]            = { NULL,     NULL,   PREC_NONE },
        [TOKEN_NIL]           = { literal,  NULL,   PREC_NONE },
        [TOKEN_OR]            = { NULL,     or_,   PREC_OR },
        [TOKEN_PRINT]         = { NULL,     NULL,   PREC_NONE },
        [TOKEN_RETURN]        = { NULL,     NULL,   PREC_NONE },
        [TOKEN_SUPER]         = { NULL,     NULL,   PREC_NONE },
        [TOKEN_THIS]          = { NULL,     NULL,   PREC_NONE },
        [TOKEN_TRUE]          = { literal,  NULL,   PREC_NONE },
        [TOKEN_VAR]           = { NULL,     NULL,   PREC_NONE },
        [TOKEN_CONST]         = { NULL,     NULL,   PREC_NONE },
        [TOKEN_WHILE]         = { NULL,     NULL,   PREC_NONE },
        [TOKEN_ERROR]         = { NULL,     NULL,   PREC_NONE },
        [TOKEN_EOF]           = { NULL,     NULL,   PREC_NONE },
};

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void expression(Compiler* compiler) {
    parsePrecendence(compiler, PREC_ASSIGNMENT);
}

ObjFunction* compile(Table* strings, Globals* globals, Obj** objectRoot, const char* source) {
    Scanner scanner;
    initScanner(&scanner, source);

    CompilationContext scriptContext;
    initCompilationContext(objectRoot, &scriptContext, TYPE_SCRIPT);

    Compiler compiler;
    initCompiler(&compiler);
    compiler.scanner = &scanner;

    compiler.context = &scriptContext;
    compiler.objectRoot = objectRoot;
    compiler.globals = globals;
    compiler.internedStrings = strings;

    advance(&compiler);

    while (!match(&compiler, TOKEN_EOF)) {
        declaration(&compiler);
    }

    ObjFunction* function = endCompilation(&compiler);
    return compiler.hadError ? NULL : function;
}

#pragma clang diagnostic pop
