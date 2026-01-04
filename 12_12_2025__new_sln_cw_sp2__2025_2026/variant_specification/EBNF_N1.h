/*
Перевірка РБНФ №1 за допомогою коду
(помістити у файл "EBNF_N1.h")
*/


#define NONTERMINALS program_name, \
value_type, \
array_specify, \
declaration_element, \
\
other_declaration_ident, \
declaration, \
\
index_action, \
unary_operator, \
unary_operation, \
binary_operator, \
binary_action, \
left_expression, \
group_expression, \
\
expression, \
\
expression_or_cond_block__with_optional_assign, \
assign_to_right, \
\
if_expression, \
body_for_true, \
false_cond_block_without_else, \
body_for_false, \
cond_block, \
\
\
cycle_begin_expression, \
cycle_end_expression, \
cycle_counter, \
cycle_counter_lr_init, \
cycle_counter_init, \
cycle_counter_last_value, \
cycle_body, \
fordownto_cycle, \
statement, \
statement__or__block_statements,\
block_statements, \
input_rule, \
argument_for_input, \
output_rule, \
\
\
program_rule, \
\
non_zero_digit, \
digit__iteration, \
digit, \
unsigned_value, \
value, \
\
sign, \
ident, \
letter_in_upper_case, \
letter_in_lower_case, \
sign_plus, \
sign_minus
#define TOKENS \
tokenINTEGER16, \
tokenCOMMA, \
tokenNOT, \
tokenAND, \
tokenOR, \
tokenEQUAL, \
tokenNOTEQUAL, \
tokenLESS, \
tokenGREATER, \
tokenPLUS, \
tokenMINUS, \
tokenMUL, \
tokenDIV, \
tokenMOD, \
tokenGROUPEXPRESSIONBEGIN, \
tokenGROUPEXPRESSIONEND, \
tokenLRASSIGN, \
tokenELSE, \
tokenIF, \
tokenDO, \
tokenFOR, \
tokenDOWNTO, \
tokenEXIT, \
tokenGET, \
tokenPUT, \
tokenNAME, \
tokenBODY, \
tokenDATA, \
tokenBEGIN, \
tokenEND, \
tokenBEGINBLOCK, \
tokenENDBLOCK, \
tokenLEFTSQUAREBRACKETS, \
tokenRIGHTSQUAREBRACKETS, \
tokenSEMICOLON, \
digit_0, \
digit_1, \
digit_2, \
digit_3, \
digit_4, \
digit_5, \
digit_6, \
digit_7, \
digit_8, \
digit_9, \
tokenUNDERSCORE, \
A, \
B, \
C, \
D, \
E, \
F, \
G, \
H, \
I, \
J, \
K, \
L, \
M, \
N, \
O, \
P, \
Q, \
R, \
S, \
T, \
U, \
V, \
W, \
X, \
Y, \
Z, \
a, \
b, \
c, \
d, \
e, \
f, \
g, \
h, \
i, \
j, \
k, \
l, \
m, \
n, \
o, \
p, \
q, \
r, \
s, \
t, \
u, \
v, \
w, \
x, \
y, \
z
#define COMMENT_BEGIN_STR "//"
#define COMMENT_END_STR   "\n"




tokenGROUPEXPRESSIONBEGIN = "(" >> BOUNDARIES;
tokenGROUPEXPRESSIONEND = ")" >> BOUNDARIES;
tokenLEFTSQUAREBRACKETS = "[" >> BOUNDARIES;
tokenRIGHTSQUAREBRACKETS = "]" >> BOUNDARIES;
tokenBEGINBLOCK = "{" >> BOUNDARIES;
tokenENDBLOCK = "}" >> BOUNDARIES;
tokenSEMICOLON = ";" >> BOUNDARIES;
tokenINTEGER16 = "Integer_2" >> STRICT_BOUNDARIES;
tokenCOMMA = "," >> BOUNDARIES;

tokenNOT = "!" >> BOUNDARIES;

tokenAND = "&" >> BOUNDARIES;

tokenOR = "|" >> BOUNDARIES;
tokenEQUAL = "==" >> BOUNDARIES;
tokenNOTEQUAL = "!=" >> BOUNDARIES;
tokenLESS = "<" >> BOUNDARIES;
tokenGREATER = ">" >> BOUNDARIES;
tokenPLUS = "++" >> BOUNDARIES;
tokenMINUS = "--" >> BOUNDARIES;
tokenMUL = "**" >> BOUNDARIES;
tokenDIV = "Div" >> STRICT_BOUNDARIES;
tokenMOD = "Mod" >> STRICT_BOUNDARIES;
tokenLRASSIGN = "->" >> BOUNDARIES;

tokenELSE = "Else" >> STRICT_BOUNDARIES;
tokenIF = "If" >> STRICT_BOUNDARIES;

tokenDO = "Do" >> STRICT_BOUNDARIES;
tokenFOR = "For" >> STRICT_BOUNDARIES;
tokenDOWNTO = "Downto" >> STRICT_BOUNDARIES;
tokenEXIT = "EXIT" >> STRICT_BOUNDARIES;
tokenGET = "Read" >> STRICT_BOUNDARIES;
tokenPUT = "Write" >> STRICT_BOUNDARIES;
tokenNAME = "#Program" >> STRICT_BOUNDARIES;
tokenBODY = "BODY" >> STRICT_BOUNDARIES;
tokenDATA = "Variable" >> STRICT_BOUNDARIES;
tokenBEGIN = "Start" >> STRICT_BOUNDARIES;
tokenEND = "Stop" >> STRICT_BOUNDARIES;


program_name = SAME_RULE(ident);
value_type = SAME_RULE(tokenINTEGER16);
array_specify = SAME_RULE(ident);
declaration_element = ident >> -(tokenLEFTSQUAREBRACKETS >> unsigned_value >> tokenRIGHTSQUAREBRACKETS);

other_declaration_ident = tokenCOMMA >> declaration_element;
declaration = value_type >> declaration_element >> *other_declaration_ident;

index_action = tokenLEFTSQUAREBRACKETS >> expression >> tokenRIGHTSQUAREBRACKETS;
unary_operator = SAME_RULE(tokenNOT);
unary_operation = unary_operator >> expression;
binary_operator = tokenAND | tokenOR | tokenEQUAL | tokenNOTEQUAL | tokenLESS | tokenGREATER | tokenPLUS | tokenMINUS | tokenMUL | tokenDIV | tokenMOD;
binary_action = binary_operator >> expression;
left_expression = group_expression | unary_operation | ident >> -index_action | value;

expression = left_expression >> *binary_action;

group_expression = tokenGROUPEXPRESSIONBEGIN >> expression >> tokenGROUPEXPRESSIONEND;
expression_or_cond_block__with_optional_assign = expression >> tokenLRASSIGN >> ident >> -index_action;
assign_to_right = cond_block >> -(tokenLRASSIGN >> ident >> -index_action);

if_expression = SAME_RULE(expression);
body_for_true = SAME_RULE(statement__or__block_statements);
false_cond_block_without_else = tokenELSE >> tokenIF >> if_expression >> body_for_true;
body_for_false = tokenELSE >> statement__or__block_statements;
cond_block = tokenIF >> if_expression >> body_for_true >> *false_cond_block_without_else >> -body_for_false;


cycle_begin_expression = SAME_RULE(expression);
cycle_end_expression = SAME_RULE(expression);
cycle_counter = SAME_RULE(ident);
cycle_counter_lr_init = cycle_begin_expression >> tokenLRASSIGN >> cycle_counter;
cycle_counter_init = SAME_RULE(cycle_counter_lr_init);
cycle_counter_last_value = SAME_RULE(cycle_end_expression);
cycle_body = tokenDO >> (statement | block_statements);
fordownto_cycle = tokenFOR >> cycle_counter_init >> tokenDOWNTO >> cycle_counter_last_value >> cycle_body;
statement__or__block_statements = tokenBEGINBLOCK >> *statement >> -expression >> tokenENDBLOCK;
input_rule = tokenGET >> (ident >> -index_action | tokenGROUPEXPRESSIONBEGIN >> ident >> -index_action >> tokenGROUPEXPRESSIONEND);
argument_for_input = SAME_RULE(ident);
output_rule = tokenPUT >> expression;
statement = expression_or_cond_block__with_optional_assign | assign_to_right | fordownto_cycle | input_rule | output_rule | tokenSEMICOLON;

block_statements = tokenBEGINBLOCK >> *statement >> tokenENDBLOCK;

program_rule = BOUNDARIES >> tokenNAME >> program_name >> tokenSEMICOLON >> tokenDATA >> (-declaration) >> tokenSEMICOLON >> tokenBEGIN >> *statement >> tokenEND;

value = (-sign) >> unsigned_value >> BOUNDARIES;

sign = qi::char_('-') >> (qi::char_('-'));
sign_plus = SAME_RULE(tokenPLUS);
sign_minus = SAME_RULE(tokenMINUS);
unsigned_value = ((non_zero_digit >> *digit) | digit_0) >> BOUNDARIES;
digit__iteration = SAME_RULE(digit);
digit_0 = '0';
digit = digit_0 | digit_1 | digit_2 | digit_3 | digit_4 | digit_5 | digit_6 | digit_7 | digit_8 | digit_9;
digit_1 = '1';
digit_2 = '2';
digit_3 = '3';
digit_4 = '4';
digit_5 = '5';
digit_6 = '6';
digit_7 = '7';
digit_8 = '8';
digit_9 = '9';
non_zero_digit = digit_1 | digit_2 | digit_3 | digit_4 | digit_5 | digit_6 | digit_7 | digit_8 | digit_9;
tokenUNDERSCORE = "_";
ident = tokenUNDERSCORE >> digit >> letter_in_upper_case >> letter_in_upper_case >> STRICT_BOUNDARIES;
A = "A";
B = "B";
C = "C";
D = "D";
E = "E";
F = "F";
G = "G";
H = "H";
I = "I";
J = "J";
K = "K";
L = "L";
M = "M";
N = "N";
O = "O";
P = "P";
Q = "Q";
R = "R";
S = "S";
T = "T";
U = "U";
V = "V";
W = "W";
X = "X";
Y = "Y";
Z = "Z";

letter_in_upper_case = A | B | C | D | E | F | G | H | I | J | K | L | M | N | O | P | Q | R | S | T | U | V | W | X | Y | Z;

letter_in_lower_case = A;
a = "a"; b = "b"; c = "c"; d = "d"; e = "e"; f = "f"; g = "g"; h = "h"; i = "i"; j = "j"; k = "k"; l = "l"; m = "m"; n = "n";
o = "o"; p = "p"; q = "q"; r = "r"; s = "s"; t = "t"; u = "u"; v = "v"; w = "w"; x = "x"; y = "y"; z = "z";

STRICT_BOUNDARIES = (BOUNDARY >> *(BOUNDARY)) | (!(qi::alpha | qi::char_("_")));
BOUNDARIES = (BOUNDARY >> *(BOUNDARY) | NO_BOUNDARY);
BOUNDARY = BOUNDARY__SPACE | BOUNDARY__TAB | BOUNDARY__VERTICAL_TAB | BOUNDARY__FORM_FEED | BOUNDARY__CARRIAGE_RETURN | BOUNDARY__LINE_FEED | BOUNDARY__NULL;
BOUNDARY__SPACE = " ";
BOUNDARY__TAB = "\t";
BOUNDARY__VERTICAL_TAB = "\v";
BOUNDARY__FORM_FEED = "\f";
BOUNDARY__CARRIAGE_RETURN = "\r";
BOUNDARY__LINE_FEED = "\n";
BOUNDARY__NULL = "\0";
NO_BOUNDARY = "";
#define WHITESPACES \
STRICT_BOUNDARIES, \
BOUNDARIES, \
BOUNDARY, \
BOUNDARY__SPACE, \
BOUNDARY__TAB, \
BOUNDARY__VERTICAL_TAB, \
BOUNDARY__FORM_FEED, \
BOUNDARY__CARRIAGE_RETURN, \
BOUNDARY__LINE_FEED, \
BOUNDARY__NULL, \
NO_BOUNDARY


