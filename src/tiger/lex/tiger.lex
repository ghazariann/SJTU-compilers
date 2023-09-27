%filenames = "scanner"

  %x COMMENT STR IGNORE
  %%

  // skip white space chars. space, tabs and LF
  [ \t]+ {adjust();}
  \n {adjust(); errormsg_->Newline();}

  // Basic specifications
    "," {adjust(); return Parser::COMMA;}
    ":" {adjust(); return Parser::COLON;}
    ";" {adjust(); return Parser::SEMICOLON;}
    "(" {adjust(); return Parser::LPAREN;}
    ")" {adjust(); return Parser::RPAREN;}
    "{" {adjust(); return Parser::LBRACE;}
    "}" {adjust(); return Parser::RBRACE;}
    "[" {adjust(); return Parser::LBRACK;}
    "]" {adjust(); return Parser::RBRACK;}
    "." {adjust(); return Parser::DOT;}
    "+" {adjust(); return Parser::PLUS;}
    "-" {adjust(); return Parser::MINUS;}
    "*" {adjust(); return Parser::TIMES;}
    "/" {adjust(); return Parser::DIVIDE;}
    "=" {adjust(); return Parser::EQ;}
    "<>" {adjust(); return Parser::NEQ;}
    "<" {adjust(); return Parser::LT;}
    "<=" {adjust(); return Parser::LE;}
    ">" {adjust(); return Parser::GT;}
    ">=" {adjust(); return Parser::GE;}
    "&" {adjust(); return Parser::AND;}
    "|" {adjust(); return Parser::OR;}
    ":=" {adjust(); return Parser::ASSIGN;}

  /* reserved words */
    "array" {adjust(); return Parser::ARRAY;}
    "if" {adjust(); return Parser::IF;}
    "then" {adjust(); return Parser::THEN;}
    "else" {adjust(); return Parser::ELSE;}
    "while" {adjust(); return Parser::WHILE;}
    "for" {adjust(); return Parser::FOR;}
    "to" {adjust(); return Parser::TO;}
    "do" {adjust(); return Parser::DO;}
    "let" {adjust(); return Parser::LET;}
    "in" {adjust(); return Parser::IN;}
    "end" {adjust(); return Parser::END;}
    "of" {adjust(); return Parser::OF;}
    "break" {adjust(); return Parser::BREAK;}
    "nil" {adjust(); return Parser::NIL;}
    "function" {adjust(); return Parser::FUNCTION;}
    "var" {adjust(); return Parser::VAR;}
    "type" {adjust(); return Parser::TYPE;}

  // Identifiers and Integers
  [a-zA-Z_][a-zA-Z0-9_]* {adjust();string_buf_ = matched(); return Parser::ID;} // if inside the string, update string_buf_
  [0-9]+ {adjust(); string_buf_ = matched();return Parser::INT;} 


  // Comments handling
  "/*" {adjust(); comment_level_ = 0; begin(StartCondition__::COMMENT); }
  <COMMENT>{
    "*/"  {adjustStr(); if (comment_level_) comment_level_--; else begin(StartCondition__::INITIAL); } // finished the comment, no return, back to initial
    \n|.  {adjustStr(); } // ignore \n or any character inside the comment 
    "/*"  {adjustStr(); comment_level_++; } // more layer 
  }
  //Strings handling
  \" {adjust(); begin(StartCondition__::STR); string_buf_.clear(); }
<STR>{
    \"    {adjustStr(); begin(StartCondition__::INITIAL); setMatched(string_buf_); return Parser::STRING; } // end of string, returnt he string, back to inital
    \\n   {adjustStr(); string_buf_ += '\n'; } // new line in string 
    \\t   {adjustStr(); string_buf_ += '\t'; } // tab in string 

    // here first and third \ are for escaping second and fourth (\ and ") lex charecters
    \\\"  {adjustStr(); string_buf_ += '\"'; } // escape * in string, see test 52
    \\\\  {adjustStr(); string_buf_ += '\\'; } // escape \ in stirng 
    \\[0-9]{3}      {adjustStr(); string_buf_ += (char)atoi(matched().c_str() + 1); } // recognize ascii charecter
    \\[ \n\t\f]+\\  {adjustStr(); }
    \\\^[A-Z]       {adjustStr(); string_buf_ += matched()[2] - 'A' + 1; }
    .     {adjustStr(); string_buf_ += matched(); }
}

// End of file handling
<<EOF>>  return 0;

// Error handling 
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
