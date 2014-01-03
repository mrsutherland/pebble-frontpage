#include <pebble.h>

// windows
static Window *menu_window;
static Window *post_window;
static ScrollLayer *post_scroll;
static TextLayer *post_text;
static Window *comment_window;
static MenuLayer *menu_layer;
// message IDs
enum {
  AKEY_REQ_POST = 0,
  AKEY_POST,
  AKEY_REQ_POST_DATA,
  AKEY_POST_DATA,
  AKEY_REQ_COMMENT,
  AKEY_COMMENT,
  AKEY_AUTHOR = 10,
  AKEY_SUBREDDIT,
  AKEY_TEXT,
  AKEY_INDEX,
  AKEY_PARENT,
  AKEY_TITLE,
  AKEY_DATA,
  AKEY_UPVOTE = 20,
  AKEY_DOWNVOTE
};
// Post data
typedef struct Post {
  int index;
  char* title;
  char* author;
  char* subreddit;
} Post;
#define POSTS_SIZE 10
static Post posts[POSTS_SIZE];
static int num_posts = 0;
static Post *selected_post = NULL; // currently selected post
static char *selected_data = NULL; // data for the currently selected post
// TODO: comment data

// constants
static const int vert_scroll_text_padding = 4;

/**********************
* Function Prototypes *
**********************/
void request_post_data(Post* post);

/******************
* Button Handlers *
******************/
/*
static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Select");
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Up");
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Down");
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}
*/
/*************
* Posts Menu *
*************/

// A callback is used to specify the amount of sections of menu items
// With this, you can dynamically add and remove sections
static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *data) {
  return 1;
}

// Each section has a number of items;  we use a callback to specify this
// You can also dynamically add and remove items using this
static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  // NOTE: only one section
  // TODO: figure out continuous scrolling (num_posts+1, then request more data on draw)
  return num_posts;
}

// A callback is used to specify the height of the cell
static int16_t menu_get_cell_height_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  // TODO: Calculate the height based on the text
  return 44;
}

// A callback is used to specify the height of the section header
static int16_t menu_get_header_height_callback(MenuLayer *menu_layer, uint16_t section_index, void *data) {
  // This is a define provided in pebble.h that you may use for the default height
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

// Here we draw what each header is
static void menu_draw_header_callback(GContext* ctx, const Layer *cell_layer, uint16_t section_index, void *data) {
  // NOTE: only one section
  menu_cell_basic_header_draw(ctx, cell_layer, "/r/all");
}

// This is the menu item draw callback where you specify what each item should look like
static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
  // NOTE: only one section
  // draw the posts based on the index (row)
  // TODO: error checking and stuff...
  Post *post = &(posts[cell_index->row]);

  // This is a basic menu item with a title and subtitle
  menu_cell_basic_draw(ctx, cell_layer, post->title, post->author, NULL);
}

// Here we capture when a user selects a menu item
void menu_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
  // Use the row to specify which item will receive the select action
  selected_post = &(posts[cell_index->row]);
  window_stack_push(post_window, true);
  request_post_data(selected_post);
}

/**********
* Windows *
**********/
static void menu_window_load(Window *window) {
  // Now we prepare to initialize the menu layer
  // We need the bounds to specify the menu layer's viewport size
  // In this case, it'll be the same as the window's
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);

  // Create the menu layer
  menu_layer = menu_layer_create(bounds);

  // Set all the callbacks for the menu layer
  menu_layer_set_callbacks(menu_layer, NULL, (MenuLayerCallbacks){
    .get_num_sections = menu_get_num_sections_callback,
    .get_num_rows = menu_get_num_rows_callback,
    .get_cell_height = menu_get_cell_height_callback,
    .get_header_height = menu_get_header_height_callback,
    .draw_header = menu_draw_header_callback,
    .draw_row = menu_draw_row_callback,
    .select_click = menu_select_callback,
  });

  // Bind the menu layer's click config provider to the window for interactivity
  menu_layer_set_click_config_onto_window(menu_layer, window);

  // Add it to the window for display
  layer_add_child(window_layer, menu_layer_get_layer(menu_layer));
}

static void menu_window_unload(Window *window) {
  // Destroy the menu layer
  menu_layer_destroy(menu_layer);
}

static void post_window_load(Window *window) {
  // window that stores the post data
  // We need the bounds to specify the menu layer's viewport size
  // In this case, it'll be the same as the window's
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  GRect max_text_bounds = GRect(0, 0, bounds.size.w, 2000);

  // create scroll with text inside
  post_scroll = scroll_layer_create(bounds);

  // configure button presses
  scroll_layer_set_click_config_onto_window(post_scroll, window);

  // create text object to be scrolled
  post_text = text_layer_create(max_text_bounds);
  text_layer_set_text(post_text, "Loading...");
  text_layer_set_font(post_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));

  // resize text object for scrolling
  GSize max_size = text_layer_get_content_size(post_text);
  text_layer_set_size(post_text, max_size);
  scroll_layer_set_content_size(post_scroll, GSize(bounds.size.w, max_size.h + vert_scroll_text_padding));

  // add text object to scroll
  scroll_layer_add_child(post_scroll, text_layer_get_layer(post_text));

  // Add it to the window for display
  layer_add_child(window_layer, scroll_layer_get_layer(post_scroll));

  // TODO: set up callback for select and long select
}

static void post_window_unload(Window *window) {
  text_layer_destroy(post_text);
  scroll_layer_destroy(post_scroll);
}


/****************
* Data transfer *
****************/
void request_post_data(Post* post) {
  // request data for post
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to create dictionary");
    return;
  }
  // set message to REQ_POST_DATA
  dict_write_int8(iter, AKEY_REQ_POST_DATA, 1);
  // set post index
  dict_write_uint32(iter, AKEY_INDEX, selected_post->index);
  dict_write_end(iter);
  // send the message
  app_message_outbox_send();
}

void out_sent_handler(DictionaryIterator *sent, void *context) {
  // outgoing message was delivered
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
  // outgoing message failed
}

void in_received_handler(DictionaryIterator *received, void *context) {
  // incoming message received
  // Check for fields you expect to receive
  Tuple *tuple;

  // Act on the found fields received
  if (dict_find(received, AKEY_POST)) {
    // this is a post, find the author, subreddit, index, and title.
    Post *post = &(posts[num_posts++]);
    post->index = dict_find(received, AKEY_INDEX)->value->uint32;
    // title
    tuple = dict_find(received, AKEY_TITLE);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Title: %s", tuple->value->cstring);
    post->title = malloc(strlen(tuple->value->cstring)+1);
    strcpy(post->title, tuple->value->cstring);
    // author
    tuple = dict_find(received, AKEY_AUTHOR);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Author: %s", tuple->value->cstring);
    post->author = malloc(strlen(tuple->value->cstring)+1);
    strcpy(post->author, tuple->value->cstring);
    // subreddit
    tuple = dict_find(received, AKEY_SUBREDDIT);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Subreddit: %s", tuple->value->cstring);
    post->subreddit = malloc(strlen(tuple->value->cstring)+1);
    strcpy(post->subreddit, tuple->value->cstring);
    // reload menu
    menu_layer_reload_data(menu_layer);
  } else if (dict_find(received, AKEY_POST_DATA)) {
    // handle POST_DATA message
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Received post data");
    int index = dict_find(received, AKEY_INDEX)->value->uint32;
    if (!selected_post || index != selected_post->index) {
      // didn't match selected post
      APP_LOG(APP_LOG_LEVEL_WARNING, "Index didn't match: %d", index);
    }
    // free existing data
    if (selected_data != NULL) {
      free(selected_data);
    }
    // copy data
    tuple = dict_find(received, AKEY_DATA);
    selected_data = malloc(strlen(tuple->value->cstring)+1);
    strcpy(selected_data, tuple->value->cstring);
    // display the data
    text_layer_set_text(post_text, selected_data);
    // resize the text layer
    GSize max_size = text_layer_get_content_size(post_text);
    text_layer_set_size(post_text, max_size);
    scroll_layer_set_content_size(post_scroll, GSize(max_size.w, max_size.h + vert_scroll_text_padding));
  }
}

void in_dropped_handler(AppMessageResult reason, void *context) {
  // incoming message dropped
}

/*******
* Init *
*******/

static void init(void) {
  // register data handlers
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_register_outbox_sent(out_sent_handler);
  app_message_register_outbox_failed(out_failed_handler);

  app_message_open(app_message_inbox_size_maximum(), 128);

  // draw menu window
  menu_window = window_create();
  window_set_window_handlers(menu_window, (WindowHandlers) {
    .load = menu_window_load,
    .unload = menu_window_unload,
  });
  window_stack_push(menu_window, true);

  // create post window
  post_window = window_create();

  // create comment window
  window_set_window_handlers(post_window, (WindowHandlers) {
    .load = post_window_load,
    .unload = post_window_unload,
  });

}

static void deinit(void) {
  window_destroy(menu_window);
  window_destroy(post_window);
  //window_destroy(comment_window);
  for (int i=0; i<num_posts; ++i) {
    if (posts[i].author) {
      free(posts[i].author);
    }
    if (posts[i].subreddit) {
      free(posts[i].subreddit);
    }
    if (posts[i].title) {
      free(posts[i].title);
    }
  }
  if (selected_data) {
    free(selected_data);
  }
}


int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", menu_window);

  app_event_loop();
  deinit();
}
