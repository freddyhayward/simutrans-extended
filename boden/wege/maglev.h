/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef BODEN_WEGE_MAGLEV_H
#define BODEN_WEGE_MAGLEV_H


#include "schiene.h"


/**
 * derived from schiene, because signals will behave similar
 */
class maglev_t : public schiene_t
{
public:
	static const way_desc_t *default_maglev;

	maglev_t() : schiene_t(maglev_wt) { set_desc(default_maglev); }

	/**
	 * File loading constructor.
	 */
	maglev_t(loadsave_t *file);

	//waytype_t get_waytype() const OVERRIDE {return maglev_wt;}

	void rdwr(loadsave_t *file) OVERRIDE;
};

#endif
