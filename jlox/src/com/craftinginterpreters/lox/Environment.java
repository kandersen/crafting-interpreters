package com.craftinginterpreters.lox;

import java.util.HashMap;
import java.util.Map;

class Environment {

     class Value {
         public boolean written;
         public Object value;

         Value() {
             this.written = false;
             this.value = null;
         }

         Value(Object value) {
             this.written = true;
             this.value = value;
         }
     }

    final Environment enclosing;
    private final Map<String, Value> values = new HashMap<>();

    Environment() {
        enclosing = null;
    }

    Environment(Environment parent) {
        enclosing = parent;
    }

    void define(String name) {
        values.put(name, new Value());
    }

    void define(String name, Object value) {
        values.put(name, new Value(value));
    }

    Object get(Token name) {
        if (values.containsKey(name.lexeme)) {
            Value val = values.get(name.lexeme);
            if (!val.written) throw new RuntimeError(name, "Accessing unwritten variable '" + name.lexeme + "'.");
            return val.value;
        }

        if (enclosing != null) return enclosing.get(name);

        throw new RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
    }

    void assign(Token name, Object value) {
        if (values.containsKey(name.lexeme)) {
            Value val = values.get(name.lexeme);
            val.written = true;
            val.value = value;
            return;
        }

        if (enclosing != null) {
            enclosing.assign(name, value);
            return;
        }

        throw new RuntimeError(name, "Undefined variable '" + name.lexeme + "'.");
    }
}
