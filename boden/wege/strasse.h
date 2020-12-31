/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef BODEN_WEGE_STRASSE_H
#define BODEN_WEGE_STRASSE_H


#include <unordered_map>
#include "weg.h"
//#include "../../tpl/minivec_tpl.h"

class fabrik_t;
//class gebaeude_t;

namespace std
{
	template <>
	struct hash<koord>
	{
		std::size_t operator()(const koord& key) const
		{
			using std::hash;
			return (uint32)key.y << 16 | key.x;
		}
	};
}
/**
 * Cars are able to drive on roads.
 */
class strasse_t : public weg_t
{
public:
	static bool show_masked_ribi;

	enum travel_times {
		WAY_TRAVEL_TIME_IDEAL,	///< number of ticks vehicles would spend traversing this way without traffic
		WAY_TRAVEL_TIME_ACTUAL,///< number of ticks vehicles actually spent passing over this way
		MAX_WAY_TRAVEL_TIMES
	};
	uint32 travel_times[MAX_WAY_STAT_MONTHS][MAX_WAY_TRAVEL_TIMES];



private:
	/**
	* @author THLeaderH
	*/
	overtaking_mode_t overtaking_mode = invalid_mode;

	/**
	* Mask used by oneway_mode road
	* @author THLeaderH
	*/
	uint8 ribi_mask_oneway:4;


public:
	static const way_desc_t *default_strasse;

	void add_travel_time_update(uint32 actual, uint32 ideal);
	// Being here rather than in weg_t might have caused heap corruption.
	//minivec_tpl<gebaeude_t*> connected_buildings;

	strasse_t(loadsave_t *file);
	strasse_t();
	virtual ~strasse_t();
	void init() OVERRIDE;

	//inline waytype_t get_waytype() const {return road_wt;}

	void set_gehweg(bool janein);

	void rdwr(loadsave_t *file) OVERRIDE;
	void new_month() OVERRIDE;

	void info(cbuffer_t & buf) const OVERRIDE;
	/**
	* Overtaking mode (declared in simtypes.h)
	* halt_mode = vehicles can stop on passing lane
	* oneway_mode = condition for one-way road
	* twoway_mode = condition for two-way road
	* prohibited_mode = overtaking is completely forbidden
	* @author teamhimeH
	*/
	overtaking_mode_t get_overtaking_mode() const { return overtaking_mode; };
	void set_overtaking_mode(overtaking_mode_t o, player_t* calling_player);

	void set_ribi_mask_oneway(ribi_t::ribi ribi) { ribi_mask_oneway = (uint8)ribi; }
	// used in wegbauer. param @allow is ribi in which vehicles can go. without this, ribi cannot be updated correctly at intersections.
	void update_ribi_mask_oneway(ribi_t::ribi mask, ribi_t::ribi allow, player_t* calling_player);
	ribi_t::ribi get_ribi_mask_oneway() const { return (ribi_t::ribi)ribi_mask_oneway; }
	ribi_t::ribi get_ribi() const OVERRIDE;

	void rotate90() OVERRIDE;

	image_id get_front_image() const OVERRIDE
	{
		if (show_masked_ribi && overtaking_mode <= oneway_mode) {
			return skinverwaltung_t::ribi_arrow->get_image_id(get_ribi());
		}
		else {
			return weg_t::get_front_image();
		}
	}

	FLAGGED_PIXVAL get_outline_colour() const OVERRIDE
	{
		uint8 restriction_colour;
		switch (overtaking_mode)
		{
			case halt_mode:
			case prohibited_mode:
			case oneway_mode:
				restriction_colour = overtaking_mode_to_color(overtaking_mode);
				break;
			case invalid_mode:
			case twoway_mode:
			default:
				return 0;
		}
		return (show_masked_ribi && restriction_colour) ? TRANSPARENT75_FLAG | OUTLINE_FLAG | color_idx_to_rgb(restriction_colour) : 0;
	}

	static uint8 overtaking_mode_to_color(overtaking_mode_t o) {
		switch (o)
		{
			// Do not set the lightest color to make a difference between the tile color and the text color
			case halt_mode:
				return COL_LIGHT_PURPLE - 1;
			case prohibited_mode:
				return COL_ORANGE + 1;
			case oneway_mode:
				return 22;
			case invalid_mode:
			case twoway_mode:
			default:
				return COL_WHITE-1;
		}
		return 0;
	}

	// This was in strasse_t, but being there possibly caused heap corruption.
	minivec_tpl<gebaeude_t*> connected_buildings;

	// Likewise, out of caution, put this here for the same reason.
	//typedef koordhashtable_tpl<koord, koord3d> old_private_car_route_map;
	//typedef std::unordered_map<koord, koord3d> private_car_route_map_2;
	//old_private_car_route_map old_private_car_routes[2];

	typedef std::unordered_map<koord, koord3d> private_car_route_map;
	private_car_route_map private_car_routes;

	//private_car_route_map_2 private_car_routes_std[2];

	void add_private_car_route(koord dest, koord3d next_tile);
	void remove_private_car_route(koord dest);

	static void clear_updates();
	static void apply_updates();

private:
	/// Set the boolean value to true to modify the set currently used for reading (this must ONLY be done when this is called from a single threaded part of the code).
	static void apply_travel_time_updates();
	static void apply_private_car_route_updates();
	static void clear_travel_time_updates();
	static void clear_private_car_route_updates();
	static slist_tpl<std::tuple<strasse_t *, uint32, uint32>> pending_travel_time_updates;
	static slist_tpl<std::tuple<strasse_t *, koord, koord3d>> pending_private_car_route_updates;
public:

	/// Delete all private car routes originating from or passing through this tile.
	/// Set the boolean value to true to modify the set currently used for reading (this must ONLY be done when this is called from a single threaded part of the code).
	void delete_all_routes_from_here();
	void delete_route_to(koord destination);

	koord3d get_private_car_route_tile_to(koord dest) const;

	void init_travel_times();
	//void increment_traffic_stopped_counter() { statistics[0][WAY_STAT_WAITING] ++; }
	inline void update_travel_times(uint32 actual, uint32 ideal)
	{
		travel_times[WAY_STAT_THIS_MONTH][WAY_TRAVEL_TIME_ACTUAL] += actual;
		travel_times[WAY_STAT_THIS_MONTH][WAY_TRAVEL_TIME_IDEAL] += ideal;
	}

	//will return the % ratio of actual to ideal traversal times
	inline uint32 get_congestion_percentage() const {
		uint32 combined_ideal = travel_times[WAY_STAT_THIS_MONTH][WAY_TRAVEL_TIME_IDEAL] + travel_times[WAY_STAT_LAST_MONTH][WAY_TRAVEL_TIME_IDEAL];
		if(combined_ideal == 0u) {
			return 0u;
		}
		uint32 combined_actual = travel_times[WAY_STAT_THIS_MONTH][WAY_TRAVEL_TIME_ACTUAL] + travel_times[WAY_STAT_LAST_MONTH][WAY_TRAVEL_TIME_ACTUAL];
		if(combined_actual <= combined_ideal) {
			return 0u;
		}
		return (combined_actual * 100u / combined_ideal) - 100u;
	}
};

#endif
