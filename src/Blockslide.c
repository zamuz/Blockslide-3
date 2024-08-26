#include <pebble.h>
#include "Blockslide.h"
#include "enamel.h"
#include <pebble-events/pebble-events.h>

#define NUMSLOTS 6
#define TILEW 11
#define TILEH 8
#define HSPACE 4
#define VSPACE 4

int CX;
int CY;

typedef struct {
	Layer    *layer;
	int      prevDigit;
	int      curDigit;
	uint32_t normTime;
} digitSlot;

Window *window;

int startDigit[6] = {	START_TOP_LEFT, START_TOP_RIGHT,
						START_MIDDLE_LEFT, START_MIDDLE_RIGHT,
						START_BOTTOM_LEFT, START_BOTTOM_RIGHT };

digitSlot slot[6];
bool clock_12 = false;

GColor bg_color;
static EventHandle* event_handle;

GRect slotFrame(int i) {
	int x, y;
	int w = TILEW*3;
	int h = TILEH*5;

	if (i%2) {
		x = CX + HSPACE/2;
	} else {
		x = CX - HSPACE/2 - w;
	}

	if (i<2) {
		y = CY - VSPACE - h*3/2;
	} else if (i<4) {
		y = CY - h/2;
	} else {
		y = CY + VSPACE + h/2;
	}

	return GRect(x, y, w, h);
}

digitSlot *findSlot(Layer *layer) {
	int i;
	for (i=0; i<NUMSLOTS; i++) {
		if (slot[i].layer == layer) {
			return &slot[i];
		}
	}
	
	return NULL;
}

GColor get_digit_color(Layer *layer) {
	GColor digit_color;
	for (int i = 0; i < NUMSLOTS; i++) {
		if (slot[i].layer == layer) {
			if (i < 2)
				digit_color = enamel_get_hour_color();
			else if (i < 4)
				digit_color = enamel_get_minute_color();
			else
				digit_color = enamel_get_second_color();
		}
	}
	return digit_color;
}

void updateSlot(Layer *layer, GContext *ctx) {
	//APP_LOG(//APP_LOG_LEVEL_DEBUG_VERBOSE, "updateSlot");
	int t, tx1, tx2, ty1, ty2, ox, oy, width_adj, height_adj;
	GRect bounds;
	digitSlot *slot;
	bool solid_style = strcmp(enamel_get_number_style(), "solid") == 0;
	bool grid_style = strcmp(enamel_get_number_style(), "grid") == 0;
	bool column_style = strcmp(enamel_get_number_style(), "columns") == 0;
	bool row_style = strcmp(enamel_get_number_style(), "rows") == 0;
	width_adj = solid_style ? 0 : (grid_style || column_style ? 1 : 0);
	height_adj = solid_style ? 0 : (grid_style || row_style ? 1 : 0);
	graphics_context_set_antialiased(ctx, false);
	slot = findSlot(layer);
	graphics_context_set_fill_color(ctx, enamel_get_bg_color());
	bounds = layer_get_bounds(slot->layer);
	graphics_fill_rect(ctx, GRect(0, 0, bounds.size.w, bounds.size.h), 0, GCornerNone);

	for (t=0; t<13; t++) {
		if (digits[slot->curDigit][t][0] != digits[slot->prevDigit][t][0]
			|| digits[slot->curDigit][t][1] != digits[slot->prevDigit][t][1]) {
			if (slot->normTime == ANIMATION_NORMALIZED_MAX) {
				ox = digits[slot->curDigit][t][0]*TILEW;
				oy = digits[slot->curDigit][t][1]*TILEH;
			} else {
				tx1 = digits[slot->prevDigit][t][0]*TILEW;
				tx2 = digits[slot->curDigit][t][0]*TILEW;
				ty1 = digits[slot->prevDigit][t][1]*TILEH;
				ty2 = digits[slot->curDigit][t][1]*TILEH;

				ox = slot->normTime * (tx2-tx1) / ANIMATION_NORMALIZED_MAX + tx1;
				oy = slot->normTime * (ty2-ty1) / ANIMATION_NORMALIZED_MAX + ty1;
			}
		} else {
			ox = digits[slot->curDigit][t][0]*TILEW;
			oy = digits[slot->curDigit][t][1]*TILEH;
		}	
		graphics_context_set_fill_color(ctx, get_digit_color(layer));
		graphics_fill_rect(ctx, GRect(ox, oy, TILEW-width_adj, TILEH-height_adj), 0, GCornerNone);
	}
}

void initSlot(int i, Layer *parent) {
	//APP_LOG(//APP_LOG_LEVEL_DEBUG_VERBOSE, "initSlot");
	//slot[i].normTime = ANIMATION_NORMALIZED_MAX;
	slot[i].normTime = ANIMATION_NORMALIZED_MIN;
	slot[i].prevDigit = 0;
	slot[i].curDigit = startDigit[i];
	slot[i].layer = layer_create(slotFrame(i));
	layer_set_update_proc(slot[i].layer, updateSlot);
	layer_add_child(parent, slot[i].layer);
}

void deinitSlot(int i) {
	layer_destroy(slot[i].layer);
}

void animateDigits(Animation *a, const AnimationProgress normTime) {
	//APP_LOG(//APP_LOG_LEVEL_DEBUG_VERBOSE, "animateDigits");
	int i;

	for (i=0; i< NUMSLOTS; i++) {
		if (slot[i].curDigit != slot[i].prevDigit) {
			slot[i].normTime = normTime;
			layer_mark_dirty(slot[i].layer);
		}
	}
}

void schedule_animation(struct tm *now, bool intro) {
	//APP_LOG(//APP_LOG_LEVEL_DEBUG_VERBOSE, "schedule_animation");
        int h, m, s, i;

        //if (animation_is_scheduled(anim))
        //      animation_unschedule(anim);

        h = now->tm_hour;
        m = now->tm_min;
        s = now->tm_sec;
        if (clock_12) {
                h = h%12;
                if (h == 0) {
                        h = 12;
                }
        }

        for (i=0; i<NUMSLOTS; i++) {
                slot[i].prevDigit = slot[i].curDigit;
        }

        slot[0].curDigit = h/10;
        slot[1].curDigit = h%10;
        slot[2].curDigit = m/10;
        slot[3].curDigit = m%10;
        slot[4].curDigit = s/10;
        slot[5].curDigit = s%10;

        static const AnimationImplementation animImpl = {
                .setup = NULL,
                .update = animateDigits,
                .teardown = NULL
        };
        Animation *anim = animation_create();
        animation_set_delay(anim, intro ? 500 : 0);
	int duration = enamel_get_animation_duration();
        animation_set_duration(anim, intro ? (duration > 100 ? duration : 100) : duration);
        animation_set_implementation(anim, &animImpl);
        animation_set_curve(anim, AnimationCurveLinear);
	if (intro) {
		animation_set_handlers(anim, (AnimationHandlers) { .stopped = finish_intro }, NULL);
	}
        animation_schedule(anim);
}

void finish_intro(Animation *animation, bool finished, void *context) {
	//APP_LOG(//APP_LOG_LEVEL_DEBUG_VERBOSE, "finish_intro");
	tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
}

void handle_tick(struct tm *now, TimeUnits units_changed) {
	//APP_LOG(//APP_LOG_LEVEL_DEBUG_VERBOSE, "handle_tick");
	schedule_animation(now, false);
}

void handle_config_change(void *context) {
	//APP_LOG(//APP_LOG_LEVEL_DEBUG_VERBOSE, "handle_config_change");
	window_set_background_color(window, enamel_get_bg_color());
        for (int i=0; i< NUMSLOTS; i++) {
                layer_mark_dirty(slot[i].layer);
        }
}

void handle_init() {
	//APP_LOG(//APP_LOG_LEVEL_DEBUG_VERBOSE, "handle_init");
	Layer *rootLayer;
	int i;
	enamel_init(0, 0);
	window = window_create();
	window_stack_push(window, true);
	window_set_background_color(window, enamel_get_bg_color());
	
	rootLayer = window_get_root_layer(window);
	GRect root_bounds = layer_get_bounds(rootLayer);
	CX = root_bounds.size.w / 2;
	CY = root_bounds.size.h / 2;

	for (i=0; i<NUMSLOTS; i++) {
		initSlot(i, rootLayer);
	}

	clock_12 = !clock_is_24h_style();

	events_app_message_open();
	event_handle = enamel_settings_received_subscribe(handle_config_change, NULL);
	time_t tm = time(NULL);
	struct tm *now = localtime(&tm);
	schedule_animation(now, true);
}

void handle_deinit() {
	int i;
	enamel_deinit();
	tick_timer_service_unsubscribe();
	for (i=0; i<NUMSLOTS; i++) {
		deinitSlot(i);
	}
	enamel_settings_received_unsubscribe(event_handle);
	window_destroy(window);
}
int main(void) {
	handle_init();
	app_event_loop();
	handle_deinit();
}
