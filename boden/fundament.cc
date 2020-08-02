/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

/*
 * Ground for buildings in Simutrans.
 * Revised January 2001
 * Hj. Malthaner
 */

#include "../simconst.h"

#include "../descriptor/ground_desc.h"
#include "../dataobj/loadsave.h"

#include "grund.h"
#include "fundament.h"


fundament_t::fundament_t(loadsave_t *file, koord pos ) : grund_t(koord3d(pos,0) )
{
	rdwr(file);
	slope = slope_t();
}


fundament_t::fundament_t(koord3d pos, slope_t hang, bool build_up ) : grund_t(pos)
{
	set_image( IMG_EMPTY );
	if(!hang.is_flat() && build_up) {
		pos = get_pos();
		pos.z += hang.max_diff();
		set_pos( pos );
	}
	slope = slope_t();
}


void fundament_t::calc_image_internal(const bool calc_only_snowline_change)
{
	set_image( ground_desc_t::get_ground_tile(this) );

	if(  !calc_only_snowline_change  ) {
		grund_t::calc_back_image( get_disp_height(), slope_t() );
	}
	set_flag( dirty );
}
