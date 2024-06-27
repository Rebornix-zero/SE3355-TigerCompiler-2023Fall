%filenames = "scanner"

/*
  * Please don't modify the lines above.
  */

/* You can add lex definitions here. */
/* TODO: Put your lab2 code here */

%x COMMENT STRING IGNORE

%%

 /*
  * Below is examples, which you can wipe out
  * and write regular expressions and actions of your own.
  *
  * All the tokens:
  *   Parser::ID
  *   Parser::STRING
  *   Parser::INT
  *   Parser::COMMA
  *   Parser::COLON
  *   Parser::SEMICOLON
  *   Parser::LPAREN
  *   Parser::RPAREN
  *   Parser::LBRACK
  *   Parser::RBRACK
  *   Parser::LBRACE
  *   Parser::RBRACE
  *   Parser::DOT
  *   Parser::PLUS
  *   Parser::MINUS
  *   Parser::TIMES
  *   Parser::DIVIDE
  *   Parser::EQ
  *   Parser::NEQ
  *   Parser::LT
  *   Parser::LE
  *   Parser::GT
  *   Parser::GE
  *   Parser::AND
  *   Parser::OR
  *   Parser::ASSIGN
  *   Parser::ARRAY
  *   Parser::IF
  *   Parser::THEN
  *   Parser::ELSE
  *   Parser::WHILE
  *   Parser::FOR
  *   Parser::TO
  *   Parser::DO
  *   Parser::LET
  *   Parser::IN
  *   Parser::END
  *   Parser::OF
  *   Parser::BREAK
  *   Parser::NIL
  *   Parser::FUNCTION
  *   Parser::VAR
  *   Parser::TYPE
  */

 /* TODO: Put your lab2 code here */

/*      INITIAL STATE      */
/* reserved words */
<INITIAL>"array" {adjust(); return Parser::ARRAY;}
<INITIAL>"if" {adjust(); return Parser::IF;}
<INITIAL>"then" {adjust();return Parser::THEN;}
<INITIAL>"else" {adjust(); return Parser::ELSE;}
<INITIAL>"while" {adjust(); return Parser::WHILE;}
<INITIAL>"for" {adjust(); return Parser::FOR;}
<INITIAL>"to" {adjust(); return Parser::TO;}
<INITIAL>"do" {adjust(); return Parser::DO;}
<INITIAL>"let" {adjust(); return Parser::LET;}
<INITIAL>"in" {adjust(); return Parser::IN;}
<INITIAL>"end" {adjust(); return Parser::END;}
<INITIAL>"of" {adjust(); return Parser::OF;}
<INITIAL>"break" {adjust(); return Parser::BREAK;}
<INITIAL>"nil" {adjust(); return Parser::NIL;}
<INITIAL>"function" {adjust(); return Parser::FUNCTION;}
<INITIAL>"var" {adjust(); return Parser::VAR;}
<INITIAL>"type" {adjust(); return Parser::TYPE;}

/* symbols */
<INITIAL>"," {adjust(); return Parser::COMMA;}
<INITIAL>":" {adjust(); return Parser::COLON;}
<INITIAL>";" {adjust(); return Parser::SEMICOLON;}
<INITIAL>"(" {adjust(); return Parser::LPAREN;}
<INITIAL>")" {adjust(); return Parser::RPAREN;}
<INITIAL>"[" {adjust(); return Parser::LBRACK;}
<INITIAL>"]" {adjust(); return Parser::RBRACK;}
<INITIAL>"{" {adjust(); return Parser::LBRACE;}
<INITIAL>"}" {adjust(); return Parser::RBRACE;}
<INITIAL>"." {adjust(); return Parser::DOT;}
<INITIAL>"+" {adjust(); return Parser::PLUS;}
<INITIAL>"-" {adjust(); return Parser::MINUS;}
<INITIAL>"*" {adjust(); return Parser::TIMES;}
<INITIAL>"/" {adjust(); return Parser::DIVIDE;}
<INITIAL>"=" {adjust(); return Parser::EQ;}
<INITIAL>"<>" {adjust(); return Parser::NEQ;}
<INITIAL>"<" {adjust(); return Parser::LT;}
<INITIAL>"<=" {adjust(); return Parser::LE;}
<INITIAL>">" {adjust(); return Parser::GT;}
<INITIAL>">=" {adjust(); return Parser::GE;}
<INITIAL>"&" {adjust(); return Parser::AND;}
<INITIAL>"|" {adjust(); return Parser::OR;}
<INITIAL>":=" {adjust(); return Parser::ASSIGN;}

/* ID and INT*/
<INITIAL>[a-zA-Z][a-zA-Z0-9_]* {adjust(); return Parser::ID;}
<INITIAL>[0-9]+ {adjust(); return Parser::INT;}

/* state transfer */
<INITIAL>"/*" {adjust(); comment_level_+=1; begin(StartCondition__::COMMENT);}
<INITIAL>"\"" {adjust(); string_buf_=""; begin(StartCondition__::STRING);}

/*      COMMENT STATE      */
<COMMENT>"/*" {adjust(); comment_level_+=1;}
<COMMENT>"*/" {
              adjust(); comment_level_-=1;
              if(comment_level_==0)begin(StartCondition__::INITIAL);
}
<COMMENT>\n {adjust();errormsg_->Newline();}
<COMMENT>. {adjust();}


/*      STRING STATE      */
/* escape character */
<STRING>\\n {adjustStr(); string_buf_.push_back('\n'); }
<STRING>\\t {adjustStr(); string_buf_.push_back('\t'); }
<STRING>\\[0-9][0-9][0-9] { 
              adjustStr();
              int reverse_num=std::stoi(matched().substr(1,3));
              string_buf_.push_back((char)(reverse_num));
}
<STRING>"\\\"" {
              adjustStr(); 
              string_buf_.push_back('\"'); 
}
<STRING>(\\)(\\)  {adjustStr(); string_buf_.push_back('\\');}
<STRING>(\\)[ (\n)(\t)(\f)]+(\\) {
              adjustStr(); 
              const std::string str=matched();
              int pos=str.length()-1;
              while(pos!=0){
                if(str[pos]=='\n'){
                  errormsg_->Newline();
                }
                --pos;
              }
}
<STRING>\\\^[A-Z] {
              adjustStr();
              char reverse_char=(matched().data())[2];
              string_buf_.push_back((char)(reverse_char-64));
}

/* state transfer */
<STRING>"\"" {
              adjustStr(); 
              setMatched(string_buf_); 
              string_buf_=""; 
              begin(StartCondition__::INITIAL); 
              return Parser::STRING;
}
<STRING>. {
              adjustStr();
              string_buf_+=matched(); }



/*      OTHERS      */
 /*
  * skip white space chars.
  * space, tabs and LF
  */
[ \t]+ {adjust();}
\n {adjust(); errormsg_->Newline();}

 /* illegal input */
. {adjust(); errormsg_->Error(errormsg_->tok_pos_, "illegal token");}
