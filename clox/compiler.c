#include <stdio.h>
#include <stdlib.h>

#include "scanner.h"
#include "chunk.h"
#include "object.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
    Scanner* scanner;
    Chunk* compilingChunk;
    VM* vm;
} Parser;

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

static void initParser(Parser* parser, Chunk* chunk) {
    parser->hadError = false;
    parser->panicMode = false;
    parser->compilingChunk = chunk;
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
    return parser->compilingChunk;
}

static void emitByte(Parser* parser, uint8_t byte) {
    writeChunk(currentChunk(parser), byte, parser->previous.line);
}

static void emitReturn(Parser* parser) {
    emitByte(parser, OP_RETURN);
}

static void emitBytes(Parser* parser, uint8_t byte1, uint8_t byte2) {
    emitByte(parser, byte1);
    emitByte(parser, byte2);
}

static void endCompiler(Parser* parser) {
    emitReturn(parser);
#ifdef DEBUG_PRINT_CODE
    if (!parser->hadError) {
        disassembleChunk(currentChunk(parser), "code");
    }
#endif
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

static void namedVariable(Parser* parser, Token name, bool canAssign) {
    uint8_t arg = identifierConstant(parser, &name);

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        expression(parser);
        emitBytes(parser, OP_SET_GLOBAL, arg);
    } else {
        emitBytes(parser, OP_GET_GLOBAL, arg);
    }
}

static void variable(Parser* parser, bool canAssign) {
    namedVariable(parser, parser->previous, canAssign);
}

static uint8_t parseVariable(Parser* parser, const char* errorMessage) {
    consume(parser, TOKEN_IDENTIFIER, errorMessage);
    return identifierConstant(parser, &parser->previous);
}

static void defineVariable(Parser* parser, uint8_t global) {
    emitBytes(parser, OP_DEFINE_GLOBAL, global);
}

static void expression(Parser* parser) {
    parsePrecendence(parser, PREC_ASSIGNMENT);
}

static void varDeclaration(Parser* parser) {
    uint8_t global = parseVariable(parser, "Expect variable name.");

    if (match(parser, TOKEN_EQUAL)) {
        expression(parser);
    } else {
        emitByte(parser, OP_NIL);
    }
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(parser, global);
}

static void printStatement(Parser* parser) {
    expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(parser, OP_PRINT);
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
    if (match(parser, TOKEN_VAR)) {
        varDeclaration(parser);
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

static void statement(Parser* parser) {
    if (match(parser, TOKEN_PRINT)) {
        printStatement(parser);
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
    [TOKEN_LEFT_PAREN]    = { grouping, NULL,   PREC_NONE },
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
    [TOKEN_AND]           = { NULL,     NULL,   PREC_NONE },
    [TOKEN_CLASS]         = { NULL,     NULL,   PREC_NONE },
    [TOKEN_ELSE]          = { NULL,     NULL,   PREC_NONE },
    [TOKEN_FALSE]         = { literal,  NULL,   PREC_NONE },
    [TOKEN_FOR]           = { NULL,     NULL,   PREC_NONE },
    [TOKEN_FUN]           = { NULL,     NULL,   PREC_NONE },
    [TOKEN_IF]            = { NULL,     NULL,   PREC_NONE },
    [TOKEN_NIL]           = { literal,  NULL,   PREC_NONE },
    [TOKEN_OR]            = { NULL,     NULL,   PREC_NONE },
    [TOKEN_PRINT]         = { NULL,     NULL,   PREC_NONE },
    [TOKEN_RETURN]        = { NULL,     NULL,   PREC_NONE },
    [TOKEN_SUPER]         = { NULL,     NULL,   PREC_NONE },
    [TOKEN_THIS]          = { NULL,     NULL,   PREC_NONE },
    [TOKEN_TRUE]          = { literal,  NULL,   PREC_NONE },
    [TOKEN_VAR]           = { NULL,     NULL,   PREC_NONE },
    [TOKEN_WHILE]         = { NULL,     NULL,   PREC_NONE },
    [TOKEN_ERROR]         = { NULL,     NULL,   PREC_NONE },
    [TOKEN_EOF]           = { NULL,     NULL,   PREC_NONE },
};

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

bool compile(VM* vm, const char* source, Chunk* chunk) {
    Scanner scanner;
    initScanner(&scanner, source);

    Parser parser;
    parser.scanner = &scanner;
    parser.vm = vm;
    initParser(&parser, chunk);

    advance(&parser);
    
    while (!match(&parser, TOKEN_EOF)) {
        declaration(&parser);
    }
    
    endCompiler(&parser);
    return !parser.hadError;
}