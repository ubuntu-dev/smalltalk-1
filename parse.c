/*
 * Smalltalk interpreter: Parser.
 *
 * $Log: $
 *
 */

#ifndef lint
static char        *rcsid =
	"$Id: $";

#endif

/* System stuff */
#ifdef unix
#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <memory.h>
#endif
#ifdef _WIN32
#include <stddef.h>
#include <windows.h>
#include <math.h>

#define malloc(x)	GlobalAlloc(GMEM_FIXED, x)
#define free(x)		GlobalFree(x)
#endif

#include "object.h"
#include "smallobjs.h"
#include "primitive.h"
#include "interp.h"
#include "fileio.h"
#include "lex.h"
#include "symbols.h"
#include "code.h"
#include "parse.h"

Token		    tstate;
Namenode	    nstate;
Codestate	    cstate;
int                 primnum;
char               *sel = NULL;
int                 inBlock;
int                 optimBlk;


/*
 * Initialize the compiler for a new compile.
 */
void
initcompile()
{
    nstate = newname();
    cstate = newCode();

    if (sel != NULL)
	free(sel);
    primnum = 0;
    sel = NULL;
}

/*
 * Add a selector to a class and catagory.
 */
void
AddSelectorToClass(Objptr aClass, char *selector, Objptr aCatagory, 
				Objptr aMethod, int pos)
{
    Objptr	aSelector;
    Objptr	methinfo;
    Objptr	dict;

    aSelector =	internString(selector);
    rootObjects[TEMP2] = aSelector;

    dict = get_pointer(aClass, METHOD_DICT);

    if (dict == NilPtr) {
	dict = new_IDictionary();
	Set_object(aClass, METHOD_DICT, dict);
    }

    /* Add it to class dictionary */
    AddSelectorToIDictionary(dict, aSelector, aMethod);

    /* Add Catogory */
    methinfo = create_new_object(MethodInfoClass, 0);
    rootObjects[TEMP2] = methinfo;
    Set_object(aMethod, METH_DESCRIPTION, methinfo);
    Set_integer(methinfo, METHINFO_SOURCE, 0);
    Set_integer(methinfo, METHINFO_POS, pos);
    Set_object(methinfo, METHINFO_CAT, aCatagory);
    rootObjects[TEMP2] = NilPtr;

}
 
/*
 * Compile a string for a class.
 */
int
CompileForClass(char *text, Objptr class, Objptr aCatagory, int pos)
{
    int			litsize;
    int			header, eheader;
    int			size, numargs, i;
    Objptr		method;
    int			savereclaim = noreclaim;

    noreclaim = TRUE;
    initcompile();
    for_class(nstate, class);
    tstate = new_tokscanner(text);
    (void) get_token(tstate);
    messagePattern();
    doTemps();
    if (parseBinary() == -1) {
        done_scan(tstate);
	return FALSE;
    }
    doBody();
    genCode(cstate, Return, Self, -1);
    done_scan(tstate);
    i =  optimize(cstate);
    sort_literals(nstate, i, get_pointer(class, SUPERCLASS));
    genblock(cstate);
    litsize = nstate->litcount;
    numargs = nstate->argcount;
    if (primnum > 0 || numargs > 12) {
	litsize++;
	eheader = (EMETH_PRIM & (primnum << 1)) +
			(EMETH_ARGS  & (numargs << 16));
        header = (METH_NUMLITS & (litsize << 1)) +
			(METH_NUMTEMPS & (nstate->tempcount << 9)) +
			(METH_STACKSIZE & (cstate->maxstack << 17)) +
			METH_EXTEND;
    } else {
        header = (METH_NUMLITS & (litsize << 1)) +
			(METH_NUMTEMPS & (nstate->tempcount << 9)) +
			(METH_STACKSIZE & (cstate->maxstack << 17)) +
			(METH_FLAG & (numargs << 28));
	eheader = 0;
    }
    size = ((cstate->code->len) + (sizeof(Objptr) - 1)) / sizeof(Objptr); 
    method = create_new_object(CompiledMethodClass, size + litsize);
    rootObjects[TEMP1] = method;
    /* Build header */
    Set_object(method, METH_HEADER, as_oop(header));
    for (i = 0; i < nstate->litcount; i++)
	Set_object(method, METH_LITSTART + i, nstate->litarray[i]->value);
    if (eheader != 0)
        Set_object(method, METH_LITSTART + i, as_oop(eheader));
    /* Copy over code */
    for (i = 0; i < cstate->code->len; i++) 
	set_byte(method, i + (METH_LITSTART + litsize) * sizeof(Objptr),
		cstate->code->u.codeblock[i]);
    freeCode(cstate);
    freename(nstate);
    AddSelectorToClass(class, sel, aCatagory, method, pos);
    rootObjects[TEMP1] = NilPtr;
    noreclaim = savereclaim;
    return TRUE;
}

/*
 * Compile a string execute.
 */
Objptr
CompileForExecute(char *text)
{
    int			litsize;
    int			header, eheader;
    int			size, i;
    Objptr		method;
    int			savereclaim = noreclaim;

    noreclaim = TRUE;
    initcompile();
    tstate = new_tokscanner(text);
    (void) get_token(tstate);
    doTemps();
    doBody();
    done_scan(tstate);
    genCode(cstate, Break, Nil, 0);
    (void)optimize(cstate);
    sort_literals(nstate, 0, NilPtr);
    genblock(cstate);
    litsize = nstate->litcount;
    if (primnum > 0) {
	litsize++;
	eheader = (EMETH_PRIM & (primnum << 1)) +
			(EMETH_ARGS  & (0 << 16));
        header = (METH_NUMLITS & (litsize << 1)) +
			(METH_NUMTEMPS & (nstate->tempcount << 9)) +
			(METH_STACKSIZE & (cstate->maxstack << 17)) +
			METH_EXTEND;
    } else {
        header = (METH_NUMLITS & (litsize << 1)) +
			(METH_NUMTEMPS & (nstate->tempcount << 9)) +
			(METH_STACKSIZE & (cstate->maxstack << 17)) +
			(METH_FLAG & (0 << 28));
	eheader = 0;
    }
    size = ((cstate->code->len) + (sizeof(Objptr) - 1)) / sizeof(Objptr);
    method = create_new_object(CompiledMethodClass, size + litsize);
    rootObjects[TEMP1] = method;
    /* Build header */
    Set_object(method, METH_HEADER, as_oop(header));
    for (i = 0; i < nstate->litcount; i++)
	Set_object(method, METH_LITSTART + i, nstate->litarray[i]->value);
    if (eheader != 0)
        Set_object(method, METH_LITSTART + i, as_oop(eheader));
    /* Copy over code */
    for (i = 0; i < cstate->code->len; i++) 
	set_byte(method, i + (METH_LITSTART + litsize) * sizeof(Objptr),
		cstate->code->u.codeblock[i]);
    freeCode(cstate);
    freename(nstate);
    noreclaim = savereclaim;
    return method;
}

/*
 * Parse the message pattern.
 */
void
messagePattern()
{
    int                 tok;

    switch (tok = get_cur_token(tstate)) {
    case KeyName:
	sel = strsave(get_token_string(tstate));
	get_token(tstate);
	break;
    case KeyKeyword:
	keyMessage();
	break;
    case KeySpecial:
	sel = strsave(get_token_spec(tstate));
	if (get_token(tstate) == KeyName) {
	    add_symbol(nstate, get_token_string(tstate), Arg);
	    get_token(tstate);
	} else {
	   parseError(tstate, "Binary not followed by name", NULL);
	}
	break;
    default:
	parseError(tstate, "Illegal message selector", NULL);
    }
}

/*
 * Parse a keywork selector pattern.
 */
void
keyMessage()
{
    int                 tok;

    sel = strsave(get_token_string(tstate));
    while (TRUE) {
	tok = get_token(tstate);
	if (tok != KeyName) {
	    parseError(tstate, "Keyword message pattern not followed by a name", NULL);
	    return;
	}
	add_symbol(nstate, get_token_string(tstate), Arg);
	tok = get_token(tstate);
	if (tok != KeyKeyword)
	    return;
	sel = strappend(sel, get_token_string(tstate));
    }
}

/*
 * Parse temporaries.
 */
void
doTemps()
{
    int                 tok;

    if (isVarSep()) {
	while ((tok = get_token(tstate)) == KeyName) 
	    add_symbol(nstate, get_token_string(tstate), Temp);
	if (isVarSep())
	    tok = get_token(tstate);
	else {
	    parseError(tstate, "Needs to be name or |", NULL);
	    return;
	}
    }
}

/*
 * Determine if next token is a variable seperator.
 */
int
isVarSep()
{
    char	*str;

    if (get_cur_token(tstate) == KeySpecial) {
	str = get_token_spec(tstate);
	if (*str == '|' && str[1] == '\0')
		return TRUE;
    }
    return FALSE;
}


/*
 * Parse body of definition.
 */
void
doBody()
{
    int                 tok;
    int			retFlag;

    if ((inBlock || optimBlk) && get_cur_token(tstate) == KeyRBrack) {
	if (optimBlk)
	    genPushNil(cstate);
	else
	    genReturnNil(cstate);
	return;
    }
    while (TRUE) {
        if (get_cur_token(tstate) == KeyReturn) {
	    get_token(tstate);
	    doExpression();
	    genReturnTOS(cstate);
	    retFlag = TRUE;
        } if (get_cur_token(tstate) != KeyEOS) {
	    doExpression();
	    retFlag = FALSE;
        }
	if ((tok = get_cur_token(tstate)) == KeyEOS) {
	    if (!retFlag)
		genPopTOS(cstate);
	    return;
	} else if (tok == KeyPeriod) {
	    tok = get_token(tstate);
	    if (tok == KeyRBrack) {		/* End of block */
	        if (retFlag && optimBlk)
		    genPushNil(cstate);		/* For optimizer */
		return;
	    } else if (tok == KeyEOS) {
	        if (retFlag && optimBlk)
		    genPopTOS(cstate);		/* Remove last value */
		return;
	    } else {
	        if (!retFlag)
		    genPopTOS(cstate);		/* Remove value of last stat. */
	    }
	} else if (tok == KeyRBrack) {
            if (retFlag && optimBlk)
	        genPushNil(cstate);		/* For optimizer */
	    return;
	} else {
	    parseError(tstate, "Invalid statement ending", NULL);
	    return;
	}
    }
}


/*
 * Parse Expression.
 */
void
doExpression()
{
    int                 superFlag = FALSE;
    int                 tok;
    Symbolnode          asgname;

    if ((tok = get_cur_token(tstate)) == KeyName) {
        if ((asgname = find_symbol(nstate, get_token_string(tstate))) == NULL) {
            parseError(tstate, "Invalid name ", get_token_string(tstate));
	    return;
        }
        if ((tok = get_token(tstate)) == KeyAssign) {
	    if (asgname->type != Inst && asgname->type != Temp &&
		     asgname->type != Variable) {
            	parseError(tstate, "Invalid asignment name ", asgname->name);
	        return;
	    }
	    (void)get_token(tstate);
	    doExpression();
	    genDupTOS(cstate);
	    genStore(cstate, asgname);
 	    return;
        } else 
	    superFlag = nameTerm(asgname);
    } else 
        superFlag = doTerm();
    if (superFlag != -1)
        doContinue(superFlag);
    return;
}

/*
 * Parse a term.
 */
int
doTerm()
{
    int                 tok = get_cur_token(tstate);
    int                 superFlag = FALSE;

    if (tok == KeyLiteral) {
	genPushLit(cstate, add_literal(nstate,  get_token_object(tstate)));
    } else if (tok == KeyName) {
	superFlag = nameTerm(find_symbol(nstate, get_token_string(tstate)));
    } else if (tok == KeyLBrack) {
	if (!doBlock())
	    return -1;
    } else if (tok == KeyRParen) {
	(void)get_token(tstate);
	doExpression();
	if (get_cur_token(tstate) != KeyLParen) {
	    parseError(tstate, "Missing )", NULL);
	    return -1;
	}
    } else if (tok == KeyLParen || tok == KeyRBrack || tok == KeyEOS
		|| tok == KeyPeriod) {
	return superFlag;
    } else {
        parseError(tstate, "Invalid terminal", NULL);
	return -1;
    }
    (void)get_token(tstate);
    return superFlag;
}

/*
 * Parse a binary special character.
 */
int
parseBinary()
{
    int                 tok;

    if (get_cur_token(tstate) == KeySpecial && 
	strcmp(get_token_spec(tstate), "<") == 0) {
       /* Handle primitive */
	if ((tok = get_token(tstate)) == KeyName &&
	    strcmp(get_token_string(tstate), "primitive") == 0 &&
	    (tok = get_token(tstate)) == KeyLiteral &&
	    is_integer(get_token_object(tstate))) {
	    primnum = as_integer(get_token_object(tstate));
	    if ((tok = get_token(tstate)) == KeySpecial &&
		strcmp(get_token_spec(tstate), ">") == 0)
		(void)get_token(tstate);
		return TRUE;
	}
	parseError(tstate, "Invalid primitive", NULL);
	return -1;
    }
    return FALSE;
}

/*
 * Parse a name terminal.
 */
int
nameTerm(Symbolnode sym)
{
    if (sym == NULL) {
	parseError(tstate, "Invalid name terminal", NULL);
	return FALSE;
    }
    genPush(cstate, sym);
    return ((sym->type == Super) ? TRUE: FALSE);
}

/*
 * Handle rest of expression.
 * message expression or
 * message expression ; message expression
 */
int
doContinue(int superFlag)
{
    superFlag = keyContinue(superFlag);
    while (superFlag != -1 && get_cur_token(tstate) == KeyCascade) {
	get_token(tstate); 
/*	genDupTOS(cstate); */
	superFlag = keyContinue(superFlag);
/*	genPopTOS(cstate); */
    }
    return superFlag;
}

/*
 * Handle keyword continuation.
 */
int
keyContinue(int superFlag)
{
    int                 argc = 0;
    char               *key;
    char               *pat;
    Literalnode         lit;

    doBinaryContinue(superFlag);
    if (get_cur_token(tstate) != KeyKeyword)
	return 0;

   /* Check if it is a built in keyword message */
    key = get_token_string(tstate);
    if (strcmp(key, "ifTrue:") == 0)
	return doIfTrue(tstate);
    if (strcmp(key, "ifFalse:") == 0)
	return doIfFalse(tstate);
    if (strcmp(key, "whileTrue:") == 0)
	return doWhileTrue(tstate);
    if (strcmp(key, "whileFalse:") == 0)
	return doWhileFalse(tstate);
    if (strcmp(key, "and:") == 0)
	return doAnd(tstate);
    if (strcmp(key, "or:") == 0)
	return doOr(tstate);

    pat = strsave("");

    while (get_cur_token(tstate) == KeyKeyword) {
	int                 sTerm;

	pat = strappend(pat, get_token_string(tstate));
	argc++;
	get_token(tstate);
	sTerm = doTerm();
	doBinaryContinue(sTerm);
    }
    lit = add_literal(nstate, internString(pat));
    genSend(cstate, lit, argc, superFlag);
    return 0;
}

/*
 * Handle binary continuation
 */
void
doBinaryContinue(int superFlag)
{
    int                 sTerm;
    Literalnode         lit;

    doUnaryContinue(superFlag);
    while (get_cur_token(tstate) == KeySpecial) {
	lit = add_literal(nstate, internString(get_token_spec(tstate)));
	(void) get_token(tstate);
	sTerm = doTerm();
	doUnaryContinue(sTerm);
        genSend(cstate, lit, 1, superFlag);
    }
}

/*
 * Handle unary continuations.
 */
void
doUnaryContinue(int superFlag)
{
    Literalnode         lit;

    while (get_cur_token(tstate) == KeyName) {
	lit = add_literal(nstate, internString(get_token_string(tstate)));
	genSend(cstate, lit, 0, superFlag);
	get_token(tstate);
    }
    return;
}

/*
 * Parse a block of code.
 */
int
doBlock()
{
    int                 savetemps = nstate->tempcount;
    int                 tok;
    int                 i, argc = 0;
    int                 saveInblock = inBlock;
    int                 saveOptimBlk = optimBlk;
    Codenode            block;

    while ((tok = get_token(tstate)) == KeyVariable) {
	add_symbol(nstate, get_token_string(tstate), Temp);
	argc++;
    }

    if (isVarSep()) {
	if (argc == 0) {
	    parseError(tstate, "No arguments defined.", NULL);
	    return FALSE;
	}
	tok = get_token(tstate);
    } else {
	if (argc > 0) {
	    parseError(tstate, "Need var seporator", NULL);
	    return FALSE;
	}
    }

   /* Jump forward to end of block. */
    block = genBlockCopy(cstate, argc);

    inBlock = TRUE;
    optimBlk = FALSE;

   /* Store arguments from stack into temps. */
    for (i = argc; i > 0; i--) 
	genStore(cstate, find_symbol_offset(nstate, i + savetemps - 1));

   /* Do body of block */
    doBody();

    if (get_cur_token(tstate) != KeyRBrack) {
	parseError(tstate, "Block must end with ]", NULL);
	return -1;
    }

    genCode(cstate, Return, Block, -1);

    setJumpTarget(cstate, block);
    inBlock = saveInblock;
    optimBlk = saveOptimBlk;
    clean_symbol(nstate, savetemps);
    return TRUE;
}

/*
 * Compile a optimized block.
 */
int
optimBlock()
{
    int                 saveInblock = inBlock;
    int                 saveOptimBlk = optimBlk;
    int                 tok;

   /* Make sure we start with a block */
    if ((tok = get_token(tstate)) != KeyLBrack) {
        parseError(tstate, "Error missing [", NULL);
	return -1;
    }
    get_token(tstate);
    inBlock = TRUE;
    optimBlk = TRUE;

   /* Do body of block */
    doBody();

    if (get_cur_token(tstate) != KeyRBrack) {
	parseError(tstate, "Block must end with ]", NULL);
	return -1;
    }

    (void)get_token(tstate);

    inBlock = saveInblock;
    optimBlk = saveOptimBlk;
    return TRUE;

}

/*
 * Compile ifTrue: as a built in.
 */
int
doIfTrue()
{
    Codenode            block, eblock = NULL;
    int                 flag;

    block = genJumpFForw(cstate);	/* Jump forward to end of block. */
    flag = optimBlock();

    if (get_cur_token(tstate) == KeyKeyword &&
	strcmp(get_token_string(tstate), "ifFalse:") == 0) {
	eblock = genJumpForw(cstate);	/* Skip around else block */
	setJumpTarget(cstate, block);
	block = eblock;
	flag = optimBlock();		/* Do else part */
    } 
    setJumpTarget(cstate, block);
    genNop(cstate);
    if (eblock != NULL)			/* Make sure stack gets cleaned up */
        genNop(cstate);
    return flag;
}

/*
 * Compile ifFalse: as a built in.
 */
int
doIfFalse()
{
    Codenode            block, eblock = NULL;
    int                 flag;

    block = genJumpTForw(cstate);	/* Jump forward to end of block. */
    flag = optimBlock();

    if (get_cur_token(tstate) == KeyKeyword &&
	strcmp(get_token_string(tstate), "ifTrue:") == 0) {
	eblock = genJumpForw(cstate);	/* Skip around else block */
	setJumpTarget(cstate, block);
	block = eblock;
	flag = optimBlock();	/* Do else part */
    } 
    setJumpTarget(cstate, block);
    genNop(cstate);
    if (eblock != NULL)			/* Make sure stack gets cleaned up */
        genNop(cstate);
    return flag;
}

/*
 * Compile whileTrue: as built in.
 */
int
doWhileTrue()
{
    Codenode            loop, block;
    int                 flag;

    genDupTOS(cstate);
    loop = getCodeLabel(cstate);
    genSend(cstate, add_literal(nstate, internString("value")), 0, FALSE);
    block = genJumpFForw(cstate);	/* Jump forward to end of block. */
    flag = optimBlock();		/* Block body */
    genPopTOS(cstate);			/* Remove result of block */
    genJump(cstate, loop);		/* Jump to start of block */
    setJumpTarget(cstate, block);
    return flag;
}

/*
 * Compile whileFalse: as built in.
 */
int
doWhileFalse()
{
    Codenode            loop, block;
    int                 flag;

    genDupTOS(cstate);
    loop = getCodeLabel(cstate);
    genSend(cstate, add_literal(nstate, internString("value")), 0, FALSE);
    block = genJumpTForw(cstate);	/* Jump forward to end of block. */
    flag = optimBlock();		/* Block body */
    genPopTOS(cstate);			/* Remove result of block */
    genJump(cstate, loop);		/* Jump to start of block */
    setJumpTarget(cstate, block);
    return flag;
}

/*
 * Compile and: as a built in.
 *
 *  expression
 *  DupTOS
 *  JF          L1
 *  PopTOS
 *  block
 * L1:
 *  
 */
int
doAnd()
{
    Codenode            block;
    int                 flag;

    genDupTOS(cstate);
    block = genJumpFForw(cstate);	/* Jump forward to end of block. */
    genPopTOS(cstate);
    flag = optimBlock();
    setJumpTarget(cstate, block);

    /* Keep grabing if we got another builtin */
    if (get_cur_token(tstate) == KeyKeyword) {
	if (strcmp(get_token_string(tstate), "ifFalse:") == 0) 
		doIfFalse();
	else if (strcmp(get_token_string(tstate), "ifTrue:") == 0) 
		doIfTrue();
	else if (strcmp(get_token_string(tstate), "or:") == 0) 
		doOr();
	else if (strcmp(get_token_string(tstate), "and:") == 0) 
		doOr();
    }
    return flag;
}

/*
 * Compile or: as a built in.
 */
int
doOr()
{
    Codenode            block;
    int                 flag;

    genDupTOS(cstate);
    block = genJumpTForw(cstate);	/* Jump forward to end of block. */
    genPopTOS(cstate);
    flag = optimBlock();
    setJumpTarget(cstate, block);

    /* Keep grabing if we got another builtin */
    if (get_cur_token(tstate) == KeyKeyword) {
	if (strcmp(get_token_string(tstate), "ifFalse:") == 0) 
		doIfFalse();
	else if (strcmp(get_token_string(tstate), "ifTrue:") == 0) 
		doIfTrue();
	else if (strcmp(get_token_string(tstate), "or:") == 0) 
		doOr();
	else if (strcmp(get_token_string(tstate), "and:") == 0) 
		doAnd();
    }
    return flag;
}

