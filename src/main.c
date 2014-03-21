#include <pebble.h>
	
#define TIMER_MS 75

enum ConfigKeys {
	CONFIG_KEY_THEME=1,
	CONFIG_KEY_FSM=2,
	CONFIG_KEY_INV=3,
	CONFIG_KEY_ANIM=4,
	CONFIG_KEY_SEP=5,
	CONFIG_KEY_DATEFMT=6,
	CONFIG_KEY_SMART=7,
	CONFIG_KEY_VIBR=8
};

typedef struct {
	bool black;
	bool white;
	bool fsm;
	bool inv;
	bool anim;
	bool sep;
	bool smart;
	bool vibr;
	uint16_t datefmt;
} CfgDta_t;

static const struct GPathInfo HAND_PATH_INFO = {
 	.num_points = 4, 
	.points = (GPoint[]) {{-3, 0}, {-3, 222}, {3, 222}, {3, 0}}
};

static const struct GPathInfo HOUR_PATH_INFO = {
 	.num_points = 4, 
	.points = (GPoint[]) {{-3, 0}, {-3, 23}, {3, 23}, {3, 0}}
};

static const struct GPathInfo MINS_PATH_INFO = {
 	.num_points = 4, 
	.points = (GPoint[]) {{-3, 0}, {-3, 13}, {3, 13}, {3, 0}}
};

static const struct GPathInfo SECS_PATH_INFO = {
 	.num_points = 4, 
	.points = (GPoint[]) {{-3, 0}, {-3, 8}, {3, 8}, {3, 0}}
};

GPath *hand_path, *hour_path, *mins_path, *secs_path;

static const uint32_t const segments[] = {100, 100, 100};
static const VibePattern vibe_pat = {
	.durations = segments,
	.num_segments = ARRAY_LENGTH(segments),
};

Window *window;
Layer *face_layer;
TextLayer* date_layer;
InverterLayer* inv_layer;
BitmapLayer *radio_layer, *battery_layer;

static GFont digitS;
char hhBuffer[] = "00";
char ddmmyyyyBuffer[] = "00.00.0000";
static GBitmap *bmp_mask, *batteryAll;
static int16_t aktHH, aktMM;
static AppTimer *timer;
static bool b_initialized;
static CfgDta_t CfgData;

//-----------------------------------------------------------------------------------------------------------------------
static void face_update_proc(Layer *layer, GContext *ctx) 
{
	GRect bounds = layer_get_bounds(layer);
	GPoint /*center = grect_center_point(&bounds),*/ clock_center = GPoint(200, 200);
	
	graphics_context_set_stroke_color(ctx, CfgData.black ? GColorWhite : GColorBlack);
	graphics_context_set_text_color(ctx, CfgData.black ? GColorWhite : GColorBlack);
	graphics_context_set_fill_color(ctx, CfgData.black ? GColorWhite : GColorBlack);
	
	//Draw Mask
	if (!CfgData.black && !CfgData.white)
		graphics_draw_bitmap_in_rect(ctx, bmp_mask, bounds);
	
	//TRIG_MAX_ANGLE * t->tm_sec / 60
	int32_t angle = (TRIG_MAX_ANGLE * (((aktHH % 12) * 60) + (aktMM / 1))) / (12 * 60), 
		sinl = sin_lookup(angle), cosl = cos_lookup(angle);
	int16_t radV = 144, radD = 175, radT = 135;
	
	GPoint sub_center, ptLin, ptDot;
	sub_center.x = (int16_t)(sinl * (int32_t)radV / TRIG_MAX_RATIO) + clock_center.x;
	sub_center.y = (int16_t)(-cosl * (int32_t)radV / TRIG_MAX_RATIO) + clock_center.y;

	GRect sub_rect = {
		.origin = GPoint(sub_center.x - bounds.size.w / 2, sub_center.y - bounds.size.h / 2),
		.size = bounds.size
	};

	for (int32_t i = 1; i<=72; i++)
	{
		int32_t angleC = TRIG_MAX_ANGLE * i / 72,
			sinC = sin_lookup(angleC), cosC = cos_lookup(angleC);
		
		ptLin.x = (int16_t)(sinC * (int32_t)(radD) / TRIG_MAX_RATIO) + clock_center.x - sub_rect.origin.x;
		ptLin.y = (int16_t)(-cosC * (int32_t)(radD) / TRIG_MAX_RATIO) + clock_center.y - sub_rect.origin.y;

		if (ptLin.x > -40 && ptLin.x < bounds.size.w+40 && ptLin.y > -40 && ptLin.y < bounds.size.h+40)
		{
			if ((i % 6) == 0)
			{
				gpath_move_to(hour_path, ptLin);
				gpath_rotate_to(hour_path, angleC);
				gpath_draw_filled(ctx, hour_path);
				
				int16_t nHrPnt = i/6;
				if (clock_is_24h_style())
					if ((aktHH > 9 && aktHH < 21 && nHrPnt > 0 && nHrPnt < 6) ||
						(aktHH > 15 && aktHH < 3 && nHrPnt >= 6  && nHrPnt <= 12))
						nHrPnt += 12;
				
				snprintf(hhBuffer, sizeof(hhBuffer), "%d", nHrPnt);
				GSize txtSize = graphics_text_layout_get_content_size(hhBuffer, 
					fonts_get_system_font(FONT_KEY_DROID_SERIF_28_BOLD), 
					bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter);

				ptDot.x = (int16_t)(sinC * (int32_t)radT / TRIG_MAX_RATIO) + clock_center.x - sub_rect.origin.x;
				ptDot.y = (int16_t)(-cosC * (int32_t)radT / TRIG_MAX_RATIO) + clock_center.y - sub_rect.origin.y;

				graphics_draw_text(ctx, hhBuffer, 
					fonts_get_system_font(FONT_KEY_DROID_SERIF_28_BOLD), 
					GRect(ptDot.x-txtSize.w/2, ptDot.y-txtSize.h/2, txtSize.w, txtSize.h), 
					GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
			}
			else if ((i % 3) == 0)
			{
				gpath_move_to(mins_path, ptLin);
				gpath_rotate_to(mins_path, angleC);
				gpath_draw_filled(ctx, mins_path);
			}
			else
			{
				gpath_move_to(secs_path, ptLin);
				gpath_rotate_to(secs_path, angleC);
				gpath_draw_filled(ctx, secs_path);
			}
		}
	}

	//Draw Hand Path
	ptLin.x = (int16_t)(sinl * (int32_t)(radV+111) / TRIG_MAX_RATIO) + clock_center.x - sub_rect.origin.x;
	ptLin.y = (int16_t)(-cosl * (int32_t)(radV+111) / TRIG_MAX_RATIO) + clock_center.y - sub_rect.origin.y;
	
	gpath_move_to(hand_path, ptLin);
	gpath_rotate_to(hand_path, angle);
	gpath_draw_filled(ctx, hand_path);
	
	if (CfgData.sep)
		graphics_draw_line(ctx, GPoint(10, bounds.size.h-1), GPoint(bounds.size.w-10, bounds.size.h-1));
}
//-----------------------------------------------------------------------------------------------------------------------
static void handle_tick(struct tm *tick_time, TimeUnits units_changed) 
{
	if (b_initialized)
	{
		aktHH = tick_time->tm_hour;
		aktMM = tick_time->tm_min;
		layer_mark_dirty(face_layer);
	}
	
	strftime(ddmmyyyyBuffer, sizeof(ddmmyyyyBuffer), 
		CfgData.datefmt == 1 ? "%d-%m-%Y" : 
		CfgData.datefmt == 2 ? "%d/%m/%Y" : 
		CfgData.datefmt == 3 ? "%m/%d/%Y" : "%d.%m.%Y", tick_time);
	text_layer_set_text(date_layer, ddmmyyyyBuffer);
	
	//Hourly vibrate
	if (CfgData.vibr && tick_time->tm_min == 0)
		vibes_enqueue_custom_pattern(vibe_pat); 	
}
//-----------------------------------------------------------------------------------------------------------------------
static void timerCallback(void *data) 
{
	if (!b_initialized)
	{
		time_t temp = time(NULL);
		struct tm *t = localtime(&temp);
		if ((aktHH % 12) != (t->tm_hour % 12) || aktMM != t->tm_min)
		{
			int16_t nStep = (aktHH % 12) != (t->tm_hour % 12) ? 5 : 1;
			if ((t->tm_hour % 12) < 6)
			{
				//Initial Value? Set correct initial
				if (aktHH == 0 && aktMM == 0)
				{
					aktHH = t->tm_hour >= 12 ? 12 : 0;
					aktMM = 0;
				}

				if (aktMM < 60-nStep)
					aktMM += nStep;
				else
				{
					aktMM = 0;
					aktHH++;
				}
			}
			else
			{
				//Initial Value? Set correct initial
				if (aktHH == 0 && aktMM == 0)
				{
					aktHH = t->tm_hour > 12 ? 23 : 11;
					aktMM = 60;
				}
				
				if (aktMM > nStep)
					aktMM -= nStep;
				else
				{
					aktMM = 60;
					aktHH--;
				}
			}

			layer_mark_dirty(face_layer);
			timer = app_timer_register(TIMER_MS, timerCallback, NULL);
		}
		else
			b_initialized = true;
	}
}
//-----------------------------------------------------------------------------------------------------------------------
void battery_state_service_handler(BatteryChargeState charge_state) 
{
	int nImage = 0;
	if (charge_state.is_charging)
		nImage = 10;
	else 
		nImage = 10 - (charge_state.charge_percent / 10);
	
	GRect sub_rect = GRect(10*nImage, 0, 10*nImage+10, 20);
	bitmap_layer_set_bitmap(battery_layer, gbitmap_create_as_sub_bitmap(batteryAll, sub_rect));
}
//-----------------------------------------------------------------------------------------------------------------------
void bluetooth_connection_handler(bool connected)
{
	layer_set_hidden(bitmap_layer_get_layer(radio_layer), connected != true);
}
//-----------------------------------------------------------------------------------------------------------------------
static void update_configuration(void)
{
    if (persist_exists(CONFIG_KEY_THEME))
    {
        int32_t theme = persist_read_int(CONFIG_KEY_THEME);
		CfgData.black = (theme == 1);
		CfgData.white = (theme == 2);
	}
	else
	{
		CfgData.black = false;
		CfgData.white = false;
	}
	
    if (persist_exists(CONFIG_KEY_FSM))
		CfgData.fsm = persist_read_bool(CONFIG_KEY_FSM);
	else	
		CfgData.fsm = false;
	
    if (persist_exists(CONFIG_KEY_INV))
		CfgData.inv = persist_read_bool(CONFIG_KEY_INV);
	else	
		CfgData.inv = false;
	
    if (persist_exists(CONFIG_KEY_ANIM))
		CfgData.anim = persist_read_bool(CONFIG_KEY_ANIM);
	else	
		CfgData.anim = true;
	
    if (persist_exists(CONFIG_KEY_SEP))
		CfgData.sep = persist_read_bool(CONFIG_KEY_SEP);
	else	
		CfgData.sep = false;
	
    if (persist_exists(CONFIG_KEY_DATEFMT))
		CfgData.datefmt = (int16_t)persist_read_int(CONFIG_KEY_DATEFMT);
	else	
		CfgData.datefmt = 0;
	
    if (persist_exists(CONFIG_KEY_SMART))
		CfgData.smart = persist_read_bool(CONFIG_KEY_SMART);
	else	
		CfgData.smart = true;
	
    if (persist_exists(CONFIG_KEY_VIBR))
		CfgData.vibr = persist_read_bool(CONFIG_KEY_VIBR);
	else	
		CfgData.vibr = false;
	
	app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "Curr Conf: black:%d, white:%d, fsm:%d, inv:%d, anim:%d, sep:%d, datefmt:%d, smart:%d, vibr:%d",
		CfgData.black, CfgData.white, CfgData.fsm, CfgData.inv, CfgData.anim, CfgData.sep, CfgData.datefmt, CfgData.smart, CfgData.vibr);
	
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_get_root_layer(window));
	window_set_background_color(window, CfgData.white ? GColorWhite : GColorBlack);
	text_layer_set_text_color(date_layer, CfgData.white ? GColorBlack : GColorWhite);
	
	layer_remove_from_parent(face_layer);
	layer_destroy(face_layer);
	face_layer = layer_create(GRect(0, 0, bounds.size.w, CfgData.fsm ? bounds.size.h : bounds.size.w));
	layer_set_update_proc(face_layer, face_update_proc);
	layer_add_child(window_layer, face_layer);
	
	layer_remove_from_parent(text_layer_get_layer(date_layer));
	layer_remove_from_parent(bitmap_layer_get_layer(radio_layer));
	layer_remove_from_parent(bitmap_layer_get_layer(battery_layer));
	if (!CfgData.fsm)
	{
		layer_add_child(window_layer, text_layer_get_layer(date_layer));
		if (CfgData.smart)
		{
			layer_add_child(window_layer, bitmap_layer_get_layer(radio_layer));
			layer_add_child(window_layer, bitmap_layer_get_layer(battery_layer));
		}
	}	

	layer_remove_from_parent(inverter_layer_get_layer(inv_layer));
	if (CfgData.inv)
		layer_add_child(window_layer, inverter_layer_get_layer(inv_layer));
	
	//Get a time structure so that it doesn't start blank
	time_t temp = time(NULL);
	struct tm *t = localtime(&temp);

	//Manually call the tick handler when the window is loading
	handle_tick(t, MINUTE_UNIT);

	//Set Battery state
	BatteryChargeState btchg = battery_state_service_peek();
	battery_state_service_handler(btchg);
	
	//Set Bluetooth state
	bool connected = bluetooth_connection_service_peek();
	bluetooth_connection_handler(connected);
}
//-----------------------------------------------------------------------------------------------------------------------
void in_received_handler(DictionaryIterator *received, void *ctx)
{
	app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "enter in_received_handler");
    
	Tuple *akt_tuple = dict_read_first(received);
    while (akt_tuple)
    {
        app_log(APP_LOG_LEVEL_DEBUG,
                __FILE__,
                __LINE__,
                "KEY %d=%s", (int16_t)akt_tuple->key,
                akt_tuple->value->cstring);

		if (akt_tuple->key == CONFIG_KEY_THEME)
			persist_write_int(CONFIG_KEY_THEME, 
				strcmp(akt_tuple->value->cstring, "black") == 0 ? 1 : 
				strcmp(akt_tuple->value->cstring, "white") == 0 ? 2 : 0);
	
		if (akt_tuple->key == CONFIG_KEY_FSM)
			persist_write_bool(CONFIG_KEY_FSM, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		if (akt_tuple->key == CONFIG_KEY_INV)
			persist_write_bool(CONFIG_KEY_INV, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		if (akt_tuple->key == CONFIG_KEY_ANIM)
			persist_write_bool(CONFIG_KEY_ANIM, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		if (akt_tuple->key == CONFIG_KEY_SEP)
			persist_write_bool(CONFIG_KEY_SEP, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		if (akt_tuple->key == CONFIG_KEY_DATEFMT)
			persist_write_int(CONFIG_KEY_DATEFMT, 
				strcmp(akt_tuple->value->cstring, "fra") == 0 ? 1 : 
				strcmp(akt_tuple->value->cstring, "eng") == 0 ? 2 : 
				strcmp(akt_tuple->value->cstring, "usa") == 0 ? 3 : 0);
		
		if (akt_tuple->key == CONFIG_KEY_SMART)
			persist_write_bool(CONFIG_KEY_SMART, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		if (akt_tuple->key == CONFIG_KEY_VIBR)
			persist_write_bool(CONFIG_KEY_VIBR, strcmp(akt_tuple->value->cstring, "yes") == 0);
		
		akt_tuple = dict_read_next(received);
	}
	
    update_configuration();
}
//-----------------------------------------------------------------------------------------------------------------------
void in_dropped_handler(AppMessageResult reason, void *ctx)
{
    app_log(APP_LOG_LEVEL_WARNING,
            __FILE__,
            __LINE__,
            "Message dropped, reason code %d",
            reason);
}
//-----------------------------------------------------------------------------------------------------------------------
static void window_load(Window *window) 
{
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	
	digitS = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITAL_24));
	bmp_mask = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MASK);
	
	// Init layers
	face_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.w));
	layer_set_update_proc(face_layer, face_update_proc);

	date_layer = text_layer_create(GRect(0, bounds.size.w-3, bounds.size.w, bounds.size.h-bounds.size.w+3));
	text_layer_set_background_color(date_layer, GColorClear);
	text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
	text_layer_set_font(date_layer, digitS);

	//Init battery
	batteryAll = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY);
	battery_layer = bitmap_layer_create(GRect(bounds.size.w-11, bounds.size.h-21, 10, 20)); 
	bitmap_layer_set_background_color(battery_layer, GColorClear);

	//Init bluetooth radio
	radio_layer = bitmap_layer_create(GRect(0, bounds.size.h-21, 10, 20));
	bitmap_layer_set_background_color(radio_layer, GColorClear);
	bitmap_layer_set_bitmap(radio_layer, gbitmap_create_as_sub_bitmap(batteryAll, GRect(110, 0, 10, 20)));
	
	//Init Inverter Layer
	inv_layer = inverter_layer_create(bounds);	
	
	//Update Configuration
	update_configuration();
	
	//Start|Skip Animation
	if (CfgData.anim)
	{
		aktHH = aktMM = 0;
		timer = app_timer_register(500, timerCallback, NULL);
	}	
	else
		b_initialized = true;
}
//-----------------------------------------------------------------------------------------------------------------------
static void window_unload(Window *window) 
{
	layer_destroy(face_layer);
	text_layer_destroy(date_layer);
	bitmap_layer_destroy(battery_layer);
	bitmap_layer_destroy(radio_layer);
	inverter_layer_destroy(inv_layer);
	fonts_unload_custom_font(digitS);
	gbitmap_destroy(batteryAll);
	gbitmap_destroy(bmp_mask);
	if (!b_initialized)
		app_timer_cancel(timer);
}
//-----------------------------------------------------------------------------------------------------------------------
static void init(void) 
{
	b_initialized = false;

	window = window_create();
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});

	// Init paths
	hand_path = gpath_create(&HAND_PATH_INFO);
	hour_path = gpath_create(&HOUR_PATH_INFO);
	mins_path = gpath_create(&MINS_PATH_INFO);
	secs_path = gpath_create(&SECS_PATH_INFO);
	
	// Push the window onto the stack
	window_stack_push(window, true);
	
	//Subscribe ticks
	tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);

	//Subscribe smart status
	battery_state_service_subscribe(&battery_state_service_handler);
	bluetooth_connection_service_subscribe(&bluetooth_connection_handler);
	
	//Subscribe messages
	app_message_register_inbox_received(in_received_handler);
    app_message_register_inbox_dropped(in_dropped_handler);
    app_message_open(128, 128);
}
//-----------------------------------------------------------------------------------------------------------------------
static void deinit(void) 
{
	app_message_deregister_callbacks();
	tick_timer_service_unsubscribe();
	battery_state_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	
	gpath_destroy(hand_path);
	gpath_destroy(hour_path);
	gpath_destroy(mins_path);
	gpath_destroy(secs_path);
	
	window_destroy(window);
}
//-----------------------------------------------------------------------------------------------------------------------
int main(void) 
{
	init();
	app_event_loop();
	deinit();
}
//-----------------------------------------------------------------------------------------------------------------------