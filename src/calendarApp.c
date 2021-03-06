#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include <math.h>
#include "resource_ids.auto.h"


#define WATCHMODE false

#define BLACK true
#define GRID true
#define INVERT true
#define SHOWTIME false

// First day of the week. Values can be between -6 and 6 
// 0 = weeks start on Sunday
// 1 =  weeks start on Monday
#define START_OF_WEEK 0

const char daysOfWeek[7][3] = {"S","M","T","W","Th","F","S"};
const char months[12][12] = {"January","Feburary","March","April","May","June","July","August","September","October", "November", "December"};




#define APP_UUID { 0xB4, 0x1E, 0x3D, 0xCF, 0x61, 0x62, 0x41, 0x47, 0x9C, 0x58, 0x64, 0x3E, 0x10, 0x91, 0xFB, 0x93 }
#define WATCH_UUID { 0x8C, 0x77, 0x18, 0xB5, 0x81, 0x58, 0x48, 0xD9, 0x9D, 0x81, 0x1E, 0x3A, 0xB2, 0x32, 0xC9, 0x5C }

#if WATCHMODE
PBL_APP_INFO(WATCH_UUID,
             "Calendar", "William Heaton",
             1, 0, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_WATCH_FACE);
#else
PBL_APP_INFO(APP_UUID,
             "Calendar", "William Heaton",
             1, 0, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_STANDARD_APP);
#endif

static int offset = 0;
Window window;
Layer days_layer;
#if SHOWTIME
TextLayer timeLayer;
int curHour;
int curMin;
int curSec;

#endif
bool calEvents[32] = {  false,false,false,false,false,
                        false,false,false,false,false,
                        false,false,false,false,false,
                        false,false,false,false,false,
                        false,false,false,false,false,
                        false,false,false,false,false,false,false};

char* intToStr(int val){
 	static char buf[32] = {0};
	int i = 30;	
	for(; val && i ; --i, val /= 10)
		buf[i] = "0123456789"[val % 10];
	
	return &buf[i+1];
}
// Calculate what day of the week it was/will be X days from the first day of the month, if mday was a wday
int wdayOfFirstOffset(int wday,int mday,int ofs){
    int a = wday - ((mday-1)%7);
    if(a<0) a += 7;

    int b;
    if(ofs>0)
        b = a + (abs(ofs)%7); 
    else
        b = a - (abs(ofs)%7); 
    
    if(b<0) b += 7;
    if(b>6) b -= 7;
    
    return b;
}

// How many days are/were in the month
int daysInMonth(int mon, int year){
    mon++;
    
    // April, June, September and November have 30 Days
    if(mon == 4 || mon == 6 || mon == 9 || mon == 11)
        return 30;
        
    // Deal with Feburary & Leap years
    else if( mon == 2 ){
        if(year%400==0)
            return 29;
        else if(year%100==0)
            return 28;
        else if(year%4==0)
            return 29;
        else 
            return 28;
    }
    // Most months have 31 days
    else
        return 31;
}
void setColors(GContext* ctx){
#if BLACK
        window_set_background_color(&window, GColorBlack);
        graphics_context_set_stroke_color(ctx, GColorWhite);
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_context_set_text_color(ctx, GColorWhite);
#else
        window_set_background_color(&window, GColorWhite);
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_fill_color(ctx, GColorWhite);
        graphics_context_set_text_color(ctx, GColorBlack);
#endif
}
void setInvColors(GContext* ctx){
#if BLACK
        window_set_background_color(&window, GColorWhite);
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_fill_color(ctx, GColorWhite);
        graphics_context_set_text_color(ctx, GColorBlack);
#else
        graphics_context_set_stroke_color(ctx, GColorWhite);
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_context_set_text_color(ctx, GColorWhite);
#endif
}


void days_layer_update_callback(Layer *me, GContext* ctx) {
    (void)me;
    
    int j;
    int i;
    
    PblTm currentTime;
    get_time(&currentTime);
    int mon = currentTime.tm_mon;
    int year = currentTime.tm_year+1900;
    
    // Figure out which month & year we are going to be looking at based on the selected offset
    // Calculate how many days are between the first of this month and the first of the month we are interested in
    int od = 0;
    j = 0;
    while(j < abs(offset)){
        j++;
        if(offset > 0){
            od = od + daysInMonth(mon,year);
            mon++;
            if(mon>11){
                mon -= 12;
                year++;
            }
        }else{
            mon--;
            if(mon < 0){
                mon += 12;
                year--;
            }
            od -= daysInMonth(mon,year);
        }
    }
    
    // Days in the target month
    int dom = daysInMonth(mon,year);
    
    // Day of the week for the first day in the target month 
    int dow = wdayOfFirstOffset(currentTime.tm_wday,currentTime.tm_mday,od);
    
    // Adjust day of week by specified offset
    dow -= START_OF_WEEK;
    if(dow>6) dow-=7;
    if(dow<0) dow+=7;
    
    // Cell geometry
    
    int l = 2;      // position of left side of left column
    int b = 168;    // position of bottom of bottom row
    int d = 7;      // number of columns (days of the week)
    int lw = 20;    // width of columns 
    int w = ceil(((float) dow + (float) dom)/7); // number of weeks this month
    
#if SHOWTIME
    int bh;
    if(w == 4)      bh = 21;
    else if(w == 5) bh = 17;
    else            bh = 14;
#else    
    // How tall rows should be depends on how many weeks there are
    int bh;
    if(w == 4)      bh = 30;
    else if(w == 5) bh = 24;
    else            bh = 20;
#endif

    int r = l+d*lw; // position of right side of right column
    int t = b-w*bh; // position of top of top row
    int cw = lw-1;  // width of textarea
    int cl = l+1;
    int ch = bh-1;
        
    setColors(ctx);

#if GRID
    // Draw the Gridlines
    // horizontal lines
    for(i=1;i<=w;i++){
        graphics_draw_line(ctx, GPoint(l, b-i*bh), GPoint(r, b-i*bh));
    }
    // vertical lines
    for(i=1;i<d;i++){
        graphics_draw_line(ctx, GPoint(l+i*lw, t), GPoint(l+i*lw, b));
    }
#endif

    // Draw days of week
    for(i=0;i<7;i++){
    
        // Adjust labels by specified offset
        j = i+START_OF_WEEK;
        if(j>6) j-=7;
        if(j<0) j+=7;
        graphics_text_draw(
            ctx, 
            daysOfWeek[j], 
            fonts_get_system_font(FONT_KEY_GOTHIC_14), 
            GRect(cl+i*lw, b-w*bh-16, cw, 15), 
            GTextOverflowModeWordWrap, 
            GTextAlignmentCenter, 
            NULL); 
    }
    
    
    
    // Fill in the cells with the month days
    int fh;
    int fo;
    GFont font;
    int wknum = 0;
    
    for(i=1;i<=dom;i++){
    
        // New Weeks begin on Sunday
        if(dow > 6){
            dow = 0;
            wknum ++;
        }

#if INVERT
        // Is this today?  If so prep special today style
        if(i==currentTime.tm_mday && offset == 0){
            setInvColors(ctx);
            graphics_fill_rect(
                ctx,
                GRect(
                    l+dow*lw+1, 
                    b-(w-wknum)*bh+1, 
                    cw, 
                    ch)
                ,0
                ,GCornerNone);
        }
#endif

        if(calEvents[i-1]){
        
            font = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
            fh = 19;
            fo = 12;
        
        // Normal (non-today) style
        }else{
            font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
            fh = 15;
            fo = 9;
        }
        
        // Draw the day
        graphics_text_draw(
            ctx, 
            intToStr(i),  
            font, 
            GRect(
                cl+dow*lw, 
                b-(-0.5+w-wknum)*bh-fo, 
                cw, 
                fh), 
            GTextOverflowModeWordWrap, 
            GTextAlignmentCenter, 
            NULL); 
        
#if INVERT
        // Fix colors if inverted
        if(offset == 0 && i==currentTime.tm_mday ) setColors(ctx);
#endif

        // and on to the next day
        dow++;   
    }
    
    
#if WATCHMODE
    char str[20] = ""; 
    string_format_time(str, sizeof(str), "%B %d, %Y", &currentTime);
#else
    // Build the MONTH YEAR string
    char str[20];
    strcpy (str,months[mon]);
    strcat (str," ");
    strcat (str,intToStr(year));
#endif

    // Draw the MONTH/YEAR String
    graphics_text_draw(
        ctx, 
        str,  
        fonts_get_system_font(FONT_KEY_GOTHIC_24), 
#if SHOWTIME
        GRect(0, 40, 144, 25), 
#else
        GRect(0, 0, 144, 25), 
#endif
        GTextOverflowModeWordWrap, 
        GTextAlignmentCenter, 
        NULL);
}

#if SHOWTIME
void updateTime(PblTm * t){

    curHour=t->tm_hour;
    curMin=t->tm_min;
    curSec=t->tm_sec;
    
    static char timeText[] = "00:00";
    if(clock_is_24h_style()){
        string_format_time(timeText, sizeof(timeText), "%H:%M", t);
        if(curHour<10)memmove(timeText, timeText+1, strlen(timeText)); 
    }else{
        string_format_time(timeText, sizeof(timeText), "%I:%M", t);
        if( (curHour > 0 && curHour<10) || (curHour>12 && curHour<22))memmove(timeText, timeText+1, strlen(timeText)); 
    }text_layer_set_text(&timeLayer, timeText);
    
    
//    static char dateText[30];
//    string_format_time(dateText, sizeof(dateText), "%B %d", t);//"%A\n%B %d", t);
//    text_layer_set_text(&dateLayer, dateText);
}
#endif

static void send_cmd(){
        
    PblTm currentTime;
    get_time(&currentTime);
        
    int year = currentTime.tm_year;
    int month = currentTime.tm_mon+offset ;
    
    while(month>11){
        month -= 12;
        year++;
    }
    while(month<0){
        month += 12;
        year--;
    }
    Tuplet tup = TupletInteger(1, year*100+month);

    DictionaryIterator *iter;
    app_message_out_get(&iter);

    if (iter == NULL)
        return;
    
    dict_write_tuplet(iter, &tup);
    dict_write_end(iter);

    app_message_out_send();
    app_message_out_release();
}
static void monthChanged(){

    for (int i = 0; i < 31; ++i){
        calEvents[i] = false;
    }
    send_cmd();
    
    
    layer_mark_dirty(&days_layer);
}
void my_out_sent_handler(DictionaryIterator *sent, void *context) {
  // outgoing message was delivered
}
void my_out_fail_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
  // outgoing message failed
}
void my_in_rcv_handler(DictionaryIterator *received, void *context) {
    // incoming message received
    
    PblTm currentTime;
    get_time(&currentTime);
    int year = currentTime.tm_year;
    int month = currentTime.tm_mon+offset ;
    
    while(month>11){
        month -= 12;
        year++;
    }
    while(month<0){
        month += 12;
        year--;
    }
    
    uint16_t dta;
    int y = 0;
    int m = 0;
    uint8_t *encoded = 0;
    
    Tuple *tuple = dict_read_first(received);
    while (tuple) {
        switch (tuple->key) {
            case 1:
                dta = tuple->value->uint16;
                m = dta%100;
                y = (dta-m)/100;
                break;
            case 3:
                encoded = tuple->value->data;
                break;
        }
        tuple = dict_read_next(received);
    }
    
    if((m==month && y == year) ){
        int index;
        for (int byteIndex = 0;  byteIndex < 4; byteIndex++){
            for (int bitIndex = 0;  bitIndex < 8; bitIndex++){
                index = byteIndex*8+bitIndex;
                calEvents[index] = (encoded[byteIndex] & (1 << bitIndex)) != 0;
            }
        }
        layer_mark_dirty(&days_layer);
    }else{
        send_cmd();
    }
}
void my_in_drp_handler(void *context, AppMessageResult reason) {
  // incoming message dropped
}


#if !WATCHMODE
void up_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
    (void)recognizer;
    (void)window;
    offset--;
    monthChanged();
}
void down_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
    (void)recognizer;
    (void)window;
    offset++;
    monthChanged();
}
void select_single_click_handler(ClickRecognizerRef recognizer, Window *window) {
    (void)recognizer;
    (void)window;
    if(offset != 0){
        offset = 0;
        monthChanged();
    }
}

void config_provider(ClickConfig **config, Window *window) {
    (void)window;
    config[BUTTON_ID_SELECT]->click.handler = (ClickHandler) select_single_click_handler;
    config[BUTTON_ID_UP]->click.handler = (ClickHandler) up_single_click_handler;
    config[BUTTON_ID_UP]->click.repeat_interval_ms = 100;
    config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) down_single_click_handler;
    config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 100;
}
#endif


void handle_init(AppContextRef ctx) {
  (void)ctx;
    window_init(&window, "Calendar");
    window_stack_push(&window, false /* Animated */);
    window_set_fullscreen(&window, true);
    
    setColors(ctx);
    
    layer_init(&days_layer, window.layer.frame);
    days_layer.update_proc = &days_layer_update_callback;
    layer_add_child(&window.layer, &days_layer);

#if SHOWTIME
    text_layer_init(&timeLayer, GRect(0, -6, 144, 43));
#if BLACK
    text_layer_set_text_color(&timeLayer, GColorWhite);
#else
    text_layer_set_text_color(&timeLayer, GColorBlack);
#endif
    text_layer_set_background_color(&timeLayer, GColorClear);
    text_layer_set_font(&timeLayer, fonts_get_system_font(FONT_KEY_GOTHAM_42_MEDIUM_NUMBERS));
    text_layer_set_text_alignment(&timeLayer, GTextAlignmentCenter);
    layer_add_child(&window.layer, &timeLayer.layer);
    
    PblTm t;
    get_time(&t);
    updateTime(&t);
#endif
#if !WATCHMODE
    window_set_click_config_provider(&window, (ClickConfigProvider) config_provider);
#endif
    send_cmd();
}

void handle_tick(AppContextRef ctx, PebbleTickEvent *t) {
    (void)ctx;
#if SHOWTIME
    if (t->units_changed & MINUTE_UNIT) {
        updateTime(t->tick_time);  
    }
#endif
    if (t->units_changed & HOUR_UNIT) {
        send_cmd();
    }
    if (t->units_changed & DAY_UNIT) {
        layer_mark_dirty(&days_layer);
    }
}
void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,

    .tick_info = {
        .tick_handler = &handle_tick,
#if SHOWTIME
        .tick_units = MINUTE_UNIT
#else
        .tick_units = HOUR_UNIT
#endif
    },
	.messaging_info = {
		.buffer_sizes = {
			.inbound = 22,
			.outbound = 16,
		},
        .default_callbacks.callbacks = {
            .out_sent = my_out_sent_handler,
            .out_failed = my_out_fail_handler,
            .in_received = my_in_rcv_handler,
            .in_dropped = my_in_drp_handler,
        }
	}
  };
  app_event_loop(params, &handlers);
}


