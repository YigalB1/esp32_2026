// tof_gate_case_v20.scad
//
// v20 changes from v19:
//   - Left/right retention walls made much taller: now reach outerH()-3
//     (close to the lid, 1mm clear of its lip) instead of stopping just
//     past the board's own thickness. interiorH() is actually driven by
//     the sensor zone's needs, not board A's much smaller zone, so the
//     old height left ~10mm of unused headroom below the lid for no
//     reason. Confirmed via render: walls now nearly as tall as the
//     corner posts.
//   - Gap between the two long walls narrowed by 3mm total (1.5mm off
//     each side) via a new wall_jst_clearance, kept separate from
//     jst_side_clearance itself (which still sizes the underlying
//     pocket cavity independently).
//   - Front-wall segment lengths tuned by COLOR this time (unambiguous,
//     since left/right kept getting crossed across several turns):
//     Orange -2mm (3->1), Turquoise +2mm (0.5->2.5).
//
// v19 changes from v18:
//   - Distinct, high-contrast colors for the two front-wall segments:
//     Orange (left) and Turquoise (right) - Coral/DarkSalmon before
//     were too close in hue to tell apart at a glance.
//   - Capped the segments' outward extent at the long walls' own outer
//     boundary - the previous wall_overlap term pushed them 1mm past
//     it unnecessarily. Each segment already spans the long wall's
//     FULL width on its way to the board, which is already far more
//     overlap than a clean union needs.
//   - Shortened both segments' reach into the board's territory by 2mm
//     (5->3 left, 2.5->0.5 right), so they block less of the sensor's
//     wires/JST socket. The right segment is now quite thin (0.5mm) -
//     may be fragile to print reliably; flag if it needs to come back
//     up.
//
// v18 changes from v17:
//   - Restored the right front-wall segment of the C3 holder (v17
//     wrongly eliminated it instead of shortening it). Both segments
//     now reach out to connect with their own (jst_side_clearance-
//     widened) long wall - left in Coral, right in DarkSalmon, right
//     one at half the left's reach into the board's own territory
//     (front_wall_end_w_r = 2.5 vs front_wall_end_w = 5). Confirmed via
//     render: both segments present, right visibly shorter, both
//     cleanly connected with no gaps.
//
// v17 changes from v16:
//   - Restored BOTH slide end-supports (v16's removal was based on a
//     misreading - that request was actually about the C3 holder's
//     stop-wall, not the sensor's own end-supports).
//   - Removed the RIGHT front-wall segment of the C3 holder (the one
//     that was actually meant last time). The remaining LEFT segment
//     is extended to start at the left retention wall's own (v16-
//     widened) position, with wall_overlap for a real connection - the
//     v16 widening had left a literal gap between the old segment
//     (still at the board's bare edge) and the now-further-out wall.
//   - Colored every internal wall distinctly: Red (left), Crimson
//     (right), Coral (front stop), Purple/Magenta (sensor supports) -
//     five genuinely distinguishable pieces instead of everything
//     sharing just Red or Magenta.
//
// v16 changes from v15:
//   - Widened board A's holder on the PIN sides (left/right), not the
//     USB sides - pushed those two retention walls out by
//     jst_side_clearance beyond the board's bare edge. A JST connector
//     plugged onto the header pins physically extends past the board's
//     own edge there; walls sitting tight against the bare PCB (as in
//     v15) would collide with that connector bulk and block it from
//     seating at all. Confirmed via render: visibly wider gap on both
//     sides now.
//   - Removed the slide end-support on side 0 (the "right hand side
//     when looking from the USB connector toward the sensor" - the
//     lower-X end in this file's coordinates). The sensor's own JST
//     connector sits on that end, and its bulk was colliding with the
//     backstop material there, physically blocking the sensor board
//     from sliding all the way down into place. Side 1's support is
//     untouched, still providing backstop + lateral confinement on its
//     end. Confirmed via render: only one magenta support remains.
//
// v15 changes from v14:
//   - Board A retention walls made taller: now reach A_top_clearance +
//     A_thk above the pocket floor (was just A_thk+1) for firmer
//     confinement over more of the board's profile. Confirmed via
//     render: walls now visibly taller relative to the corner posts.
//   - Relocated the perpendicular stop-wall from BACK to FRONT. The USB
//     connector needs to press firmly against the case's own back
//     wall/cutout - a stop at the back left room for the board to sit
//     away from that wall. A stop at the FRONT (sensor side) correctly
//     blocks the board from drifting forward instead, which is what
//     keeps it pressed back against the USB cutout - and happens to be
//     the right side for the cable gap too, since the sensor's wires
//     approach from the front (interior channel), not the back.
//     Confirmed via render: wall profile is now a "U" open at the back,
//     not the previous "n" open at the front.
//   - Fixed the long (left/right) and short (front) walls only touching
//     edge-to-edge at their junction, not actually overlapping - a
//     zero-width seam. The long walls now extend wall_overlap (1mm)
//     past the front wall's plane, giving a real overlapping volume at
//     each corner. Confirmed via render: clean solid L-shaped corners,
//     no visible gap or seam.
//
// v14 changes from v13:
//   - Lid is now printed flipped (flat plate down, lip up) via a
//     rotate+translate at the top-level call. As originally modeled,
//     the flat plate is wider than the lip (inset by lip_clr), so
//     printing plate-up/lip-down leaves the plate's outer rim
//     overhanging with nothing beneath it - a real support-needing
//     shelf. Flipped, the lip is simply narrower than its own base and
//     needs no support at all. Confirmed via render: flat plate now
//     sits as the thin base layer, lip rises above it.
//   - Colored the lid's two parts differently: Tan for the flat plate,
//     DarkOliveGreen for the lip - previously both were a single
//     uniform SeaGreen.
//   - Swapped the OLED window dimensions back (12mm=X, 7mm=Y) - the
//     v12 swap had it backwards; this reverts to the long edge on X
//     (board width direction) per direct, repeated correction.
//   - Replaced the small locating nubs (red) with proper LEFT, RIGHT,
//     and BACK retention walls for board A - the nubs weren't
//     providing real stability. The back wall is split into two 5mm
//     end-segments with an open gap between them, so the sensor's
//     wires/JST connector can still reach the C3 board - a full-width
//     back wall would have blocked that entirely. Confirmed via render:
//     clear U-shaped wall profile (left/right/back), open on the front
//     side facing the sensor.
//
// v13 changes from v12:
//   - Fixed the end-supports providing no actual lateral (side-to-side)
//     confinement: the slot spans the board's full width, and the
//     previous 4mm supports sat ENTIRELY inside that width - so the
//     slot completely hollowed them out sideways, leaving only the
//     shallow Y-backstop behind them and zero wall material to stop
//     the board sliding side-to-side. Widened to 8mm and re-centered
//     ON the slot's edge: half still inside the slot (still cut, still
//     the Y-backstop), half now OUTSIDE it, completely uncut - that
//     outside half is the real solid wall providing lateral
//     confinement. Confirmed via render: each support now shows solid
//     material on its outer edge, not just the cut inner portion.
//
// v12 changes from v11:
//   - Fixed the lid not closing: the corner posts and slide boss both
//     rise to the FULL case height, but the lid's lip (which descends
//     2mm into the case when closed) occupied that exact same space at
//     those X,Y positions - the screw holes were narrow (screw shaft
//     only), nowhere near wide enough to clear the posts' actual 7mm
//     diameter or the slide boss's footprint. Added wider relief cuts
//     through just the lip (not the top plate, which keeps its narrow
//     screw holes for a proper head shoulder) at all 4 corners and both
//     end-supports. Confirmed via render: visible notches now cut into
//     the lip at all 6 locations.
//   - Split the single full-width slide boss into two 4mm end-supports
//     with an open gap between them, per request - a full-width solid
//     boss blocked wire/JST connector access from the sensor toward the
//     C3 board entirely. The slide slot itself still spans the whole
//     board (one flat piece needs one channel), it just now only
//     notches the two end-supports - the open middle has no material to
//     cut anyway, which is harmless.
//   - Fixed the OLED window: was 10mm(X) x 6mm(Y), swapped from the
//     board's actual long/short axes - corrected to 7mm(X) x 12mm(Y)
//     per direct correction, with the long edge now matching the
//     board's length direction (Y) as it should.
//
// v11 changes from v10:
//   - Pinned the USB-C cutout to a directly MEASURED height
//     (usb_socket_start_height = 14mm above the case's external
//     bottom), rather than continuing to derive its position from
//     floorThk/A_thk math - that approach had already been wrong
//     twice in opposite directions (too high when floorThk was
//     bloated for wire clearance, then too low once that was
//     corrected back down). outerH now also satisfies two
//     requirements (component fit vs USB position), same "take
//     whichever is larger" pattern already used for outerW - confirmed
//     via render the box grew (18.64mm -> 21mm) specifically to contain
//     the cutout at its real position, not an arbitrary guess.
//
// v10 changes from v9:
//   - Widened the slide slot's depth by 2.5mm (component_bulge_clearance)
//     to clear the SENSOR CHIP soldered onto the board, not just the
//     bare PCB - the chip adds real bulk the board-thickness-only slot
//     didn't account for. This leaves a visible gap behind the board,
//     intentionally - to be filled later with a printed filler piece
//     hanging from the lid, or soft fabric, once the board's real
//     resting position is confirmed. boss_depth grows to match, so a
//     real backstop (backstop_thickness) still remains behind the slot.
//   - Reverted floor_solid_below from the v5 "+12mm" approach back to a
//     sensible 2mm. That change had put all the requested wire/JST
//     clearance into needless solid base material rather than usable
//     space, and pushed the USB cutout out of visual proportion with
//     the wall around it. Confirmed via render: the cutout now sits
//     properly contained within the back wall, not floating above it.
//   - Fixed a real inconsistency while in there: usb_body_h (the
//     connector's actual height) was defined but never actually used -
//     usb_cut_h was a disconnected guess with no relationship to it.
//     Now usb_cut_h derives from usb_body_h directly.
//
// v9 changes from v8:
//   - Replaced the two separate end-tracks with ONE single slot/boss
//     sized to the whole sensor board (length + 0.5mm clearance to
//     insert easily). The two-track version had each track only
//     capture its own end, open toward the wall but not toward the
//     middle - functionally two uncoordinated guides rather than one
//     real channel, which is what left no real slide path for the
//     board as a whole. A flat board only needs one channel; this is
//     both simpler and more correct.
//
// v8 changes from v7:
//   - Fixed a real bug: the track slot cut was nested inside the shell's
//     own inner difference(), which runs BEFORE the boss gets unioned in
//     as a separate, later step. So the cut was removing material from
//     empty space (the boss didn't exist yet at that point in the CSG
//     tree), and the boss then got added fully solid afterward with no
//     slot in it at all - same class of ordering bug as v4's corner-post
//     fix. Moved the slot cut to the outer difference(), alongside the
//     M2 insert holes, so it now cuts the FINAL combined shape after the
//     boss actually exists. Confirmed via render: the slot is now
//     visibly present, not just written in the code with no effect.
//
// v7 changes from v6:
//   - Fixed the locating nub height (board A) - keyed to A_thk + margin
//     instead of a fixed 1mm rise, which fell 0.6mm short of the
//     board's actual thickness.
//   - Added color() to every distinct feature for easy visual reference:
//     shell=LightSteelBlue, corner posts=Orange, locating nubs=Red,
//     track bosses=Magenta, lid=SeaGreen.
//
// v6 changes from v5:
//   - Fixed a real bug in the slide tracks: the slot was cut through the
//     boss's ENTIRE depth (front to back), which constrained nothing in
//     the front-to-back direction - the board could shift backward
//     freely once inserted. Also had the board-thickness dimension on
//     the wrong axis entirely (X instead of Y). Fixed: the slot is now
//     shallow (only board thickness + small clearance, track_slot_depth,
//     cut in Y starting right at the wall), leaving solid boss material
//     behind it (track_depth - track_slot_depth) as a real, structural
//     backstop - confirmed via a rendered cross-section, not just
//     inferred from the code.
//
// v5 changes from v4:
//   - Height increased by 12mm for wire/JST routing slack. Implemented
//     as +12mm added to floor_solid_below - since board A's surface,
//     the USB cutout, and the sensor's Z position (via B_z0/
//     sensorZOffset) are all already referenced from floorThk, this one
//     change raises all three together automatically, exactly as
//     requested, without needing a separate riser structure.
//   - Replaced the ToF sensor's barbed peg mounting with vertical slide
//     tracks: two boss/slot pairs at the board's actual edges (not its
//     mounting holes - holes are no longer used for mounting at all).
//     The board slides down from above before the lid goes on; solid
//     material below the slot's start (at B_z0()) stops it at the
//     correct height, and the lid sitting on top afterward stops it
//     sliding back up. No press-fit, no barb to snap past, no risk of
//     working loose over time the way plain friction-fit pegs could.
//
// v4 changes from v3:
//   - Fixed a real collision: the front two corner posts were sitting
//     almost exactly on top of the ToF mounting pegs (confirmed via
//     render - the pegs were essentially swallowed inside the posts).
//     outerW now satisfies TWO independent requirements (board
//     footprint fit, and post/track clearance) and takes whichever is
//     larger - grows the box only as much as actually needed.
//   - Fixed a follow-on bug this exposed: the interior cavity and lid
//     lip were sized to the narrower board-driven width specifically,
//     which would've left them off-center (or not even reaching the
//     tracks) once clearance made the box wider. Both now use
//     cavityW(), which always matches the real final outerW.
//
// v3 changes from v2:
//   - Fixed interiorW to actually accommodate the widened JST pocket
//     (v2's pocket could exceed the interior cavity width slightly)
//   - Added 4 corner posts with blind holes for M2 heat-set inserts,
//     in both the case and matching clearance holes in the lid, so
//     the two halves can be screwed together rather than relying on
//     the lid's friction-fit lip alone.
//
// M2 heat-set insert hole size/depth are ASSUMED (3.2mm dia x 4mm
// deep, typical for common M2 inserts) - verify against your
// specific insert's datasheet before printing, this varies by brand.

$fn = 32;

case_box();
// Lid is flipped for printing: as modeled, the flat plate is WIDER
// than the lip (inset by lip_clr), so printing it plate-up/lip-down
// leaves that outer rim of the plate overhanging with nothing beneath
// it - a real support-needing shelf. Flipped (plate down, lip up),
// the lip is simply narrower than its own base and needs no support.
translate([outerW() + 15, outerLen(), wall]) rotate([180, 0, 0]) lid();

// ==================== Parameters ====================

wall = 2;
clr  = 0.3;

pin_protrusion = 3.64;  // from datasheet
// Kept thin - 2mm of solid material below the pin pocket is plenty for
// strength. The earlier +12mm-here approach (for wire/JST clearance)
// made the base needlessly thick solid plastic rather than actual
// usable space, and pushed the USB cutout out of alignment with where
// the connector actually sits on the board. Wire/JST clearance needs
// to come from real interior room, not a thicker floor slab.
floor_solid_below = 2;
floorThk = pin_protrusion + floor_solid_below;

// --- Board A: ESP32-C3 + 0.42" OLED (measured: 25 x 20mm PCB) ---
A_w   = 20;
A_len = 25;
A_thk = 1.6;              // ASSUMED
A_top_clearance = 4;      // ASSUMED

usb_stickout = 2;
usb_body_h   = 3.5;
usb_cut_w = 9;
// Was a disconnected guess (5) with no relationship to usb_body_h above,
// despite usb_body_h existing for exactly this purpose - fixed to
// actually derive from it (connector height + margin top and bottom).
usb_cut_h = usb_body_h + 1.5;

// MEASURED directly on the real assembly: the USB-C opening starts
// 14mm above the case's external bottom surface. Using this hard
// number directly rather than continuing to derive the connector's
// position from floorThk/A_thk math, which had already been wrong
// twice (too high when floorThk was bloated for wire clearance, then
// too low once that was corrected back down).
usb_socket_start_height = 14;

oled_from_front_edge = 9;  // ASSUMED
// Swapped back again per direct correction - long axis (12mm) actually
// matches the board's WIDTH direction (X), short axis (7mm) matches
// its LENGTH direction (Y). The v12 swap had this backwards.
oled_window_w = 12;
oled_window_h = 7;

pin_pitch = 2.54;
pins_per_row = 8;
pin_row_span = (pins_per_row - 1) * pin_pitch;
pin_row_margin = (A_len - pin_row_span) / 2;

jst_side_clearance = 3;

// --- Board B: VL53L0X breakout (measured: 25 x 10.7mm PCB) ---
B_len = 25;
B_w   = 10.7;
B_thk = 1.6;               // ASSUMED

sensor_dia = 6;
gap_floor_to_boardB = 2;

jst_clearance = 6;

// --- Sensor slide mounting: ONE slot sized to the whole board, not two
// separate end-tracks. The two-track version had each track only
// captured its own end, with nothing actually joining them across the
// middle - functionally two independent, uncoordinated guides rather
// than one real channel. A single slot spanning the board's full
// length avoids that entirely - the board is one flat piece, so it
// only needs one channel.
// The slot itself still spans the whole board (one flat piece needs
// one channel to slide down through), but the SOLID backstop material
// behind it doesn't need to be continuous - only enough at each end to
// actually hold the board. A full-width solid boss blocked wire/JST
// connector access from the sensor toward the C3 board entirely -
// end_support_w of solid material at each end is enough to hold the
// board, leaving the middle open for that routing.
slide_w = B_len + 0.5;        // slot width (X) - board length + clearance to insert easily
// Depth (Y) needs to clear the SENSOR CHIP soldered onto the board, not
// just the bare PCB - the chip adds real bulk beyond B_thk alone. +2.5mm
// (within the 2-3mm suggested) leaves a visible gap behind the board,
// which is intentional for now - to be filled later with a printed
// filler piece hanging from the lid, or soft fabric, either of which is
// easier to fit once the board's real resting position is confirmed
// than trying to guess the exact chip bulge now.
component_bulge_clearance = 2.5;
slide_depth = B_thk + component_bulge_clearance;
// Widened and recentered - the previous 4mm supports sat ENTIRELY
// inside the slot's width, so the slot completely hollowed them out
// sideways, leaving zero wall material to actually stop lateral
// movement (only the shallow Y-backstop behind them remained). Each
// support is now centered ON the slot's edge: half inside the slot's
// territory (still cut, still doing the Y-backstop job) and half
// OUTSIDE it, completely uncut - that outside half is the actual
// solid wall that stops the board sliding sideways.
end_support_w = 8;
// Must stay deeper than slide_depth or there's no backstop left at all -
// backstop_thickness is what actually remains solid behind the slot.
backstop_thickness = 2;
boss_depth = slide_depth + backstop_thickness;

// X-ranges of the two end supports, used by both the case (solid
// bosses + their slots) and the lid (relief cutouts so it can close).
function supportX0(side) = side == 0 ? (B_center_x() - slide_w/2 - end_support_w/2)
                                      : (B_center_x() + slide_w/2 - end_support_w/2);

// --- M2 heat-set insert corners ---
insert_hole_dia = 3.2;    // ASSUMED - verify against your insert
insert_hole_depth = 4;    // ASSUMED
corner_post_dia = 7;      // enough material around the insert for strength
corner_inset = 5;         // distance from each outer edge to post center
screw_clearance_dia = 2.4;  // M2 screw shaft clearance through the lid

// Minimum gap between a corner post's edge and the slide boss's edge -
// the box widens as needed to guarantee this.
min_post_track_gap = 2;

// --- Derived layout ---
front_zone = boss_depth + jst_clearance;
back_zone  = usb_stickout + 3;

function interiorW() = max(B_len + 2*clr, A_w + 2*jst_side_clearance + 2*clr);
function interiorLen() = front_zone + A_len + back_zone;
function interiorH() = max(B_w + gap_floor_to_boardB + clr,
                            A_thk + A_top_clearance);

// outerW must satisfy two independent requirements: fit the board
// footprints (boardDrivenW), AND keep the front corner posts clear of
// the slide supports, whose outer edge now extends end_support_w/2
// beyond the slot itself (that's the new solid wall material) - not
// just to the slot's own edge as before.
function boardDrivenW() = interiorW() + 2*wall;
function slideClearanceW() = 2 * (slide_w/2 + end_support_w/2 + min_post_track_gap +
                                   corner_inset + corner_post_dia/2);
function outerW() = max(boardDrivenW(), slideClearanceW());
function outerLen() = interiorLen() + 2*wall;
// outerH must also satisfy two independent requirements now: fit the
// components (componentDrivenH), AND be tall enough to actually contain
// the USB cutout at its measured real-world height - same "take
// whichever is larger" pattern as the width constraints above.
function componentDrivenH() = interiorH() + floorThk;
function usbDrivenH() = usb_socket_start_height + usb_cut_h + 2;  // +2mm clearance above the cutout before the lid
function outerH() = max(componentDrivenH(), usbDrivenH());

// The actual cavity/lid-lip must always span the FULL available
// width symmetrically - if outerW ends up wider than boardDrivenW
// (because slide clearance required it), using the narrower interiorW
// for the cavity would leave it off-center, and could even fail to
// reach the slide boss (now further out) at all.
function cavityW() = outerW() - 2*wall;

function A_y0() = wall + front_zone;
function A_x0() = (outerW() - A_w) / 2;

function B_center_x() = outerW() / 2;
function B_z0() = floorThk + gap_floor_to_boardB;
function sensorZOffset() = B_z0() + B_w/2;

// Corner post centers - same X,Y used by both the case (posts) and
// the lid (clearance holes), so they always line up.
function cornerX(side) = side == 0 ? corner_inset : outerW() - corner_inset;
function cornerY(side) = side == 0 ? corner_inset : outerLen() - corner_inset;

// ==================== Case body ====================

module case_box() {
  ow = outerW();
  ol = outerLen();
  oh = outerH();

  difference() {
    union() {
      color("LightSteelBlue")
      difference() {
        cube([ow, ol, oh]);

        translate([wall, wall, floorThk])
          cube([cavityW(), interiorLen(), oh]);

        translate([B_center_x(), -1, sensorZOffset()])
          rotate([-90, 0, 0])
          cylinder(d = sensor_dia, h = wall + 2);

        translate([A_x0() + A_w/2 - usb_cut_w/2,
                   ol - wall - 1,
                   usb_socket_start_height])
          cube([usb_cut_w, wall + 2, usb_cut_h]);

        translate([A_x0() - jst_side_clearance,
                   A_y0(),
                   floorThk - pin_protrusion])
          cube([A_w + 2*jst_side_clearance, A_len, pin_protrusion + 0.5]);
      }

      // Corner posts - solid, added after the cavity cut above
      color("Orange")
      for (xs = [0, 1])
        for (ys = [0, 1])
          translate([cornerX(xs), cornerY(ys), 0])
            cylinder(d = corner_post_dia, h = oh);

      // Board A retention: walls on LEFT, RIGHT, and FRONT.
      //
      // Height: now reaches A_top_clearance + A_thk above the pocket
      // floor (was just A_thk+1) - taller walls grip more of the
      // board's profile for firmer confinement, per request.
      //
      // Stop-wall side: moved from BACK to FRONT. The USB connector
      // needs to press firmly against the case's own back wall/cutout -
      // a stop at the back would leave room for the board to sit away
      // from that wall instead of flush against it. A stop at the
      // FRONT (sensor side) does the opposite job correctly: it blocks
      // the board from drifting forward, which is what keeps it pressed
      // back against the USB cutout. This also happens to be the
      // right side for the cable gap too, since the sensor's wires
      // approach from the front (interior channel), not the back.
      //
      // Overlap: the long left/right walls now extend PAST the front
      // wall's plane by wall_overlap, instead of just touching it
      // edge-to-edge - a real overlapping volume at the junction,
      // not a zero-width seam that prints/renders as a weak joint.
      retain_wall_thk = 1.5;
      // Height increased to reach close to the actual interior ceiling
      // (outerH), not just past the board's own thickness - the
      // previous height (A_top_clearance+A_thk above the pocket floor)
      // left roughly 10mm of unused headroom below the lid, since
      // interiorH() is actually driven by the sensor zone's needs
      // (B_w+gap+clr), not board A's much smaller zone. 3mm short of
      // the lid's underside keeps clear of the lid's lip (top 2mm)
      // without needing a relief cutout there.
      retain_wall_h = outerH() - 3 - (floorThk - pin_protrusion);
      // Both front-wall segments present, each reaching out to connect
      // with its own long wall - the right one restored (was wrongly
      // eliminated). Lengths tuned per color (unambiguous, unlike
      // left/right which kept getting crossed): Orange -2mm (3->1),
      // Turquoise +2mm (0.5->2.5).
      front_wall_end_w = 1;        // Orange segment's reach past the board's edge
      front_wall_end_w_r = 2.5;    // Turquoise segment's reach
      wall_overlap = 1;  // extra length pushed into a neighboring wall's
                          // territory at every junction, for a real
                          // overlap rather than a zero-width seam
      // Gap between the two long (left/right) walls narrowed by 3mm
      // total (1.5mm off each side) from the previous jst_side_clearance
      // push-out - that push was slightly more than needed. Kept
      // separate from jst_side_clearance itself, which still sizes the
      // underlying pocket cavity independently.
      wall_jst_clearance = jst_side_clearance - 1.5;

      // Left wall - pushed out by wall_jst_clearance beyond the board's
      // bare edge (JST connector clearance on the pin-row side)
      color("Red")
        translate([A_x0() - wall_jst_clearance - retain_wall_thk, A_y0() - wall_overlap, floorThk - pin_protrusion])
          cube([retain_wall_thk, A_len + wall_overlap, retain_wall_h]);

      // Right wall - same push-out, distinct color
      color("Crimson")
        translate([A_x0() + A_w + wall_jst_clearance, A_y0() - wall_overlap, floorThk - pin_protrusion])
          cube([retain_wall_thk, A_len + wall_overlap, retain_wall_h]);

      // Front wall, Orange segment - starts EXACTLY at the left wall's
      // own outer edge, not past it (the previous version added
      // wall_overlap here too, poking past the long wall's own
      // boundary, which wasn't needed - the segment already spans the
      // long wall's FULL width on its way to the board, which is
      // already far more overlap than a clean union needs).
      color("Orange")
        translate([A_x0() - wall_jst_clearance - retain_wall_thk,
                   A_y0() - retain_wall_thk,
                   floorThk - pin_protrusion])
          cube([wall_jst_clearance + retain_wall_thk + front_wall_end_w,
                retain_wall_thk, retain_wall_h]);

      // Front wall, Turquoise segment - same fix mirrored.
      color("Turquoise")
        translate([A_x0() + A_w - front_wall_end_w_r,
                   A_y0() - retain_wall_thk,
                   floorThk - pin_protrusion])
          cube([front_wall_end_w_r + wall_jst_clearance + retain_wall_thk,
                retain_wall_thk, retain_wall_h]);

      // Both slide end-supports restored - the v16 removal was based on
      // a misreading of "back blockers preventing the sensor going
      // down": that was actually about the C3 holder's stop-wall (fixed
      // below), not the sensor's own end-supports. Both are back and
      // colored distinctly for easy reference.
      color("Purple")
        translate([supportX0(0), wall, floorThk])
          cube([end_support_w, boss_depth, oh - floorThk]);
      color("Magenta")
        translate([supportX0(1), wall, floorThk])
          cube([end_support_w, boss_depth, oh - floorThk]);
    }

    // M2 insert holes - drilled into the FINAL combined shape, after
    // the posts already exist as solid material to drill into
    for (xs = [0, 1])
      for (ys = [0, 1])
        translate([cornerX(xs), cornerY(ys), oh - insert_hole_depth])
          cylinder(d = insert_hole_dia, h = insert_hole_depth + 1);

    // Slide slot cut - still one cut spanning the board's full width;
    // since only the two end-supports (added above) have any material
    // there now, this only actually notches those two - the open middle
    // has nothing to cut, which is harmless. Cut into the FINAL combined
    // shape, after the supports exist as solid material - same lesson
    // as the earlier two-track version, which had this cut nested
    // inside the shell's own inner difference() (before the supports
    // existed) and so removed nothing from them. Shallow (slide_depth,
    // board thickness + clearance) - NOT through the full boss_depth,
    // so solid material remains behind it at each end as the backstop
    // that stops the board sliding backward.
    translate([B_center_x() - slide_w/2,
               wall - 0.5,
               B_z0()])
      cube([slide_w, slide_depth + 0.5, oh]);
  }
}

// ==================== Lid ====================

module lid() {
  ow = outerW();
  ol = outerLen();
  lid_thk = wall;
  lip_h = 2;
  lip_clr = 0.25;

  difference() {
    union() {
      color("Tan") cube([ow, ol, lid_thk]);
      color("DarkOliveGreen")
        translate([wall + lip_clr, wall + lip_clr, -lip_h])
        cube([cavityW() - 2*lip_clr, interiorLen() - 2*lip_clr, lip_h]);
    }

    translate([A_x0() + A_w/2 - oled_window_w/2,
               A_y0() + oled_from_front_edge - oled_window_h/2,
               -lip_h - 1])
      cube([oled_window_w, oled_window_h, lid_thk + lip_h + 2]);

    // Screw holes through the TOP PLATE only, kept narrow (screw shaft)
    // so the screw head has a proper shoulder to seat against.
    for (xs = [0, 1])
      for (ys = [0, 1])
        translate([cornerX(xs), cornerY(ys), 0])
          cylinder(d = screw_clearance_dia, h = lid_thk + 1);

    // Wider relief through just the LIP (not the top plate) at each
    // corner post - without this, the lip's solid material occupies
    // the exact same space as the post's top, and the lid can't
    // physically close at all. corner_post_dia + margin, not just the
    // narrow screw hole above.
    post_relief_dia = corner_post_dia + 1;
    for (xs = [0, 1])
      for (ys = [0, 1])
        translate([cornerX(xs), cornerY(ys), -lip_h - 0.5])
          cylinder(d = post_relief_dia, h = lip_h + 1);

    // Same relief need at the two slide end-supports - their tops also
    // reach full case height, colliding with the lip the same way.
    support_relief_margin = 1;
    for (side = [0, 1])
      translate([supportX0(side) - support_relief_margin/2,
                 wall - support_relief_margin/2,
                 -lip_h - 0.5])
        cube([end_support_w + support_relief_margin,
              boss_depth + support_relief_margin,
              lip_h + 1]);
  }
}
