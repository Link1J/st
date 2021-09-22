/*
 * Copyright 2018 Avi Halachmi (:avih) avihpit@yahoo.com https://github.com/avih
 * MIT/X Consortium License
 */
+
#include <X11/Xft/Xft.h>
#include "st.h"
#include "boxdraw_data.h"
+
/* Rounded non-negative integers division of n / d  */
#define DIV(n, d) (((n) + (d) / 2) / (d))
+
static Display *xdpy;
static Colormap xcmap;
static XftDraw *xd;
static Visual *xvis;
+
static void drawbox(int, int, int, int, XftColor *, XftColor *, ushort);
static void drawboxlines(int, int, int, int, XftColor *, ushort);
+
/* public API */
+
void
boxdraw_xinit(Display *dpy, Colormap cmap, XftDraw *draw, Visual *vis)
{
	xdpy = dpy; xcmap = cmap; xd = draw, xvis = vis;
}
+
int
isboxdraw(Rune u)
{
	Rune block = u & ~0xff;
	return (boxdraw && block == 0x2500 && boxdata[(uint8_t)u]) ||
	       (boxdraw_braille && block == 0x2800);
}
+
/* the "index" is actually the entire shape data encoded as ushort */
ushort
boxdrawindex(const Glyph *g)
{
	if (boxdraw_braille && (g->u & ~0xff) == 0x2800)
		return BRL | (uint8_t)g->u;
	if (boxdraw_bold && (g->mode & ATTR_BOLD))
		return BDB | boxdata[(uint8_t)g->u];
	return boxdata[(uint8_t)g->u];
}
+
void
drawboxes(int x, int y, int cw, int ch, XftColor *fg, XftColor *bg,
          const XftGlyphFontSpec *specs, int len)
{
	for ( ; len-- > 0; x += cw, specs++)
		drawbox(x, y, cw, ch, fg, bg, (ushort)specs->glyph);
}
+
/* implementation */
+
void
drawbox(int x, int y, int w, int h, XftColor *fg, XftColor *bg, ushort bd)
{
	ushort cat = bd & ~(BDB | 0xff);  /* mask out bold and data */
	if (bd & (BDL | BDA)) {
		/* lines (light/double/heavy/arcs) */
		drawboxlines(x, y, w, h, fg, bd);
+
	} else if (cat == BBD) {
		/* lower (8-X)/8 block */
		int d = DIV((uint8_t)bd * h, 8);
		XftDrawRect(xd, fg, x, y + d, w, h - d);
+
	} else if (cat == BBU) {
		/* upper X/8 block */
		XftDrawRect(xd, fg, x, y, w, DIV((uint8_t)bd * h, 8));
+
	} else if (cat == BBL) {
		/* left X/8 block */
		XftDrawRect(xd, fg, x, y, DIV((uint8_t)bd * w, 8), h);
+
	} else if (cat == BBR) {
		/* right (8-X)/8 block */
		int d = DIV((uint8_t)bd * w, 8);
		XftDrawRect(xd, fg, x + d, y, w - d, h);
+
	} else if (cat == BBQ) {
		/* Quadrants */
		int w2 = DIV(w, 2), h2 = DIV(h, 2);
		if (bd & TL)
			XftDrawRect(xd, fg, x, y, w2, h2);
		if (bd & TR)
			XftDrawRect(xd, fg, x + w2, y, w - w2, h2);
		if (bd & BL)
			XftDrawRect(xd, fg, x, y + h2, w2, h - h2);
		if (bd & BR)
			XftDrawRect(xd, fg, x + w2, y + h2, w - w2, h - h2);
+
	} else if (bd & BBS) {
		/* Shades - data is 1/2/3 for 25%/50%/75% alpha, respectively */
		int d = (uint8_t)bd;
		XftColor xfc;
		XRenderColor xrc = { .alpha = 0xffff };
+
		xrc.red = DIV(fg->color.red * d + bg->color.red * (4 - d), 4);
		xrc.green = DIV(fg->color.green * d + bg->color.green * (4 - d), 4);
		xrc.blue = DIV(fg->color.blue * d + bg->color.blue * (4 - d), 4);
+
		XftColorAllocValue(xdpy, xvis, xcmap, &xrc, &xfc);
		XftDrawRect(xd, &xfc, x, y, w, h);
		XftColorFree(xdpy, xvis, xcmap, &xfc);
+
	} else if (cat == BRL) {
		/* braille, each data bit corresponds to one dot at 2x4 grid */
		int w1 = DIV(w, 2);
		int h1 = DIV(h, 4), h2 = DIV(h, 2), h3 = DIV(3 * h, 4);
+
		if (bd & 1)   XftDrawRect(xd, fg, x, y, w1, h1);
		if (bd & 2)   XftDrawRect(xd, fg, x, y + h1, w1, h2 - h1);
		if (bd & 4)   XftDrawRect(xd, fg, x, y + h2, w1, h3 - h2);
		if (bd & 8)   XftDrawRect(xd, fg, x + w1, y, w - w1, h1);
		if (bd & 16)  XftDrawRect(xd, fg, x + w1, y + h1, w - w1, h2 - h1);
		if (bd & 32)  XftDrawRect(xd, fg, x + w1, y + h2, w - w1, h3 - h2);
		if (bd & 64)  XftDrawRect(xd, fg, x, y + h3, w1, h - h3);
		if (bd & 128) XftDrawRect(xd, fg, x + w1, y + h3, w - w1, h - h3);
+
	}
}
+
void
drawboxlines(int x, int y, int w, int h, XftColor *fg, ushort bd)
{
	/* s: stem thickness. width/8 roughly matches underscore thickness. */
	/* We draw bold as 1.5 * normal-stem and at least 1px thicker.      */
	/* doubles draw at least 3px, even when w or h < 3. bold needs 6px. */
	int mwh = MIN(w, h);
	int base_s = MAX(1, DIV(mwh, 8));
	int bold = (bd & BDB) && mwh >= 6;  /* possibly ignore boldness */
	int s = bold ? MAX(base_s + 1, DIV(3 * base_s, 2)) : base_s;
	int w2 = DIV(w - s, 2), h2 = DIV(h - s, 2);
	/* the s-by-s square (x + w2, y + h2, s, s) is the center texel.    */
	/* The base length (per direction till edge) includes this square.  */
+
	int light = bd & (LL | LU | LR | LD);
	int double_ = bd & (DL | DU | DR | DD);
+
	if (light) {
		/* d: additional (negative) length to not-draw the center   */
		/* texel - at arcs and avoid drawing inside (some) doubles  */
		int arc = bd & BDA;
		int multi_light = light & (light - 1);
		int multi_double = double_ & (double_ - 1);
		/* light crosses double only at DH+LV, DV+LH (ref. shapes)  */
		int d = arc || (multi_double && !multi_light) ? -s : 0;
+
		if (bd & LL)
			XftDrawRect(xd, fg, x, y + h2, w2 + s + d, s);
		if (bd & LU)
			XftDrawRect(xd, fg, x + w2, y, s, h2 + s + d);
		if (bd & LR)
			XftDrawRect(xd, fg, x + w2 - d, y + h2, w - w2 + d, s);
		if (bd & LD)
			XftDrawRect(xd, fg, x + w2, y + h2 - d, s, h - h2 + d);
	}
+
	/* double lines - also align with light to form heavy when combined */
	if (double_) {
		/*
		* going clockwise, for each double-ray: p is additional length
		* to the single-ray nearer to the previous direction, and n to
		* the next. p and n adjust from the base length to lengths
		* which consider other doubles - shorter to avoid intersections
		* (p, n), or longer to draw the far-corner texel (n).
		*/
		int dl = bd & DL, du = bd & DU, dr = bd & DR, dd = bd & DD;
		if (dl) {
			int p = dd ? -s : 0, n = du ? -s : dd ? s : 0;
			XftDrawRect(xd, fg, x, y + h2 + s, w2 + s + p, s);
			XftDrawRect(xd, fg, x, y + h2 - s, w2 + s + n, s);
		}
		if (du) {
			int p = dl ? -s : 0, n = dr ? -s : dl ? s : 0;
			XftDrawRect(xd, fg, x + w2 - s, y, s, h2 + s + p);
			XftDrawRect(xd, fg, x + w2 + s, y, s, h2 + s + n);
		}
		if (dr) {
			int p = du ? -s : 0, n = dd ? -s : du ? s : 0;
			XftDrawRect(xd, fg, x + w2 - p, y + h2 - s, w - w2 + p, s);
			XftDrawRect(xd, fg, x + w2 - n, y + h2 + s, w - w2 + n, s);
		}
		if (dd) {
			int p = dr ? -s : 0, n = dl ? -s : dr ? s : 0;
			XftDrawRect(xd, fg, x + w2 + s, y + h2 - p, s, h - h2 + p);
			XftDrawRect(xd, fg, x + w2 - s, y + h2 - n, s, h - h2 + n);
		}
	}
}
iff --git a/boxdraw_data.h b/boxdraw_data.h
ew file mode 100644
ndex 0000000..7890500
-- /dev/null
++ b/boxdraw_data.h
@ -0,0 +1,214 @@
/*
 * Copyright 2018 Avi Halachmi (:avih) avihpit@yahoo.com https://github.com/avih
 * MIT/X Consortium License
 */
+
/*
 * U+25XX codepoints data
 *
 * References:
 *   http://www.unicode.org/charts/PDF/U2500.pdf
 *   http://www.unicode.org/charts/PDF/U2580.pdf
 *
 * Test page:
 *   https://github.com/GNOME/vte/blob/master/doc/boxes.txt
 */
+
/* Each shape is encoded as 16-bits. Higher bits are category, lower are data */
/* Categories (mutually exclusive except BDB): */
/* For convenience, BDL/BDA/BBS/BDB are 1 bit each, the rest are enums */
#define BDL (1<<8)   /* Box Draw Lines (light/double/heavy) */
#define BDA (1<<9)   /* Box Draw Arc (light) */
+
#define BBD (1<<10)  /* Box Block Down (lower) X/8 */
#define BBL (2<<10)  /* Box Block Left X/8 */
#define BBU (3<<10)  /* Box Block Upper X/8 */
#define BBR (4<<10)  /* Box Block Right X/8 */
#define BBQ (5<<10)  /* Box Block Quadrants */
#define BRL (6<<10)  /* Box Braille (data is lower byte of U28XX) */
+
#define BBS (1<<14)  /* Box Block Shades */
#define BDB (1<<15)  /* Box Draw is Bold */
+
/* (BDL/BDA) Light/Double/Heavy x Left/Up/Right/Down/Horizontal/Vertical      */
/* Heavy is light+double (literally drawing light+double align to form heavy) */
#define LL (1<<0)
#define LU (1<<1)
#define LR (1<<2)
#define LD (1<<3)
#define LH (LL+LR)
#define LV (LU+LD)
+
#define DL (1<<4)
#define DU (1<<5)
#define DR (1<<6)
#define DD (1<<7)
#define DH (DL+DR)
#define DV (DU+DD)
+
#define HL (LL+DL)
#define HU (LU+DU)
#define HR (LR+DR)
#define HD (LD+DD)
#define HH (HL+HR)
#define HV (HU+HD)
+
/* (BBQ) Quadrants Top/Bottom x Left/Right */
#define TL (1<<0)
#define TR (1<<1)
#define BL (1<<2)
#define BR (1<<3)
+
/* Data for U+2500 - U+259F except dashes/diagonals */
static const unsigned short boxdata[256] = {
	/* light lines */
	[0x00] = BDL + LH,       /* light horizontal */
	[0x02] = BDL + LV,       /* light vertical */
	[0x0c] = BDL + LD + LR,  /* light down and right */
	[0x10] = BDL + LD + LL,  /* light down and left */
	[0x14] = BDL + LU + LR,  /* light up and right */
	[0x18] = BDL + LU + LL,  /* light up and left */
	[0x1c] = BDL + LV + LR,  /* light vertical and right */
	[0x24] = BDL + LV + LL,  /* light vertical and left */
	[0x2c] = BDL + LH + LD,  /* light horizontal and down */
	[0x34] = BDL + LH + LU,  /* light horizontal and up */
	[0x3c] = BDL + LV + LH,  /* light vertical and horizontal */
	[0x74] = BDL + LL,       /* light left */
	[0x75] = BDL + LU,       /* light up */
	[0x76] = BDL + LR,       /* light right */
	[0x77] = BDL + LD,       /* light down */
+
	/* heavy [+light] lines */
	[0x01] = BDL + HH,
	[0x03] = BDL + HV,
	[0x0d] = BDL + HR + LD,
	[0x0e] = BDL + HD + LR,
	[0x0f] = BDL + HD + HR,
	[0x11] = BDL + HL + LD,
	[0x12] = BDL + HD + LL,
	[0x13] = BDL + HD + HL,
	[0x15] = BDL + HR + LU,
	[0x16] = BDL + HU + LR,
	[0x17] = BDL + HU + HR,
	[0x19] = BDL + HL + LU,
	[0x1a] = BDL + HU + LL,
	[0x1b] = BDL + HU + HL,
	[0x1d] = BDL + HR + LV,
	[0x1e] = BDL + HU + LD + LR,
	[0x1f] = BDL + HD + LR + LU,
	[0x20] = BDL + HV + LR,
	[0x21] = BDL + HU + HR + LD,
	[0x22] = BDL + HD + HR + LU,
	[0x23] = BDL + HV + HR,
	[0x25] = BDL + HL + LV,
	[0x26] = BDL + HU + LD + LL,
	[0x27] = BDL + HD + LU + LL,
	[0x28] = BDL + HV + LL,
	[0x29] = BDL + HU + HL + LD,
	[0x2a] = BDL + HD + HL + LU,
	[0x2b] = BDL + HV + HL,
	[0x2d] = BDL + HL + LD + LR,
	[0x2e] = BDL + HR + LL + LD,
	[0x2f] = BDL + HH + LD,
	[0x30] = BDL + HD + LH,
	[0x31] = BDL + HD + HL + LR,
	[0x32] = BDL + HR + HD + LL,
	[0x33] = BDL + HH + HD,
	[0x35] = BDL + HL + LU + LR,
	[0x36] = BDL + HR + LU + LL,
	[0x37] = BDL + HH + LU,
	[0x38] = BDL + HU + LH,
	[0x39] = BDL + HU + HL + LR,
	[0x3a] = BDL + HU + HR + LL,
	[0x3b] = BDL + HH + HU,
	[0x3d] = BDL + HL + LV + LR,
	[0x3e] = BDL + HR + LV + LL,
	[0x3f] = BDL + HH + LV,
	[0x40] = BDL + HU + LH + LD,
	[0x41] = BDL + HD + LH + LU,
	[0x42] = BDL + HV + LH,
	[0x43] = BDL + HU + HL + LD + LR,
	[0x44] = BDL + HU + HR + LD + LL,
	[0x45] = BDL + HD + HL + LU + LR,
	[0x46] = BDL + HD + HR + LU + LL,
	[0x47] = BDL + HH + HU + LD,
	[0x48] = BDL + HH + HD + LU,
	[0x49] = BDL + HV + HL + LR,
	[0x4a] = BDL + HV + HR + LL,
	[0x4b] = BDL + HV + HH,
	[0x78] = BDL + HL,
	[0x79] = BDL + HU,
	[0x7a] = BDL + HR,
	[0x7b] = BDL + HD,
	[0x7c] = BDL + HR + LL,
	[0x7d] = BDL + HD + LU,
	[0x7e] = BDL + HL + LR,
	[0x7f] = BDL + HU + LD,
+
	/* double [+light] lines */
	[0x50] = BDL + DH,
	[0x51] = BDL + DV,
	[0x52] = BDL + DR + LD,
	[0x53] = BDL + DD + LR,
	[0x54] = BDL + DR + DD,
	[0x55] = BDL + DL + LD,
	[0x56] = BDL + DD + LL,
	[0x57] = BDL + DL + DD,
	[0x58] = BDL + DR + LU,
	[0x59] = BDL + DU + LR,
	[0x5a] = BDL + DU + DR,
	[0x5b] = BDL + DL + LU,
	[0x5c] = BDL + DU + LL,
	[0x5d] = BDL + DL + DU,
	[0x5e] = BDL + DR + LV,
	[0x5f] = BDL + DV + LR,
	[0x60] = BDL + DV + DR,
	[0x61] = BDL + DL + LV,
	[0x62] = BDL + DV + LL,
	[0x63] = BDL + DV + DL,
	[0x64] = BDL + DH + LD,
	[0x65] = BDL + DD + LH,
	[0x66] = BDL + DD + DH,
	[0x67] = BDL + DH + LU,
	[0x68] = BDL + DU + LH,
	[0x69] = BDL + DH + DU,
	[0x6a] = BDL + DH + LV,
	[0x6b] = BDL + DV + LH,
	[0x6c] = BDL + DH + DV,
+
	/* (light) arcs */
	[0x6d] = BDA + LD + LR,
	[0x6e] = BDA + LD + LL,
	[0x6f] = BDA + LU + LL,
	[0x70] = BDA + LU + LR,
+
	/* Lower (Down) X/8 block (data is 8 - X) */
	[0x81] = BBD + 7, [0x82] = BBD + 6, [0x83] = BBD + 5, [0x84] = BBD + 4,
	[0x85] = BBD + 3, [0x86] = BBD + 2, [0x87] = BBD + 1, [0x88] = BBD + 0,
+
	/* Left X/8 block (data is X) */
	[0x89] = BBL + 7, [0x8a] = BBL + 6, [0x8b] = BBL + 5, [0x8c] = BBL + 4,
	[0x8d] = BBL + 3, [0x8e] = BBL + 2, [0x8f] = BBL + 1,
+
	/* upper 1/2 (4/8), 1/8 block (X), right 1/2, 1/8 block (8-X) */
	[0x80] = BBU + 4, [0x94] = BBU + 1,
	[0x90] = BBR + 4, [0x95] = BBR + 7,
+
	/* Quadrants */
	[0x96] = BBQ + BL,
	[0x97] = BBQ + BR,
	[0x98] = BBQ + TL,
	[0x99] = BBQ + TL + BL + BR,
	[0x9a] = BBQ + TL + BR,
	[0x9b] = BBQ + TL + TR + BL,
	[0x9c] = BBQ + TL + TR + BR,
	[0x9d] = BBQ + TR,
	[0x9e] = BBQ + BL + TR,
	[0x9f] = BBQ + BL + TR + BR,
+
	/* Shades, data is an alpha value in 25% units (1/4, 1/2, 3/4) */
	[0x91] = BBS + 1, [0x92] = BBS + 2, [0x93] = BBS + 3,
+
	/* U+2504 - U+250B, U+254C - U+254F: unsupported (dashes) */
	/* U+2571 - U+2573: unsupported (diagonals) */
};