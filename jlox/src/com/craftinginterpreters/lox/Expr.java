package com.craftinginterpreters.lox;

public abstract class Expr {

    static class Binary extends Expr {

        Binary(Expr left, Token operator, Expr right) {
            this.left = left;
            this.operator = operator;
            this.right = right;
        }

        final Expr left;
        final Token operator;
        final Expr right;

    }

    static class Unary extends Expr {

        Unary(Token operator, Expr operand) {
            this.operator = operator;
            this.operand = operand;
        }

        final Token operator;
        final Expr operand;

    }

}
