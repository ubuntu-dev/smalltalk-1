/*
 * Smalltalk graphics: Routines for doing graphics primitive.
 *
 * $Log: $
 *
 *
 */

#ifndef lint
static char        *rcsid = "$Id: $";

#endif

#include "smalltalk.h"
#include "object.h"
#include "interp.h"
#include "smallobjs.h"
#include "primitive.h"
#include "graphic.h"
#include "system.h"

Objptr              display_object = NilPtr;
Objptr		    cursor_object = NilPtr;
Objptr              tick_semaphore = NilPtr;
Objptr              input_semaphore = NilPtr;
event_queue         input_queue;

static unsigned long low_mask[] = {
    0x00000000, 0x00000001, 0x00000003, 0x00000007,
    0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
    0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
    0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
    0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
    0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
    0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
    0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
    0xffffffff,
};

#define IsInteger(op, value) \
	if (is_integer((op))) { \
	    (value) = as_integer((op)); \
	} else { \
	    if(class_of((op)) == NilPtr) \
		(value) = 0; \
	    else  \
	        return FALSE; \
	}

#define IntValue(op, off, value) { \
 		  Objptr	_temp_ = get_pointer((op), (off)); \
		  IsInteger(_temp_, (value)); \
		  }

#define GetValue(op, off, value) { \
	Objptr	_temp_ = get_pointer((op), (off)); \
	if (is_integer(_temp_)) \
		(value) = as_integer(_temp_); \
	else \
	    return NilPtr; \
	}

void
PostEvent(int type, unsigned long value)
{

    if (type == EVENT_TIMER) {
	if (tick_semaphore != NilPtr)
	    asynchronusSignal(tick_semaphore);
    } else {
	if (input_semaphore != NilPtr) {
	    add_event(&input_queue,
		      as_integer_object((value << 4) + type));
	    asynchronusSignal(input_semaphore);
	}
    }
}

/*
 * Return the bit at a location in a form.
 */
Objptr BitAt(Objptr form, Objptr point)
{
    Objptr              op;
    unsigned long      *bits;
    int                 x, y;
    int                 w, h;
    unsigned long       word;

    /* Make sure point is of correct class */
    if (class_of(point) != PointClass)
	return NilPtr;

    /* Retrive values */
    GetValue(form, FORM_WIDTH, w);
    GetValue(form, FORM_HEIGHT, h);
    GetValue(point, XINDEX, x);
    GetValue(point, YINDEX, y);

    /* Range check */
    if (x < 0 || x >= w)
	return NilPtr;
    if (y < 0 || y >= h)
	return NilPtr;

    /* Get bitmap pointer */
    op = get_pointer(form, FORM_BITMAP);
    if (form == display_object)
	bits = display_bits;
    else
	bits = (unsigned long *) (get_object_base(op) +
				  (fixed_size(op) * sizeof(Objptr)));
    if (bits == NULL)
	return NilPtr;
    /* Grab actual word */
    word = bits[(x / 32) + ((w / 32) * y)];
    return as_integer_object(1 & (word >> (x & 0x1f)));
}

/*
 * Set the bit at a location in a form.
 */
Objptr BitAtPut(Objptr form, Objptr point, Objptr bitCode)
{
    Objptr              op;
    unsigned long      *bits;
    int                 x, y;
    int                 w, h;
    unsigned long       mask;
    int                 bit;

    /* Make sure point is of correct class */
    if (class_of(point) != PointClass)
	return NilPtr;

    if (!is_integer(bitCode))
	return NilPtr;
    bit = as_integer(bitCode);
    if (bit != 0 || bit != 1)
	return NilPtr;

    /* Retrive values */
    GetValue(form, FORM_WIDTH, w);
    GetValue(form, FORM_HEIGHT, h);
    GetValue(point, XINDEX, x);
    GetValue(point, YINDEX, y);

    /* Range check */
    if (x < 0 || x >= w)
	return NilPtr;
    if (y < 0 || y >= h)
	return NilPtr;

    /* Get bitmap pointer */
    op = get_pointer(form, FORM_BITMAP);
    if (form == display_object)
	bits = display_bits;
    else
	bits = (unsigned long *) (get_object_base(op) +
				  (fixed_size(op) * sizeof(Objptr)));
    if (bits == NULL)
	return NilPtr;
    /* Grab actual word */
    bits = &bits[(x / 32) + ((w / 32) * y)];
    mask = 1 << (x & 0x1f);
    *bits &= ~mask;		/* Clear bit */
    if (bit)
	*bits |= mask;		/* Set it if need be */
    return bitCode;
}

/*
 * Load the contents of BlitBit object into C structure to improve
 * preformace.
 */
static INLINE int
load_blit(Objptr blitOp, Copy_bits blit_data)
{
    Objptr              form;

    /* Extract destination form values */
    form = get_pointer(blitOp, DEST_FORM);
    if (form != NilPtr) {
	Objptr              op = get_pointer(form, FORM_BITMAP);

	if (form == display_object)
	    blit_data->dest_bits = display_bits;
	else
	    blit_data->dest_bits = (unsigned long *) (get_object_base(op) +
						      (fixed_size(op) *
						       sizeof(Objptr)));
	if (blit_data->dest_bits == NULL)
	    return FALSE;
	op = get_pointer(form, FORM_WIDTH);
	if (!is_integer(op))
	    return FALSE;
	blit_data->dw = as_integer(op);
	op = get_pointer(form, FORM_HEIGHT);
	if (!is_integer(op))
	    return FALSE;
	blit_data->dh = as_integer(op);
	blit_data->display = display_object == form;

	/* Extract sizes */
	IntValue(blitOp, DEST_X, blit_data->dx);
        IntValue(blitOp, DEST_Y, blit_data->dy);
        IntValue(blitOp, DEST_WIDTH, blit_data->w);
        IntValue(blitOp, DEST_HEIGHT, blit_data->h);
    } else {
	return FALSE;		/* Can't copy if no destination */
    }

    /* Extract source form values */
    form = get_pointer(blitOp, SRC_FORM);
    if (form != NilPtr) {
	Objptr              op = get_pointer(form, FORM_BITMAP);

	if (form == display_object)
	    blit_data->source_bits = display_bits;
	else
	    blit_data->source_bits =
		(unsigned long *) (get_object_base(op) +
				   (fixed_size(op) * sizeof(Objptr)));
	if (blit_data->source_bits == NULL)
	    return FALSE;
	op = get_pointer(form, FORM_WIDTH);
	if (!is_integer(op))
	    return FALSE;
	blit_data->sw = as_integer(op);
	op = get_pointer(form, FORM_HEIGHT);
	if (!is_integer(op))
	    return FALSE;
	blit_data->sh = as_integer(op);
        IntValue(blitOp, SRC_X, blit_data->sx);
        IntValue(blitOp, SRC_Y, blit_data->sy);
    } else {
	blit_data->source_bits = NULL;
	blit_data->sx = blit_data->dx;
	blit_data->sy = blit_data->dy;
	blit_data->sw = blit_data->w;
	blit_data->sh = blit_data->h;
    }

    /* Extract halftone form values */
    form = get_pointer(blitOp, HALF_FORM);
    if (form != NilPtr) {
	Objptr              op = get_pointer(form, FORM_BITMAP);

	if (form == display_object)
	    blit_data->half_bits = display_bits;
	else
	    blit_data->half_bits = (unsigned long *) (get_object_base(op) +
						      (fixed_size(op) *
						       sizeof(Objptr)));
	if (blit_data->half_bits == NULL)
	    return FALSE;
	op = get_pointer(form, FORM_WIDTH);
	if (!is_integer(op))
	    return FALSE;
	blit_data->hw = as_integer(op);
	op = get_pointer(form, FORM_HEIGHT);
	if (!is_integer(op))
	    return FALSE;
	blit_data->hh = as_integer(op);
    } else {
	blit_data->half_bits = NULL;
	blit_data->hw = 0;
	blit_data->hh = 0;
    }

    /* Copy rest of data, should all be integers */
    IntValue(blitOp, COMB_RULE, blit_data->comb_rule);
    IntValue(blitOp, CLIP_X, blit_data->cx);
    IntValue(blitOp, CLIP_Y, blit_data->cy);
    IntValue(blitOp, CLIP_WIDTH, blit_data->cw);
    IntValue(blitOp, CLIP_HEIGHT, blit_data->ch);
    return TRUE;
}

/*
 * Adjust blit data for clipping.
 */
static INLINE void
clip_data(Copy_bits blit_data)
{
    /* Now we have it loaded, compute clip range */
    if (blit_data->dx < blit_data->cx) {
	blit_data->sx += blit_data->cx - blit_data->dx;
	blit_data->w -= blit_data->cx - blit_data->dx;
	blit_data->dx = blit_data->cx;
    }
    if ((blit_data->dx + blit_data->w) > (blit_data->cx + blit_data->cw)) {
	blit_data->w -= (blit_data->dx + blit_data->w) -
	    (blit_data->cx + blit_data->cw);
    }

    /* Now do Y */
    if (blit_data->dy < blit_data->cy) {
	blit_data->sy += blit_data->cy - blit_data->dy;
	blit_data->w -= blit_data->cy - blit_data->dy;
	blit_data->dy = blit_data->cy;
    }
    if ((blit_data->dy + blit_data->h) > (blit_data->cy + blit_data->ch)) {
	blit_data->h -= (blit_data->dy + blit_data->h) -
	    (blit_data->cy + blit_data->ch);
    }

    /* Sanity check things */
    if (blit_data->sx < 0) {
	blit_data->dx -= blit_data->sx;
	blit_data->w += blit_data->sx;
	blit_data->sx = 0;
    }
    if ((blit_data->sx + blit_data->w) > blit_data->sw) {
	blit_data->w -= blit_data->sx + blit_data->w - blit_data->sw;
    }

    if (blit_data->sy < 0) {
	blit_data->dy -= blit_data->sy;
	blit_data->h += blit_data->sy;
	blit_data->sy = 0;
    }
    if ((blit_data->sy + blit_data->h) > blit_data->sh) {
	blit_data->h -= blit_data->sy + blit_data->h - blit_data->sh;
    }
}

/*
 * Do actual work of copying image.
 */
static INLINE void
doblit(Copy_bits blit_data)
{
    int                 skew;
    int                 startBits, endBits;
    unsigned long       mask1, mask2, skewMask;
    int                 preload;
    int                 hDir, vDir;
    int                 nWords;
    unsigned long      *source_ptr, *dest_ptr;
    int                 source_index, source_delta;
    int                 dest_index, dest_delta;
    int			dx, dy, sx, sy;
    int                 i;

    /* Compute masks */
    skew = (blit_data->sx - blit_data->dx) & 0x1f;
    startBits = 32 - (blit_data->dx & 0x1f);
    mask1 = low_mask[startBits];
    endBits = 31 - ((blit_data->dx + blit_data->w - 1) & 0x1f);
    mask2 = ~low_mask[endBits];
    skewMask = (skew == 0) ? 0 : (low_mask[32 - skew]);

    /* Merge masks if needed */
    if (blit_data->w < startBits) {
	mask1 = mask1 & mask2;
	mask2 = 0;
	nWords = 1;
    } else {
	nWords = (blit_data->w - startBits - 1) / 32 + 2;
    }

    /* Check for overlap */
    hDir = 1;
    vDir = 1;
    dx = blit_data->dx;
    dy = blit_data->dy;
    sx = blit_data->sx;
    sy = blit_data->sy;
    if (blit_data->source_bits == blit_data->dest_bits) {
	if (dy > sy) {
	    vDir = -1;
	    sy += blit_data->h - 1;
	    dy += blit_data->h - 1;
	} else if (dx > sx) {
	    unsigned long       t;

	    hDir = -1;
	    sx += blit_data->w - 1;
	    dx += blit_data->w - 1;
	    skewMask = ~skewMask;
	    t = mask1;
	    mask1 = mask2;
	    mask2 = t;
	}
    }

    /* Calculate offsets */
    if (blit_data->source_bits != NULL && skew != 0 && skew < -(sx & 0x1f))
	preload = 1;
    else
	preload = 0;

    if (hDir < 0)
	preload = !preload;

    /* Calculate starting offsets and increments */
    source_index = sy * (((blit_data->sw - 1) / 32) + 1) + (sx / 32);
    if (blit_data->source_bits != NULL)
	source_ptr = &blit_data->source_bits[source_index];
    else
	source_ptr = NULL;
    dest_index = dy * (((blit_data->dw - 1) / 32) + 1) + (dx / 32);
    dest_ptr = &blit_data->dest_bits[dest_index];
    source_delta = vDir * (((blit_data->sw - 1) / 32) + 1) -
	(nWords + preload) * hDir;
    dest_delta = vDir * (((blit_data->dw - 1) / 32) + 1) - (nWords * hDir);

    /* Interloop */
    for (i = 0; i < blit_data->h; i++) {	/* Vertical loop */
	unsigned long       halftone;
	unsigned long       prevword;
	unsigned long       skewWord;
	unsigned long       mergeMask;
	int                 word;

	if (blit_data->half_bits != NULL) {
	    halftone = blit_data->half_bits[dy & 0x1f];
	    dy += vDir;
	} else {
	    halftone = 0xffffffff;
	}
	skewWord = halftone;
	if (preload) {
	    prevword = *source_ptr;
	    source_ptr += hDir;
	} else {
	    prevword = 0;
	}
	mergeMask = mask1;
	for (word = 0; word < nWords; word++) {	/* Horizontal loop */
	    unsigned long       skewWord;
	    unsigned long       mergeWord;
	    unsigned long       dest;

	    if (source_ptr != NULL) {
		unsigned long       thisword;

		thisword = *source_ptr;
		skewWord = (prevword & skewMask) |
		    (thisword & (~skewMask));
		prevword = thisword;
		skewWord = (skewWord << skew) | (skewWord >> (32 - skew));
		skewWord &= halftone;
		source_ptr += hDir;
	    } else
		skewWord = halftone;
	    dest = *dest_ptr;
	    switch (blit_data->comb_rule) {
	    default:
	    case 0:
		mergeWord = 0;
		break;
	    case 1:
		mergeWord = skewWord & dest;
		break;
	    case 2:
		mergeWord = skewWord & (~dest);
		break;
	    case 3:
		mergeWord = skewWord;
		break;
	    case 4:
		mergeWord = (~skewWord) & dest;
		break;
	    case 5:
		mergeWord = dest;
		break;
	    case 6:
		mergeWord = skewWord ^ dest;
		break;
	    case 7:
		mergeWord = skewWord | dest;
		break;
	    case 8:
		mergeWord = (~skewWord) & (~dest);
		break;
	    case 9:
		mergeWord = (~skewWord) ^ dest;
		break;
	    case 10:
		mergeWord = ~dest;
		break;
	    case 11:
		mergeWord = skewWord | (~dest);
		break;
	    case 12:
		mergeWord = ~skewWord;
		break;
	    case 13:
		mergeWord = (~skewWord) | dest;
		break;
	    case 14:
		mergeWord = (~skewWord) | (~dest);
		break;
	    case 15:
		mergeWord = 0xffffffff;
		break;
	    }
	    *dest_ptr = (mergeWord & mergeMask) | ((~mergeMask) & dest);
	    dest_ptr += hDir;
	    mergeMask = (word == nWords) ? mask2 : 0xffffffff;
	}
	if (source_ptr != NULL)
	    source_ptr += source_delta;
	dest_ptr += dest_delta;
    }

}

/*
 * Wrapper function for doing bit blit operation.
 */
int
copybits(Objptr blitOp)
{
    copy_bits           blit_data;

    if (!load_blit(blitOp, &blit_data))
	return FALSE;

    clip_data(&blit_data);

    /* Check for null copy */
    if (blit_data.w <= 0 || blit_data.h <= 0)
	return TRUE;

    doblit(&blit_data);

    /* If display, update changed bits */
    if (blit_data.display)
	UpdateDisplay(&blit_data);
    return TRUE;
}

/*
 * Primitive to scan a character array and display it.
 */
Objptr character_scanword(Objptr op, Objptr arg)
{
    Objptr              text;
    Objptr              except;
    Objptr              xtable;
    int                 textpos;
    int                 printing;
    int                 endrun;
    int                 stopx;
    Objptr              ci;
    int                 c;
    Objptr              e;
    int                 sx = 0;
    int                 dx;
    int                 odx;
    int                 cxw;
    int                 w = 0;
    copy_bits           blit_data;

    /* Load peices into local varaibles */
    IntValue(op, CHAR_TEXT_POS, textpos);
    IntValue(op, CHAR_STOPX, stopx);
    IsInteger(arg, endrun);
    text = get_pointer(op, CHAR_TEXT);
    xtable = get_pointer(op, CHAR_XTABLE);
    except = get_pointer(op, CHAR_EXCEPT);
    printing = FALSE;
    if (class_of(get_pointer(op, CHAR_PRINTING)) == TrueClass) {
	printing = TRUE;
	if (!load_blit(op, &blit_data))
	    return FALSE;
	/* Do cliping in y direction */
	if (blit_data.dy < blit_data.cy) {
	    blit_data.sy += blit_data.cy - blit_data.dy;
	    blit_data.w -= blit_data.cy - blit_data.dy;
	    blit_data.dy = blit_data.cy;
	}
	if ((blit_data.dy + blit_data.h) > (blit_data.cy + blit_data.ch)) {
	    blit_data.h -= (blit_data.dy + blit_data.h) -
		(blit_data.cy + blit_data.ch);
	}

	if (blit_data.sy < 0) {
	    blit_data.dy -= blit_data.sy;
	    blit_data.h += blit_data.sy;
	    blit_data.sy = 0;
	}
	if ((blit_data.sy + blit_data.h) > blit_data.sh) {
	    blit_data.h -= blit_data.sy + blit_data.h - blit_data.sh;
	}
    }

    e = NilPtr;
    cxw = blit_data.cx + blit_data.cw;
    IntValue(op, DEST_X, dx);
    odx = dx;
    while (textpos < endrun) {
	Objptr              off;
	Objptr              wid;

	/* Get the character */
	if (!arrayAt(text, textpos, &ci) || !is_integer(ci))
	    return FALSE;
	c = as_integer(ci);

	/* Check if we should stop on this one */
	if (arrayAt(except, c, &e) && e != NilPtr)
	    break;

	/* Get offset and next offset */
	if (!arrayAt(xtable, c, &off) || !is_integer(off))
	    return FALSE;
	sx = as_integer(off);
	if (!arrayAt(xtable, c + 1, &wid) || !is_integer(wid))
	    return FALSE;
	w = as_integer(wid) - sx;

	/* If we are printing, do work */
	if (printing) {
	    int                 ddx = dx;

	    /* Inline handle cliping. */
	    /* Now we have it loaded, compute clip range */
	    if (ddx < blit_data.cx) {
		sx += blit_data.cx - ddx;
		w -= blit_data.cx - ddx;
		ddx = blit_data.cx;
	    }
	    if ((ddx + w) > cxw) {
		w -= (ddx + w) - cxw;
	    }

	    /* Sanity check things */
	    if (sx < 0)
		return FALSE;

	    if ((sx + w) > blit_data.sw)
		w -= sx + w - blit_data.sw;

	    blit_data.dx = ddx;
	    blit_data.sx = sx;
	    blit_data.w = w;

	    if (w != 0)
		doblit(&blit_data);
	}
	dx += w;
	if (dx > stopx) {
	    if (!arrayAt(except, 256, &e))
		return FALSE;
	    break;
	}
	textpos++;
    }

    /* If no exception we must have hit end of run, return it's code */
    if (e == NilPtr)
	if (!arrayAt(except, 257, &e))
	    return FALSE;

    /* Ok, now we can update display if we need too */
    if (blit_data.display && printing) {
	blit_data.dx = odx;
	blit_data.w = dx - odx;
	UpdateDisplay(&blit_data);
    }

    /* Restore pointers after loop complete */
    Set_integer(op, DEST_WIDTH, w);
    Set_integer(op, SRC_X, sx);
    Set_integer(op, DEST_X, dx);
    Set_integer(op, CHAR_TEXT_POS, textpos);

    return e;
}

/*
 * Line drawing primitivve.
 */

int
drawLoop(Objptr blitOp, int x, int y)
{
    int                 dx, px;
    int                 dy, py;
    copy_bits           blit_data;
    copy_bits           inst_data;

    if (x < 0) {
	dx = -1;
	px = -x;
    } else if (x > 0) {
	dx = 1;
	px = x;
    } else {
	dx = 0;
	px = 0;
    }

    if (y < 0) {
	dy = -1;
	py = -y;
    } else if (y > 0) {
	dy = 1;
	py = y;
    } else {
	dy = 0;
	py = 0;
    }

    if (!load_blit(blitOp, &blit_data))
	return FALSE;

    inst_data = blit_data;
    /* Do initial copy */
    clip_data(&inst_data);

    /* Check for null copy */
    if (inst_data.w > 0 && inst_data.h > 0) {
	doblit(&inst_data);

	/* If display, update changed bits */
	if (inst_data.display)
	    UpdateDisplay(&inst_data);
    }

    if (py > px) {
	int                 p = py / 2;
	int                 i;

	for (i = 1; i < py; i++) {
	    blit_data.dx += dx;
	    p -= px;
	    if (p < 0) {
		blit_data.dy += dy;
		p += py;
	    }
	    inst_data = blit_data;
	    clip_data(&inst_data);

	    /* Check for null copy */
	    if (inst_data.w > 0 && inst_data.h > 0) {
		doblit(&inst_data);

		/* If display, update changed bits */
		if (inst_data.display)
		    UpdateDisplay(&inst_data);
	    }

	}
    } else {
	int                 p = px / 2;
	int                 i;

	for (i = 1; i < px; i++) {
	    blit_data.dy += dy;
	    p -= py;
	    if (p < 0) {
		blit_data.dx += dx;
		p += p;
	    }
	    inst_data = blit_data;
	    clip_data(&inst_data);

	    /* Check for null copy */
	    if (inst_data.w > 0 && inst_data.h > 0) {
		doblit(&inst_data);

		/* If display, update changed bits */
		if (inst_data.display)
		    UpdateDisplay(&inst_data);
	    }

	}
    }
    /* Set source BitBlit to current location */
    Set_integer(blitOp, DEST_X, blit_data.dx);
    Set_integer(blitOp, DEST_Y, blit_data.dy);
    return TRUE;
}