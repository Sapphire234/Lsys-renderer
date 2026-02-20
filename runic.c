// Oprea Alexandra Hrstina 312CD

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#ifndef M_PI
#define M_PI (3.14159265358979323846264338327950288)
#endif

typedef struct {
	char symbol, *succesor;
} rule;

typedef struct {
	char *filename;
	char *axiom;
	int nrules;
	rule *rules; // vector of rules
} lsys;

typedef struct {
	unsigned char r, g, b;
} color;

typedef struct {
	char *filename;
	int w, h;
	color **pixel;
} image;

typedef struct {
	double x, y; // current position
	double orientation; // angle from Ox
	double distance; // "pas deplasare"
	double delta; // "pas unghiular"
	color pigment;
} turtle;

typedef struct {
	int encoding; // ascii code
	int bbw, bbh; // bitmap dimensions
	int xoff, yoff; // cursor offset at letter start
	int dx, dy; // how much the cursor moves after
	char **bitmap; // the matrix
} glyph;

typedef struct {
	char *filename;
	char *fontname;
	glyph chars[256]; // all ascii characters
} design;

typedef struct {
	lsys sys;
	image img;
	design font;
	char message[256]; // for the "redo" command
} state; // overall

double deg_to_rad(double deg)
{
	return deg * M_PI / 180.0;
}

int hexa_to_int(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return 10 + (c - 'A');
	if (c >= 'a' && c <= 'f')
		return 10 + (c - 'a');
	return -1; // 0 is a valid value => fail = -1
}

void free_bitmap(char **bitmap, int h)
{
	if (!bitmap)
		return;
	for (int i = 0; i < h; i++)
		free(bitmap[i]);
	free(bitmap);
}

void bitmap_line_to_binary(const char *hex, char *row, int w)
{ // converts one hex line from bitmap into a binary
	int bit = 0;

	for (int i = 0; hex[i] && bit < w; i++) {
		int val = hexa_to_int(hex[i]);

		if (val < 0)
			return;

		for (int b = 3; b >= 0 && bit < w; b--) {
			row[bit] = (val >> b) & 1;
			bit++;
		}
	}
}

int read_command(char *line, char **command)
{
	*command = NULL;
	if (!fgets(line, 1024, stdin))
		return 0; // nothing left to read
	if (line[strlen(line) - 1] == '\n')
		line[strlen(line) - 1] = '\0';

	*command = strtok(line, " "); // first part of the line
	return 1;
}

char *read_line_dynamic(FILE *f)
{
	int c = fgetc(f); // it's int to check for errors
	if (c == EOF)
		return NULL;

	int l = 0;
	char *line = NULL;

	while (c != '\n' && c != EOF) {
		char *tmp = realloc(line, l + 2);
		if (!tmp) {
			free(line);
			return NULL;
		}
		line = tmp;
		line[l] = (char)c;
		l++;

		c = fgetc(f);
	}

	if (line) {
		line[l] = '\0';
	} else {
		line = calloc(1, 1); // empty string
	}

	return line;
}

int glyph_alloc_bitmap(glyph *g, int w, int h)
{
	g->bbw = w;
	g->bbh = h;
	g->bitmap = calloc(h, sizeof(char *));
	if (!g->bitmap)
		return 0;

	for (int i = 0; i < h; i++) {
		g->bitmap[i] = calloc(w, sizeof(char));
		if (!g->bitmap[i]) {
			for (int j = 0; j < i; j++)
				free(g->bitmap[j]);
			free(g->bitmap);
			g->bitmap = NULL; g->bbw = 0; g->bbh = 0;
			return 0;
		}
	}
	return 1;
}

int image_alloc_pixels(image *img, int w, int h)
{
	img->pixel = calloc(h, sizeof(color *));
	if (!img->pixel)
		return 0;

	// put w & h in history only if there's memory for the pixel matrix
	img->w = w;
	img->h = h;

	for (int i = 0; i < h; i++) {
		img->pixel[i] = calloc(w, sizeof(color));
		if (!img->pixel[i]) {
			for (int j = 0; j < i; j++)
				free(img->pixel[j]);
			free(img->pixel);
			img->pixel = NULL; img->w = 0; img->h = 0;
			return 0;
		}
	}
	return 1;
}

void free_lsys(lsys *s)
{
	for (int i = 0; i < s->nrules; i++)
		free(s->rules[i].succesor);
	free(s->rules);
	free(s->axiom);
	free(s->filename);

	s->rules = NULL; s->axiom = NULL; s->nrules = 0;
	s->filename = NULL;
}

void free_image(image *img)
{
	for (int i = 0; i < img->h; i++)
		free(img->pixel[i]);
	free(img->pixel);
	free(img->filename);
	img->pixel = NULL; img->filename = NULL; img->w = 0; img->h = 0;
}

void free_font(design *f)
{
	free(f->filename);
	free(f->fontname);

	for (int i = 0; i < 256; i++) {
		if (f->chars[i].bitmap) {
			for (int j = 0; j < f->chars[i].bbh; j++)
				free(f->chars[i].bitmap[j]);
			free(f->chars[i].bitmap);
			f->chars[i].bitmap = NULL;
		}
	}

	f->filename = NULL;
	f->fontname = NULL;
}

void free_state(state *s)
{ // the big free
	free_lsys(&s->sys);
	free_image(&s->img);
	free_font(&s->font);
}

void lsys_init_empty(lsys *sys)
{
	sys->filename = NULL;
	sys->axiom = NULL;
	sys->rules = NULL;
	sys->nrules = 0;
}

void image_init_empty(image *img)
{
	img->filename = NULL; img->pixel = NULL;
	img->w = 0; img->h = 0;
}

void font_init_empty(design *f)
{
	f->filename = NULL;
	f->fontname = NULL;
	for (int i = 0; i < 256; i++) {
		f->chars[i].bbw = 0; f->chars[i].bbh = 0;
		f->chars[i].xoff = 0; f->chars[i].yoff = 0;
		f->chars[i].dx = 0; f->chars[i].dy = 0;
		f->chars[i].bitmap = NULL;
	}
}

void state_init_empty(state *st)
{
	// using the functiones we already have
	lsys_init_empty(&st->sys);
	image_init_empty(&st->img);
	font_init_empty(&st->font);
}

void draw_pixel(image *img, int x, int y, int r, int g, int b)
{
	if (x >= 0 && x < img->w && y >= 0 && y < img->h) { // if we're in the grid
		img->pixel[y][x].r = (unsigned char)r;
		img->pixel[y][x].g = (unsigned char)g;
		img->pixel[y][x].b = (unsigned char)b;
	}
}

void draw_line(image *img, int x0, int y0, int x1, int y1, int r, int g, int b)
{ // Bresenham algorithm
	int dx, dy, sx, sy, err, e2;
	dx = abs(x1 - x0);
	sx = (x0 < x1) ? 1 : -1;
	dy = -abs(y1 - y0);
	sy = (y0 < y1) ? 1 : -1;
	err = dx + dy;

	while (1) {
		draw_pixel(img, x0, y0, r, g, b);
		// sending more parameters than in the pseudocode
		if (x0 == x1 && y0 == y1)
			break;

		e2 = 2 * err;

		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

void draw_glyph(image *img, glyph *gly, int x, int y, int r, int g, int b)
{
	if (!gly->bitmap)
		return;

	// starting coords of our glyph
	int start_x = x + gly->xoff;
	int start_y = y + gly->yoff;

	for (int i = 0; i < gly->bbh; i++) {
		for (int j = 0; j < gly->bbw; j++) {
			if (gly->bitmap[i][j] == 1) {
				// the first row (y) in the bdf file is our last in the matrix
				int glyph_x = start_x + j;
				int glyph_y = start_y + (gly->bbh - 1 - i);

				// inverting the y axis for the ppm image, x stays the same
				int img_y = (img->h - 1) - glyph_y;

				draw_pixel(img, glyph_x, img_y, r, g, b);
			}
		}
	}
}

state *prepare_history_slot(state **history, int *cursor,
							int *size, int *max_size)
{
	// deleting the redo possibilities
	if (*cursor < *size - 1) {
		for (int i = *cursor + 1; i < *size; i++)
			free_state(&(*history)[i]);
		*size = *cursor + 1;
	}

	// we'll add more space if needed
	if (*size == *max_size) {
		*max_size *= 2;
		state *tmp = realloc(*history, (*max_size) * sizeof(state));
		if (!tmp)
			return NULL;
		*history = tmp;
	}

	state *slot = &(*history)[*size]; // for readability
	state_init_empty(slot); // now its "clean"

	return slot;
}

void lsys_clone(lsys *dest, lsys *src)
{
	lsys_init_empty(dest);
	if (!src->filename && !src->axiom) // noting to copy
		return;

	if (src->filename) { // 1
		dest->filename = calloc(strlen(src->filename) + 1, sizeof(char));
		strcpy(dest->filename, src->filename);
	}
	if (src->axiom) { // 2
		dest->axiom = calloc(strlen(src->axiom) + 1, sizeof(char));
		strcpy(dest->axiom, src->axiom);
	}

	dest->nrules = src->nrules; // 3
	if (src->nrules > 0) { // 4
		dest->rules = calloc(src->nrules, sizeof(rule));
		for (int i = 0; i < src->nrules; i++) {
			dest->rules[i].symbol = src->rules[i].symbol;

			int l = strlen(src->rules[i].succesor);
			dest->rules[i].succesor = calloc(l + 1, sizeof(char));
			strcpy(dest->rules[i].succesor, src->rules[i].succesor);
		}
	}
}

void image_clone(image *dest, image *src)
{
	image_init_empty(dest);
	if (!src->pixel || src->w == 0 || src->h == 0)
		return; // noting to copy

	if (src->filename) {
		dest->filename = calloc(strlen(src->filename) + 1, sizeof(char));
		strcpy(dest->filename, src->filename);
	}

	if (image_alloc_pixels(dest, src->w, src->h))
		for (int i = 0; i < src->h; i++)
			memcpy(dest->pixel[i], src->pixel[i], src->w * sizeof(color));
	else {
		free_image(dest); return;
	}
}

void font_clone(design *dest, design *src)
{
	font_init_empty(dest);
	if (src->filename) {
		dest->filename = calloc(strlen(src->filename) + 1, sizeof(char));
		if (!dest->filename) {
			free_font(dest); return;
		}
		strcpy(dest->filename, src->filename);
	}

	if (src->fontname) {
		dest->fontname = calloc(strlen(src->fontname) + 1, sizeof(char));
		if (!dest->fontname) {
			free_font(dest); return;
		}
		strcpy(dest->fontname, src->fontname);
	}

	for (int i = 0; i < 256; i++) {
		glyph *src_g = &src->chars[i];
		glyph *dest_g = &dest->chars[i];

		*dest_g = *src_g; //copy all of these: bbw,bbh,dx,dy,xoff,yoff,encoding
		dest_g->bitmap = NULL; // but not the bitmap

		if (!src_g->bitmap)
			continue; // for characters that aren't used

		if (glyph_alloc_bitmap(dest_g, src_g->bbw, src_g->bbh))
			for (int j = 0; j < src_g->bbh; j++)
				memcpy(dest_g->bitmap[j], src_g->bitmap[j], src_g->bbw);
		else {
			free_font(dest); return;
		}
	}
}

void lsys_parse(FILE *f, char *filename, lsys *sys)
{
	// we'll store the current info into the history array
	sys->filename = calloc(strlen(filename) + 1, sizeof(char));
	strcpy(sys->filename, filename); // 1.filename

	sys->axiom = read_line_dynamic(f); // 2.axiom

	char *line = read_line_dynamic(f); // 3.nr of rules
	if (line) {
		sys->nrules = atoi(line); free(line); // we dont need line anymore
	}

	if (sys->nrules > 0) { // 4.the rules themselves
		sys->rules = calloc(sys->nrules, sizeof(rule));
		for (int i = 0; i < sys->nrules; i++) {
			line = read_line_dynamic(f);
			char *c = strtok(line, " ");
			char *string = strtok(NULL, " ");

			if (c && string) {
				sys->rules[i].symbol = c[0];
				sys->rules[i].succesor = calloc(strlen(string) + 1,
												sizeof(char));
				strcpy(sys->rules[i].succesor, string);
			}
			free(line);
		}
	}
}

void image_parse(FILE *f, char *filename, image *img)
{
	char type[3];
	int w, h, max_val;

	img->filename = calloc(strlen(filename) + 1, sizeof(char));
	strcpy(img->filename, filename);

	fscanf(f, "%2s", type); // P6
	fscanf(f, "%d %d", &w, &h);
	fscanf(f, "%d", &max_val);
	fgetc(f); // getting the '/n'

	if (!image_alloc_pixels(img, w, h))
		return;

	for (int i = 0; i < h; i++) {
		// in the ppm format the pixel data is all on one line
		// reads one row of 3 * w, h times
		fread(img->pixel[i], sizeof(color), w, f);
	}
}

int font_item_interpret(FILE *f, char *line, int *tmp_encoding, int *tmp_bbw,
						int *tmp_bbh, int *tmp_xoff, int *tmp_yoff,
						int *tmp_dx, int *tmp_dy, char ***tmp_bitmap)
{ // -1 is fail, 0 is at the end and 1 is continue reading
	if (strcmp(line, "ENDCHAR") == 0) {
		free(line); return 0;
	}
	if (strncmp(line, "ENCODING ", 9) == 0) {
		*tmp_encoding = atoi(line + 9);
		free(line); // we dont need this anymore
		return 1;
	}
	if (strncmp(line, "DWIDTH ", 7) == 0) {
		if (sscanf(line + 7, "%d %d", tmp_dx, tmp_dy) != 2) {
			free(line); return -1; // sscanf check
		}
		free(line); return 1;
	}
	if (strncmp(line, "BBX ", 4) == 0) {
		if (sscanf(line + 4, "%d %d %d %d",
				   tmp_bbw, tmp_bbh, tmp_xoff, tmp_yoff) != 4) {
			free(line); return -1;
		}
		free(line); return 1;
	}
	if (strcmp(line, "BITMAP") == 0) {
		free(line);

		// alloc local bitmap
		*tmp_bitmap = calloc(*tmp_bbh, sizeof(char *));
		if (!*tmp_bitmap)
			return -1;

		for (int i = 0; i < *tmp_bbh; i++) {
			(*tmp_bitmap)[i] = calloc(*tmp_bbw, sizeof(char));
			if (!(*tmp_bitmap)[i]) {
				free_bitmap(*tmp_bitmap, *tmp_bbh);
				tmp_bitmap = NULL; return -1;
			}
		}

		// now reading the hex rows and converting them to binary
		for (int i = 0; i < *tmp_bbh; i++) {
			char *hex = read_line_dynamic(f);
			if (!hex) {
				free_bitmap(*tmp_bitmap, *tmp_bbh);
				tmp_bitmap = NULL; return -1;
			}
			bitmap_line_to_binary(hex, (*tmp_bitmap)[i], *tmp_bbw);
			free(hex);
		}
		return 1;
	}
	free(line); // other items are ignored
	return 1;
}

void font_parse(FILE *f, char *filename, design *font)
{
	char *line = NULL;
	int in_chars = 0; // checks if we can start reading the items we want

	font->filename = calloc(strlen(filename) + 1, sizeof(char));
	if (!font->filename)
		return;
	strcpy(font->filename, filename);

	while ((line = read_line_dynamic(f)) != NULL) {
		if (strncmp(line, "FONT ", 5) == 0) { //fontname
			free(font->fontname);
			font->fontname = calloc(strlen(line + 5) + 1, 1);
			if (!font->fontname) {
				free(line); return;
			}
			strcpy(font->fontname, line + 5);
			free(line); continue;
		}

		if (strncmp(line, "CHARS ", 6) == 0) { // tells us the chars are next
			in_chars = 1;
			free(line); continue;
		}

		// STARTCHAR -> ENDCHAR blocks
		if (in_chars && strncmp(line, "STARTCHAR", 9) == 0) {
			free(line);

			// local vars (& initialised) for this character
			int tmp_encoding = -1;
			int tmp_bbw = 0, tmp_bbh = 0;
			int tmp_xoff = 0, tmp_yoff = 0;
			int tmp_dx = 0, tmp_dy = 0;
			char **tmp_bitmap = NULL;

			while ((line = read_line_dynamic(f)) != NULL) {
				int ok = font_item_interpret(f, line, &tmp_encoding, &tmp_bbw,
											 &tmp_bbh, &tmp_xoff, &tmp_yoff,
											 &tmp_dx, &tmp_dy, &tmp_bitmap);
				// if ok == 1 we continue reading items
				if (ok == 0)
					break; // we read "endchar"

				if (ok < 0) { // if memory allocation went wrong
					free_bitmap(tmp_bitmap, tmp_bbh);
					tmp_bitmap = NULL; return;
				}
			}

			// now we can actually store the values but
			// only if it's a valid ascii character and has a bitmap
			if (tmp_encoding >= 0 && tmp_encoding < 256 && tmp_bitmap) {
				glyph *g = &font->chars[tmp_encoding];

				g->encoding = tmp_encoding;
				g->dx = tmp_dx; g->dy = tmp_dy;
				g->bbw = tmp_bbw; g->bbh = tmp_bbh;
				g->xoff = tmp_xoff; g->yoff = tmp_yoff;
				g->bitmap = tmp_bitmap;

				tmp_bitmap = NULL;
			} else {
				if (tmp_bitmap) {
					free_bitmap(tmp_bitmap, tmp_bbh);
					tmp_bitmap = NULL;
				}
			}
			continue;
		}

		free(line); // free the lines that we ignored
	}
}

void load_lsystem(char *filename, state **history, int *cursor,
				  int *max_size, int *size)
{
	if (!filename) {
		printf("Invalid command\n"); return;
	}
	FILE *f = fopen(filename, "r");
	if (!f) { // file doesnt exist
		printf("Failed to load %s\n", filename); return;
	}

	// the whole big state = slot => state *slot = &(*history)[*size];
	state *new_slot = prepare_history_slot(history, cursor, size, max_size);
	if (!new_slot) {
		fclose(f); return;
	}

	// cursor still points to the "old" state here
	// we update it after the new state is fully ready
	state *cur_slot = &(*history)[*cursor];
	image_clone(&new_slot->img, &cur_slot->img);
	font_clone(&new_slot->font, &cur_slot->font);

	// we are now modifying the lsys part of the big state = sys
	lsys_parse(f, filename, &new_slot->sys);

	printf("Loaded %s (L-system with %d rules)\n",
		   filename, new_slot->sys.nrules);
	snprintf(new_slot->message, 256, "Loaded %s (L-system with %d rules)\n",
			 filename, new_slot->sys.nrules); // saving the message
	*cursor = *size;
	(*size)++;

	fclose(f);
}

void load_image(char *filename, state **history, int *cursor,
				int *max_size, int *size)
{
	if (!filename) {
		printf("Invalid command\n"); return;
	}
	FILE *f = fopen(filename, "rb");
	if (!f) {
		printf("Failed to load %s\n", filename); return;
	}

	state *new_slot = prepare_history_slot(history, cursor, size, max_size);
	if (!new_slot) {
		fclose(f); return;
	}

	// keeping the old lsystem & font
	state *cur_slot = &(*history)[*cursor];
	lsys_clone(&new_slot->sys, &cur_slot->sys);
	font_clone(&new_slot->font, &cur_slot->font);

	image_parse(f, filename, &new_slot->img);

	printf("Loaded %s (PPM image %dx%d)\n",
		   filename, new_slot->img.w, new_slot->img.h);
	snprintf(new_slot->message, 256, "Loaded %s (PPM image %dx%d)\n",
			 filename, new_slot->img.w, new_slot->img.h);

	*cursor = *size;
	(*size)++;
	fclose(f);
}

void load_font(char *filename, state **history, int *cursor,
			   int *max_size, int *size)
{
	if (!filename) {
		printf("Invalid command\n"); return;
	}
	FILE *f = fopen(filename, "r");
	if (!f) {
		printf("Failed to load %s\n", filename); return;
	}

	state *new_slot = prepare_history_slot(history, cursor, size, max_size);
	if (!new_slot) {
		fclose(f); return;
	}

	state *cur_slot = &(*history)[*cursor];
	lsys_clone(&new_slot->sys, &cur_slot->sys);
	image_clone(&new_slot->img, &cur_slot->img); // keeping what we already had

	font_parse(f, filename, &new_slot->font);

	printf("Loaded %s (bitmap font %s)\n", filename, new_slot->font.fontname);
	snprintf(new_slot->message, 256, "Loaded %s (bitmap font %s)\n",
			 filename, new_slot->font.fontname);

	*cursor = *size;
	(*size)++;
	fclose(f);
}

void type_draw(image *img, design *font, char *string,
			   int start_x, int start_y, int r, int g, int b)
{
	int len = strlen(string);
	for (int i = 0; i < len; i++) {
		int code = (unsigned char)string[i]; // ascii code of the current glyph
		glyph *cur_glyph = &font->chars[code];

		draw_glyph(img, cur_glyph, start_x, start_y, r, g, b);
		start_x += cur_glyph->dx; // moving the cursor for the next glyph
		start_y += cur_glyph->dy;
	}
}

void load_type(char *args, state **history, int *cursor, int *max_size,
			   int *size)
{
	// delimit the argument and making the verifications
	char *p1 = strchr(args, '"');
	char *p2 = strrchr(args, '"');
	if (!p1 || !p2) {
		printf("Invalid command\n"); return;
	}

	*p2 = '\0'; // terminate the string at the second quote
	char *string = p1 + 1; // the string is between the quotes
	char *rest = p2 + 1; // the numbers are after

	int x, y, r, g, b;
	if (sscanf(rest, "%d %d %d %d %d", &x, &y, &r, &g, &b) != 5) {
		printf("Invalid command\n"); return;
	}
	if (!(*history)[*cursor].img.pixel) {
		printf("No image loaded\n"); return;
	}
	if (!(*history)[*cursor].font.fontname) {
		printf("No font loaded\n"); return;
	}

	state *new_slot = prepare_history_slot(history, cursor, size, max_size);
	if (!new_slot)
		return;

	state *cur_slot = &(*history)[*cursor];
	lsys_clone(&new_slot->sys, &cur_slot->sys);
	image_clone(&new_slot->img, &cur_slot->img);
	font_clone(&new_slot->font, &cur_slot->font);

	type_draw(&new_slot->img, &new_slot->font, string, x, y, r, g, b);

	printf("Text written\n");
	strcpy(new_slot->message, "Text written\n");

	*cursor = *size;
	(*size)++;
}

void print_bitcheck(image *img, long total_bits, int window)
{
	int w = img->w;
	int h = img->h;

	// 3rd in the window => second to last from the total
	long wrong_bit = total_bits - 2;
	long byte_id = wrong_bit / 8;
	long pixel_id = byte_id / 3;
	int color_id = byte_id % 3;

	// the coords from our pixel matrix
	int pixel_mat_y = pixel_id / w;
	int image_x = pixel_id % w; // this is the same as image

	// making the y into image coords
	int image_y = (h - 1) - pixel_mat_y;

	color p = img->pixel[pixel_mat_y][image_x];
	int r = (int)p.r;
	int g = (int)p.g;
	int b = (int)p.b;

	// calculate the bit position inside the byte from left to right now 0 -> 7
	int bit_pos = 7 - (wrong_bit % 8);
	switch (color_id) {
	case 0: // red
		if (window == 2)
			r &= ~(1 << bit_pos);
		else
			r |= (1 << bit_pos);
		break;
	case 1: // green
		if (window == 2)
			g &= ~(1 << bit_pos);
		else
			g |= (1 << bit_pos);
		break;
	case 2: // blue
		if (window == 2)
			b &= ~(1 << bit_pos);
		else
			b |= (1 << bit_pos);
		break;
	}
	printf("Warning: pixel at (%d, %d) may be read as (%d, %d, %d)\n",
		   image_x, image_y, r, g, b);
}

void load_bitcheck(image *img)
{
	if (!img->pixel) {
		printf("No image loaded\n"); return;
	}

	int w = img->w;
	int h = img->h;
	long total_bits = 0;
	int window = 0; // this will be the 4 bits we will focus on at a time

	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			// going through the matrix by the color bytes of a pixel

			char colors[3];
			colors[0] = img->pixel[i][j].r;
			colors[1] = img->pixel[i][j].g;
			colors[2] = img->pixel[i][j].b;

			for (int k = 0; k < 3; k++) {
				char byte = colors[k];
				// going through the bits of the byte
				for (int t = 7; t >= 0; t--) {
					int bit = (byte >> t) & 1;

					// adding the bit to the window
					// multiply by 2 moves everything to the left
					window = (window * 2) + bit;
					window = window & 15; // 15 = 1111 => keep only 4 bits

					total_bits++;

					if (total_bits >= 4 && (window == 2 || window == 13))
						print_bitcheck(img, total_bits, window);
				}
			}
		}
	}
}

char *find_rule(char c, rule *rules, int nrules)
{
	if (!rules)
		return NULL;
	for (int i = 0; i < nrules; i++) {
		if (rules[i].symbol == c)
			return rules[i].succesor;
	}
	return NULL;
}

char *derive(state **history, int cursor, int arg)
{
	if (arg < 0) {
		printf("Invalid command\n"); return NULL;
	}

	lsys *slot = &((*history)[cursor].sys);
	// the slot is now just the lsys part, we are only working on that here

	if (cursor == 0 || !slot->axiom) {
		printf("No L-system loaded\n"); return NULL;
	}
	char *old_deriv = NULL;

	old_deriv = calloc(strlen(slot->axiom) + 1, sizeof(char));
	if (!old_deriv)
		return NULL;
	strcpy(old_deriv, slot->axiom); // at first it's the axiom

	for (int i = 0; i < arg; i++) { // however many times we derive
		int old_len = strlen(old_deriv);
		int length = 0, max_length = 24;

		char *new_deriv = calloc(max_length, sizeof(char));
		if (!new_deriv) {
			free(old_deriv); return NULL;
		}

		for (int j = 0; j < old_len; j++) { // we are building the deriv
			char *chunk = NULL;

			chunk = find_rule(old_deriv[j], slot->rules, slot->nrules);
			if (!chunk) { // it has no rule
				char mini_chunk[2] = {old_deriv[j], '\0'};
				chunk = mini_chunk;
			}

			int chunk_len = strlen(chunk);

			while (length + chunk_len + 1 >= max_length) {
				// adding more space if needed
				max_length *= 2;
				char *tmp = realloc(new_deriv, max_length * sizeof(char));
				if (!tmp) {
					free(new_deriv);
					free(old_deriv);
					return NULL;
				}
				new_deriv = tmp;
			}
				strcpy(new_deriv + length, chunk);
				length += chunk_len;
				new_deriv[length] = '\0';
		}
		free(old_deriv);
		old_deriv = new_deriv;
	}
	return old_deriv;
}

void turtle_draw(image *img, const char *string, turtle t)
{
	int length = 0, max_length = 8;
	// how many current elements & capacity, similar in the derive function

	// the array of the turtle's states
	turtle *S = calloc(max_length, sizeof(turtle));
	if (!S)
		return;

	for (int i = 0; string[i] != '\0'; i++) {
		char symbol = string[i];

		if (symbol == 'F') {
			double rad = deg_to_rad(t.orientation); // sin & cos want radians
			double nx = t.x + t.distance * cos(rad); // next x => nx
			double ny = t.y + t.distance * sin(rad); // next y => ny

			int x0 = (int)lround(t.x);
			int x1 = (int)lround(nx);
			int y0 = (int)lround(t.y);
			int y1 = (int)lround(ny);

			// flipping the y axis
			y0 = (img->h - 1) - y0;
			y1 = (img->h - 1) - y1;

			draw_line(img, x0, y0, x1, y1,
					  t.pigment.r, t.pigment.g, t.pigment.b);

			t.x = nx;
			t.y = ny;
		} else if (symbol == '+')
			t.orientation += t.delta;
		else if (symbol == '-')
			t.orientation -= t.delta;
		else if (symbol == '[') {
			if (length == max_length) { // firstly verify space
				max_length *= 2;
				turtle *tmp = realloc(S, max_length * sizeof(turtle));
				if (!tmp) {
					free(S); return;
				}
				S = tmp;
			}
			S[length] = t; length++; // saving current turtle state
		} else if (symbol == ']')
			if (length > 0) {
				length--;
				t = S[length];
			}
	}

	free(S);
}

void load_turtle(state **history, int *cursor, int *max_size, int *size,
				 char *x, char *y, char *step, char *orient, char *delta,
				 char *n_str, char *R, char *G, char *B)
{
	// verifications
	if (!x || !y || !step || !orient || !delta || !n_str ||
		!R || !G || !B) {
		printf("Invalid command\n"); return;
	}
	if (!(*history)[*cursor].img.pixel) {
		printf("No image loaded\n"); return;
	}
	if (!(*history)[*cursor].sys.axiom) {
		printf("No L-system loaded\n"); return;
	}

	// the conversions
	turtle t;
	t.x = atof(x); t.y = atof(y);
	t.distance = atof(step); t.orientation = atof(orient);
	int n = atoi(n_str); t.delta = atof(delta);
	t.pigment.r = (unsigned char)atoi(R);
	t.pigment.g = (unsigned char)atoi(G);
	t.pigment.b = (unsigned char)atoi(B);

	char *deriv = derive(history, *cursor, n);
	if (!deriv)
		return;

	state *new_slot = prepare_history_slot(history, cursor, size, max_size);
	if (!new_slot) {
		free(deriv); return;
	}

	state *cur_slot = &(*history)[*cursor];
	lsys_clone(&new_slot->sys, &cur_slot->sys);
	image_clone(&new_slot->img, &cur_slot->img);
	font_clone(&new_slot->font, &cur_slot->font);

	turtle_draw(&new_slot->img, deriv, t);
	free(deriv);

	printf("Drawing done\n");
	strcpy(new_slot->message, "Drawing done\n");

	*cursor = *size;
	(*size)++;
}

void save_image(char *filename, image *img)
{
	if (!img->pixel) {
		printf("No image loaded\n"); return;
	}
	if (!filename) {
		printf("Invalid command\n"); return;
	}
	FILE *f = fopen(filename, "wb");

	fprintf(f, "P6\n%d %d\n255\n", img->w, img->h);
	for (int i = 0; i < img->h; i++)
		fwrite(img->pixel[i], sizeof(color), img->w, f);

	printf("Saved %s\n", filename);
	fclose(f);
}

int main(void)
{
	char line[1024], *command = NULL, *arg = NULL;
	state *history;
	int size = 1, max_size = 4, cursor = 0;
	// the empty state is also valid so size = 1
	history = calloc(max_size, sizeof(state));
	state_init_empty(&history[0]);
	history[0].message[0] = '\0';

	while (read_command(line, &command) != 0) {
		if (!command)
			continue; //blank line
		else if (strcmp(command, "EXIT") == 0) {
			for (int i = 0; i < size; i++)
				free_state(&history[i]);
			free(history);
			break;
		} else if (strcmp(command, "LSYSTEM") == 0) {
			arg = strtok(NULL, " "); // filename
			load_lsystem(arg, &history, &cursor, &max_size, &size);
		} else if (strcmp(command, "DERIVE") == 0) {
			arg = strtok(NULL, " "); // nr of steps
			int n = -1; // -1 to verify
			if (arg)
				n = atoi(arg);
			char *deriv = derive(&history, cursor, n);
			if (deriv) {
				printf("%s\n", deriv);
				free(deriv);
			}
		} else if (strcmp(command, "LOAD") == 0) {
			arg = strtok(NULL, " ");
			load_image(arg, &history, &cursor, &max_size, &size);
		} else if (strcmp(command, "SAVE") == 0) {
			arg = strtok(NULL, " ");
			save_image(arg, &history[cursor].img);
		} else if (strcmp(command, "TURTLE") == 0) {
			char *x = strtok(NULL, " "), *y = strtok(NULL, " ");
			char *dist = strtok(NULL, " "), *orient = strtok(NULL, " ");
			char *delta = strtok(NULL, " "), *n = strtok(NULL, " ");
			char *r = strtok(NULL, " "), *g = strtok(NULL, " ");
			char *b = strtok(NULL, " "); // all the arguments
			load_turtle(&history, &cursor, &max_size, &size,
						x, y, dist, orient, delta, n, r, g, b);
		} else if (strcmp(command, "FONT") == 0) {
			arg = strtok(NULL, " ");
			load_font(arg, &history, &cursor, &max_size, &size);
		} else if (strcmp(command, "TYPE") == 0) {
			char *rest_of_line = strtok(NULL, ""); // here we take everything
			if (rest_of_line)
				load_type(rest_of_line, &history, &cursor, &max_size, &size);
			else
				printf("Invalid command\n");
		} else if (strcmp(command, "BITCHECK") == 0) {
			load_bitcheck(&history[cursor].img);
		} else if (strcmp(command, "UNDO") == 0) {
			if (cursor > 0)
				cursor--;
			else
				printf("Nothing to undo\n");
		} else if (strcmp(command, "REDO") == 0) {
			if (cursor < size - 1) {
				cursor++;
				printf("%s", history[cursor].message); // latest command msg
			} else
				printf("Nothing to redo\n");
		} else {
			printf("Invalid command\n");
		}
	}
	return 0;
}
