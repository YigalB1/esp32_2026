// ============================================================
// Train_ctrl_esp32_dev - enclosure
// ============================================================
// VERSION 8.6   (see VERSION below - echoed to the console on every
//                render and engraved on the underside of the base)
//
// CHANGELOG
//  v1.0  first draft: separate PCB standoffs and lid bosses, 3 mm LEDs.
//        (Superseded - standoffs and bosses collided at the corners.)
//  v2.0  corner holes became stepped combo posts (PCB + lid screw in
//        one); fixed USB slot not reaching the outer wall face.
//  v3.0  8 mm LED holes, 4 desk-mounting ears, lid inset into the box
//        with a 45-degree bevel, corner-hole inset as parameters.
//  v4.0  version number (echo + engraved), every part in its own color
//        (incl. the M2 posts), fixed "unknown variable CASE_EXT_Y"
//        warning (EAR_Y_POSITIONS was defined before the value it uses).
//  v5.0  FIX: the v2-v4 stepped corner post could not be assembled - its
//        upper fat section sat above the neck, so the PCB's 2 mm hole could
//        never pass over it. Lid screws are now separate bosses in the
//        corners of a box made longer (EXTRA_Y each end), clear of the PCB.
//        The PCB sits on 6 plain M2 standoffs (4 corners + 2 inner).
//        Desk ears reduced from 4 to 2, one centered on each side wall.
//  v6.0  DXF no longer used: every position is now a plain PCB-local number
//        you can edit directly. Corner-hole Y inset 3 -> 4. PCB standoffs
//        5 -> 10 mm tall and 4.5 -> 7 mm wide (the whole box is 5 mm taller;
//        all side-hole heights follow STANDOFF_HEIGHT). Both left-wall holes
//        (DC in + voltmeter tap) are now the same, larger size (9.5 mm).
//  v7.0  all M2 holes in the box (6 PCB standoffs + 4 lid bosses) are now
//        sized for M2 heat-set threaded inserts (previously small pilot
//        holes for screwing straight into the plastic): INSERT_* parameters
//        and an entry chamfer for the displaced plastic.
//  v8.0  (superseded by 8.1 - connector shapes were misread) round DC holes,
//        wider DC wall, panel micro-USB cutout + screws. Added: 30 mm
//        voltmeter hole in the lid, "hole test" plate.
//  v8.1  CORRECTION. DC jack holes are RECTANGLES, 9 wide x 12 tall (+0.4 mm
//        print clearance), resting on the PCB; the jack sticks 3 mm out past
//        the PCB edge, which is exactly WALL + MARGIN, so its front face is
//        flush with the wall's outside (box back to 126 mm wide). The USB
//        POWER INPUT is a single ROUND hole (measured 10.6 mm).
//        PART = "usb_hole_test" prints a plate with 4 holes
//        (10.4 / 10.6 / 10.8 / 11.0 mm) to pick the production size.
//        Kept from 8.0: 30 mm voltmeter hole in the lid.
//        TBD: programming over USB (see note at the USB parameters).
//        No on/off switch: power is switched on the external USB supply.
//  v8.2  TEST-FIT CORRECTIONS (PCB placed in the printed v8.1 box):
//        - both DC jack holes moved 4 mm toward the motor-out jack (+Y)
//        - the two PCB standoffs next to the DC wall moved 2 mm away from
//          that wall (+X) and 0.5 mm toward the DC-in side (-Y)
//        - FLIP_Y_FEATURES: the DXF is a MIRROR IMAGE of the real top view
//          (checked against the test-fit photo), so the DXF-derived Y
//          positions of the inner standoffs, the LEDs and the USB hole
//          were flipped end-for-end. Set FLIP_Y_FEATURES = false to undo.
//  v8.3  - the two PCB standoffs next to the DC wall, ADDITIONAL correction on
//          top of v8.2 (viewed from the USB wall looking at the DC wall):
//          2 mm to the right (+Y, toward the motor-out jack) and 0.5 mm back
//          toward the box center (+X). Totals vs v8.1: +2.5 mm in X, +1.5 mm in Y.
//        - box 20 mm taller (CLEARANCE_ABOVE_PCB 15 -> 35) for the voltmeter.
//          NOTE: the 8 mm LEDs now sit about 20 mm below the lid holes - they
//          need long leads or light pipes to reach up to the holes.
//        Other M2 positions: still TBD.
//  v8.4  - USB opening is now a RECTANGLE, 12 wide x 9 tall (was a 10.8 mm
//          round hole), centered 5 mm higher than the old round hole, so a
//          USB-C plug can go straight into the ESP32's socket, which sits
//          very close to the wall.
//        - ventilation holes at the motor driver (VENT_* parameters): 6 holes
//          through the front/back walls and 2 through the side walls. Cut
//          through the WALLS only, not through the standoffs inside.
//  v8.5  - LED retainer strip (PART = "led_strip"): ONE printed strip, glued
//          under the lid, that holds all 3 LEDs so they cannot be pushed
//          down. See the LED_FLANGE_* / STRIP_* parameters.
//  v8.6  - LED strip: two more glue pads, one between each pair of LEDs (4 in
//          total), for a stronger bond.
// ============================================================
// Two parts:
//  - base: open box with 2 desk-mounting ears, 6 PCB standoffs, 4 lid-screw
//    bosses (in the extra length beyond the PCB), and a 45-degree bevel
//    around the top opening.
//  - lid : flat plate with a matching 45-degree bevel. It sits INSIDE
//    the opening, flush with the rim, so from outside you see only the
//    top face and a thin diagonal seam - no thick wall line.
//
// PCB and lid are fastened independently:
//  - The PCB sits on 6 plain standoffs matching its M2 holes (4 corners +
//    2 inner). Each standoff takes an M2 heat-set insert, flush with its
//    top; an M2 screw goes down through the PCB hole into the insert.
//    Do this before the lid.
//  - The lid is held by 4 bosses in the corners of the extra length
//    (EXTRA_Y) added at each end of the box, so they never touch the PCB.
//    Each takes an M2 heat-set insert; M2 screws go down through the lid.
//  - Screws: M2 x 5 or M2 x 6 (PCB 1.6 mm / lid 2.4 mm + insert length).
//
// DIMENSIONS
//  Everything is a plain number in mm - edit freely. PCB-local coordinates
//  have their origin at the board's bottom-left corner (X along the 120 mm
//  side, Y along the 94 mm side). The DXF is no longer used: the inner
//  holes, LEDs and connector positions below were frozen from the last
//  DXF-based version (v5) and are now yours to tune.
//
// Other flagged assumptions (search "ASSUMPTION" / "CHECK"):
//  - CLEARANCE_ABOVE_PCB, USB_Z (height of the USB hole).
//  - JACK_CLEARANCE: the 9 x 12 jack holes get +0.4 mm so the housing slides
//    in after printing. Set it to 0 for the exact size.
//  - USB hole size: pick it with the PART = "usb_hole_test" plate.

// ============================================================
// GENERATE MODEL
// ============================================================
PART = "led_strip";       // "case" = box + lid + LED strip;  "led_strip" = the LED strip only;  "usb_hole_test" = round-hole test plate
ASSEMBLED = false;   // true = everything shown in place (lid on the base, strip under the lid); false = print layout
echo(str("train_ctrl_esp32_dev_case  v", VERSION, "  part=", PART));

if (PART == "usb_hole_test") {
    usb_hole_test();
} else if (PART == "led_strip") {
    led_strip();
} else {
    base();
    if (ASSEMBLED) {
        translate([0, 0, WALL + WALL_HEIGHT]) lid();
        translate([STRIP_X0, STRIP_Y0, WALL + WALL_HEIGHT - STRIP_T - STRIP_GAP_BELOW_LID]) led_strip();
    } else {
        // Lid flipped top-face-down (smooth visible face on the bed, bevel
        // faces up so nothing overhangs), placed beside the base.
        translate([0, 2 * CASE_EXT_Y + 15, LID_THICKNESS])
            rotate([180, 0, 0])
                lid();
        // LED strip, flat on the bed (glue pads face up - no supports needed).
        translate([0, 2 * CASE_EXT_Y + 30, 0])
            led_strip();
    }
}

// ============================================================
// PARAMETERS
// ============================================================

VERSION = "8.6";
ENGRAVE_VERSION = true;   // engrave "v<VERSION>" on the underside of the floor
ENGRAVE_DEPTH = 0.6;

// ---- Part colors (preview / 3MF only - no effect on printing) ----
C_SHELL     = "SlateGray";
C_LID       = "SteelBlue";
C_LIDBOSS   = "OrangeRed";   // 4 lid-screw bosses
C_STANDOFF  = "Gold";        // 6 PCB M2 standoffs (4 corners + 2 inner)
C_EAR       = "SeaGreen";    // desk-mounting ears
C_TEST      = "Orange";      // USB hole test plate
C_STRIP     = "MediumPurple"; // LED retainer strip

// ---- Board ----
BOARD_X = 120;
BOARD_Y = 94;
PCB_THICKNESS = 1.6;

// Corner M2 holes: center distance from the board edges
CORNER_INSET_X = 3;
CORNER_INSET_Y = 4;

// ---- Test-fit corrections (v8.2), all in mm ----
DC_HOLES_SHIFT_Y = 4;              // both DC jack holes: toward the motor-out jack (+Y)
DC_SIDE_STANDOFF_SHIFT_X = 2 + 0.5;  // the 2 PCB holes next to the DC wall: away from that wall (+X). v8.2: +2, v8.3: +0.5 more
DC_SIDE_STANDOFF_SHIFT_Y = -0.5 + 2; // ...along the wall (+Y = toward the motor-out jack). v8.2: -0.5, v8.3: +2 more
FLIP_Y_FEATURES = true;            // DXF is mirrored vs the real board: flip inner holes, LEDs, USB along Y.
                                    // CHECK: with the PCB in the box, the two inner PCB holes (not at a
                                    // corner) should line up with the two inner standoffs. If they already
                                    // lined up in the v8.1 box, set this to false.

// ---- DC jacks (both on the left wall) ----
JACK_OVERHANG = 3;        // measured: jack front face sticks out this far past the PCB edge
JACK_HEIGHT = 12;         // measured: body height; it rests on the PCB
JACK_WIDTH = 9;           // measured: body width (along the wall)
JACK_CLEARANCE = 0.4;     // added to width and height of the wall hole (print tolerance); 0 = exact size

// ---- Construction ----
WALL = 2;               // side wall / floor thickness
MARGIN = 1;              // clearance between PCB edge and inner wall face (all sides except DC)
MARGIN_DC_SIDE = JACK_OVERHANG - WALL;   // PCB edge -> inner face of the LEFT (DC) wall. The jacks
                                          // stick out JACK_OVERHANG past the PCB, so with this their
                                          // front faces end flush with the wall's outside and the
                                          // housing sits in a rectangular pocket through the wall.
LID_THICKNESS = 2.4;     // lid plate
LID_BEVEL = 1.0;         // 45-degree bevel size, lid + opening (wall rim keeps WALL - LID_BEVEL flat)
LID_GAP = 0.2;           // clearance between lid and opening (visible seam width)
STANDOFF_HEIGHT = 10;    // floor to PCB underside (box height follows this)
STANDOFF_OD = 7;         // PCB standoffs (all 6)

// ---- M2 heat-set insert holes (used by ALL 10 M2 holes in the box) ----
// Defaults follow a typical M2 insert spec: ~3.5 mm OD, 3-5 mm long, 3.2 mm hole,
// blind hole >= 1 mm deeper than the insert (room for the displaced plastic).
// Holes print slightly SMALLER than modeled - print one standoff as a test and
// adjust INSERT_HOLE_D to your insert (the insert should push in with light
// pressure and the melt should not squeeze out underneath).
INSERT_HOLE_D = 3.2;      // check your insert's datasheet
INSERT_LENGTH = 4;        // e.g. M2 x 4
INSERT_HOLE_DEPTH = INSERT_LENGTH + 1;
INSERT_CHAMFER = 0.4;     // 45-degree lead-in; catches plastic pushed up around the insert
                          // so the PCB / lid still sits flat on the standoff / boss

EXTRA_Y = 8;             // extra length added at EACH end (Y) so the lid bosses sit beyond
                          // the PCB. Only Y: the DC jack and USB are on the X walls and
                          // must stay close to them.

CLEARANCE_ABOVE_PCB = 15 + 20;  // PCB top to lid underside. Was 15; +20 mm for the height of the voltmeter.
                                 // (An 8 mm LED stands roughly 10-12 mm tall, the ESP32 on its headers about
                                 // as much - the voltmeter is now the tallest part.)

// Lid-screw bosses (4, in the corners of the extra length)
LID_BOSS_OD = 6;
LID_SCREW_CLEARANCE_D = 2.4;

// ---- Derived ----
PCB_OFFSET_X = WALL + MARGIN_DC_SIDE;       // PCB-local (0,0) -> case coords
PCB_OFFSET_Y = WALL + MARGIN + EXTRA_Y;
CASE_EXT_X = BOARD_X + (WALL + MARGIN_DC_SIDE) + (WALL + MARGIN);
CASE_EXT_Y = BOARD_Y + 2 * PCB_OFFSET_Y;
BOSS_INSET = WALL + LID_BOSS_OD / 2 - 0.3;  // boss center from the outer edges; fused 0.3 mm into the walls
IX = CASE_EXT_X - 2 * WALL;     // inner cavity
IY = CASE_EXT_Y - 2 * WALL;
WALL_HEIGHT = STANDOFF_HEIGHT + PCB_THICKNESS + CLEARANCE_ABOVE_PCB; // floor top -> lid underside
SHELL_H = WALL + WALL_HEIGHT + LID_THICKNESS;                        // total base height

// ---- Desk-mounting ears (2x: one centered on each side wall, flat at floor level) ----
// (defined AFTER the derived sizes above: top-level variables cannot reference later ones)
EAR_THICKNESS = 3;
EAR_LENGTH = 16;           // protrusion beyond the wall face
EAR_WIDTH = 14;
EAR_HOLE_D = 4.5;          // M4 / #8 screw clearance
EAR_Y_POSITIONS = [CASE_EXT_Y / 2];       // one ear per side wall, centered


// ---- Layout (PCB-local mm, origin = bottom-left corner of the board) ----
// PCB-local positions
corner_mount_holes = [
    [CORNER_INSET_X + DC_SIDE_STANDOFF_SHIFT_X,  CORNER_INSET_Y + DC_SIDE_STANDOFF_SHIFT_Y],   // next to the DC wall
    [BOARD_X - CORNER_INSET_X,                   CORNER_INSET_Y],
    [BOARD_X - CORNER_INSET_X,                   BOARD_Y - CORNER_INSET_Y],
    [CORNER_INSET_X + DC_SIDE_STANDOFF_SHIFT_X,  BOARD_Y - CORNER_INSET_Y + DC_SIDE_STANDOFF_SHIFT_Y],   // next to the DC wall
];

// The DXF is a mirror image of the real top view, so Y positions taken from it are flipped
// end-for-end (BOARD_Y - y) when FLIP_Y_FEATURES is true.
function fy(y) = FLIP_Y_FEATURES ? BOARD_Y - y : y;
lid_boss_xy = [
    [BOSS_INSET,               BOSS_INSET],
    [CASE_EXT_X - BOSS_INSET,  BOSS_INSET],
    [CASE_EXT_X - BOSS_INSET,  CASE_EXT_Y - BOSS_INSET],
    [BOSS_INSET,               CASE_EXT_Y - BOSS_INSET],
];
inner_mount_holes = [ for (p = [[39.67, 60.03], [51.23, 20.76]]) [p[0], fy(p[1])] ];
led_holes         = [ for (p = [[44.04, 83.87], [73.07, 83.61], [101.46, 84.39]]) [p[0], fy(p[1])] ];

LED_HOLE_D = 8.4;   // 8 mm LED + clearance for print tolerance

// ---- Voltmeter hole in the lid ----
VOLT_HOLE_D = 30;
VOLT_X = led_holes[1][0];     // centered under the MIDDLE LED (PCB-local X)...
VOLT_Y = BOARD_Y / 2;         // ...and in the middle of the PCB top-to-bottom (PCB-local Y)
                               // (use BOARD_X / 2 for VOLT_X if you want it at the exact PCB center)

// ---- LED retainer strip (v8.5) ----
// The LEDs are pushed up through the lid holes from below until their base flange meets the lid.
// This strip slides sideways in UNDER the three flanges (each lead slot is open at the front
// edge) and its two end pads are glued to the underside of the lid. The flange is then trapped
// between the lid (above) and the strip (below): the LED can't be pushed down or pulled out.
LED_FLANGE_D = 9.0;       // CHECK with calipers: the wide ring at the base of your 8 mm LEDs
LED_FLANGE_T = 1.0;       // CHECK: thickness of that ring
STRIP_FIT = 0.3;          // clearance between the flange and the lid / the strip
STRIP_T = 2.0;            // strip thickness
STRIP_SLOT_W = 5.5;       // lead slot width - must be clearly narrower than the flange
STRIP_PAD_L = 8;          // glue pad length at each end
STRIP_MID_PAD_L = 8;      // length of the 2 extra glue pads, one midway between each pair of LEDs
STRIP_MARGIN = 7;         // LED center to the start of a glue pad (x), must be >= flange radius + 2
STRIP_FRONT = 10;         // front edge of the strip, this far in front of the LEDs (slots open here)
STRIP_BACK = 3;           // strip material behind the flange
STRIP_GAP_BELOW_LID = LED_FLANGE_T + STRIP_FIT;   // top of the strip is this far below the lid underside

// ---- DC jack holes, left wall: RECTANGLES, 9 wide x 12 tall, resting on the PCB ----
DC_HOLE_W = JACK_WIDTH + JACK_CLEARANCE;             // along the wall
DC_HOLE_H = JACK_HEIGHT + JACK_CLEARANCE;            // vertical
DC_IN_Y = 77.51 + DC_HOLES_SHIFT_Y;                  // PCB-local Y of the hole center. NOTE: on the real board this (higher-Y) hole
                                                      // lines up with the MOTOR-OUT jack and the lower one with DC-in; both holes are identical.
DC_IN_Z = STANDOFF_HEIGHT + PCB_THICKNESS + JACK_HEIGHT / 2;   // jack sits on the PCB -> center is half its height up

CN2_Y = 14.41 + DC_HOLES_SHIFT_Y;                     // second jack hole (lines up with the DC-in jack on the real board)
CN2_Z = STANDOFF_HEIGHT + PCB_THICKNESS + JACK_HEIGHT / 2;

// ---- USB power input, right wall: RECTANGULAR opening, 12 wide x 9 tall ----
// Big enough for a USB-C plug with its overmold to go straight into the ESP32's socket
// (the socket sits very close to the wall). Exact size - no print clearance added.
// POWER ONLY for now. TBD: programming through this socket - needs a data-capable cable
// (check the one you have) and access to the devkit's BOOT/EN buttons if it needs them.
USB_HOLE_W = 12;                                      // along the wall
USB_HOLE_H = 9;                                       // vertical
USB_TEST_DIAMETERS = [10.4, 10.6, 10.8, 11.0];        // only for PART = "usb_hole_test" (round-hole test plate)
USB_Y = fy(65.91);                                    // PCB-local Y of the opening center (flipped like the other DXF positions)
USB_Z = STANDOFF_HEIGHT + PCB_THICKNESS + 9 + 5;      // opening center above the PCB: the old round hole was +9, now +5 higher

// ---- Ventilation holes (motor driver area), CASE coordinates in mm ----
// Same positions as the holes added in the test copy. Each set is cut straight through the box.
VENT_FB_X = [20, 40];          // front/back walls (cut along Y): X positions...
VENT_FB_Z = [10, 25, 40];      // ...and heights above the box bottom
VENT_FB_D = 6;                 // diameter (your code had 8 here; 6 as you said)
VENT_SIDE_Y = [100, 90];       // side walls (cut along X, through the left AND right wall): Y positions...
VENT_SIDE_Z = 40;              // ...and height
VENT_SIDE_D = 6;

// Strip extents in CASE coordinates (derived from the LED hole positions)
LEDS_X = [ for (p = led_holes) PCB_OFFSET_X + p[0] ];
LEDS_Y = [ for (p = led_holes) PCB_OFFSET_Y + p[1] ];
STRIP_X0 = min(LEDS_X) - STRIP_MARGIN - STRIP_PAD_L;
STRIP_X1 = max(LEDS_X) + STRIP_MARGIN + STRIP_PAD_L;
STRIP_Y0 = min(LEDS_Y) - STRIP_FRONT;
STRIP_Y1 = max(LEDS_Y) + LED_FLANGE_D / 2 + STRIP_BACK;

EPS = 0.2;

// ============================================================
// MODULES
// ============================================================

// Negative volume for one M2 heat-set insert, opening upward at local z = top.
// Flat-bottomed blind hole (3D printing) with a 45-degree entry chamfer.
module insert_hole(top) {
    translate([0, 0, top - INSERT_HOLE_DEPTH]) {
        cylinder(h = INSERT_HOLE_DEPTH + EPS, d = INSERT_HOLE_D, $fn = 32);
        translate([0, 0, INSERT_HOLE_DEPTH - INSERT_CHAMFER])
            cylinder(h = INSERT_CHAMFER + EPS, d1 = INSERT_HOLE_D,
                     d2 = INSERT_HOLE_D + 2 * (INSERT_CHAMFER + EPS), $fn = 32);
    }
}

module plain_standoff() {
    difference() {
        cylinder(h = STANDOFF_HEIGHT, d = STANDOFF_OD, $fn = 48);
        insert_hole(STANDOFF_HEIGHT);
    }
}

// Lid-screw boss: full height floor -> lid underside, pilot hole in the top.
// Local z=0 is the floor's top surface.
module lid_boss() {
    difference() {
        cylinder(h = WALL_HEIGHT, d = LID_BOSS_OD, $fn = 48);
        insert_hole(WALL_HEIGHT);
    }
}

// Test plate: one hole per size in USB_TEST_DIAMETERS, same thickness as the box wall.
// Round-hole test plate. The USB opening is rectangular now, so this is only for testing
// other round holes - set the sizes in USB_TEST_DIAMETERS.
module usb_hole_test() {
    n = len(USB_TEST_DIAMETERS);
    pitch = 16;
    edge = 8;
    plate_w = (n - 1) * pitch + 2 * edge;
    plate_h = 26;
    color(C_TEST)
        difference() {
            cube([plate_w, plate_h, WALL]);
            for (i = [0 : n - 1]) {
                translate([edge + i * pitch, 15, -EPS])
                    cylinder(h = WALL + 2 * EPS, d = USB_TEST_DIAMETERS[i], $fn = 64);
                translate([edge + i * pitch, 4.5, WALL - 0.6])
                    linear_extrude(0.7)
                        text(str(USB_TEST_DIAMETERS[i]), size = 3.2, halign = "center", valign = "center");
            }
        }
}

// LED retainer strip, in its own local frame: origin at its front-left bottom corner,
// bottom face on z = 0. Glue pads rise from the top face; in the assembled position their
// tops touch the underside of the lid and the strip's middle part sits STRIP_GAP_BELOW_LID below it.
module led_strip() {
    w = STRIP_X1 - STRIP_X0;
    d = STRIP_Y1 - STRIP_Y0;
    color(C_STRIP)
        difference() {
            union() {
                cube([w, d, STRIP_T]);
                cube([STRIP_PAD_L, d, STRIP_T + STRIP_GAP_BELOW_LID]);                    // left glue pad
                translate([w - STRIP_PAD_L, 0, 0])
                    cube([STRIP_PAD_L, d, STRIP_T + STRIP_GAP_BELOW_LID]);                // right glue pad
                // extra glue pads midway between neighbouring LEDs (LEDS_X is in ascending order)
                for (i = [0 : len(LEDS_X) - 2])
                    translate([(LEDS_X[i] + LEDS_X[i + 1]) / 2 - STRIP_X0 - STRIP_MID_PAD_L / 2, 0, 0])
                        cube([STRIP_MID_PAD_L, d, STRIP_T + STRIP_GAP_BELOW_LID]);
            }
            // one lead slot per LED, open at the front edge, rounded end centered on the LED
            for (i = [0 : len(LEDS_X) - 1])
                hull() {
                    translate([LEDS_X[i] - STRIP_X0, LEDS_Y[i] - STRIP_Y0, -EPS])
                        cylinder(h = STRIP_T + STRIP_GAP_BELOW_LID + 2 * EPS, d = STRIP_SLOT_W, $fn = 40);
                    translate([LEDS_X[i] - STRIP_X0, -1, -EPS])
                        cylinder(h = STRIP_T + STRIP_GAP_BELOW_LID + 2 * EPS, d = STRIP_SLOT_W, $fn = 40);
                }
        }
}

// Desk-mount ear pointing toward -X from the wall face at x=0,
// base at z=0. Mirror it for the right side.
module mounting_ear() {
    hole_x = EAR_LENGTH - EAR_WIDTH / 2;
    difference() {
        hull() {
            translate([0.5, -EAR_WIDTH / 2, 0])
                cube([WALL - 0.5, EAR_WIDTH, EAR_THICKNESS]);   // overlaps into the wall
            translate([-hole_x, 0, 0])
                cylinder(h = EAR_THICKNESS, d = EAR_WIDTH, $fn = 48);
        }
        translate([-hole_x, 0, -EPS])
            cylinder(h = EAR_THICKNESS + 2 * EPS, d = EAR_HOLE_D, $fn = 32);
    }
}

module base() {
    color(C_SHELL) shell();

    // PCB standoffs: all 6 holes (4 corners + 2 inner)
    color(C_STANDOFF)
        for (p = concat(corner_mount_holes, inner_mount_holes))
            translate([PCB_OFFSET_X + p[0], PCB_OFFSET_Y + p[1], WALL])
                plain_standoff();

    // Lid-screw bosses, in the extra length beyond the PCB
    color(C_LIDBOSS)
        for (p = lid_boss_xy)
            translate([p[0], p[1], WALL])
                lid_boss();

    // Desk-mounting ears, both side walls
    color(C_EAR)
        for (y = EAR_Y_POSITIONS) {
            translate([0, y, 0]) mounting_ear();
            translate([CASE_EXT_X, y, 0]) mirror([1, 0, 0]) mounting_ear();
        }
}

module shell() {
    difference() {
        cube([CASE_EXT_X, CASE_EXT_Y, SHELL_H]);

        // Cavity (open top)
        translate([WALL, WALL, WALL])
            cube([IX, IY, WALL_HEIGHT + LID_THICKNESS + EPS]);

        // 45-degree bevel around the top opening. Runs a hair past the
        // rim (EPS) while keeping the same slope, so the cut is clean.
        hull() {
            translate([WALL, WALL, SHELL_H - LID_BEVEL])
                cube([IX, IY, 0.01]);
            translate([WALL - LID_BEVEL - EPS, WALL - LID_BEVEL - EPS, SHELL_H + EPS])
                cube([IX + 2 * (LID_BEVEL + EPS), IY + 2 * (LID_BEVEL + EPS), 0.01]);
        }

        // Left wall: DC_IN + CN2, rectangular pockets for the jack housings
        translate([-EPS, PCB_OFFSET_Y + DC_IN_Y - DC_HOLE_W / 2, WALL + DC_IN_Z - DC_HOLE_H / 2])
            cube([WALL + 2 * EPS, DC_HOLE_W, DC_HOLE_H]);
        translate([-EPS, PCB_OFFSET_Y + CN2_Y - DC_HOLE_W / 2, WALL + CN2_Z - DC_HOLE_H / 2])
            cube([WALL + 2 * EPS, DC_HOLE_W, DC_HOLE_H]);

        // Right wall: rectangular USB opening
        translate([CASE_EXT_X - WALL - EPS, PCB_OFFSET_Y + USB_Y - USB_HOLE_W / 2, WALL + USB_Z - USB_HOLE_H / 2])
            cube([WALL + 2 * EPS, USB_HOLE_W, USB_HOLE_H]);

        // Ventilation holes. Cut in the SHELL only, so they open the walls but do not slice
        // through the standoffs and lid bosses inside (one of the holes at x = 40, z = 10 would
        // otherwise cut the inner standoff).
        for (x = VENT_FB_X)
            for (z = VENT_FB_Z)
                translate([x, -EPS, z])
                    rotate([-90, 0, 0])
                        cylinder(h = CASE_EXT_Y + 2 * EPS, d = VENT_FB_D, $fn = 40);
        for (y = VENT_SIDE_Y)
            translate([-EPS, y, VENT_SIDE_Z])
                rotate([0, 90, 0])
                    cylinder(h = CASE_EXT_X + 2 * EPS, d = VENT_SIDE_D, $fn = 40);

        // Version number, engraved into the underside of the floor.
        // Mirrored in X so it reads correctly when the base is flipped over.
        if (ENGRAVE_VERSION)
            translate([CASE_EXT_X / 2, CASE_EXT_Y / 2, -EPS])
                mirror([1, 0, 0])
                    linear_extrude(ENGRAVE_DEPTH + EPS)
                        text(str("v", VERSION), size = 6, halign = "center", valign = "center");
    }
}

// Lid in assembled orientation: z=0 is its underside, z=LID_THICKNESS its
// visible top face. XY uses the same case coordinates as base().
module lid() {
    color(C_LID) lid_body();
}

module lid_body() {
    lx = WALL + LID_GAP;                  // lower body, inset by the fit gap
    ly = WALL + LID_GAP;
    lw = IX - 2 * LID_GAP;
    ld = IY - 2 * LID_GAP;
    straight_h = LID_THICKNESS - LID_BEVEL;

    difference() {
        union() {
            // straight lower body
            translate([lx, ly, 0])
                cube([lw, ld, straight_h + 0.01]);
            // 45-degree bevel, widening to the visible top face
            hull() {
                translate([lx, ly, straight_h])
                    cube([lw, ld, 0.01]);
                translate([lx - LID_BEVEL, ly - LID_BEVEL, LID_THICKNESS - 0.01])
                    cube([lw + 2 * LID_BEVEL, ld + 2 * LID_BEVEL, 0.01]);
            }
        }

        // 8 mm LED holes
        for (p = led_holes)
            translate([PCB_OFFSET_X + p[0], PCB_OFFSET_Y + p[1], -EPS])
                cylinder(h = LID_THICKNESS + 2 * EPS, d = LED_HOLE_D, $fn = 48);

        // Voltmeter hole
        translate([PCB_OFFSET_X + VOLT_X, PCB_OFFSET_Y + VOLT_Y, -EPS])
            cylinder(h = LID_THICKNESS + 2 * EPS, d = VOLT_HOLE_D, $fn = 96);

        // Lid screw clearance holes over the 4 lid bosses
        for (p = lid_boss_xy)
            translate([p[0], p[1], -EPS])
                cylinder(h = LID_THICKNESS + 2 * EPS, d = LID_SCREW_CLEARANCE_D, $fn = 16);
    }
}
