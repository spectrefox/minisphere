#include "minisphere.h"
#include "tileset.h"

#include "atlas.h"
#include "image.h"
#include "obsmap.h"

struct tileset
{
	unsigned int id;
	atlas_t*     atlas;
	int          atlas_pitch;
	int          height;
	image_t*     texture;
	int          num_tiles;
	struct tile* tiles;
	int          width;
};

struct tile
{
	int        delay;
	int        frames_left;
	image_t*   image;
	int        image_index;
	lstring_t* name;
	int        next_index;
	int        num_obs_lines;
	obsmap_t*  obsmap;
};

#pragma pack(push, 1)
struct rts_header
{
	uint8_t  signature[4];
	uint16_t version;
	uint16_t num_tiles;
	uint16_t tile_width;
	uint16_t tile_height;
	uint16_t tile_bpp;
	uint8_t  compression;
	uint8_t  has_obstructions;
	uint8_t  reserved[240];
};

struct rts_tile_header
{
	uint8_t  unknown_1;
	uint8_t  animated;
	int16_t  next_tile;
	int16_t  delay;
	uint8_t  unknown_2;
	uint8_t  obsmap_type;
	uint16_t num_segments;
	uint16_t name_length;
	uint8_t  terraformed;
	uint8_t  reserved[19];
};
#pragma pack(pop)

static unsigned int s_next_tileset_id = 0;

tileset_t*
tileset_new(const char* filename)
{
	sfs_file_t* file;
	tileset_t*  tileset;

	console_log(2, "loading tileset #%u as `%s`", s_next_tileset_id, filename);

	if ((file = sfs_fopen(g_fs, filename, NULL, "rb")) == NULL)
		goto on_error;
	tileset = tileset_read(file);
	sfs_fclose(file);
	return tileset;

on_error:
	console_log(2, "failed to load tileset #%u", s_next_tileset_id++);
	return NULL;
}

tileset_t*
tileset_read(sfs_file_t* file)
{
	atlas_t*               atlas = NULL;
	long                   file_pos;
	struct rts_header      rts;
	rect_t                 segment;
	struct rts_tile_header tilehdr;
	struct tile*           tiles = NULL;
	tileset_t*             tileset = NULL;

	int i, j;

	memset(&rts, 0, sizeof(struct rts_header));
	
	console_log(2, "reading tileset #%u from open file", s_next_tileset_id);

	if (file == NULL) goto on_error;
	file_pos = sfs_ftell(file);
	if ((tileset = calloc(1, sizeof(tileset_t))) == NULL) goto on_error;
	if (sfs_fread(&rts, sizeof(struct rts_header), 1, file) != 1)
		goto on_error;
	if (memcmp(rts.signature, ".rts", 4) != 0 || rts.version < 1 || rts.version > 1)
		goto on_error;
	if (rts.tile_bpp != 32) goto on_error;
	if (!(tiles = calloc(rts.num_tiles, sizeof(struct tile)))) goto on_error;
	
	// read in all the tile bitmaps (use atlasing)
	if (!(atlas = atlas_new(rts.num_tiles, rts.tile_width, rts.tile_height)))
		goto on_error;
	atlas_lock(atlas);
	for (i = 0; i < rts.num_tiles; ++i)
		if (!(tiles[i].image = atlas_load(atlas, file, i, rts.tile_width, rts.tile_height)))
			goto on_error;
	atlas_unlock(atlas);
	tileset->atlas = atlas;

	// read in tile headers and obstruction maps
	for (i = 0; i < rts.num_tiles; ++i) {
		if (sfs_fread(&tilehdr, sizeof(struct rts_tile_header), 1, file) != 1)
			goto on_error;
		tiles[i].name = read_lstring_raw(file, tilehdr.name_length, true);
		tiles[i].next_index = tilehdr.animated ? tilehdr.next_tile : i;
		tiles[i].delay = tilehdr.animated ? tilehdr.delay : 0;
		tiles[i].image_index = i;
		tiles[i].frames_left = tiles[i].delay;
		if (rts.has_obstructions) {
			switch (tilehdr.obsmap_type) {
			case 1:  // pixel-perfect obstruction (no longer supported)
				sfs_fseek(file, rts.tile_width * rts.tile_height, SFS_SEEK_CUR);
				break;
			case 2:  // line segment-based obstruction
				tiles[i].num_obs_lines = tilehdr.num_segments;
				if ((tiles[i].obsmap = obsmap_new()) == NULL) goto on_error;
				for (j = 0; j < tilehdr.num_segments; ++j) {
					if (!fread_rect_16(file, &segment))
						goto on_error;
					obsmap_add_line(tiles[i].obsmap, segment);
				}
				break;
			default:
				goto on_error;
			}
		}
	}

	// wrap things up
	tileset->id = s_next_tileset_id++;
	tileset->width = rts.tile_width;
	tileset->height = rts.tile_height;
	tileset->num_tiles = rts.num_tiles;
	tileset->tiles = tiles;
	return tileset;

on_error:  // oh no!
	console_log(2, "failed to read tileset #%u", s_next_tileset_id);
	if (file != NULL)
		sfs_fseek(file, file_pos, SFS_SEEK_SET);
	if (tiles != NULL) {
		for (i = 0; i < rts.num_tiles; ++i) {
			lstr_free(tiles[i].name);
			obsmap_free(tiles[i].obsmap);
			image_free(tiles[i].image);
		}
		free(tileset->tiles);
	}
	atlas_free(atlas);
	free(tileset);
	return NULL;
}

void
tileset_free(tileset_t* tileset)
{
	int i;

	console_log(3, "disposing tileset #%u as it is no longer in use", tileset->id);

	for (i = 0; i < tileset->num_tiles; ++i) {
		lstr_free(tileset->tiles[i].name);
		image_free(tileset->tiles[i].image);
		obsmap_free(tileset->tiles[i].obsmap);
	}
	atlas_free(tileset->atlas);
	free(tileset->tiles);
	free(tileset);
}

int
tileset_get_next(const tileset_t* tileset, int tile_index)
{
	int next_index;
	
	next_index = tileset->tiles[tile_index].next_index;
	return next_index >= 0 && next_index < tileset->num_tiles
		? next_index : tile_index;
}

int
tileset_len(const tileset_t* tileset)
{
	return tileset->num_tiles;
}

int
tileset_get_delay(const tileset_t* tileset, int tile_index)
{
	return tileset->tiles[tile_index].delay;
}

image_t*
tileset_get_image(const tileset_t* tileset, int tile_index)
{
	return tileset->tiles[tile_index].image;
}

const lstring_t*
tileset_get_name(const tileset_t* tileset, int tile_index)
{
	return tileset->tiles[tile_index].name;
}

const obsmap_t*
tileset_obsmap(const tileset_t* tileset, int tile_index)
{
	if (tile_index >= 0)
		return tileset->tiles[tile_index].obsmap;
	else
		return NULL;
}

void
tileset_get_size(const tileset_t* tileset, int* out_w, int* out_h)
{
	*out_w = tileset->width;
	*out_h = tileset->height;
}

void
tileset_set_next(tileset_t* tileset, int tile_index, int next_index)
{
	tileset->tiles[tile_index].next_index = next_index;
}

void
tileset_set_delay(tileset_t* tileset, int tile_index, int delay)
{
	tileset->tiles[tile_index].delay = delay;
}

void
tileset_set_image(tileset_t* tileset, int tile_index, image_t* image)
{
	// CAUTION: the new tile image is assumed to be the same same size as the tile it
	//     replaces.  if it's not, the engine won't crash, but it may cause graphical
	//     glitches.
	
	rect_t xy;
	
	xy = atlas_xy(tileset->atlas, tile_index);
	image_blit(image, tileset->texture, xy.x1, xy.y1);
}

bool
tileset_set_name(tileset_t* tileset, int tile_index, const lstring_t* name)
{
	lstring_t* new_name;

	if (!(new_name = lstr_dup(name)))
		return false;
	lstr_free(tileset->tiles[tile_index].name);
	tileset->tiles[tile_index].name = new_name;
	return true;
}

void
tileset_update(tileset_t* tileset)
{
	struct tile* tile;
	
	int i;

	for (i = 0; i < tileset->num_tiles; ++i) {
		tile = &tileset->tiles[i];
		if (tile->frames_left > 0 && --tile->frames_left == 0) {
			tile->image_index = tileset_get_next(tileset, tile->image_index);
			tile->frames_left = tileset_get_delay(tileset, tile->image_index);
		}
	}
}

void
tileset_draw(const tileset_t* tileset, color_t mask, float x, float y, int tile_index)
{
	if (tile_index < 0)
		return;
	tile_index = tileset->tiles[tile_index].image_index;
	al_draw_tinted_bitmap(image_bitmap(tileset->tiles[tile_index].image),
		al_map_rgba(mask.r, mask.g, mask.b, mask.a), x, y, 0x0);
}
