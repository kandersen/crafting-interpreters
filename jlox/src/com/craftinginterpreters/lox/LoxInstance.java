package com.craftinginterpreters.lox;

import java.util.AbstractMap;
import java.util.HashMap;
import java.util.Map;

public class LoxInstance {
    private LoxClass klass;
    private final Map<Object, Object> fields = new HashMap<>();

    LoxInstance(LoxClass klass) {
        this.klass = klass;
    }

    @Override
    public String toString() {
        return klass.name + " instance";
    }

    public Object get(Token name) {
        if (fields.containsKey(name.lexeme)) {
            return fields.get(name.lexeme);
        }

        throw new RuntimeError(name, "Undefined property '" + name.lexeme + "'.");
    }
}


