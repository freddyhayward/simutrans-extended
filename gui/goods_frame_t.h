/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef GUI_GOODS_FRAME_T_H
#define GUI_GOODS_FRAME_T_H


#include "gui_frame.h"
#include "components/gui_button.h"
#include "components/gui_numberinput.h"
#include "components/gui_combobox.h"
#include "components/gui_scrollpane.h"
#include "components/gui_label.h"
#include "components/action_listener.h"
#include "goods_stats_t.h"
#include "../utils/cbuffer_t.h"

// for waytype_t
#include "../simtypes.h"

class goods_desc_t;

/**
 * Shows statistics. Only goods so far.
 */
class goods_frame_t : public gui_frame_t, private action_listener_t
{
private:
	enum sort_mode_t {
		by_number   = 0,
		by_name     = 1,
		by_revenue  = 2,
		by_category = 3,
		by_weight   = 4,
		SORT_MODES  = 5
	};
	static const char *sort_text[SORT_MODES];

	// static, so we remember the last settings
	// Distance in meters
	static uint32 distance_meters;
	// Distance in km
	static uint16 distance;

	static uint32 vehicle_speed;
	static uint8 comfort;
	static uint8 catering_level;
	static uint8 g_class;
	static bool sortreverse;
	static sort_mode_t sortby;
	static bool filter_goods;

	char speed[6];
	char distance_txt[6];
	char comfort_txt[6];
	char catering_txt[6];
	char class_txt[6];
	cbuffer_t descriptive_text;
	vector_tpl<const goods_desc_t*> good_list;

	gui_combobox_t sortedby;
	button_t sort_asc, sort_desc;

	// replace button list with numberinput components for faster navigation
	// @author: HeinBloed, April 2012
	gui_numberinput_t distance_input, comfort_input, catering_input, speed_input, class_input;

	gui_aligned_container_t *sort_row;

	button_t filter_goods_toggle;

	goods_stats_t goods_stats;
	gui_scrollpane_t scrolly;

	// creates the list and pass it to the child function good_stats, which does the display stuff ...
	static bool compare_goods(goods_desc_t const* const w1, goods_desc_t const* const w2);
	void sort_list();

public:
	goods_frame_t();

	// yes we can reload
	uint32 get_rdwr_id() OVERRIDE;
	void rdwr( loadsave_t *file ) OVERRIDE;

	bool has_min_sizer() const OVERRIDE {return true;}

	/**
	 * Set the window associated helptext
	 * @return the filename for the helptext, or NULL
	 */
	const char * get_help_filename() const OVERRIDE {return "goods_filter.txt"; }

	/**
	 * Draw the component
	 */
	void draw(scr_coord pos, scr_size size) OVERRIDE;

	bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;
};

#endif
