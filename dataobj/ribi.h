/*
 * This file is part of the Simutrans-Extended project under the Artistic License.
 * (see LICENSE.txt)
 */

#ifndef DATAOBJ_RIBI_H
#define DATAOBJ_RIBI_H


#include "../simtypes.h"
#include "../simconst.h"
#include "../simdebug.h"

class koord;
class koord3d;
class ribi_t;

class slope_t {
private:
    uint8 value;

    constexpr uint8 cnr_hgt(uint8 c) const {return value / c % NUM_CNR_HGTS;} //TODO: remove once value can be sufficiently accessed through other methods.
public:
   enum special : uint8 {
        flat = 0,

        sw = 1, se = 3, ne = 9, nw = 27,

        n = sw + se,
        s = ne + nw,
        e = sw + nw,
        w = se + ne,

        all_up_one = sw + se + ne + nw,
        all_up_two = all_up_one * 2,

        raised = all_up_two,
        max_number = all_up_two,
    };
    static constexpr uint8 MAX_CNR_HGT = 2;
    static constexpr uint8 NUM_CNR_HGTS = MAX_CNR_HGT + 1;
    static constexpr uint8 hgt_max(uint8 a, uint8 b) {return a > b ? a : b;}
    static constexpr uint8 hgt_min(uint8 a, uint8 b) {return a < b ? a : b;}
    constexpr uint8 sw_cnr() const {return cnr_hgt(sw);}
    constexpr uint8 se_cnr() const {return cnr_hgt(se);}
    constexpr uint8 ne_cnr() const {return cnr_hgt(ne);}
    constexpr uint8 nw_cnr() const {return cnr_hgt(nw);}
    constexpr bool any_eq(uint8 hgt) const {return cnr_hgt(sw) == hgt || cnr_hgt(se) == hgt || cnr_hgt(ne) == hgt || cnr_hgt(nw) == hgt;}
    constexpr bool any_gt(uint8 hgt) const {return cnr_hgt(sw) > hgt || cnr_hgt(se) > hgt || cnr_hgt(ne) > hgt || cnr_hgt(nw) > hgt;}
    constexpr slope_t() : value(flat) {}; // Flat slope by default
    constexpr explicit slope_t(uint8 _v) : value(_v) {};
    constexpr slope_t(uint8 _sw, uint8 _se, uint8 _ne, uint8 _nw) : slope_t(_sw*sw + _se*se + _ne*ne + _nw*nw) {};

    constexpr bool is_flat() const {return value == flat;} //TODO: remove once value can be sufficiently accessed through other methods.

    constexpr bool allows_way_ns() const {return cnr_hgt(ne) == cnr_hgt(nw) && cnr_hgt(sw) == cnr_hgt(se);} //TODO: use generalised function with ribi_t::ribi as argument
    constexpr bool allows_way_ew() const {return cnr_hgt(se) == cnr_hgt(ne) && cnr_hgt(sw) == cnr_hgt(nw);} //TODO: use generalised function with ribi_t::ribi as argument
    constexpr bool allows_way() const {return allows_way_ns() || allows_way_ew();}
    constexpr bool allows_junction() const {return allows_way_ns() && allows_way_ew();}

    constexpr bool is_single() const {return allows_way() && !allows_junction();}

    constexpr bool is_all_up() const {return !any_eq(0);}
    constexpr bool is_two_high() const {return any_eq(2);}
    constexpr bool is_one_high() const {return any_eq(1);}

    constexpr uint8 max_cnr_hgt() const {return any_eq(2) ? 2 : (any_eq(1) ? 1 : 0);}
    constexpr uint8 min_cnr_hgt() const {return any_eq(0) ? 0 : (any_eq(1) ? 1 : 2);}

    constexpr uint8 max_diff() const {return max_cnr_hgt() - min_cnr_hgt();}

    constexpr uint8 cnr_diff(uint8 c, slope_t other) const {return other.cnr_hgt(c) > cnr_hgt(c) ? other.cnr_hgt(c) - cnr_hgt(c) : cnr_hgt(c) - other.cnr_hgt(c);}

    constexpr slope_t diff(slope_t other) const {return {cnr_diff(sw, other), cnr_diff(se, other), cnr_diff(ne, other), cnr_diff(nw, other)};}
    constexpr slope_t opposite() const {return is_single() ? (diff(filled(max_cnr_hgt()))) : slope_t();}

    static uint8 min_diff(slope_t high, slope_t low) {return high.diff(low).min_cnr_hgt();}

    constexpr slope_t rotate90() const {return {cnr_hgt(se), cnr_hgt(ne), cnr_hgt(nw), cnr_hgt(sw)};}

    constexpr uint8 get_value() const {return value;} //TODO: remove once value can be sufficiently accessed through other methods.

    constexpr bool is_legal() const {return !is_all_up() && !any_gt(MAX_CNR_HGT);}

    constexpr bool operator==(slope_t other) const {return get_value() == other.get_value();}
    //bool operator==(uint8 other) const {assert(false); return false;}
    constexpr bool operator!=(slope_t other) const {return get_value() != other.get_value();}

    constexpr slope_t operator-=(slope_t rhs) const {return slope_t(get_value() - rhs.get_value());}

    constexpr slope_t doubled() const {return slope_t(value * 2);}

    static constexpr slope_t filled(uint8 hgt) {return {hgt, hgt, hgt, hgt};}

    static constexpr slope_t combined(slope_t a, slope_t b) {return slope_t(a.get_value() | b.get_value());}
    static constexpr slope_t combined_max(slope_t a, slope_t b) {return {hgt_max(a.sw_cnr(), b.sw_cnr()), hgt_max(a.se_cnr(), b.se_cnr()), hgt_max(a.ne_cnr(), b.ne_cnr()), hgt_max(a.ne_cnr(), b.ne_cnr())};}
    static constexpr slope_t combined_min(slope_t a, slope_t b) {return {hgt_min(a.sw_cnr(), b.sw_cnr()), hgt_min(a.se_cnr(), b.se_cnr()), hgt_min(a.ne_cnr(), b.ne_cnr()), hgt_min(a.ne_cnr(), b.ne_cnr())};}
};

/**
* Slopes of tiles.
*/
class old_slope_t {

    /// Static lookup table
	static const int flags[81];

	/// Named constants for the flags table
	enum {
		doubles = 1,   ///< two-height difference slopes
		way_ns = 2,   ///< way possible in north-south direction
		way_ew = 4,   ///< way possible in east-west direction
		single = 8,   ///< way possible
		all_up = 16,  ///< all corners raised
	};

public:

	typedef sint8 type;


	/**
	* Named constants for special cases.
	*/
	enum _type {
		flat=0,

		northwest = 27, ///< NW corner
		northeast = 9,  ///< NE corner
		southeast = 3,  ///< SE corner
		southwest = 1,  ///< SW corner

		north = old_slope_t::southeast + old_slope_t::southwest,	///< North slope
		west  = old_slope_t::northeast + old_slope_t::southeast,  ///< West slope
		east  = old_slope_t::northwest + old_slope_t::southwest,  ///< East slope
		south = old_slope_t::northwest + old_slope_t::northeast,  ///< South slope

		all_up_one = old_slope_t::southwest + old_slope_t::southeast + old_slope_t::northeast + old_slope_t::northwest, ///all corners 1 high
		all_up_two = old_slope_t::all_up_one * 2,                                                     ///all corners 2 high

		raised = all_up_two,    ///< special meaning: used as slope of bridgeheads and in terraforming tools (keep for compatibility)

		max_number = all_up_two
	};

	/*
	 * Macros to access the height of the 4 corners:
	 */
#define corner_sw_old(i)  ((i)%old_slope_t::southeast)                      // sw corner
#define corner_se_old(i) (((i)/old_slope_t::southeast)%old_slope_t::southeast)  // se corner
#define corner_ne_old(i) (((i)/old_slope_t::northeast)%old_slope_t::southeast)  // ne corner
#define corner_nw_old(i)  ((i)/old_slope_t::northwest)                      // nw corner

#define encode_corners(sw, se, ne, nw) ( (sw) * old_slope_t::southwest + (se) * old_slope_t::southeast + (ne) * old_slope_t::northeast + (nw) * old_slope_t::northwest )

#define is_one_high_old(i)   ((i) & 7)  // quick method to know whether a slope is one high - relies on two high slopes being divisible by 8 -> i&7=0 (only works for slopes with flag single)

	/// Compute the slope opposite to @p x. Returns flat if @p x does not allow ways on it.
	static type opposite(type x) { return is_single(x) ? (is_one_high_old(x) ? (old_slope_t::all_up_one - x) : (old_slope_t::all_up_two - x)) : flat; }
	/// Rotate.
	static type rotate90(type x) { return (((x % old_slope_t::southeast) * old_slope_t::northwest ) + ((x - (x % old_slope_t::southeast) ) / old_slope_t::southeast ) ); }
	/// Returns true if @p x has all corners raised.
	static bool is_all_up(type x) { return (flags[x] & all_up)>0; }
	/// Returns maximal height difference between the corners of this slope.
	static uint8 max_diff(type x) { return (x != 0) + (flags[x] & doubles); }
	/// Computes minimum height differnce between corners of  @p high and @p low.
	static sint8 min_diff(type high, type low) { return min(min(corner_sw_old(high) - corner_sw_old(low), corner_se_old(high) - corner_se_old(low)), min(corner_ne_old(high) - corner_ne_old(low), corner_nw_old(high) - corner_nw_old(low))); }

	/// Returns if slope prefers certain way directions (either n/s or e/w).
	static bool is_single(type x) { return (flags[x] & single) != 0; }

	static bool is_doubles(type x) { return (flags[x] & doubles) != 0; }
	/// Returns if way can be build on this slope.
	static bool is_way(type x) { return (flags[x] & (way_ns | way_ew)) != 0; }
	/// Returns if way in n/s direction can be build on this slope.
	static bool is_way_ns(type x) { return (flags[x] & way_ns) != 0; }
	/// Returns if way in e/w direction can be build on this slope.
	static bool is_way_ew(type x) { return (flags[x] & way_ew) != 0; }

	/**
	* Check if the slope is upwards, relative to the previous tile.
	* @returns 1 for single upwards and 2 for double upwards
	*/
	static sint16 get_sloping_upwards(const type slope, const sint16 relative_pos_x, const sint16 relative_pos_y);
};


/**
* Old implementation of slopes: one bit per corner.
* Used as bitfield to refer to specific corners of a tile
* as well as for compatibility.
*/
struct slope4_t {
	/* bit-field:
	* Bit 0 is set if southwest corner is raised
	* Bit 1 is set if southeast corner is raised
	* Bit 2 is set if northeast corner is raised
	* Bit 3 is set if northwest corner is raised
	*
	* Don't get confused - the southern/southward slope has its northern corners raised
	*
	* Macros to access the height of the 4 corners for single slope:
	* One bit per corner
	*/
	typedef sint8 type;

#define scorner_sw(i) (i%2)    	// sw corner
#define scorner_se(i) ((i/2)%2)	// se corner
#define scorner_ne(i) ((i/4)%2)	// ne corner
#define scorner_nw(i) (i/8)    	// nw corner
	enum _corners {
		corner_SW = 1,
		corner_SE = 2,
		corner_NE = 4,
		corner_NW = 8
	};
};


/**
* Directions in simutrans.
* ribi_t = Richtungs-Bit = Directions-Bitfield
* @author Hj. Malthaner
*/
class ribi_t {
	/// Static lookup table
	static const int flags[16];

	/// Named constants for properties of directions
	enum {
		single = 1,  ///< only one bit set, way ends here
		straight_ns = 2,  ///< contains straight n/s connection
		straight_ew = 4,  ///< contains straight e/w connection
		bend = 8,   ///< is a bend
		twoway = 16, ///< two bits set
		threeway = 32, ///< three bits set
	};

public:
	/**
	* Named constants for all possible directions.
	* 1=North, 2=East, 4=South, 8=West
	*/
	enum _ribi {
		none = 0,
		north = 1,
		east = 2,
		northeast = 3,
		south = 4,
		northsouth = 5,
		southeast = 6,
		northsoutheast = 7,
		west = 8,
		northwest = 9,
		eastwest = 10,
		northeastwest = 11,
		southwest = 12,
		northsouthwest = 13,
		southeastwest = 14,
		all = 15
	};
	typedef uint8 ribi;

	/**
	* Named constants to translate direction to image number for vehicles, signs.
	*/
	enum _dir {
		dir_invalid = 0,
		dir_south = 0,
		dir_west = 1,
		dir_southwest = 2,
		dir_southeast = 3,
		dir_north = 4,
		dir_east = 5,
		dir_northeast = 6,
		dir_northwest = 7
	};
	typedef uint8 dir;

private:
	/// Lookup table to compute backward direction
	static const ribi backwards[16];
	/// Lookup table ...
	static const ribi doppelr[16];
	/// Lookup table to convert ribi to dir.
	static const dir  dirs[16];
public:
	/// Table containing the four compass directions
	static const ribi nsew[4];
	/// Convert building layout to ribi (four rotations), use doubles in case of two rotations
	static const ribi layout_to_ribi[4];	// building layout to ribi (for four rotations, for two use doubles()!

	static bool is_twoway(ribi x) { return (flags[x] & twoway) != 0; }
	static bool is_threeway(ribi x) { return (flags[x] & threeway) != 0; }
	static bool is_perpendicular(ribi x, ribi y);
	static bool is_single(ribi x) { return (flags[x] & single) != 0; }
	static bool is_bend(ribi x) { return (flags[x] & bend) != 0; }
	static bool is_straight(ribi x) { return (flags[x] & (straight_ns | straight_ew)) != 0; }
	static bool is_straight_ns(ribi x) { return (flags[x] & straight_ns) != 0; }
	static bool is_straight_ew(ribi x) { return (flags[x] & straight_ew) != 0; }

	/// Convert single/straight direction into their doubled form (n, ns -> ns), map all others to zero
	static ribi doubles(ribi x) { return doppelr[x]; }
	/// Backward direction for single ribi's, bitwise-NOT for all others
	static ribi backward(ribi x) { return backwards[x]; }

	/**
	 * Same as rueckwaerts, but for single directions only.
	 * Effectively does bit rotation. Avoids lookup table backwards.
	 * @returns rueckwaerts(x) for single ribis, 0 for x==0.
	 */
	static inline ribi reverse_single(ribi x) {
		return ((x | x << 4) >> 2) & 0xf;
	}

	/// Rotate 90 degrees to the right. Does bit rotation.
	static ribi rotate90(ribi x) { return ((x | x << 4) >> 3) & 0xf; }
	/// Rotate 90 degrees to the left. Does bit rotation.
	static ribi rotate90l(ribi x) { return ((x | x << 4) >> 1) & 0xf; }
	static ribi rotate45(ribi x) { return (is_single(x) ? x | rotate90(x) : x&rotate90(x)); } // 45 to the right
	static ribi rotate45l(ribi x) { return (is_single(x) ? x | rotate90l(x) : x&rotate90l(x)); } // 45 to the left

																								 /// Convert ribi to dir
	static dir get_dir(ribi x) { return dirs[x]; }
};

/**
* Calculate directions from slopes.
* Go upward on the slope: slope_t::north translates to ribi_t::south.
*/
slope_t slope_type(koord dir);
slope_t slope_type(ribi_t::ribi);

/**
* Calculate direction bit from coordinate differences.
*/
ribi_t::ribi ribi_typ_intern(sint16 dx, sint16 dy);

/**
* Calculate direction bit from direction.
*/
ribi_t::ribi ribi_type(const koord& dir);
ribi_t::ribi ribi_type(const koord3d& dir);

/**
* Calculate direction bit from slope.
* Note: slope_t::north (slope north) will be translated to ribi_t::south (direction south).
*/
ribi_t::ribi ribi_type(slope_t slope);

/**
* Calculate direction bit for travel from @p from to @p to.
*/
template<class K1, class K2>
ribi_t::ribi ribi_type(const K1&from, const K2& to)
{
	return ribi_typ_intern(to.x - from.x, to.y - from.y);
}

#endif
