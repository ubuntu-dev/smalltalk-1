/*
 * Smalltalk interpreter: Initialize basic Known and builtin objects.
 *
 * Copyright 1999-2017 Richard P. Cornwell.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the the Artistic License (2.0). You may obtain a copy
 * of the full license at:
 *
 * http://www.perlfoundation.org/artistic_license_2_0
 *
 * Any use, modification, and distribution of the Standard or Modified
 * Versions is governed by this Artistic License. By using, modifying or
 * distributing the Package, you accept this license. Do not use, modify, or
 * distribute the Package, if you do not accept this license.
 *
 * If your Modified Version has been derived from a Modified Version made by
 * someone other than you, you are nevertheless required to ensure that your
 * Modified Version complies with the requirements of this license.
 *
 * This license does not grant you the right to use any trademark, service
 * mark, tradename, or logo of the Copyright Holder.
 *
 * Disclaimer of Warranty: THE PACKAGE IS PROVIDED BY THE COPYRIGHT HOLDER
 * AND CONTRIBUTORS "AS IS' AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES.
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, OR NON-INFRINGEMENT ARE DISCLAIMED TO THE EXTENT PERMITTED BY
 * YOUR LOCAL LAW.  UNLESS REQUIRED BY LAW, NO COPYRIGHT HOLDER OR
 * CONTRIBUTOR WILL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, OR
 * CONSEQUENTIAL DAMAGES ARISING IN ANY WAY OUT OF THE USE OF THE PACKAGE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Log: init.c,v $
 * Revision 1.9  2001/08/29 20:16:35  rich
 * Make sure we don't have any events waiting when we start a new image.
 *
 * Revision 1.8  2001/08/18 16:17:01  rich
 * Added support for graphics system.
 * Added support for large integers.
 *
 * Revision 1.7  2001/08/01 16:42:31  rich
 * Make sure top of message chain has a nil sender.
 *
 * Revision 1.6  2001/07/31 14:09:48  rich
 * Fixed spelling error "reciever"
 * Added classes for Magnitude and Integer and Number
 * Fixed flags on classes so that they are defined correctly.
 *
 * Revision 1.5  2001/01/07 18:00:48  rich
 * Changed flags on Float class.
 * Changed value of SchedulerAssociationPointer.
 * Added Smalltalk to Smalltalk dictionary.
 * Removed Smalltalk keywords.
 *
 * Revision 1.4  2000/08/19 15:01:59  rich
 * Always create instance variable arrays even if no instance variables.
 *
 * Revision 1.3  2000/02/02 16:11:08  rich
 * Changed order of includes.
 *
 * Revision 1.2  2000/02/01 18:09:52  rich
 * Added stack checking code.
 * Changed class variables to an Array.
 * Moved image start up to here.
 *
 * Revision 1.1  1999/09/02 15:57:59  rich
 * Initial revision
 *
 *
 */

#ifndef lint
static char        *rcsid =
	"$Id: init.c,v 1.9 2001/08/29 20:16:35 rich Exp rich $";

#endif

/* System stuff */
#include "smalltalk.h"
#include "object.h"
#include "smallobjs.h"
#include "interp.h"
#include "primitive.h"
#include "fileio.h"
#include "graphic.h"
#include "dump.h"
#include "lex.h"
#include "symbols.h"
#include "parse.h"
#include "image.h"
#include "system.h"

struct _class_template {
	    Objptr              object;
	    int                 flags;
	    int                 size;
	    Objptr              superclass;
	    char               *name;
	    char               *instvars;
	} class_template[] = {
    /* Order is important for reference counting. */
    { ClassClass, CLASS_PTRS, CLASS_SIZE, BehaviorClass, "Class",
	    "name vars comment category classvars pool methcats"
    },
    { BehaviorClass, CLASS_PTRS, CLASS_NAME, ObjectClass, "Behavior",
	    "superclass subclasses methoddict flags"
    },
    { MetaClass, CLASS_PTRS, META_INSTCLASS + 1, ClassClass, "MetaClass",
	    "instanceClass"
    },
    { ObjectClass, CLASS_PTRS, 0, NilPtr, "Object", NULL },
    { StringClass, CLASS_BYTE | CLASS_INDEX, 0, ObjectClass, "String", NULL },
    { ArrayClass, CLASS_PTRS | CLASS_INDEX, 0, ObjectClass, "Array", NULL },
    { MethodContextClass, CLASS_PTRS | CLASS_INDEX, BLOCK_STACK,
	    ObjectClass, "MethodContext",
	    "sender ip sp method unused receiver argcount"
    },
    { BlockContextClass, CLASS_PTRS | CLASS_INDEX,
	    BLOCK_STACK, ObjectClass, "BlockContext",
	    "caller ip sp home iip unused argcount"
    },
    { PointClass, CLASS_PTRS, 2, ObjectClass, "Point", "x y" },
    { MessageClass, CLASS_PTRS, MESSAGE_SIZE, ObjectClass, "Message",
		 "args selector" },
    { MagnitudeClass, CLASS_PTRS, 0, ObjectClass, "Magnitude", NULL },
    { NumberClass, CLASS_PTRS, 0, MagnitudeClass, "Number", NULL },
    { IntegerClass, CLASS_PTRS, 0, NumberClass, "Integer", NULL },
    { SmallIntegerClass, 0, 0, IntegerClass, "SmallInteger", NULL },
    { CharacterClass, CLASS_PTRS, 1, MagnitudeClass, "Character", "value" },
    { FloatClass, CLASS_INDEX | CLASS_BYTE, 0, NumberClass, "Float", NULL },
    { LargePosIntegerClass, CLASS_INDEX | CLASS_BYTE, 0, IntegerClass, 
		"LargePositiveInteger", NULL },
    { LargeNegIntegerClass, CLASS_INDEX | CLASS_BYTE, 0, IntegerClass, 
		"LargeNegativeInteger", NULL },
    { UndefinedClass, CLASS_PTRS, 0, ObjectClass, "Undefined", NULL },
    { TrueClass, CLASS_PTRS, 0, BooleanClass, "True", NULL },
    { FalseClass, CLASS_PTRS, 0, BooleanClass, "False", NULL },
    { LinkClass, CLASS_PTRS, 1, ObjectClass, "Link", "next" },
    { SymLinkClass, CLASS_PTRS, 2, LinkClass, "Symbol", "value" },
    { BooleanClass, CLASS_PTRS, 0, ObjectClass, "Boolean", NULL },
    { CompiledMethodClass, CLASS_INDEX, 2, ObjectClass, "CompiledMethod",
	    "description header"
    },
    { MethodInfoClass, CLASS_PTRS, 3, ObjectClass, "MethodInfo",
	    "sourceFile sourcePos category"
    },
    { IdentityDictionaryClass, CLASS_PTRS | CLASS_INDEX, 2, DictionaryClass,
	    "IdentityDictionary", "values"
    },
    { DictionaryClass, CLASS_PTRS | CLASS_INDEX, 1, SSetClass, "Dictionary",
	    "tally"
    },
    { AssociationClass, CLASS_PTRS, 2, ObjectClass, "Association",
		 "key value" },
    { SSetClass, CLASS_PTRS | CLASS_INDEX, 1, ObjectClass, "Set", "tally" },
    { FileClass, CLASS_PTRS, 3, ObjectClass, "File", 
	     "name mode position" },
};

char               *specialSelector[32] =
{
    "+", "-", "<", ">", "<=", ">=", "=", "~=", "*",
    "/", "\\", "@", "bitShift:", "\\\\", "bitAnd:", "bitOr:", "at:",
    "at:put:", "size", "next", "nextPut:", "atEnd", "==", "class",
    "blockCopy:",
    "value", "value:", "do:", "new", "new:", "x", "y"};

struct _prims {
     Objptr		class;
     char		*selector;
     int		args;
     int		primnum;
     char		*cat;
} prims[] = {
    { ClassClass, "comment:", 1, primitiveClassComment, "accessing"},
    { ClassClass, "methodsFor:", 1, primitiveMethodsFor, "creating"},
/*    { FileClass,  "loadFile:", 1, primitiveFileLoad, "loading"} */
};


/*
 * Initialize the classes from template.
 */
static void
create_classes()
{
    int                 i;
    Objptr              op;
    struct _class_template *ct;

    for (i = 0; i < (sizeof(class_template) / sizeof(struct _class_template));

	 i++) {
	ct = &class_template[i];
	op = ct->object;
	create_object(op, CLASS_PTRS, ClassClass, CLASS_SIZE * sizeof(Objptr));
	Set_integer(op, CLASS_FLAGS, (ct->size * 8) + (ct->flags / 2));
    }
}

/*
 * Fill in rest of class data.
 */
static void
name_classes()
{
    int                 i;
    Objptr              op;
    Objptr              name;
    Objptr		assoc;
    Objptr		meta;
    struct _class_template *ct;

    /* Finish initializing classes */
    for (i=0; i<(sizeof(class_template)/sizeof(struct _class_template)); i++) {
	ct = &class_template[i];
	op = ct->object;
	name = MakeString(ct->name);
	Set_object(op, CLASS_NAME, name);
	assoc = create_association(internString(ct->name), op);
        Set_object(op, CLASS_VARS, NilPtr);
	Set_object(op, SUPERCLASS, ct->superclass);

	/* Make metaclass to follow class */
	meta = create_new_object(MetaClass, 0);
	Set_integer(op, CLASS_FLAGS, (ct->size * 8) + (ct->flags / 2));
	Set_object(meta, CLASS_NAME, name);
	Set_object(meta, META_INSTCLASS, op);
	Set_integer(meta, CLASS_FLAGS, (CLASS_SIZE * 8) + (CLASS_PTRS / 2));

	/* Point class class to new meta */
	set_class_of(op, meta);
	AddSelectorToDictionary(SmalltalkPointer, assoc);
    }

    /* Fix up MetaClass's superclasses */
    for (i=0; i<(sizeof(class_template)/sizeof(struct _class_template)); i++) {
	ct = &class_template[i];
	op = ct->object;
	Set_object(class_of(op), SUPERCLASS, class_of(ct->superclass));
    }

    /* Lastly fix Objects Metaclass's superClass to Class */
    Set_object(class_of(ObjectClass), SUPERCLASS, ClassClass);
}

/*
 * Install instance variables to classes.
 */
static void
instvar_classes()
{
    int                 i;
    Objptr              op;
    Objptr		sup;
    char                *ptr, *ptr2, *str, c;
    Objptr		array;
    int			count;
    struct _class_template *ct;

    /* Finish initializing classes */
    for (i=0; i<(sizeof(class_template)/sizeof(struct _class_template)); i++) {
	ct = &class_template[i];
	op = ct->object;

	/* Create array to old instance variable names */
	sup = get_pointer(op, SUPERCLASS);
	count = ct->size - (get_integer(sup, CLASS_FLAGS) / 8);
        array = create_new_object(ArrayClass, count);
        Set_object(op, CLASS_VARS, array);

	/* Continue on if there are no variables */
        if (ct->instvars == NULL || count == 0)
	    continue;
	
	/* Make a copy of instance names so we can modify them */
	if ((str = strsave(ct->instvars)) == NULL)
	   /* Should error here, but just punt, error will show up real soon */
	    continue;

	/* Split them out and make each it's own string */
        ptr2 = ptr = str;
	count = 0;
        do {
	    c = *ptr++;
	    if (c == ' ' || c == '\0') {
	       if (ptr2 != ptr) {
		    *--ptr = '\0';
	    	    Set_object(array, count++, MakeString(ptr2));
		    *ptr++ = c;
	            ptr2 = ptr;
	       }
            }
        } while (c != '\0');
	free(str);
    }
}

/*
 * Install object into symbol table.
 */
void
makeSymbol(Objptr op, char *str)
{
    int                 hash;
    char               *ptr;
    int                 size;
    int                 symbase;

   /* Compute a hash value */
    for (hash = 0, ptr = str; *ptr != '\0';
		 hash = 0x0fffffff & ((hash << 2) + (0xff & *ptr++))) ;

    symbase = fixed_size(SymbolTable) / sizeof(Objptr);

   /* Find first link in symboltable. */
    size = length_of(SymbolTable);
    hash = hash % size;

   /* Create a new entry */
    Set_object(op, SYM_VALUE, MakeString(str));
    Set_object(op, LINK_NEXT, get_pointer(SymbolTable, hash + symbase));
    Set_object(SymbolTable, hash + symbase, op);
}

/*
 * Load Bootstrap primitive methods into system.
 */
void
loadPrimitives()
{
    int			i;
    Objptr		method;

    for (i = 0; i < (sizeof(prims) / sizeof(struct _prims)); i++) {
        method = create_new_object(CompiledMethodClass, 1);
        /* Build header */
        Set_object(method, METH_HEADER,
		 as_oop(METH_EXTEND | (METH_NUMLITS & (1<<1))));
        Set_object(method, METH_LITSTART,
		 as_oop((EMETH_PRIM & (prims[i].primnum << 1)) |
			(EMETH_ARGS & (prims[i].args << 17))));

        /* Add it to class */
        AddSelectorToClass(prims[i].class, prims[i].selector,
		internString(prims[i].cat), method, 0);
    }
}

/*
 * Load initial bootstrap objects into object memory.
 */
void
smallinit(int otsize)
{
    Objptr              temp;
    int                 i;

    new_objectable(otsize);
    noreclaim = TRUE;		/* Don't allow a reclaim until done */

   /*
    * Initialize kernel known classes and objects, these object's can't
    *be moved without recompiling the system.
    */
    create_classes();
    create_object(FalsePointer, 0, FalseClass, 0);
    create_object(TruePointer, 0, TrueClass, 0);
    create_object(SchedulerAssociationPointer, 0, TrueClass, 0 /*4 * sizeof(Objptr)*/);
    create_object(DoesNotUnderstandSelector, 0, SymLinkClass,
		  2 * sizeof(Objptr));
    create_object(CannotReturnSelector, 0, SymLinkClass, 2 * sizeof(Objptr));
    create_object(MustBeBooleanSelector, 0, SymLinkClass, 2 * sizeof(Objptr));
    create_object(UnknownClassVar, 0, SymLinkClass, 2 * sizeof(Objptr));
    create_object(InterpStackFault, 0, SymLinkClass, 2 * sizeof(Objptr));
    create_object(SpecialSelectors, CLASS_PTRS | CLASS_INDEX, ArrayClass,
		  32 * sizeof(Objptr));
    create_object(CharacterTable, CLASS_PTRS | CLASS_INDEX, ArrayClass,
		  256 * sizeof(Objptr));
    create_object(SymbolTable, CLASS_PTRS | CLASS_INDEX, ArrayClass,
		  SYMBOL_SIZE * sizeof(Objptr));

    create_object(SmalltalkPointer, CLASS_PTRS | CLASS_INDEX,
		  DictionaryClass, (32 + 1) * sizeof(Objptr));
    Set_integer(SmalltalkPointer, DICT_TALLY, 0);

   /* Load known objects that can never be removed */
    rootObjects[SMALLTLK] = SmalltalkPointer;
    rootObjects[SYMTAB] = SymbolTable;
    rootObjects[CHARTAB] = CharacterTable;
    rootObjects[OBJECT] = ObjectClass;
    rootObjects[SPECSEL] = SpecialSelectors;
    rootObjects[TRUEOBJ] = TruePointer;
    rootObjects[FALSEOBJ] = FalsePointer;
    rootObjects[SELECT0] = DoesNotUnderstandSelector;
    rootObjects[SELECT1] = CannotReturnSelector;
    rootObjects[SELECT2] = MustBeBooleanSelector;
    rootObjects[SELECT3] = UnknownClassVar;
    rootObjects[SELECT4] = InterpStackFault;

   /* Rebuild free list */
    rebuild_free();

   /*
    * Basic object memory is not installed and we can continue on using
    * the virtual memory system now to create and allocate objects.
    */

   /* Fill in class names */
    name_classes();

    makeSymbol(DoesNotUnderstandSelector, "doesNotUnderstand:");
    makeSymbol(CannotReturnSelector, "cannotReturn");
    makeSymbol(MustBeBooleanSelector, "mustBeBoolean");
    makeSymbol(UnknownClassVar, "unknownClassVar");
    makeSymbol(InterpStackFault, "interpStackFault");
   /* Build Character table */
    for (i = 0; i < 256; i++) {
	temp = create_new_object(CharacterClass, 0);
	Set_integer(temp, CHARVALUE, i);
	Set_object(CharacterTable, i, temp);
    }

   /* Add Smalltalk as a class variable of Object. */
    temp = new_Dictionary();
    AddSelectorToDictionary(temp,
	create_association(internString("Smalltalk"), SmalltalkPointer));
    Set_object(ObjectClass, CLASS_DICT, temp);

   /* Add known objects to symbol table */
    AddSelectorToDictionary(SmalltalkPointer,
	create_association(internString("true"), TruePointer));
    AddSelectorToDictionary(SmalltalkPointer,
	create_association(internString("false"), FalsePointer));
    AddSelectorToDictionary(SmalltalkPointer,
	create_association(internString("nil"), NilPtr));
    AddSelectorToDictionary(SmalltalkPointer,
	create_association(internString("SpecialSelectors"), SpecialSelectors));
    AddSelectorToDictionary(SmalltalkPointer,
	create_association(internString("Processor"),
				 SchedulerAssociationPointer)); 
    AddSelectorToDictionary(SmalltalkPointer,
	create_association(internString("charsymbols"), CharacterTable));

    /* Create a object to old initial source file name. */
    AddSelectorToDictionary(SmalltalkPointer,
	create_association(internString("initSourceFile"), 
    		create_new_object(FileClass, 0)));

#if 0
    /* Create a object to hold currently loading source file */
    AddSelectorToDictionary(SmalltalkPointer,
	create_association(internString("currentSourceFile"),
				 create_new_object(FileClass, 0)));
#endif

    for (i = 0; i < 32; i++)
	if (specialSelector[i] != NULL)
	    Set_object(SpecialSelectors, i,
		       internString(specialSelector[i]));

    /* Create list for console waiting */
    /*
     * Class is not important since it is not accessable, needs to look like
     * a linked list head.
     */
    init_console(create_new_object(PointClass, 0));
    rootObjects[CONSOLELIST] = console.file_oop;

    /* Install class instance variable names now */
    instvar_classes();
    loadPrimitives();
    noreclaim = FALSE;		/* Ok, safe now to free unused junk */
}

void
load_file(char *str)
{
    int                 sstack;
    Objptr              caller;

    noreclaim = TRUE;
   /* Load old image into memory */
    if (!load_image(str))
	error("Failed to load image");
 
   /* Load known pointers */
    rootObjects[SMALLTLK] = SmalltalkPointer;
    rootObjects[SYMTAB] = SymbolTable;
    rootObjects[CHARTAB] = CharacterTable;
    rootObjects[OBJECT] = ObjectClass;
    rootObjects[SPECSEL] = SpecialSelectors;
    rootObjects[TRUEOBJ] = TruePointer;
    rootObjects[FALSEOBJ] = FalsePointer;
    rootObjects[SELECT0] = DoesNotUnderstandSelector;
    rootObjects[SELECT1] = CannotReturnSelector;
    rootObjects[SELECT2] = MustBeBooleanSelector;
    rootObjects[SELECT3] = UnknownClassVar;
    rootObjects[SELECT4] = InterpStackFault;
    rootObjects[CURCONT] = current_context;
    rootObjects[DISPOBJ] = display_object;
    rootObjects[CURSOBJ] = cursor_object;
    rootObjects[TICKSEMA] = tick_semaphore;
    rootObjects[INPUTSEMA] = input_semaphore;
    rootObjects[CONSOLELIST] = console.file_oop;
    init_console(console.file_oop);

   /* Just to be clean we clear out any outstanding events  */
    Set_integer(tick_semaphore, SEM_SIGNALS, 0);
    Set_integer(input_semaphore, SEM_SIGNALS, 0);

   /* Initialize the display and cursor */
    display_object = NilPtr;
    cursor_object = NilPtr;
    BeDisplay(rootObjects[DISPOBJ]);
    BeCursor(rootObjects[CURSOBJ]);
    newContextFlag = 1;

   /* Inlined return of False. */

   /* Find out who original sender was. */
    caller = get_pointer(current_context, BLOCK_CALLER);
    sstack = get_integer(caller, BLOCK_SP);

   /* Remove arguments. */
    Set_object(caller, sstack++, NilPtr);
    Set_object(caller, sstack++, NilPtr);

   /* Push False value */
    Set_object(caller, --sstack, FalsePointer);
    Set_integer(caller, BLOCK_SP, sstack);

    object_decr_ref(current_context);
   /* Mark caller as current */
    rootObjects[CURCONT] = current_context = caller;
    rebuild_free();
    
    noreclaim = FALSE;
    reclaimSpace();

    interp();
}

