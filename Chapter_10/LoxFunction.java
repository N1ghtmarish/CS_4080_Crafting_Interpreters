package com.craftinginterpreters.lox;

import java.util.List;

class LoxFunction implements LoxCallable {
  private final Token name; // May be null for anonymous functions.
  private final Expr.Function function;
  private final Environment closure;

  LoxFunction(Token name, Expr.Function function, Environment closure) {
    this.name = name;
    this.function = function;
    this.closure = closure;
  }

  @Override
  public String toString() {
    if (name == null) return "<fn>";
    return "<fn " + name.lexeme + ">";
  }

  @Override
  public int arity() {
    return function.params.size();
  }

  @Override
  public Object call(Interpreter interpreter, List<Object> arguments) {
    Environment environment = new Environment(closure);
    for (int i = 0; i < function.params.size(); i++) {
      environment.define(function.params.get(i).lexeme, arguments.get(i));
    }

    try {
      interpreter.executeBlock(function.body, environment);
    } catch (Return returnValue) {
      return returnValue.value;
    }
    return null;
  }
}
