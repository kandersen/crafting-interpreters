#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scanner.h"
#include "chunk.h"
#include "object.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

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

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
    VarState globalStates[UINT8_COUNT];
} Compiler;

static void initCompiler(VM* vm, Compiler* compiler, FunctionType type) {
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction(vm);

    Local* local = &compiler->locals[compiler->localCount++];
    local->depth = 0;
    local->name.start = "";
    local->name.length = 0;
}

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
    Scanner* scanner;
    Compiler currentCompiler;
    VM* vm;
} Parser;


typedef void (*ParseFn)(Parser*, bool);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;


static void errorAt(Parser* parser, Token* token, const char* message) {
    if (parser->panicMode) return;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;
}

static void errorAtCurrent(Parser* parser) {
    errorAt(parser, &parser->current, parser->current.start);
}

static void error(Parser* parser, const char* message) {
    errorAt(parser, &parser->previous, message); 
}


static void advance(Parser* parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = scanToken(parser->scanner);
        if (parser->current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser);
    }

    
}

static void initParser(VM* vm, Parser* parser) {
    parser->hadError = false;
    parser->panicMode = false;
    initCompiler(vm, &parser->currentCompiler, TYPE_SCRIPT);
    parser->currentCompiler.enclosing = NULL;
}

static void consume(Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }

    error(parser, message);
}

static bool check(Parser* parser, TokenType type) {
    return parser->current.type == type;
}

static bool match(Parser* parser, TokenType type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

static Chunk* currentChunk(Parser* parser) {
    return &parser->currentCompiler.function->chunk;
}

static void emitByte(Parser* parser, uint8_t byte) {
    writeChunk(currentChunk(parser), byte, parser->previous.line);
}

static void emitReturn(Parser* parser) {
    emitByte(parser, OP_NIL);
    emitByte(parser, OP_RETURN);
}

static void emitBytes(Parser* parser, uint8_t byte1, uint8_t byte2) {
    emitByte(parser, byte1);
    emitByte(parser, byte2);
}

static void emitLoop(Parser* parser, int loopStart) {
    emitByte(parser, OP_LOOP);

    int offset = currentChunk(parser)->count - loopStart + 2;
    if (offset > UINT16_MAX) error(parser, "Loop body too large.");

    emitByte(parser, (offset >> 8) & 0xff);
    emitByte(parser, offset & 0xff);
}

static int emitJump(Parser* parser, uint8_t instruction) {
    emitByte(parser, instruction);
    emitByte(parser, 0xff);
    emitByte(parser, 0xff);
    return currentChunk(parser)->count - 2;
}

static void patchJump(Parser* parser, int offset) {
    int jump = currentChunk(parser)->count - offset - 2;
    if (jump > UINT16_MAX) {
        error(parser, "Too much code to jump over.");
    }

    currentChunk(parser)->code[offset] = (jump >> 8) & 0xff;
    currentChunk(parser)->code[offset + 1] = jump & 0xff;
}


static ObjFunction* endCompiler(Parser* parser) {
    emitReturn(parser);
    ObjFunction* function = parser->currentCompiler.function;
#ifdef DEBUG_PRINT_CODE
    if (!parser->hadError) {
        disassembleChunk(currentChunk(parser), function->name != NULL ? function->name->chars : "<script>");
    }
#endif
    parser->currentCompiler = *parser->currentCompiler.enclosing;
    return function;
}

static void beginScope(Parser* parser) {
    parser->currentCompiler.scopeDepth++;
}

static void endScope(Parser* parser) {
    parser->currentCompiler.scopeDepth--;

    while (parser->currentCompiler.localCount > 0 &&
           parser->currentCompiler.locals[parser->currentCompiler.localCount - 1].depth > parser->currentCompiler.scopeDepth) {
        emitByte(parser, OP_POP); //TODO(kjaa): add an OP_POPN, instead of many at a time.
        parser->currentCompiler.localCount--;
    }
}

static void expression(Parser* parser);
static void statement(Parser* parser);
static void declaration(Parser* parser);
static ParseRule* getRule(TokenType type);

static uint8_t makeConstant(Parser* parser, Value value) {
    int constant = addConstant(currentChunk(parser), value);
    if (constant > UINT8_MAX) {
        error(parser, "Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(Parser* parser, Value value) {
    emitBytes(parser, OP_CONSTANT, makeConstant(parser, value));
}

static void number(Parser* parser, bool canAssign) {
    double value = strtod(parser->previous.start, NULL);
    emitConstant(parser, NUMBER_VAL(value));
}

static void string(Parser* parser, bool canAssign) {
    emitConstant(parser, OBJ_VAL(copyString(parser->vm,
                                            parser->previous.start + 1,
                                            parser->previous.length - 2)));
}

static void parsePrecendence(Parser* parser, Precedence precedence) {
    advance(parser);
    ParseFn prefixRule = getRule(parser->previous.type)->prefix;
    if (prefixRule == NULL) {
        error(parser, "Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(parser, canAssign);
    
    while (precedence <= getRule(parser->current.type)->precedence) {
        advance(parser);
        ParseFn infixRule = getRule(parser->previous.type)->infix;
        infixRule(parser, true); // TODO(kjaa): NO IDEA if this is right, it might not need canAssign.
    }

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        error(parser, "Invalid assignment target.");
    }
}

static uint8_t identifierConstant(Parser* parser, Token* name) {
    Value index;
    ObjString* identifier = copyString(parser->vm, name->start, name->length);
    if (tableGet(&parser->vm->globalNames, identifier, &index)) {
        return (uint8_t)AS_NUMBER(index);
    }

    uint8_t newIndex = (uint8_t)parser->vm->globalValues.count;
    writeValueArray(&parser->vm->globalValues, UNDEFINED_VAL);

    tableSet(&parser->vm->globalNames, identifier, NUMBER_VAL((double)newIndex));
    return newIndex;
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Parser* parser, Token* name) {
    Compiler* compiler = &parser->currentCompiler;
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->state == VAR_UNINITIALIZED) {
                error(parser, "Cant read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static void addLocal(Parser* parser, Token name) {
    if (parser->currentCompiler.localCount == UINT8_COUNT) {
        error(parser, "Too many local variables in function.");
        return;
    }

    Local* local = &parser->currentCompiler.locals[parser->currentCompiler.localCount++];
    local->name = name;
    local->depth = parser->currentCompiler.scopeDepth;
    local->state = VAR_UNINITIALIZED;
}

static void declareVariable(Parser* parser) {
    if (parser->currentCompiler.scopeDepth == 0) return;

    Token* name = &parser->previous;
    for (int i = parser->currentCompiler.localCount - 1; i >= 0; i--) {
        Local* local = &parser->currentCompiler.locals[i];
        if (local->depth != -1 && local->depth < parser->currentCompiler.scopeDepth) {
            break;
        }
        if (identifiersEqual(name, &local->name)) {
            error(parser, "Already variable with this name in this scope.");
        }
    }


    addLocal(parser, *name);
}

static void namedVariable(Parser* parser, Token name, bool canAssign) {
    uint8_t getOp, setOp;
    VarState varState;
    int arg = resolveLocal(parser, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
        varState = parser->currentCompiler.locals[arg].state;
    } else {
        arg = identifierConstant(parser, &name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
        varState = parser->currentCompiler.globalStates[arg];
    }

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        if (varState != VAR_WRITEABLE) {
            error(parser, "Writing to const variable.");
        }
        expression(parser);
        emitBytes(parser, setOp, (uint8_t) arg);
    } else {
        emitBytes(parser, getOp, (uint8_t) arg);
    }
}

static void variable(Parser* parser, bool canAssign) {
    namedVariable(parser, parser->previous, canAssign);
}

static uint8_t parseVariable(Parser* parser, const char* errorMessage) {
    consume(parser, TOKEN_IDENTIFIER, errorMessage);

    declareVariable(parser);
    if (parser->currentCompiler.scopeDepth > 0) return 0;

    return identifierConstant(parser, &parser->previous);
}

static void markInitialized(Compiler* compiler, VarState varState) {
    if (compiler->scopeDepth == 0) return;
    compiler->locals[compiler->localCount - 1].state = varState;
}

static void defineVariable(Parser* parser, uint8_t global, VarState varState) {
    // If Local Variable
    if (parser->currentCompiler.scopeDepth > 0) {
        markInitialized(&parser->currentCompiler, varState);
        return;
    }
    // Else, global variable
    parser->currentCompiler.globalStates[global] = varState;
    emitBytes(parser, OP_DEFINE_GLOBAL, global);
}

static uint8_t argumentList(Parser* parser) {
    uint8_t argCount = 0;
    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            expression(parser);

            if (argCount == 255) {
                error(parser, "Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(parser, TOKEN_COMMA));
    }

    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static void and_(Parser* parser, bool canAssign) {
    int endJump = emitJump(parser, OP_JUMP_IF_FALSE);

    emitByte(parser, OP_POP);
    parsePrecendence(parser, PREC_AND);

    patchJump(parser, endJump);
}

static void or_(Parser* parser, bool canAssign) {
    int elseJump = emitJump(parser, OP_JUMP_IF_FALSE);
    int endJump = emitJump(parser, OP_JUMP);

    patchJump(parser, elseJump);
    emitByte(parser, OP_POP);

    parsePrecendence(parser, PREC_OR);
    patchJump(parser, endJump);
}


static void expression(Parser* parser) {
    parsePrecendence(parser, PREC_ASSIGNMENT);
}

static void block(Parser* parser) {
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        declaration(parser);
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after block");
}

static void function(Parser* parser, FunctionType type) {
    Compiler compiler;
    initCompiler(parser->vm, &compiler, type);
    compiler.enclosing = &parser->currentCompiler;
    parser->currentCompiler = compiler;


    beginScope(parser);

    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before function body");
    block(parser);

    ObjFunction* function = endCompiler(parser);
    emitBytes(parser, OP_CONSTANT, makeConstant(parser, OBJ_VAL(function)));
}

static void funDeclaration(Parser* parser) {
    uint8_t global = parseVariable(parser, "Expect function name.");
    markInitialized(&parser->currentCompiler, VAR_READABLE);
    function(parser, TYPE_FUNCTION);
    defineVariable(parser, global, VAR_WRITEABLE);
}

static void varDeclaration(Parser* parser, VarState varState) {
    uint8_t global = parseVariable(parser, "Expect variable name.");

    if (match(parser, TOKEN_EQUAL)) {
        expression(parser);
    } else {
        emitByte(parser, OP_NIL);
    }
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(parser, global, varState);
}

static void printStatement(Parser* parser) {
    expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(parser, OP_PRINT);
}

static void returnStatement(Parser* parser) {
    if (parser->current.type == TYPE_SCRIPT) {
        error(parser, "Can't return from top-level code.");
    }

    if (match(parser, TOKEN_SEMICOLON)) {
        emitReturn(parser);
    } else {
        expression(parser);
        consume(parser, TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(parser, OP_RETURN);
    }
}



static void synchronize(Parser* parser) {
    parser->panicMode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON) return;

        switch (parser->current.type) {
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

        advance(parser);
    }
}

static void declaration(Parser* parser) {
    if (match(parser, TOKEN_FUN)) {
        funDeclaration(parser);
    } else if (match(parser, TOKEN_VAR)) {
        varDeclaration(parser, VAR_WRITEABLE);
    } else if (match(parser, TOKEN_CONST)) {
        varDeclaration(parser, VAR_READABLE);
    } else {
        statement(parser);
    }

    if (parser->panicMode) synchronize(parser);
}

static void expressionStatement(Parser* parser) {
    expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(parser, OP_POP);
}

static void forStatement(Parser* parser) {
    beginScope(parser);

    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(parser, TOKEN_SEMICOLON)) {
        // No initializer;
    } else if (match(parser, TOKEN_VAR)) {
        varDeclaration(parser, VAR_WRITEABLE);
    } else {
        expressionStatement(parser);
    }
    consume(parser, TOKEN_SEMICOLON, "Expect ';'.");

    int loopStart = currentChunk(parser)->count;

    int exitJump = -1;
    if (!match(parser, TOKEN_SEMICOLON)) {
        expression(parser);
        consume(parser, TOKEN_SEMICOLON, "Expect ';' after loop condition");

        exitJump = emitJump(parser, OP_JUMP_IF_FALSE);
        emitByte(parser, OP_POP);
    }

    consume(parser, TOKEN_SEMICOLON, "Expect ';'.");

    if (!match(parser, TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(parser, OP_JUMP);

        int incrementStart = currentChunk(parser)->count;
        expression(parser);
        emitByte(parser, OP_POP);
        consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(parser, loopStart);
        loopStart = incrementStart;
        patchJump(parser, bodyJump);
    }

    statement(parser);

    emitLoop(parser, loopStart);

    if (exitJump != -1) {
        patchJump(parser, exitJump);
        emitByte(parser, OP_POP);
    }

    endScope(parser);
}

static void ifStatement(Parser* parser) {
    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "expect ')' after condition.");

    int thenJump = emitJump(parser, OP_JUMP_IF_FALSE);
    emitByte(parser, OP_POP);
    statement(parser);

    int elseJump = emitJump(parser, OP_JUMP);
    patchJump(parser, thenJump);
    emitByte(parser, OP_POP);

    if (match(parser,TOKEN_ELSE)) statement(parser);
    patchJump(parser, elseJump);
}

static void whileStatement(Parser* parser) {
    int loopStart = currentChunk(parser)->count;

    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(parser, OP_JUMP_IF_FALSE);

    emitByte(parser, OP_POP);
    statement(parser);

    emitLoop(parser, loopStart);

    patchJump(parser, exitJump);
    emitByte(parser, OP_POP);
}

static void statement(Parser* parser) {
    if (match(parser, TOKEN_PRINT)) {
        printStatement(parser);
    } else if (match(parser, TOKEN_FOR)) {
        forStatement(parser);
    } else if (match(parser, TOKEN_IF)) {
        ifStatement(parser);
    } else if (match(parser, TOKEN_RETURN)) {
        returnStatement(parser);
    } else if (match(parser, TOKEN_WHILE)) {
        whileStatement(parser);
    } else if (match(parser, TOKEN_LEFT_BRACE)) {
        beginScope(parser);
        block(parser);
        endScope(parser);
    } else {
        expressionStatement(parser);
    }
}

static void grouping(Parser* parser, bool canAssign) {
    expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(Parser* parser, bool canAssign) {
    //TODO(kjaa): See note on error reporting in 17.4.3
    TokenType operatorType = parser->previous.type;
    parsePrecendence(parser, PREC_UNARY);

    switch (operatorType) {
        case TOKEN_BANG: emitByte(parser, OP_NOT); break;
        case TOKEN_MINUS: emitByte(parser, OP_NEGATE); break;
        default:
            return; // Unreachable
    }
}

static void binary(Parser* parser, bool canAssign) {
    TokenType operatorType = parser->previous.type;

    ParseRule* rule = getRule(operatorType);
    parsePrecendence(parser, (Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_PLUS: emitByte(parser, OP_ADD); break;
        case TOKEN_MINUS: emitByte(parser, OP_SUBTRACT); break;
        case TOKEN_STAR: emitByte(parser, OP_MULTIPLY); break;
        case TOKEN_SLASH: emitByte(parser, OP_DIVIDE); break;
        case TOKEN_BANG_EQUAL: emitBytes(parser, OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL: emitByte(parser, OP_EQUAL); break;
        case TOKEN_LESS: emitByte(parser, OP_LESS); break;
        case TOKEN_GREATER: emitByte(parser, OP_GREATER); break;
        case TOKEN_LESS_EQUAL: emitBytes(parser, OP_GREATER, OP_NOT); break;
        case TOKEN_GREATER_EQUAL: emitBytes(parser, OP_LESS, OP_NOT); break;
        default:
            return; // Unreachable
    }
}

static void call(Parser* parser, bool canAssign) {
    uint8_t argCount = argumentList(parser);
    emitBytes(parser, OP_CALL, argCount);
}

static void literal(Parser* parser, bool canAssign) {
    switch (parser->previous.type) {
        case TOKEN_NIL: emitByte(parser, OP_NIL); break;
        case TOKEN_TRUE: emitByte(parser, OP_TRUE); break;
        case TOKEN_FALSE: emitByte(parser, OP_FALSE); break;
        default:
            return; // Unreachable
    }
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

ObjFunction* compile(VM* vm, const char* source) {
    Scanner scanner;
    initScanner(&scanner, source);

    Parser parser;
    parser.scanner = &scanner;
    parser.vm = vm;
    initParser(vm, &parser);

    advance(&parser);
    
    while (!match(&parser, TOKEN_EOF)) {
        declaration(&parser);
    }
    
    ObjFunction* function = endCompiler(&parser);
    return parser.hadError ? NULL : function;
}