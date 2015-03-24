#include "minisphere.h"
#include "api.h"
#include "color.h"

#include "primitives.h"

static duk_ret_t js_GetClippingRectangle   (duk_context* ctx);
static duk_ret_t js_SetClippingRectangle   (duk_context* ctx);
static duk_ret_t js_ApplyColorMask         (duk_context* ctx);
static duk_ret_t js_GradientCircle         (duk_context* ctx);
static duk_ret_t js_GradientRectangle      (duk_context* ctx);
static duk_ret_t js_Line                   (duk_context* ctx);
static duk_ret_t js_OutlinedCircle         (duk_context* ctx);
static duk_ret_t js_OutlinedRectangle      (duk_context* ctx);
static duk_ret_t js_OutlinedRoundRectangle (duk_context* ctx);
static duk_ret_t js_Point                  (duk_context* ctx);
static duk_ret_t js_PointSeries            (duk_context* ctx);
static duk_ret_t js_Rectangle              (duk_context* ctx);
static duk_ret_t js_RoundRectangle         (duk_context* ctx);
static duk_ret_t js_Triangle               (duk_context* ctx);

void
init_primitives_api(void)
{
	register_api_func(g_duktape, NULL, "GetClippingRectangle", js_GetClippingRectangle);
	register_api_func(g_duktape, NULL, "SetClippingRectangle", js_SetClippingRectangle);
	register_api_func(g_duktape, NULL, "ApplyColorMask", js_ApplyColorMask);
	register_api_func(g_duktape, NULL, "GradientCircle", js_GradientCircle);
	register_api_func(g_duktape, NULL, "GradientRectangle", js_GradientRectangle);
	register_api_func(g_duktape, NULL, "Line", js_Line);
	register_api_func(g_duktape, NULL, "OutlinedCircle", js_OutlinedCircle);
	register_api_func(g_duktape, NULL, "OutlinedRectangle", js_OutlinedRectangle);
	register_api_func(g_duktape, NULL, "OutlinedRoundRectangle", js_OutlinedRoundRectangle);
	register_api_func(g_duktape, NULL, "Point", js_Point);
	register_api_func(g_duktape, NULL, "PointSeries", js_PointSeries);
	register_api_func(g_duktape, NULL, "Rectangle", js_Rectangle);
	register_api_func(g_duktape, NULL, "RoundRectangle", js_RoundRectangle);
	register_api_func(g_duktape, NULL, "Triangle", js_Triangle);
}

static duk_ret_t
js_GetClippingRectangle(duk_context* ctx)
{
	rect_t clip;

	clip = get_clip_rectangle();
	duk_push_object(ctx);
	duk_push_int(ctx, clip.x1); duk_put_prop_string(ctx, -2, "x");
	duk_push_int(ctx, clip.y1); duk_put_prop_string(ctx, -2, "y");
	duk_push_int(ctx, clip.x2 - clip.x1); duk_put_prop_string(ctx, -2, "width");
	duk_push_int(ctx, clip.y2 - clip.y1); duk_put_prop_string(ctx, -2, "height");
	return 1;
}

static duk_ret_t
js_SetClippingRectangle(duk_context* ctx)
{
	int x = duk_require_int(ctx, 0);
	int y = duk_require_int(ctx, 1);
	int width = duk_require_int(ctx, 2);
	int height = duk_require_int(ctx, 3);

	set_clip_rectangle(new_rect(x, y, x + width, y + height));
	return 0;
}

static duk_ret_t
js_ApplyColorMask(duk_context* ctx)
{
	color_t color = duk_require_sphere_color(ctx, 0);
	
	float rect_w, rect_h;

	rect_w = al_get_display_width(g_display);
	rect_h = al_get_display_height(g_display);
	if (!is_skipped_frame())
		al_draw_filled_rectangle(0, 0, rect_w, rect_h, nativecolor(color));
	return 0;
}

static duk_ret_t
js_GradientCircle(duk_context* ctx)
{
	color_t inner_color;
	color_t outer_color;
	float   radius;
	float   x, y;

	x = (float)duk_require_number(ctx, 0);
	y = (float)duk_require_number(ctx, 1);
	radius = (float)duk_require_number(ctx, 2);
	inner_color = duk_require_sphere_color(ctx, 3);
	outer_color = duk_require_sphere_color(ctx, 4);
	// TODO: actually draw a gradient circle instead of a solid one
	if (!is_skipped_frame())
		al_draw_filled_circle(x, y, radius, nativecolor(inner_color));
	return 0;
}

static duk_ret_t
js_GradientRectangle(duk_context* ctx)
{
	int x1 = duk_require_int(ctx, 0);
	int y1 = duk_require_int(ctx, 1);
	int x2 = x1 + duk_require_int(ctx, 2);
	int y2 = y1 + duk_require_int(ctx, 3);
	color_t color_ul = duk_require_sphere_color(ctx, 4);
	color_t color_ur = duk_require_sphere_color(ctx, 5);
	color_t color_lr = duk_require_sphere_color(ctx, 6);
	color_t color_ll = duk_require_sphere_color(ctx, 7);

	if (!is_skipped_frame()) {
		ALLEGRO_VERTEX verts[] = {
			{ x1, y1, 0, 0, 0, nativecolor(color_ul) },
			{ x2, y1, 0, 0, 0, nativecolor(color_ur) },
			{ x1, y2, 0, 0, 0, nativecolor(color_ll) },
			{ x2, y2, 0, 0, 0, nativecolor(color_lr) }
		};
		al_draw_prim(verts, NULL, NULL, 0, 4, ALLEGRO_PRIM_TRIANGLE_STRIP);
	}
	return 0;
}

static duk_ret_t
js_Line(duk_context* ctx)
{
	int x1 = duk_require_int(ctx, 0);
	int y1 = duk_require_int(ctx, 1);
	int x2 = duk_require_int(ctx, 2);
	int y2 = duk_require_int(ctx, 3);
	color_t color = duk_require_sphere_color(ctx, 4);

	if (!is_skipped_frame())
		al_draw_line(x1, y1, x2, y2, nativecolor(color), 1);
	return 0;
}

static duk_ret_t
js_OutlinedCircle(duk_context* ctx)
{
	bool    antialiased = false;
	color_t color;
	int     n_args;
	int     x, y, radius;

	n_args = duk_get_top(ctx);
	x = duk_to_int(ctx, 0);
	y = duk_to_int(ctx, 1);
	radius = duk_to_int(ctx, 2);
	color = duk_require_sphere_color(ctx, 3);
	if (n_args >= 5) antialiased = duk_require_boolean(ctx, 4);
	if (!is_skipped_frame()) al_draw_circle(x, y, radius, nativecolor(color), 1);
	return 0;
}

static duk_ret_t
js_OutlinedRectangle(duk_context* ctx)
{
	color_t color;
	int     n_args;
	float   x1, y1, x2, y2;

	n_args = duk_get_top(ctx);
	x1 = duk_to_int(ctx, 0) + 0.5;
	y1 = duk_to_int(ctx, 1) + 0.5;
	x2 = x1 + duk_to_int(ctx, 2) - 1;
	y2 = y1 + duk_to_int(ctx, 3) - 1;
	color = duk_require_sphere_color(ctx, 4);
	int thickness = n_args >= 6 ? duk_to_int(ctx, 5) : 1;
	if (!is_skipped_frame())
		al_draw_rectangle(x1, y1, x2, y2, nativecolor(color), thickness);
	return 0;
}

static duk_ret_t
js_OutlinedRoundRectangle(duk_context* ctx)
{
	int n_args = duk_get_top(ctx);
	float x = duk_require_int(ctx, 0) + 0.5;
	float y = duk_require_int(ctx, 1) + 0.5;
	int w = duk_require_int(ctx, 2);
	int h = duk_require_int(ctx, 3);
	float radius = duk_require_number(ctx, 4);
	color_t color = duk_require_sphere_color(ctx, 5);
	int thickness = n_args >= 7 ? duk_require_int(ctx, 6) : 1;

	if (!is_skipped_frame())
		al_draw_rounded_rectangle(x, y, x + w - 1, y + h - 1, radius, radius, nativecolor(color), thickness);
	return 0;
}

static duk_ret_t
js_Point(duk_context* ctx)
{
	float x = duk_require_int(ctx, 0) + 0.5;
	float y = duk_require_int(ctx, 1) + 0.5;
	color_t color = duk_require_sphere_color(ctx, 2);
	
	if (!is_skipped_frame())
		al_draw_pixel(x, y, nativecolor(color));
	return 0;
}

static duk_ret_t
js_PointSeries(duk_context* ctx)
{
	duk_require_object_coercible(ctx, 0);
	color_t color = duk_require_sphere_color(ctx, 1);

	size_t          num_points;
	int             x, y;
	ALLEGRO_VERTEX* vertices;
	ALLEGRO_COLOR   vtx_color;

	unsigned int i;

	if (!duk_is_array(ctx, 0))
		duk_error_ni(ctx, -1, DUK_ERR_ERROR, "PointSeries(): First argument must be an array");
	duk_get_prop_string(ctx, 0, "length"); num_points = duk_get_uint(ctx, 0); duk_pop(ctx);
	if ((vertices = calloc(num_points, sizeof(ALLEGRO_VERTEX))) == NULL)
		duk_error_ni(ctx, -1, DUK_ERR_ERROR, "PointSeries(): Failed to allocate vertex buffer (internal error)");
	vtx_color = nativecolor(color);
	for (i = 0; i < num_points; ++i) {
		duk_get_prop_index(ctx, 0, i);
		duk_get_prop_string(ctx, 0, "x"); x = duk_require_int(ctx, -1); duk_pop(ctx);
		duk_get_prop_string(ctx, 0, "y"); y = duk_require_int(ctx, -1); duk_pop(ctx);
		duk_pop(ctx);
		vertices[i].x = x; vertices[i].y = y;
		vertices[i].color = vtx_color;
	}
	al_draw_prim(vertices, NULL, NULL, 0, num_points, ALLEGRO_PRIM_POINT_LIST);
	free(vertices);
	return 0;
}

static duk_ret_t
js_Rectangle(duk_context* ctx)
{
	int x = duk_require_int(ctx, 0);
	int y = duk_require_int(ctx, 1);
	int w = duk_require_int(ctx, 2);
	int h = duk_require_int(ctx, 3);
	color_t color = duk_require_sphere_color(ctx, 4);

	if (!is_skipped_frame())
		al_draw_filled_rectangle(x, y, x + w, y + h, nativecolor(color));
	return 0;
}

static duk_ret_t
js_RoundRectangle(duk_context* ctx)
{
	int x = duk_require_int(ctx, 0);
	int y = duk_require_int(ctx, 1);
	int w = duk_require_int(ctx, 2);
	int h = duk_require_int(ctx, 3);
	float radius = duk_require_number(ctx, 4);
	color_t color = duk_require_sphere_color(ctx, 5);

	if (!is_skipped_frame())
		al_draw_filled_rounded_rectangle(x, y, x + w, y + h, radius, radius, nativecolor(color));
	return 0;
}

static duk_ret_t
js_Triangle(duk_context* ctx)
{
	int x1 = duk_require_int(ctx, 0);
	int y1 = duk_require_int(ctx, 1);
	int x2 = duk_require_int(ctx, 2);
	int y2 = duk_require_int(ctx, 3);
	int x3 = duk_require_int(ctx, 4);
	int y3 = duk_require_int(ctx, 5);
	color_t color = duk_require_sphere_color(ctx, 6);

	if (!is_skipped_frame())
		al_draw_filled_triangle(x1, y1, x2, y2, x3, y3, nativecolor(color));
	return 0;
}
