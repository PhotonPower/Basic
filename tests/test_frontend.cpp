// Tests of lexer and parser (src/frontend).

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "frontend/lexer.hpp"
#include "frontend/parser.hpp"
#include "frontend/sema.hpp"
#include "test_util.hpp"

using namespace basic;

namespace {

std::string tokens(const std::string& line) {
    Diagnostics d;
    std::string r;
    for (const Token& t : lexLine(line, 1, d)) {
        if (t.kind == Tok::End) break;
        if (!r.empty()) r += ' ';
        r += t.kind == Tok::String ? "\"" + t.text + "\"" : t.text;
    }
    for (const Diagnostic& x : d) r += " !" + x.message;
    return r;
}

// AST dump of a program without line numbers; diagnostics as "error: ..."
std::string parse(const std::string& src) {
    Diagnostics d;
    const ast::Program p = parseProgram(src, d);
    std::string r;
    for (const ast::Stmt& s : p.stmts) {
        std::string line = ast::dump(s);
        r += line.substr(line.find(": ") + 2) + "\n";
    }
    for (const Diagnostic& x : d) r += "error " + std::to_string(x.line) + ":" + std::to_string(x.col) + ": " + x.message + "\n";
    return r;
}

std::string expr(const std::string& e) {
    std::string r = parse("x=" + e);
    return r.substr(r.find(' ', r.find(' ') + 1) + 1, r.size() - r.find(' ', r.find(' ') + 1) - 2);
}

void testLexer() {
    CHECK_EQ(tokens("a%=b&+c|*d!-e#/f$"), std::string("A% = B& + C| * D! - E# / F$"));
    CHECK_EQ(tokens("x=&HFF+$1f+&O17+&X101+%101"), std::string("X = &HFF + $1f + &O17 + &X101 + %101"));
    CHECK_EQ(tokens("y=1.5E3+.5+1e-5+3."), std::string("Y = 1.5E3 + .5 + 1e-5 + 3."));
    CHECK_EQ(tokens("IF a=<b OR c=>d AND e><f"), std::string("IF A <= B OR C >= D AND E <> F"));
    CHECK_EQ(tokens("PRINT \"a!b\" ! comment"), std::string("PRINT \"a!b\""));
    CHECK_EQ(tokens("x=5! comment"), std::string("X = 5"));
    CHECK_EQ(tokens("' comment"), std::string(""));
    CHECK_EQ(tokens("REM comment"), std::string(""));
    CHECK_EQ(tokens("rem.x=1"), std::string("REM.X = 1"));  // a variable, not REM
    CHECK_EQ(tokens("DATA 1,a!b,\"x\""), std::string("DATA"));
    CHECK_EQ(tokens("a.b_c=x.1"), std::string("A.B_C = X.1"));
    CHECK_EQ(tokens("x$=\"abc"), std::string("X$ = \"abc\" !unterminated string"));
    CHECK_EQ(tokens("PRINT a'b"), std::string("PRINT A ' B"));
    CHECK_EQ(tokens("x=VAL?(a$)+DIM?(b())"), std::string("X = VAL? ( A$ ) + DIM? ( B ( ) )"));
}

void testExpressions() {
    // precedence as on the original (probe4 T35-T40, probe5 T1-T14)
    CHECK_EQ(expr("-2^2"), std::string("(- (2 ^ 2))"));
    CHECK_EQ(expr("2^3^2"), std::string("((2 ^ 3) ^ 2)"));
    CHECK_EQ(expr("2*-3"), std::string("(2 * (- 3))"));
    CHECK_EQ(expr("2^-1"), std::string("(2 ^ (- 1))"));
    CHECK_EQ(expr("7 MOD 3*2"), std::string("(7 MOD (3 * 2))"));
    CHECK_EQ(expr("7 MOD 3+1"), std::string("((7 MOD 3) + 1)"));
    CHECK_EQ(expr("8/2 MOD 3"), std::string("((8 / 2) MOD 3)"));
    CHECK_EQ(expr("9 DIV 4 MOD 2"), std::string("((9 DIV 4) MOD 2)"));
    CHECK_EQ(expr("7\\2+1"), std::string("((7 \\ 2) + 1)"));
    CHECK_EQ(expr("1+1=2"), std::string("((1 + 1) = 2)"));
    CHECK_EQ(expr("1 OR 2 AND 0"), std::string("((1 OR 2) AND 0)"));
    CHECK_EQ(expr("0 AND 0 XOR 1"), std::string("((0 AND 0) XOR 1)"));
    CHECK_EQ(expr("NOT 0+1"), std::string("(NOT (0 + 1))"));
    CHECK_EQ(expr("NOT 1 AND 1"), std::string("(NOT (1 AND 1))"));
    CHECK_EQ(expr("a AND NOT b OR c"), std::string("(A AND (NOT (B OR C)))"));
    CHECK_EQ(expr("a AND (b OR c)"), std::string("(A AND (B OR C))"));
    CHECK_EQ(expr("1==1+2^(-26)"), std::string("(1 == (1 + (2 ^ (- 26))))"));
    // calls, arrays, user functions
    CHECK_EQ(expr("LEFT$(a$,3)+MID$(b$,2)"), std::string("(LEFT$(A$,3) + MID$(B$,2))"));
    CHECK_EQ(expr("f%(1,2)+g()"), std::string("(F%(1,2) + G())"));
    CHECK_EQ(expr("@f(1)+FN g$(x)"), std::string("(@F(1) + @G$(X))"));
    CHECK_EQ(expr("PI*TRUE+RND"), std::string("((PI * TRUE) + RND)"));
    CHECK_EQ(expr("AND(3,5)+MOD(7,2)"), std::string("(AND(3,5) + MOD(7,2))"));
    CHECK_EQ(expr("DIM?(a())"), std::string("DIM?(A())"));
}

void testStatements() {
    CHECK_EQ(parse("LET a=1\nb%(2)=3\nMID$(a$,2,1)=\"x\""),
             std::string("assign A 1\nassign B%(2) 3\nassign MID$(A$,2,1) \"x\"\n"));
    CHECK_EQ(parse("PRINT\nPRINT a;b,c'd;\nPRINT #1,\"x\"\nPRINT ,5\nPRINT USING \"##.#\",x"),
             std::string("print PRINT\n"
                         "print PRINT [A ;] [B ,] [C '] [D ;]\n"
                         "print PRINT #1 [\"x\"]\n"
                         "print PRINT [ ,] [5]\n"
                         "print PRINT using \"##.#\" [X]\n"));
    CHECK_EQ(parse("IF a THEN\nELSE IF b\nELSE\nENDIF"), std::string("if A\nelseif B\nelse\nendif\n"));
    CHECK_EQ(parse("FOR i%=1 TO 10 STEP 2\nNEXT i%\nFOR x=5 DOWNTO 1\nENDFOR x"),
             std::string("for I% 1 10 2\nnext I%\nfor.1 X 5 1\nnext X\n"));
    CHECK_EQ(parse("WHILE a\nENDWHILE\nREPEAT\nENDREPEAT b\nDO WHILE x\nLOOP UNTIL y\nDO\nENDDO\nEXIT IF z"),
             std::string("while A\nwend\nrepeat\nuntil B\ndo.1 X\nloop.2 Y\ndo\nloop\nexitif Z\n"));
    CHECK_EQ(parse("SELECT x\nCASE 1,3 TO 5\nCASE TO 0\nCASE 9 TO\nCASE \"a\" TO \"z\"\nCONT\nOTHERWISE\nENDSELECT"),
             std::string("select X\ncase {1} {3 TO 5}\ncase { TO 0}\ncase {9 TO }\ncase {\"a\" TO \"z\"}\ncont\n"
                         "default\nendselect\n"));
    CHECK_EQ(parse("PROCEDURE p(a,VAR b%,c())\nLOCAL x,y$\nRETURN\nSUB q\nENDSUB\nFUNCTION f$(n)\nRETURN \"x\"\nENDFUNC"),
             std::string("procedure P (A,VAR B%,VAR C())\nlocal X Y$\nreturn\nprocedure Q\nreturn\nfunction F$ (N)\n"
                         "return \"x\"\nendfunc\n"));
    CHECK_EQ(parse("SUB x,1\nADD a%,b%"), std::string("command SUB X 1\ncommand ADD A% B%\n"));
    CHECK_EQ(parse("@p(1,2)\nGOSUB q\nr\nr(3)\nstop.it"),
             std::string("call P 1 2\ncall Q\ncall R\ncall R 3\ncall STOP.IT\n"));
    CHECK_EQ(parse("lbl:\nGOTO lbl\n100:\nRESTORE 100"), std::string("label LBL\ngoto LBL\nlabel 100\nrestore 100\n"));
    CHECK_EQ(parse("DIM a(10),b$(2,3)\nREAD a,b$(1,1)\nRESTORE"),
             std::string("dim A(10) B$(2,3)\nread A B$(1,1)\nrestore\n"));
    CHECK_EQ(parse("DATA 1, &HFF ,abc d,\"x,y\" ,\nDATA"), std::string("data <1> <&HFF> <abc d> \"x,y\" <>\ndata\n"));
    CHECK_EQ(parse("DEFFN sq(x)=x*x"), std::string("deffn SQ (X) (X * X)\n"));
    CHECK_EQ(parse("ON ERROR GOSUB f\nON ERROR\nONERROR GOSUB g\nRESUME\nRESUME NEXT\nRESUME l"),
             std::string("onerror F\nonerror\nonerror G\nresume\nresume.1\nresume.2 L\n"));
    CHECK_EQ(parse("ON x GOSUB a,b"), std::string("command ON X A B\n"));
    CHECK_EQ(parse("VOID f(1)\n~INP(2)"), std::string("void F(1)\nvoid INP(2)\n"));
    CHECK_EQ(parse("INC a%\nDEFINT \"i-k\"\nOPTION BASE 1\nEND"),
             std::string("command INC A%\ncommand DEFINT \"i-k\"\ncommand OPTION BASE 1\ncommand END\n"));
    CHECK_EQ(parse("LET print=3"), std::string("assign PRINT 3\n"));
    CHECK_EQ(parse("OPEN \"O\",#1,f$\nCLOSE #1"), std::string("command OPEN \"O\" (# 1) F$\ncommand CLOSE (# 1)\n"));
    // errors
    CHECK_EQ(parse("x=(1+2"), std::string("error 1:7: ')' expected\n"));
    CHECK_EQ(parse("FOR i=1 10"), std::string("error 1:9: TO expected\n"));
    CHECK_EQ(parse("x=1 2"), std::string("error 1:5: unexpected '2'\n"));
    CHECK_EQ(parse("x="), std::string("error 1:3: expression expected\n"));
}

// HIR of the main program (without the "main" line), or the diagnostics.
std::string hir(const std::string& src) {
    Diagnostics d;
    const ast::Program a = parseProgram(src, d);
    const hir::Program h = analyze(a, d);
    std::string r;
    if (d.empty()) {
        r = hir::dump(h);
        r = r.substr(r.find('\n') + 1);
        std::string out;
        std::size_t pos = 0;
        while (pos < r.size()) {  // remove the indentation of the main program
            std::size_t eol = r.find('\n', pos);
            std::string line = r.substr(pos, eol - pos);
            if (line.rfind("  ", 0) == 0) line = line.substr(2);
            out += line + "\n";
            pos = eol + 1;
        }
        return out;
    }
    for (const Diagnostic& x : d) r += "error " + std::to_string(x.line) + ": " + x.message + "\n";
    return r;
}

void testSema() {
    // types from postfix and DEFxxx; integer expressions are computed as float (probe2 T1-T4)
    CHECK_EQ(hir("a%=1\nb=a%+1\nc$=\"x\"+c$"),
             std::string("A% = 1\nB# = (add (conv.float A%) 1#)\nC$ = (concat \"x\" C$)\n"));
    CHECK_EQ(hir("DEFINT \"i-k\"\ni=1\nk$=\"\"\nDEFFLT \"i\"\ni=2"),
             std::string("I% = 1\nK$ = \"\"\nI# = 2#\n"));
    CHECK_EQ(hir("b|=w&\nw&=x\nf!=3"), std::string("B| = (conv.byte W&)\nW& = (conv.word X#)\nF! = (conv.bool 3)\n"));
    CHECK_EQ(hir("x=7 MOD 2.5\ny=NOT x AND 3"), std::string("X# = (conv.float (imod 7 (toint 2.5#)))\nY# = (conv.float (not (and (toint X#) 3)))\n"));
    CHECK_EQ(hir("x=&HFFFFFFFF+2147483648+1.5E3"), std::string("X# = (add (add -1# 2147483648#) 1500#)\n"));
    CHECK_EQ(hir("a$(2)=LEFT$(b$)\nn=LEN(a$(1))"), std::string("A$(2) = LEFT$(B$):str\nN# = (conv.float LEN(A$(1)):int)\n"));
    CHECK_EQ(hir("PRINT TAB(5);SPC(2);1"), std::string("print [tab 5# ;] [spc 2# ;] [1]\n"));
    // "" in a string literal is one quote (probe14 T3: WRITE #2,"c""d" wrote "c"d")
    CHECK_EQ(hir("PRINT \"c\"\"d\""), std::string("print [\"c\"d\"]\n"));
    // structure test
    CHECK_EQ(hir("IF a\nPRINT"), std::string("error 1: ENDIF missing\n"));
    CHECK_EQ(hir("FOR i=1 TO 2\nWEND"), std::string("error 1: NEXT missing\nerror 2: WEND without WHILE\n"));
    CHECK_EQ(hir("NEXT i\nPRINT 1"), std::string("error 1: NEXT without FOR\n"));
    CHECK_EQ(hir("FOR i=1 TO 2\nNEXT j"), std::string("error 2: NEXT J does not match FOR I\n"));
    CHECK_EQ(hir("EXIT IF 1"), std::string("error 1: EXIT IF outside a loop\n"));
    CHECK_EQ(hir("PROCEDURE p\nPRINT"), std::string("error 1: RETURN missing at the end of PROCEDURE P\n"));
    CHECK_EQ(hir("IF 1\nPROCEDURE p\nRETURN\nENDIF"),
             std::string("error 2: PROCEDURE/FUNCTION inside a block\nerror 3: RETURN without PROCEDURE\n"));
    CHECK_EQ(hir("PROCEDURE p\nIF 1\nRETURN\nENDIF\nRETURN"),
             std::string("error 3: RETURN must be the last statement of the procedure, not inside a block\n"));
    CHECK_EQ(hir("SELECT 1\nCASE 1\nCONT\nENDSELECT"), std::string("error 1: CONT must be followed by CASE or DEFAULT\n"));
    CHECK_EQ(hir("CASE \"abcde\""), std::string("error 1: CASE without SELECT\n"));
    // names, calls, types
    CHECK_EQ(hir("x$=1"), std::string("error 1: type mismatch\n"));
    CHECK_EQ(hir("x=\"a\"<1"), std::string("error 1: type mismatch\n"));
    CHECK_EQ(hir("q(1)"), std::string("error 1: procedure 'Q' not found\n"));
    CHECK_EQ(hir("x=@f(1)"), std::string("error 1: function 'F' not found\n"));
    CHECK_EQ(hir("p(1,2)\nPROCEDURE p(a)\nRETURN"), std::string("error 1: P expects 1 argument(s)\n"));
    CHECK_EQ(hir("p(1)\nPROCEDURE p(VAR a)\nRETURN"), std::string("error 1: var-type mismatch: VAR parameter needs a variable\n"));
    CHECK_EQ(hir("GOTO x"), std::string("error 1: label 'X' not found\n"));
    CHECK_EQ(hir("GOTO x\nPROCEDURE p\nx:\nRETURN"), std::string("error 1: GOTO into or out of a procedure\n"));
    CHECK_EQ(hir("x=TIMER"), std::string("error 1: TIMER is not supported yet\n"));
    CHECK_EQ(hir("x=LEFT$(1)"), std::string("error 1: LEFT$ expects a string here\n"));
    CHECK_EQ(hir("SELECT 1\nCASE \"abcde\"\nENDSELECT"), std::string("error 2: CASE strings have at most 4 characters\n"));
}

void testProbes() {
    // Every line of the probe programs was accepted by the original's editor.
    namespace fs = std::filesystem;
    const fs::path dir = fs::path(BASIC_SOURCE_DIR) / "tests" / "probes";
    int count = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".lst") continue;
        std::ifstream in(entry.path());
        std::stringstream ss;
        ss << in.rdbuf();
        Diagnostics d;
        const ast::Program p = parseProgram(ss.str(), d);
        for (const Diagnostic& x : d)
            std::cerr << entry.path().filename().string() << ":" << x.line << ":" << x.col << ": " << x.message << "\n";
        CHECK(d.empty());
        CHECK(p.stmts.size() > 20);
        ++count;
    }
    CHECK(count >= 8);
}

}  // namespace

int main() {
    testLexer();
    testExpressions();
    testStatements();
    testSema();
    testProbes();
    return report("frontend");
}
