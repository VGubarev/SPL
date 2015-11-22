#include "lab6.h"

static pixel_t *get_pixel(pixel_t *pixels, uint32_t line, uint32_t x, uint32_t y){
	return pixels+y*line+x;
}

static uint32_t round_4(uint32_t n){
	return n%4?n+(4-n%4):n;
}

static char *ts_name(){
	char *name = malloc(256);
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	sprintf(name, "pics/%d-%d-%d %d:%d:%d.bmp", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	return name;
}


/* TODO: MOVE AWAY TO OTHER FILES*/

/* Read 2 first bytes of file
 * @RETURN error code
 * EREAD - file < FORMAT_SIZE
 * SUCCESS - everything is OK
 */
int get_type(FILE *f_image, uint16_t *type){
	uint16_t format;
	int count = fread(&format, sizeof(char), FORMAT_SIZE, f_image);
	fseek(f_image, 0, SEEK_SET);
	if(count != FORMAT_SIZE)
		return EREAD;

	*type = format;
	return SUCCESS;
}
/* read first 54 bytes of file and set up file pointer to bitmap or something
   same*/
int read_bmp_head(FILE *f_image, image_t *image){
	struct bmp_header_t header;
	size_t count;

	count = fread(&header, 1, BMP_HEADER_SIZE, f_image);
	if(count != BMP_HEADER_SIZE || header.biBitCount != 24)
		return EWRONGHEAD; 

	count = fseek(f_image, header.bOffBits, SEEK_SET);
	if (count != 0)
		return EREAD;

	image->width = header.biWidth;
	image->height = header.biHeight;

	return SUCCESS;
}

/* at this time f_image points to bitmap. Need to read it without trash */
int read_bmp_body(FILE *f_image, image_t *image){
	size_t count;
	uint32_t i;
	unsigned int diff = round_4(image->width*3)-image->width*3;
	pixel_t *t, *buf;
	image->pixels = malloc(image->width*image->height*sizeof(pixel_t));
	t = image->pixels;
	buf = malloc(diff + image->width*3);

	for(i = 0; i < image->height; i++){
		count = fread(buf, sizeof(pixel_t), image->width, f_image);
		fseek(f_image, diff, SEEK_CUR);
		if(count != image->width){
			free(buf);
			return EREAD;
		}
		memcpy(t, buf, image->width*3);
		t += image->width;
	}
	free(buf);
	return SUCCESS;
}

int write_bmp_head(FILE *f_image, image_t *image){
	bmp_header_t bmp_header;
	size_t count;
	FILL_BMP_HEADER(bmp_header, image->width, image->height)
	count = fwrite(&bmp_header, sizeof(char), BMP_HEADER_SIZE, f_image);
	if(count != BMP_HEADER_SIZE)
		return EWRITE;
	count = fseek(f_image, 122, SEEK_SET);
	if(count != 0)
		return EWRITE;

	return SUCCESS;
}

int write_bmp_body(FILE *f_image, image_t *image){
	uint32_t i;
	unsigned int diff = round_4(image->width*3)-image->width*3;
	char trash[4] = {
		0,0,0
	};
	size_t count;
	pixel_t *t = image->pixels;
	for(i = 0; i < image->height; i++){
		count = fwrite(t, sizeof(pixel_t), image->width, f_image);
		fwrite(&trash, 1, diff, f_image);
		if(count != image->width){
			return EWRITE;
		}
		t += count;
	}
	return SUCCESS;
}

int get_spec_ops(uint16_t type, spec_ops_t *ops){
	int i;

	spec_ops_t ops_list[1];
	OPERATION_LIST(0x4D42, bmp)
	ops_list[0] = bmp_ops;

	for(i = 0; i < 1; i++){
		if(ops_list[i].type == type){
			*ops = ops_list[i];	
			return SUCCESS;
		}
	}

	return ENOIMAGE; 
}

int read_image(const char *imagepath, image_t *image){
	/* first of all we need to recognize which type of this file*/
	int err;
	uint16_t type;
	/* dont forget free dat memory */
	struct spec_ops_t *ops = malloc(sizeof(spec_ops_t));  

	FILE *f_image = fopen(imagepath, "rb");
	if(f_image == NULL)
		return EOPENFILE;

	DO_AND_CHECK(get_type(f_image, &type))

	/* now we need to get list of operations for this filetype*/
	DO_AND_CHECK(get_spec_ops(type, ops))
	image->ops = ops;
	
	/* and now we can read file */
	DO_AND_CHECK((image->ops)->read_spec_head(f_image, image))			
	DO_AND_CHECK((image->ops)->read_spec_body(f_image, image))
	fclose(f_image);
	return SUCCESS;
}

int write_image(char *imagepath, image_t *image){
	int err;
	FILE *f_image = fopen(imagepath, "wb");
	if(f_image == NULL)
		return EWRITE;

	DO_AND_CHECK((image->ops)->write_spec_head(f_image, image))
	DO_AND_CHECK((image->ops)->write_spec_body(f_image, image))

	fclose(f_image);
	return SUCCESS;
}

int rotate(image_t *old, image_t *new){
	uint32_t i, j;
	pixel_t *pixel;
	new->width = old->height;
	new->height = old->width;

	for(i = 0; i < old->height; i++){
		for(j = 0; j < old->width; j++){
			pixel =	get_pixel(new->pixels, new->width, i, new->height - j - 1);
			*pixel = *(get_pixel(old->pixels, old->width, j, i));	
		}
	}
	return 0;
}

int main(int argc, char **argv){
	image_t image;
	image_t rotated;
	char *name;
	int err;
	struct spec_ops_t *ops = malloc(sizeof(spec_ops_t));

	DO_AND_CHECK(read_image(argv[1], &image))

	rotated.pixels = malloc(sizeof(pixel_t)*image.width*image.height);
	get_spec_ops(0x4D42, ops);
	rotated.ops = ops;
	
	rotate(&image, &rotated);
	
	/*make a name for rotated pic*/
	name = ts_name(); 
	
	DO_AND_CHECK(write_image(name, &rotated))


	free(image.pixels);	free(image.ops); free(rotated.pixels); free(rotated.ops); free(name);

	return SUCCESS;
}


