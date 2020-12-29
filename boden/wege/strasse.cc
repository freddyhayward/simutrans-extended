/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#include <stdio.h>
#include <tuple>

#include "strasse.h"
#include "../../simworld.h"
#include "../../obj/bruecke.h"
#include "../../obj/tunnel.h"
#include "../../dataobj/loadsave.h"
#include "../../descriptor/way_desc.h"
#include "../../descriptor/tunnel_desc.h"
#include "../../bauer/wegbauer.h"
#include "../../dataobj/translator.h"
#include "../../dataobj/ribi.h"
#include "../../utils/cbuffer_t.h"
#include "../../vehicle/simvehicle.h" /* for calc_direction */
#include "../../obj/wayobj.h"
#include "../../player/simplay.h"
#include "../../simcity.h"

const way_desc_t *strasse_t::default_strasse=NULL;

bool strasse_t::show_masked_ribi = false;

static slist_tpl<std::tuple<strasse_t*, uint32, uint32>> pending_travel_time_updates;

#ifdef MULTI_THREAD
#include "../../utils/simthread.h"
static pthread_mutexattr_t mutex_attributes;
#endif

void strasse_t::set_gehweg(bool janein)
{
	grund_t *gr = welt->lookup(get_pos());
	wayobj_t *wo = gr ? gr->get_wayobj(road_wt) : NULL;

	if (wo && wo->get_desc()->is_noise_barrier()) {
		janein = false;
	}

	weg_t::set_gehweg(janein);
	if(janein && get_desc())
	{
		if(welt->get_settings().get_town_road_speed_limit())
		{
			set_max_speed(min(welt->get_settings().get_town_road_speed_limit(), get_desc()->get_topspeed()));
		}
		else
		{
			set_max_speed(get_desc()->get_topspeed());
		}
	}
	if(!janein && get_desc()) {
		set_max_speed(get_desc()->get_topspeed());
	}
	if(gr) {
		gr->calc_image();
	}
}



strasse_t::strasse_t(loadsave_t *file) : weg_t(road_wt)
{
	rdwr(file);
}


strasse_t::strasse_t() : weg_t(road_wt)
{
	set_gehweg(false);
	set_desc(default_strasse);
	ribi_mask_oneway =ribi_t::none;
	overtaking_mode = twoway_mode;
}


void strasse_t::rdwr(loadsave_t *file)
{
	xml_tag_t s( file, "strasse_t" );

	weg_t::rdwr(file);

	if(  file->get_extended_version() >= 14  ) {
		uint8 mask_oneway = get_ribi_mask_oneway();
		file->rdwr_byte(mask_oneway);
		set_ribi_mask_oneway(mask_oneway);
		sint8 ov = get_overtaking_mode();
		if (file->is_loading() && file->get_extended_version() == 14 && (ov == 2 || ov == 4)) {
			ov = twoway_mode; // loading_only_mode and inverted_mode has been removed
		}
		file->rdwr_byte(ov);
		switch ((sint32)ov) {
		case halt_mode:
		case oneway_mode:
		case twoway_mode:
		case prohibited_mode:
		case invalid_mode:
			overtaking_mode = (overtaking_mode_t)ov;
			break;
		default:
			dbg->warning("strasse_t::rdwr", "Unrecognized overtaking mode %d, changed to invalid mode", (sint32)ov);
			assert(file->is_loading());
			overtaking_mode = invalid_mode;
			break;
		}
	} else {
		set_ribi_mask_oneway(ribi_t::none);
		overtaking_mode = twoway_mode;
	}

	if(file->get_version_int()<89000) {
		bool gehweg;
		file->rdwr_bool(gehweg);
		set_gehweg(gehweg);
	}

	if(file->is_saving())
	{
		const char *s = get_desc()->get_name();
		file->rdwr_str(s);
		if(file->get_extended_version() >= 12)
		{
			s = replacement_way ? replacement_way->get_name() : "";
			file->rdwr_str(s);
		}
	}
	else
	{
		char bname[128];
		file->rdwr_str(bname, lengthof(bname));
		const way_desc_t *desc = way_builder_t::get_desc(bname);

#ifndef SPECIAL_RESCUE_12_3
		const way_desc_t* loaded_replacement_way = NULL;
		if(file->get_extended_version() >= 12)
		{
			char rbname[128];
			file->rdwr_str(rbname, lengthof(rbname));
			loaded_replacement_way = way_builder_t::get_desc(rbname);
		}
#endif

		const sint32 old_max_speed = get_max_speed();
		const uint32 old_max_axle_load = get_max_axle_load();
		const uint32 old_bridge_weight_limit = get_bridge_weight_limit();
		if(desc==NULL) {
			desc = way_builder_t::get_desc(translator::compatibility_name(bname));
			if(desc==NULL) {
				desc = default_strasse;
				welt->add_missing_paks( bname, karte_t::MISSING_WAY );
			}
			dbg->warning("strasse_t::rdwr()", "Unknown street %s replaced by %s (old_max_speed %i)", bname, desc->get_name(), old_max_speed );
		}

		set_desc(desc, file->get_extended_version() >= 12);
#ifndef SPECIAL_RESCUE_12_3
		if(file->get_extended_version() >= 12)
		{
			replacement_way = loaded_replacement_way;
		}
#endif

		if(old_max_speed > 0)
		{
			if (is_degraded() && old_max_speed == desc->get_topspeed())
			{
				// The maximum speed has to be reduced on account of the degridation.
				if (get_remaining_wear_capacity() > 0)
				{
					set_max_speed(old_max_speed / 2);
				}
				else
				{
					set_max_speed(0);
				}
			}
			else
			{
				set_max_speed(old_max_speed);
			}
		}
		if(old_max_axle_load > 0)
		{
			set_max_axle_load(old_max_axle_load);
		}
		if(old_bridge_weight_limit > 0)
		{
			set_bridge_weight_limit(old_bridge_weight_limit);
		}
		const grund_t* gr = welt->lookup(get_pos());
		const bruecke_t *bridge = gr ? gr->find<bruecke_t>() : NULL;
		const tunnel_t *tunnel = gr ? gr->find<tunnel_t>() : NULL;
		const slope_t::type hang = gr ? gr->get_weg_hang() : slope_t::flat;

		if(hang != slope_t::flat)
		{
			const uint slope_height = (hang & 7) ? 1 : 2;
			if(slope_height == 1)
			{
				uint32 gradient_speed = desc->get_topspeed_gradient_1();
				if (is_degraded())
				{
					if (get_remaining_wear_capacity() > 0)
					{
						gradient_speed /= 2;
					}
					else
					{
						gradient_speed = 0;
					}
				}
				if(bridge)
				{
					set_max_speed(min(gradient_speed, bridge->get_desc()->get_topspeed_gradient_1()));
				}
				else if(tunnel)
				{
					set_max_speed(min(gradient_speed, tunnel->get_desc()->get_topspeed_gradient_1()));
				}
				else
				{
					set_max_speed(gradient_speed);
				}
			}
			else
			{
				uint32 gradient_speed = desc->get_topspeed_gradient_2();
				if (is_degraded())
				{
					if (get_remaining_wear_capacity() > 0)
					{
						gradient_speed /= 2;
					}
					else
					{
						gradient_speed = 0;
					}
				}
				if(bridge)
				{
					set_max_speed( min(gradient_speed, bridge->get_desc()->get_topspeed_gradient_2()));
				}
				else if(tunnel)
				{
					set_max_speed(min(gradient_speed, tunnel->get_desc()->get_topspeed_gradient_2()));
				}
				else
				{
					set_max_speed(desc->get_topspeed_gradient_2());
				}
			}
		}
		else
		{
			if(bridge)
				{
					set_max_speed(min(desc->get_topspeed(), bridge->get_desc()->get_topspeed()));
				}
			else if(tunnel)
				{
					set_max_speed(min(desc->get_topspeed(), tunnel->get_desc()->get_topspeed()));
				}
			else if (old_max_speed == 0)
			{
				if (is_degraded())
				{
					// The maximum speed has to be reduced on account of the degridation.
					if (get_remaining_wear_capacity() > 0)
					{
						set_max_speed(desc->get_topspeed() / 2);
					}
					else
					{
						set_max_speed(0);
					}
				}
				else
				{
					set_max_speed(desc->get_topspeed());
				}
			}
		}

		if(hat_gehweg() && desc->get_wtyp() == road_wt)
		{
			set_max_speed(min(get_max_speed(), 50));
		}
	}

	if (file->is_loading() && ((file->get_extended_version() < 14) || (file->get_extended_version() == 14 && file->get_extended_revision() < 24)))
	{
		init_travel_times();
	}
	if (file->get_extended_version() >= 15 || (file->get_extended_version() >= 14 && file->get_extended_revision() >= 33))
	{
		if (file->is_saving())
		{
			for (uint32 i = 0; i < 2; i++)
			{
				uint32 private_car_routes_count = private_car_routes[i].get_count();
				file->rdwr_long(private_car_routes_count);
				FOR(private_car_route_map, element, private_car_routes[i])
									{
										koord destination = element.key;
										koord3d next_tile = element.value;

										destination.rdwr(file);
										next_tile.rdwr(file);
									}
			}
		}
		else // Loading
		{
			for (uint32 i = 0; i < 2; i++)
			{
				uint32 private_car_routes_count = 0;
				file->rdwr_long(private_car_routes_count);
				for (uint32 j = 0; j < private_car_routes_count; j++)
				{
					koord destination;
					destination.rdwr(file);
					koord3d next_tile;
					next_tile.rdwr(file);
					bool put_succeeded = private_car_routes[i].put(destination, next_tile);
					assert(put_succeeded);
					(void)put_succeeded;
				}
			}
		}
	}
}

void strasse_t::set_overtaking_mode(overtaking_mode_t o, player_t* calling_player)
{
	if (o == invalid_mode) { return; }
	grund_t* gr = welt->lookup(get_pos());
	if ((!calling_player || !calling_player->is_public_service()) && is_public_right_of_way() && gr && gr->removing_way_would_disrupt_public_right_of_way(road_wt))
	{
		return;
	}
	if (is_deletable(calling_player) == NULL)
	{
		overtaking_mode = o;
	}
}

void strasse_t::update_ribi_mask_oneway(ribi_t::ribi mask, ribi_t::ribi allow, player_t* calling_player)
{
	if (is_deletable(calling_player) != NULL) {
		return;
	}

	// assertion. @mask and @allow must be single or none.
	if(!(ribi_t::is_single(mask)||(mask==ribi_t::none))) dbg->error( "weg_t::update_ribi_mask_oneway()", "mask is not single or none.");
	if(!(ribi_t::is_single(allow)||(allow==ribi_t::none))) dbg->error( "weg_t::update_ribi_mask_oneway()", "allow is not single or none.");

	if(  mask==ribi_t::none  ) {
		if(  ribi_t::is_twoway(get_ribi_unmasked())  ) {
			// auto complete
			ribi_mask_oneway |= (get_ribi_unmasked()-allow);
		}
	} else {
		ribi_mask_oneway |= mask;
	}
	// remove backward ribi
	if(  allow==ribi_t::none  ) {
		if(  ribi_t::is_twoway(get_ribi_unmasked())  ) {
			// auto complete
			ribi_mask_oneway &= ~(get_ribi_unmasked()-mask);
		}
	} else {
		ribi_mask_oneway &= ~allow;
	}
}

ribi_t::ribi strasse_t::get_ribi() const {
	ribi_t::ribi ribi = get_ribi_unmasked();
	ribi_t::ribi ribi_maske = get_ribi_maske();
	if(  get_waytype()==road_wt  &&  overtaking_mode<=oneway_mode  ) {
		return (ribi_t::ribi)((ribi & ~ribi_maske) & ~ribi_mask_oneway);
	} else {
		return (ribi_t::ribi)(ribi & ~ribi_maske);
	}
}

void strasse_t::rotate90() {
	weg_t::rotate90();
	ribi_mask_oneway = ribi_t::rotate90( ribi_mask_oneway );
}

strasse_t::~strasse_t() {
	if(welt->is_destroying()) {
#ifdef MULTI_THREAD
		welt->await_private_car_threads();
#endif
		delete_all_routes_from_here();
	}
#ifdef MULTI_THREAD
	pthread_mutex_destroy(&private_car_store_route_mutex);
#endif
}

void strasse_t::info(cbuffer_t &buf) const {
	weg_t::info(buf);
	buf.printf("\n");
	buf.printf("%s: ", translator::translate("overtaking"));
	switch (get_overtaking_mode()) {
		case halt_mode:
			buf.append(translator::translate("halt mode"));
			break;
		case oneway_mode:
			buf.append(translator::translate("oneway"));
			break;
		case twoway_mode:
			buf.append(translator::translate("twoway"));
			break;
		case prohibited_mode:
			buf.append(translator::translate("prohibited"));
			break;
		default:
			buf.append(translator::translate("ERROR"));
			break;
	}
	buf.append("\n");
	buf.append("\n");
	// TODO: Add translator entry for this text
	buf.append(translator::translate("Road routes from here:"));
	buf.append("\n");

	uint32 cities_count = 0;
	uint32 buildings_count = 0;
	FOR(private_car_route_map, const& route, private_car_routes[private_car_routes_currently_reading_element])
	{

		const grund_t* gr = welt->lookup_kartenboden(route.key);
		const gebaeude_t* building = gr ? gr->get_building() : NULL;
		if (building)
		{
			buildings_count++;
#ifdef DEBUG
			buf.append("\n");
			buf.append(translator::translate(building->get_individual_name()));
#endif
		}

		const stadt_t* city = welt->get_city(route.key);
		if (city && route.key == city->get_townhall_road())
		{
			cities_count++;
#ifdef DEBUG
			buf.append("\n");
			buf.append(city->get_name());
#endif
		}
	}
#ifdef DEBUG
	buf.printf("\n");
#endif
	buf.printf("%u buildings\n%u cities\n\n", buildings_count, cities_count);
	buf.printf("\n");
	buf.printf(translator::translate("Congestion: %i%%"), get_congestion_percentage()); // TODO: Set up this text for translating
	buf.printf("\n");
}

void strasse_t::add_private_car_route(koord dest, koord3d next_tile) {

#ifdef MULTI_THREAD
	int error = pthread_mutex_lock(&private_car_store_route_mutex);
	assert(error == 0);
	(void)error;
#endif
	private_car_routes[get_private_car_routes_currently_writing_element()].set(dest, next_tile);

	//private_car_routes_std[get_private_car_routes_currently_writing_element()].emplace(dest, next_tile); // Old performance test - but this was worse than the Simutrans type
#ifdef MULTI_THREAD
	error = pthread_mutex_unlock(&private_car_store_route_mutex);
	assert(error == 0);
#endif
#ifdef DEBUG_PRIVATE_CAR_ROUTES
	calc_image();
#endif
}

void strasse_t::remove_private_car_route(koord dest, bool reading_set) {
	const uint32 routes_index = reading_set ? private_car_routes_currently_reading_element : get_private_car_routes_currently_writing_element();
#ifdef MULTI_THREAD
	int error = pthread_mutex_lock(&private_car_store_route_mutex);
	assert(error == 0);
	(void)error;
#endif
	private_car_routes[routes_index].remove(dest);
	//private_car_routes_std[routes_index].erase(dest); // Old test - but this was much slower than the Simutrans hashtable.
#ifdef MULTI_THREAD
	error = pthread_mutex_unlock(&private_car_store_route_mutex);
	assert(error == 0);
	(void)error;
#endif
}

void strasse_t::delete_all_routes_from_here(bool reading_set) {
	const uint32 routes_index = reading_set ? private_car_routes_currently_reading_element : get_private_car_routes_currently_writing_element();

	if (!private_car_routes[routes_index].empty())
	{
		vector_tpl<koord> destinations_to_delete;
		FOR(private_car_route_map, const& route, private_car_routes[routes_index])
		{
			koord dest = route.key;
			destinations_to_delete.append(dest);
		}

		FOR(vector_tpl<koord>, dest, destinations_to_delete)
		{
			// This must be done in a two stage process to avoid memory corruption as the delete_route_to function will affect the very hashtable being iterated.
			delete_route_to(dest, reading_set);
		}
	}
#ifdef DEBUG_PRIVATE_CAR_ROUTES
	calc_image();
#endif
}

void strasse_t::delete_route_to(koord destination, bool reading_set) {
	const uint32 routes_index = reading_set ? private_car_routes_currently_reading_element : get_private_car_routes_currently_writing_element();

	koord3d next_tile = get_pos();
	koord3d this_tile = next_tile;
	while (next_tile != koord3d::invalid && next_tile != koord3d(0, 0, 0))
	{
		const grund_t* gr = welt->lookup(next_tile);

		next_tile = koord3d::invalid;
		if (gr)
		{
			strasse_t* const str = (strasse_t*)gr->get_weg(road_wt);
			if (str)
			{
				next_tile = str->private_car_routes[routes_index].get(destination);
				str->remove_private_car_route(destination, reading_set);
			}
		}
		if (this_tile == next_tile)
		{
			break;
		}
		this_tile = next_tile;
	}
}

void strasse_t::add_travel_time_update(uint32 actual, uint32 ideal) {
	pending_travel_time_updates.append(std::make_tuple(this, actual, ideal));
}

void strasse_t::apply_travel_time_updates() {
	while(!pending_travel_time_updates.empty() ) {
		strasse_t* str;
		uint32 actual;
		uint32 ideal;
		std::tie(str, actual, ideal) = pending_travel_time_updates.remove_first();
		if(str) {
			str->update_travel_times(actual,ideal);
		}
	}
}

void strasse_t::clear_travel_time_updates() {
	pending_travel_time_updates.clear();
}

void strasse_t::init() {
	weg_t::init();
	init_travel_times();
#ifdef MULTI_THREAD
	pthread_mutexattr_init(&mutex_attributes);
	pthread_mutex_init(&private_car_store_route_mutex, &mutex_attributes);
#endif
}

void strasse_t::init_travel_times() {
	for(  int type=0;  type<MAX_WAY_TRAVEL_TIMES;  type++  ) {
		for(auto & travel_time : travel_times) {
			travel_time[type] = 0;
		}
	}
}

void strasse_t::new_month() {
	init_travel_times();
	weg_t::new_month();
}
