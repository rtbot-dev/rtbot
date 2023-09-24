grammar rtbot;

start : (initialization)? '---' (expr|declaration)+ ;
initialization: streamId '(' entries ')' ;
entries: (COMA)+ ;
declaration: streamId '=' expr ;

expr  : literal                                                                       # LiteralTerm
      | identifier                                                                    # IdentifierTerm
      | stream                                                                        # StreamTerm
      |  '(' expr ')'                                                                 # Parenthesis
      | '!' expr                                                                      # Not
      | '-' expr                                                                      # UnaryMinus
      | left=expr '*' right=expr                                                      # Multiply
      | left=expr '/' right=expr                                                      # Divide
      | left=expr '%' right=expr                                                      # Module
      | left=expr '+' right=expr                                                      # Plus
      | left=expr '-' right=expr                                                      # Minus
      | left=expr '==' right=expr                                                     # EqualTo
      | left=expr '<'  right=expr                                                     # LessThan
      | left=expr '>'  right=expr                                                     # GreaterThan
      | left=expr '>=' right=expr                                                     # GreaterThanOrEqualTo
      | left=expr '<=' right=expr                                                     # LessThanOrEqualTo
      | left=expr '&&' right=expr                                                     # And
      | left=expr '||' right=expr                                                     # Or
      ;



stream: streamId | operator;
operator: operatorName '('  arguments  ')' ;
operatorName: ID;
arguments: ( '[' exprs ']' COMA '[' (exprs)? ']' COMA '[' (exprs)? ']' ) | ( '[' exprs ']' COMA '[' (exprs)? ']' ) | ( '[' exprs ']' );
exprs: (expr (COMA expr)*);
streamId: STREAMID;
identifier: ID;
literal: intLiteral | floatLiteral;
intLiteral: INT_LITERAL;
floatLiteral: DOUBLE_LITERAL;

COMA: ',';
INT_LITERAL: [1-9][0-9]* | '0';
DOUBLE_LITERAL: [0-9]*.[0-9]+ ;
ID : [a-zA-Z_][a-zA-Z0-9_]*;
STREAMID: [a-zA-Z_][a-zA-Z0-9_]*('$');

WS    : [ \t\r\n]+ -> skip ;
