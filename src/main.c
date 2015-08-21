#include <pebble.h>
	
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

enum TimerKey {
	TIMER_ANIM_FACE = 0x0001,
	TIMER_ANIM_FACE_MS = 75,
	TIMER_ANIM_BATT = 0x0002,
	TIMER_ANIM_BATT_MS = 1000
};

typedef struct {
	bool circle;
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

static const uint32_t segments_hr[] = {100, 100, 100};
static const VibePattern vibe_pat_hr = {
	.durations = segments_hr,
	.num_segments = ARRAY_LENGTH(segments_hr),
};

static const uint32_t segments_bt[] = {100, 100, 100, 100, 400, 400, 100, 100, 100};
static const VibePattern vibe_pat_bt = {
	.durations = segments_bt,
	.num_segments = ARRAY_LENGTH(segments_bt),
};

Window *window;
Layer *face_layer;
TextLayer* date_layer;
BitmapLayer *radio_layer, *battery_layer;

static GFont digitS;
char hhBuffer[] = "00";
char ddmmyyyyBuffer[] = "00.00.0000";
static GBitmap *bmp_mask, *bmp_batt, *bmp_radio, *batteryAll;
static int16_t aktHH, aktMM, aktBatt, aktBattAnim, aktBT;
static AppTimer *timer_face, *timer_batt;
static bool b_initialized, b_charging;
static CfgDta_t CfgData;
static PropertyAnimation *s_prop_anim_date, *s_prop_anim_bt, *s_prop_anim_batt;

//-----------------------------------------------------------------------------------------------------------------------
static void face_update_proc(Layer *layer, GContext *ctx) 
{
	GRect bounds = layer_get_bounds(layer);
	GPoint /*center = grect_center_point(&bounds),*/ clock_center = GPoint(200, 200);
	
	//Draw Mask
	if (CfgData.circle)
		graphics_draw_bitmap_in_rect(ctx, bmp_mask, bounds);
	
	graphics_context_set_stroke_color(ctx, CfgData.circle || CfgData.inv ? GColorBlack : GColorWhite);
	graphics_context_set_text_color(ctx, CfgData.circle || CfgData.inv ? GColorBlack : GColorWhite);
	graphics_context_set_fill_color(ctx, CfgData.circle || CfgData.inv ? GColorBlack : GColorWhite);
	
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
						(((aktHH > 15 && aktHH <= 23) || (aktHH >= 0 && aktHH < 3)) && nHrPnt >= 6  && nHrPnt <= 12))
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
	
#ifdef PBL_COLOR
	graphics_context_set_fill_color(ctx, GColorOrange);
#else
	graphics_context_set_fill_color(ctx, CfgData.circle || CfgData.inv ? GColorBlack : GColorWhite);
#endif

	gpath_move_to(hand_path, ptLin);
	gpath_rotate_to(hand_path, angle);
	gpath_draw_filled(ctx, hand_path);

	//Only if no Mask...
	if (!CfgData.circle) 
	{
		graphics_context_set_stroke_color(ctx, CfgData.inv ? GColorWhite : GColorBlack);
		gpath_draw_outline(ctx, hand_path);
	}
	
	//Draw Separator Line
	if (CfgData.sep && !CfgData.circle)
	{
		graphics_context_set_stroke_color(ctx, CfgData.inv ? GColorBlack : GColorWhite);
		graphics_draw_line(ctx, GPoint(10, bounds.size.h-1), GPoint(bounds.size.w-10, bounds.size.h-1));
	}
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
		CfgData.datefmt == 3 ? "%m/%d/%Y" : 
		CfgData.datefmt == 4 ? "%Y-%m-%d" :  
		CfgData.datefmt == 5 ? "%d.%m.%y" : 
		CfgData.datefmt == 6 ? "%d-%m-%y" : 
		CfgData.datefmt == 7 ? "%d/%m/%y" : 
		CfgData.datefmt == 8 ? "%m/%d/%y" : 
		CfgData.datefmt == 9 ? "%y-%m-%d" : "%d.%m.%Y", tick_time);
	/*
	snprintf(ddmmyyyyBuffer, sizeof(ddmmyyyyBuffer), 
		CfgData.datefmt == 1 ? "%d-%d-%d" : 
		CfgData.datefmt == 2 ? "%d/%d/%d" : 
		CfgData.datefmt == 3 ? "%d/%d/%d" : 
		CfgData.datefmt == 4 ? "%d-%d-%d" : "%d.%d.%d", 88, 88, 8888);
	*/
	//strcpy(ddmmyyyyBuffer, "00.00.0000");
	
	text_layer_set_text(date_layer, ddmmyyyyBuffer);
	
	//Hourly vibrate
	if (CfgData.vibr && tick_time->tm_min == 0)
		vibes_enqueue_custom_pattern(vibe_pat_hr); 	
}
//-----------------------------------------------------------------------------------------------------------------------
static void timerCallback(void *data) 
{
	if ((int)data == TIMER_ANIM_FACE && !b_initialized)
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
				
				//Little workaround if time is close to the 12 o'clock
				if ((aktHH % 12) == (t->tm_hour % 12) && aktMM < t->tm_min)
					aktMM = t->tm_min;
			}

			layer_mark_dirty(face_layer);
			timer_face = app_timer_register(TIMER_ANIM_FACE_MS, timerCallback, (void*)TIMER_ANIM_FACE);
		}
		else
			b_initialized = true;
	}
	else if ((int)data == TIMER_ANIM_BATT && b_charging)
	{
		int nImage = 10 - (aktBattAnim / 10);
		
		bitmap_layer_set_bitmap(battery_layer, NULL);
		gbitmap_destroy(bmp_batt);
		bmp_batt = gbitmap_create_as_sub_bitmap(batteryAll, GRect(10*nImage, 0, 10, 20));
		bitmap_layer_set_bitmap(battery_layer, bmp_batt);

		aktBattAnim += 10;
		if (aktBattAnim > 100)
			aktBattAnim = aktBatt;
		timer_batt = app_timer_register(TIMER_ANIM_BATT_MS, timerCallback, (void*)TIMER_ANIM_BATT);
	}
}
//-----------------------------------------------------------------------------------------------------------------------
void battery_state_service_handler(BatteryChargeState charge_state) 
{
	int nImage = 0;
	aktBatt = charge_state.charge_percent;
	
	if (charge_state.is_charging)
	{
		if (!b_charging)
		{
			nImage = 10;
			b_charging = true;
			aktBattAnim = aktBatt;
			timer_batt = app_timer_register(TIMER_ANIM_BATT_MS, timerCallback, (void*)TIMER_ANIM_BATT);
		}
	}
	else
	{
		nImage = 10 - (aktBatt / 10);
		b_charging = false;
	}
	
	bmp_batt = gbitmap_create_as_sub_bitmap(batteryAll, GRect(10*nImage, 0, 10, 20));
	bitmap_layer_set_bitmap(battery_layer, bmp_batt);
}
//-----------------------------------------------------------------------------------------------------------------------
void bluetooth_connection_handler(bool connected)
{
	layer_set_hidden(bitmap_layer_get_layer(radio_layer), connected != true);
	
	if (!connected && aktBT == 1)
		vibes_enqueue_custom_pattern(vibe_pat_bt); 	
	
	aktBT = connected;
}
//-----------------------------------------------------------------------------------------------------------------------
static void update_configuration(void)
{
    if (persist_exists(CONFIG_KEY_THEME))
    {
        int32_t theme = persist_read_int(CONFIG_KEY_THEME);
		CfgData.circle = (theme == 0);
	}
	else
		CfgData.circle = false;
	
    if (persist_exists(CONFIG_KEY_FSM))
		CfgData.fsm = persist_read_bool(CONFIG_KEY_FSM);
	else	
		CfgData.fsm = false;
	
    if (persist_exists(CONFIG_KEY_INV))
		CfgData.inv = !CfgData.circle && persist_read_bool(CONFIG_KEY_INV);
	else	
		CfgData.inv = true;
	
    if (persist_exists(CONFIG_KEY_ANIM))
		CfgData.anim = persist_read_bool(CONFIG_KEY_ANIM);
	else	
		CfgData.anim = true;
	
    if (persist_exists(CONFIG_KEY_SEP))
		CfgData.sep = persist_read_bool(CONFIG_KEY_SEP);
	else	
		CfgData.sep = true;
	
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
	
	app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "Curr Conf: circle:%d, fsm:%d, inv:%d, anim:%d, sep:%d, datefmt:%d, smart:%d, vibr:%d",
		CfgData.circle, CfgData.fsm, CfgData.inv, CfgData.anim, CfgData.sep, CfgData.datefmt, CfgData.smart, CfgData.vibr);
	
	gbitmap_destroy(batteryAll);
	batteryAll = gbitmap_create_with_resource(CfgData.inv ? RESOURCE_ID_IMAGE_BATTERY_INV : RESOURCE_ID_IMAGE_BATTERY);
	
	bitmap_layer_set_bitmap(radio_layer, NULL);
	gbitmap_destroy(bmp_radio);
	bmp_radio = gbitmap_create_as_sub_bitmap(batteryAll, GRect(110, 0, 10, 20));
	bitmap_layer_set_bitmap(radio_layer, bmp_radio);
	
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_get_root_layer(window));
	window_set_background_color(window, CfgData.inv ? GColorWhite : GColorBlack);
	
	//Bottom Layer first
	layer_remove_from_parent(text_layer_get_layer(date_layer));
	layer_remove_from_parent(bitmap_layer_get_layer(radio_layer));
	layer_remove_from_parent(bitmap_layer_get_layer(battery_layer));
	if (!CfgData.fsm)
	{
		layer_add_child(window_layer, text_layer_get_layer(date_layer));
		#ifdef PBL_COLOR
			text_layer_set_text_color(date_layer, CfgData.inv ? GColorDarkGray : GColorLightGray);
		#else
			text_layer_set_text_color(date_layer, CfgData.inv ? GColorBlack : GColorWhite);
		#endif
		text_layer_set_background_color(date_layer, CfgData.inv ? GColorWhite : GColorBlack);

		if (CfgData.smart)
		{
			layer_add_child(window_layer, bitmap_layer_get_layer(radio_layer));
			layer_add_child(window_layer, bitmap_layer_get_layer(battery_layer));
		}
	}	

	layer_remove_from_parent(face_layer);
	layer_destroy(face_layer);
	face_layer = layer_create(GRect(0, 0, bounds.size.w, CfgData.fsm ? bounds.size.h : bounds.size.w));
	layer_set_update_proc(face_layer, face_update_proc);
	layer_add_child(window_layer, face_layer);

	//Get a time structure so that it doesn't start blank
	time_t temp = time(NULL);
	struct tm *t = localtime(&temp);

	//Manually call the tick handler when the window is loading
	aktHH = t->tm_hour;
	aktMM = t->tm_min;
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
				strcmp(akt_tuple->value->cstring, "circle") == 0 ? 0 : 1);
	
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
				strcmp(akt_tuple->value->cstring, "usa") == 0 ? 3 : 
				strcmp(akt_tuple->value->cstring, "iso") == 0 ? 4 :  
				strcmp(akt_tuple->value->cstring, "gers") == 0 ? 5 : 
				strcmp(akt_tuple->value->cstring, "fras") == 0 ? 6 : 
				strcmp(akt_tuple->value->cstring, "engs") == 0 ? 7 : 
				strcmp(akt_tuple->value->cstring, "usas") == 0 ? 8 : 
				strcmp(akt_tuple->value->cstring, "isos") == 0 ? 9 : 0);
		
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
	
	digitS = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_DIGITAL_23));
	bmp_mask = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MASK);
	
	// Init layers
	face_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.w));
	layer_set_update_proc(face_layer, face_update_proc);

	date_layer = text_layer_create(GRect(-bounds.size.w, bounds.size.w-2, bounds.size.w, bounds.size.h-bounds.size.w));
	text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
	text_layer_set_font(date_layer, digitS);

	//Init bluetooth radio
	radio_layer = bitmap_layer_create(GRect(1, bounds.size.h, 10, 20));
	bitmap_layer_set_background_color(radio_layer, GColorClear);
		
	//Init battery
	battery_layer = bitmap_layer_create(GRect(bounds.size.w-11, bounds.size.h, 10, 20)); 
	bitmap_layer_set_background_color(battery_layer, GColorClear);

	//Update Configuration
	update_configuration();
	
	//Start|Skip Animation
	if (CfgData.anim)
	{
		aktHH = aktMM = 0;
		timer_face = app_timer_register(500, timerCallback, (void*)TIMER_ANIM_FACE);
		
		//Animate Date
		GRect rc_from = layer_get_frame(text_layer_get_layer(date_layer));
		GRect rc_to = rc_from;
		rc_to.origin.x = 0;

		s_prop_anim_date = property_animation_create_layer_frame(text_layer_get_layer(date_layer), &rc_from, &rc_to);
		animation_set_curve((Animation*)s_prop_anim_date, AnimationCurveEaseOut);
		animation_set_delay((Animation*)s_prop_anim_date, 500);
		animation_set_duration((Animation*)s_prop_anim_date, 1000);
		animation_schedule((Animation*)s_prop_anim_date);
		
		//Animate Bluetooth
		rc_from = layer_get_frame(bitmap_layer_get_layer(radio_layer));
		rc_to = rc_from;
		rc_to.origin.y -= 21;

		s_prop_anim_bt = property_animation_create_layer_frame(bitmap_layer_get_layer(radio_layer), &rc_from, &rc_to);
		animation_set_curve((Animation*)s_prop_anim_bt, AnimationCurveEaseOut);
		animation_set_delay((Animation*)s_prop_anim_bt, 1500);
		animation_set_duration((Animation*)s_prop_anim_bt, 1000);
		animation_schedule((Animation*)s_prop_anim_bt);
		
		//Animate Battery
		rc_from = layer_get_frame(bitmap_layer_get_layer(battery_layer));
		rc_to = rc_from;
		rc_to.origin.y -= 21;

		s_prop_anim_batt = property_animation_create_layer_frame(bitmap_layer_get_layer(battery_layer), &rc_from, &rc_to);
		animation_set_curve((Animation*)s_prop_anim_batt, AnimationCurveEaseOut);
		animation_set_delay((Animation*)s_prop_anim_batt, 2000);
		animation_set_duration((Animation*)s_prop_anim_batt, 1000);
		animation_schedule((Animation*)s_prop_anim_batt);
	}	
	else
	{	
		GRect rc = layer_get_frame(text_layer_get_layer(date_layer));
		rc.origin.x = 0;
		layer_set_frame(text_layer_get_layer(date_layer), rc);
		
		rc = layer_get_frame(bitmap_layer_get_layer(radio_layer));
		rc.origin.y -= 21;
		layer_set_frame(bitmap_layer_get_layer(radio_layer), rc);
		
		rc = layer_get_frame(bitmap_layer_get_layer(battery_layer));
		rc.origin.y -= 21;
		layer_set_frame(bitmap_layer_get_layer(battery_layer), rc);
		
		b_initialized = true;
	}
}
//-----------------------------------------------------------------------------------------------------------------------
static void window_unload(Window *window) 
{
	layer_destroy(face_layer);
	text_layer_destroy(date_layer);
	bitmap_layer_destroy(battery_layer);
	bitmap_layer_destroy(radio_layer);
	fonts_unload_custom_font(digitS);
	gbitmap_destroy(bmp_batt);
	gbitmap_destroy(bmp_radio);
	gbitmap_destroy(bmp_mask);
	gbitmap_destroy(batteryAll);
	
	if (!b_initialized)
		app_timer_cancel(timer_face);
	if (b_charging)
		app_timer_cancel(timer_batt);
}
//-----------------------------------------------------------------------------------------------------------------------
static void init(void) 
{
	b_initialized = false;
	b_charging = false;
	aktBT = -1;

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
	animation_unschedule_all();
	
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