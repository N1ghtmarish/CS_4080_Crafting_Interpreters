package com.craftinginterpreters.lox;

import java.util.List;

import static com.craftinginterpreters.lox.TokenType.*;

class Parser {
  private static class ParseError extends RuntimeException {}

  private final List<Token> tokens;
  private int current = 0;

  Parser(List<Token> tokens) {
    this.tokens = tokens;
  }
  Expr parse() {
    try {
      return expression();
    } catch (ParseError error) {
      return null;
    }
  }
  private Expr expression() {
    return comma();
  }

  // Lowest precedence.
  private Expr comma() {
    Expr expr = conditional();

    while (match(COMMA)) {
      Token operator = previous();
      Expr right = conditional();
      expr = new Expr.Binary(expr, operator, right);
    }

    return expr;
  }

  private Expr conditional() {
    Expr expr = equality();

    if (match(QUESTION)) {
      Expr thenBranch = expression();
      consume(COLON, "Expect ':' after then branch of conditional expression.");
      Expr elseBranch = conditional();
      expr = new Expr.Conditional(expr, thenBranch, elseBranch);
    }

    return expr;
  }
  private Expr equality() {
    // Error production: missing left-hand operand.
    if (match(BANG_EQUAL, EQUAL_EQUAL)) {
      Token operator = previous();
      error(operator, "Missing left-hand operand.");
      Expr right = comparison();
      return right;
    }

    Expr expr = comparison();

    while (match(BANG_EQUAL, EQUAL_EQUAL)) {
      Token operator = previous();
      Expr right = comparison();
      expr = new Expr.Binary(expr, operator, right);
    }

    return expr;
  }
  private Expr comparison() {
    // Error production: missing left-hand operand.
    if (match(GREATER, GREATER_EQUAL, LESS, LESS_EQUAL)) {
      Token operator = previous();
      error(operator, "Missing left-hand operand.");
      Expr right = term();
      return right;
    }

    Expr expr = term();

    while (match(GREATER, GREATER_EQUAL, LESS, LESS_EQUAL)) {
      Token operator = previous();
      Expr right = term();
      expr = new Expr.Binary(expr, operator, right);
    }

    return expr;
  }
  private Expr term() {
    // Error production: missing left-hand operand.
    // Note: '-' is a valid unary operator, so we only treat leading '+' as an error.
    if (match(PLUS)) {
      Token operator = previous();
      error(operator, "Missing left-hand operand.");
      Expr right = factor();
      return right;
    }

    Expr expr = factor();

    while (match(MINUS, PLUS)) {
      Token operator = previous();
      Expr right = factor();
      expr = new Expr.Binary(expr, operator, right);
    }

    return expr;
  }
  private Expr factor() {
    // Error production: missing left-hand operand.
    if (match(SLASH, STAR)) {
      Token operator = previous();
      error(operator, "Missing left-hand operand.");
      Expr right = unary();
      return right;
    }

    Expr expr = unary();

    while (match(SLASH, STAR)) {
      Token operator = previous();
      Expr right = unary();
      expr = new Expr.Binary(expr, operator, right);
    }

    return expr;
  }
  private Expr unary() {
    if (match(BANG, MINUS)) {
      Token operator = previous();
      Expr right = unary();
      return new Expr.Unary(operator, right);
    }

    return primary();
  }
  private Expr primary() {
    if (match(FALSE)) return new Expr.Literal(false);
    if (match(TRUE)) return new Expr.Literal(true);
    if (match(NIL)) return new Expr.Literal(null);

    if (match(NUMBER, STRING)) {
      return new Expr.Literal(previous().literal);
    }

    if (match(LEFT_PAREN)) {
      Expr expr = expression();
      consume(RIGHT_PAREN, "Expect ')' after expression.");
      return new Expr.Grouping(expr);
    }

    throw error(peek(), "Expect expression.");
  }
  private boolean match(TokenType... types) {
    for (TokenType type : types) {
      if (check(type)) {
        advance();
        return true;
      }
    }

    return false;
  }
  private Token consume(TokenType type, String message) {
    if (check(type)) return advance();

    throw error(peek(), message);
  }
  private boolean check(TokenType type) {
    if (isAtEnd()) return false;
    return peek().type == type;
  }
  private Token advance() {
    if (!isAtEnd()) current++;
    return previous();
  }
  private boolean isAtEnd() {
    return peek().type == EOF;
  }

  private Token peek() {
    return tokens.get(current);
  }

  private Token previous() {
    return tokens.get(current - 1);
  }
  private ParseError error(Token token, String message) {
    Lox.error(token, message);
    return new ParseError();
  }
  private void synchronize() {
    advance();

    while (!isAtEnd()) {
      if (previous().type == SEMICOLON) return;

      switch (peek().type) {
        case CLASS:
        case FUN:
        case VAR:
        case FOR:
        case IF:
        case WHILE:
        case PRINT:
        case RETURN:
          return;
      }

      advance();
    }
  }
}
