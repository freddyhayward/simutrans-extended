/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#include "../simdebug.h"
#include "../simworld.h"

#include "../obj/bruecke.h"
#include "../bauer/brueckenbauer.h"

#include "../descriptor/ground_desc.h"
#include "../descriptor/bridge_desc.h"

#include "../dataobj/loadsave.h"
#include "../dataobj/environment.h"
#include "../dataobj/translator.h"

#include "../utils/cbuffer_t.h"

#include "brueckenboden.h"
#include "wege/weg.h"

#include "../vehicle/simvehicle.h"

brueckenboden_t::brueckenboden_t(koord3d pos, slope_t grund_hang, slope_t weg_hang) : grund_t(pos)
{
	slope = grund_hang;
	this->weg_hang = weg_hang;
}


void brueckenboden_t::calc_image_internal(const bool calc_only_snowline_change)
{
	if(  ist_karten_boden()  ) {

		set_image( ground_desc_t::get_ground_tile(this) );

		if(  !calc_only_snowline_change  ) {
			grund_t::calc_back_image( get_pos().z, slope );
			set_flag( draw_as_obj );
			if((get_grund_hang() == slope_t(slope_t::w) && abs(back_imageid) > 11) || (get_grund_hang() == slope_t(slope_t::n) && get_back_image(0) != IMG_EMPTY)  ) {
				// must draw as obj, since there is a slop here nearby
				koord pos = get_pos().get_2d() + koord( get_grund_hang() );
				grund_t *gr = welt->lookup_kartenboden( pos );
				gr->set_flag( grund_t::draw_as_obj );
			}
		}
	}
	else {
		clear_back_image();
		set_image(IMG_EMPTY);
	}

}


void brueckenboden_t::rdwr(loadsave_t *file)
{
	xml_tag_t t( file, "brueckenboden_t" );

	grund_t::rdwr(file);

	if(file->get_version()<88009) {
		uint8 sl;
		file->rdwr_byte(sl);
		slope = slope_t(sl);
	}
	if(  file->is_saving()  &&  file->get_version() < 112007  ) {
		// truncate double weg_hang to single weg_hang, better than nothing
		uint8 sl = min(weg_hang.sw_cnr(), 1 ) + min(weg_hang.se_cnr(), 1 ) * 2 + min(weg_hang.ne_cnr(), 1 ) * 4 + min(weg_hang.nw_cnr(), 1 ) * 8;
		file->rdwr_byte(sl);
	}
	else {
	    uint8 tmp_hang = weg_hang.get_value();
		file->rdwr_byte(tmp_hang);
		weg_hang = slope_t(tmp_hang);
	}

	if(  file->is_loading()  &&  file->get_version() < 112007  ) {
		// convert slopes from old single height saved game
        uint8 tmp_hang = weg_hang.get_value();
		weg_hang = slope_t(scorner_sw(tmp_hang), scorner_se(tmp_hang), scorner_ne(tmp_hang), scorner_nw(tmp_hang));
		weg_hang = slope_t(weg_hang.get_value() * env_t::pak_height_conversion_factor);
	}

	if(!find<bruecke_t>()) {
		dbg->error( "brueckenboden_t::rdwr()","no bridge on bridge ground at (%s); try replacement", pos.get_str() );
		weg_t *w = get_weg_nr(0);
		if(w) {
			const bridge_desc_t *br_desc = bridge_builder_t::find_bridge( w->get_waytype(), w->get_max_speed(), 0 );
			const grund_t *kb = welt->lookup_kartenboden(get_pos().get_2d());
			int height = 1;
			if(  kb && get_pos().z - kb->get_pos().z > 1 ) {
				height = 2;
			}
			bruecke_t *br = new bruecke_t( get_pos(), welt->get_public_player(), br_desc, ist_karten_boden() ? br_desc->get_end( slope, get_grund_hang(), get_weg_hang() ) : br_desc->get_straight( w->get_ribi_unmasked(), height ) );
			obj_add( br );
		}
	}
}


void brueckenboden_t::rotate90()
{
	if( sint8 way_offset = get_weg_yoff() ) {
		pos.rotate90( welt->get_size().y-1 );
		slope = slope.rotate90();
		// since the y_off contains also the way height, we need to remove it before rotations and add it back afterwards
		for( uint8 i = 0; i < objlist.get_top(); i++ ) {
			obj_t * obj = obj_bei( i );
			if(  !dynamic_cast<vehicle_base_t*>(obj)  ) {
				obj->set_yoff( obj->get_yoff() + way_offset );
				obj->rotate90();
				obj->set_yoff( obj->get_yoff() - way_offset );
			}
			else {
				// vehicle corrects its offset themselves
				obj->rotate90();
			}
		}
	}
	else {
		weg_hang = weg_hang.rotate90();
		grund_t::rotate90();
	}
}


sint8 brueckenboden_t::get_weg_yoff() const
{
	if(  ist_karten_boden()  &&  weg_hang.is_flat() ) {
		// we want to find maximum height of slope corner shortcut as we know this is n, s, e or w and single heights are not integer multiples of 8
		return TILE_HEIGHT_STEP * slope.max_diff();
	}
	else {
		return 0;
	}
}

void brueckenboden_t::info(cbuffer_t & buf) const
{
	const bruecke_t *bridge = find<bruecke_t>();
	if(bridge  &&  bridge->get_desc()) {
		const bridge_desc_t *desc = bridge->get_desc();
		buf.append(translator::translate(desc->get_name()));
		buf.append("\n");
	}
	grund_t::info(buf);
}
