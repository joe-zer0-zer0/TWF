#include "categories.h"

// ============================================================
// Blind Flight — Whiskey Category Data (Session 12, updated Session 21)
// ============================================================
// ~120 distilleries, ~650+ products across 9 whiskey types.
// All rye products consolidated under American Rye.
//
// Session 21: Converted product arrays from const char* to
// LibraryEntry structs with proof, price tier, and age metadata.
// Metadata populated for well-known products; unknowns use 0.
// Price tiers: 1=$ (<$30), 2=$$ ($30-60), 3=$$$ ($60-120), 4=$$$$ ($120+)
// ============================================================

// ============================================================
// BOURBON
// ============================================================

static const LibraryEntry p_bbn_buffalo_trace[] = {
    {"Buffalo Trace",        90,  1, 0},
    {"Eagle Rare 10yr",      90,  2, 10},
    {"Blanton's Original",   93,  3, 0},
    {"W.L. Weller Special",  90,  2, 0},
    {"W.L. Weller 107",     107,  2, 0},
    {"W.L. Weller 12yr",     90,  3, 12},
    {"E.H. Taylor SmB",     100,  2, 0},
    {"E.H. Taylor SiB",     100,  3, 0},
    {"Stagg Jr",              0,  3, 0},
    {"Elmer T. Lee SiB",     90,  2, 0},
    {"Benchmark No. 8",      80,  1, 0},
};
static const LibraryEntry p_bbn_heaven_hill[] = {
    {"Elijah Craig SmB",     94,  2, 0},
    {"Elijah Craig BP",       0,  3, 0},
    {"Elijah Craig Toasted", 94,  2, 0},
    {"Evan Williams Black",  86,  1, 0},
    {"Evan Williams BiB",   100,  1, 0},
    {"Evan Williams 1783",   86,  1, 0},
    {"Evan Williams SiB",    87,  2, 0},
    {"Larceny SmB",          92,  1, 0},
    {"Larceny BP",            0,  3, 0},
    {"Henry McKenna 10yr",  100,  2, 10},
    {"Old Fitzgerald BiB",  100,  3, 0},
    {"Fighting Cock",       103,  1, 0},
    {"Heaven Hill BiB",     100,  1, 0},
};
static const LibraryEntry p_bbn_jim_beam[] = {
    {"Jim Beam White",       80,  1, 0},
    {"Jim Beam Black",       86,  1, 0},
    {"Jim Beam Double Oak",  86,  1, 0},
    {"Knob Creek 9yr",      100,  2, 9},
    {"Knob Creek 12yr",     100,  3, 12},
    {"Knob Creek SiB",      120,  2, 9},
    {"Booker's",              0,  3, 0},
    {"Baker's 7yr SiB",     107,  3, 7},
    {"Basil Hayden's",       80,  2, 0},
    {"Old Grand-Dad 114",   114,  1, 0},
    {"Old Grand-Dad BiB",   100,  1, 0},
    {"Little Book",           0,  4, 0},
    {"Legent",               94,  2, 0},
};
static const LibraryEntry p_bbn_wild_turkey[] = {
    {"Wild Turkey 101",     101,  1, 0},
    {"Wild Turkey 81",       81,  1, 0},
    {"Rare Breed",            0,  2, 0},
    {"Russell's Res 10yr",   90,  2, 10},
    {"Russell's Res SiB",   110,  3, 0},
    {"Kentucky Spirit",     101,  3, 0},
    {"Longbranch",           86,  2, 0},
};
static const LibraryEntry p_bbn_woodford[] = {
    {"Woodford Reserve",     90,  2, 0},
    {"Woodford Double Oak",  90,  3, 0},
    {"Woodford Batch Proof",  0,  4, 0},
    {"Woodford Wheat",       90,  2, 0},
    {"Woodford Malt",        90,  2, 0},
};
static const LibraryEntry p_bbn_makers[] = {
    {"Maker's Mark",         90,  1, 0},
    {"Maker's 46",           94,  2, 0},
    {"Maker's Cask Str",      0,  3, 0},
    {"Maker's Private Sel",   0,  3, 0},
    {"Maker's Cellar Aged",   0,  4, 0},
    {"Maker's FAE-01",        0,  3, 0},
    {"Maker's FAE-02",        0,  3, 0},
};
static const LibraryEntry p_bbn_four_roses[] = {
    {"Four Roses Yellow",    80,  1, 0},
    {"Four Roses SmB",       90,  2, 0},
    {"Four Roses SmB Sel",  104,  3, 0},
    {"Four Roses SiB",      100,  2, 0},
    {"Four Roses SiB BS",     0,  3, 0},
};
static const LibraryEntry p_bbn_old_forester[] = {
    {"Old Forester 86",      86,  1, 0},
    {"Old Forester 100",    100,  1, 0},
    {"Old Forester 1870",    90,  2, 0},
    {"Old Forester 1897",   100,  2, 0},
    {"Old Forester 1910",    93,  3, 0},
    {"Old Forester 1920",   115,  3, 0},
    {"Old Forester SiB",      0,  3, 0},
};
static const LibraryEntry p_bbn_angels_envy[] = {
    {"Angel's Envy Bourbon", 87,  3, 0},
    {"Angel's Envy Cask Str", 0,  4, 0},
};
static const LibraryEntry p_bbn_michters[] = {
    {"Michter's US*1 Brbn",  91,  2, 0},
    {"Michter's Toasted",    91,  3, 0},
    {"Michter's US*1 SrMsh", 91,  2, 0},
    {"Michter's 10yr Brbn",  94,  4, 10},
};
static const LibraryEntry p_bbn_bulleit[] = {
    {"Bulleit Bourbon",      90,  1, 0},
    {"Bulleit 10yr",         91,  2, 10},
    {"Bulleit Barrel Str",    0,  3, 0},
    {"Bulleit BiB",         100,  2, 0},
};
static const LibraryEntry p_bbn_1792[] = {
    {"1792 Small Batch",     94,  2, 0},
    {"1792 Full Proof",     125,  2, 0},
    {"1792 Single Barrel",   99,  2, 0},
    {"1792 BiB",            100,  2, 0},
    {"1792 Sweet Wheat",     91,  2, 0},
    {"1792 Port Finish",     89,  2, 0},
};
static const LibraryEntry p_bbn_bardstown[] = {
    {"Bardstown Discovery",   0,  4, 0},
    {"Bardstown Fusion",      0,  3, 0},
    {"Bardstown Origin",      0,  3, 0},
    {"Bardstown Collab",      0,  4, 0},
};
static const LibraryEntry p_bbn_willet[] = {
    {"Willet Pot Still",     94,  2, 0},
    {"Noah's Mill",         114,  3, 0},
    {"Rowan's Creek",       100,  2, 0},
    {"Johnny Drum",          0,  2, 0},
};
static const LibraryEntry p_bbn_new_riff[] = {
    {"New Riff Bourbon",    100,  2, 0},
    {"New Riff SiB",          0,  3, 0},
    {"New Riff Backsetter",   0,  2, 0},
    {"New Riff Maltster",     0,  2, 0},
};
static const LibraryEntry p_bbn_wilderness[] = {
    {"WT Bourbon BiB",      100,  2, 0},
    {"WT SiB Bourbon",        0,  3, 0},
    {"WT Wheated BiB",      100,  2, 0},
};
static const LibraryEntry p_bbn_peerless[] = {
    {"Peerless Bourbon",      0,  3, 0},
    {"Peerless SmB",          0,  3, 0},
    {"Peerless SiB",          0,  4, 0},
    {"Peerless Double Oak",   0,  4, 0},
};
static const LibraryEntry p_bbn_rabbit_hole[] = {
    {"Dareringer Bourbon",   93,  3, 0},
    {"Cavehill Bourbon",     95,  3, 0},
    {"Heigold Bourbon",      95,  3, 0},
};
static const LibraryEntry p_bbn_castle_key[] = {
    {"Castle & Key SmB",      0,  2, 0},
    {"Roots of Ruin",         0,  2, 0},
};
static const LibraryEntry p_bbn_lux_row[] = {
    {"Ezra Brooks 99",      99,  1, 0},
    {"Ezra Brooks SiB",      0,  2, 0},
    {"Rebel 100",           100,  1, 0},
    {"Rebel SiB",             0,  2, 0},
    {"Blood Oath",            0,  4, 0},
    {"David Nicholson 1843",100,  2, 0},
    {"David Nicholson Res",  0,  2, 0},
};
static const LibraryEntry p_bbn_garrison[] = {
    {"Garrison SmB",          0,  3, 0},
    {"Garrison SiB",          0,  4, 0},
    {"Garrison Balmorhea",    0,  4, 0},
    {"Garrison Cowboy",       0,  4, 0},
    {"Garrison HoneyDew",     0,  3, 0},
};
static const LibraryEntry p_bbn_stellum[] = {
    {"Stellum Bourbon",       0,  3, 0},
    {"Stellum Equinox",       0,  3, 0},
    {"Stellum SiB",           0,  3, 0},
};
static const LibraryEntry p_bbn_smoke_wagon[] = {
    {"Smoke Wagon SmB",       0,  2, 0},
    {"Smoke Wagon Uncut",     0,  3, 0},
    {"Smoke Wagon Str Brbn",  0,  2, 0},
};
static const LibraryEntry p_bbn_starlight[] = {
    {"Starlight Bourbon",     0,  3, 0},
    {"Starlight Double Oak",  0,  3, 0},
    {"Starlight Carl T.",     0,  3, 0},
};
static const LibraryEntry p_bbn_penelope[] = {
    {"Penelope Bourbon",      0,  2, 0},
    {"Penelope Barrel Str",   0,  3, 0},
    {"Penelope Four Grain",   0,  2, 0},
    {"Penelope Architect",    0,  3, 0},
    {"Penelope Toasted",      0,  2, 0},
};
static const LibraryEntry p_bbn_jeffersons[] = {
    {"Jefferson's SmB",      82,  2, 0},
    {"Jefferson's Reserve",  90,  3, 0},
    {"Jefferson's Ocean",    90,  3, 0},
    {"Jefferson's Grand Sel", 0,  4, 0},
};
static const LibraryEntry p_bbn_town_branch[] = {
    {"Town Branch Bourbon",  80,  2, 0},
    {"Town Branch SiB",       0,  3, 0},
};
static const LibraryEntry p_bbn_calumet[] = {
    {"Calumet Farm Bourbon",  0,  2, 0},
    {"Calumet Farm SmB",      0,  3, 0},
    {"Calumet Srk Black",     0,  3, 0},
};
static const LibraryEntry p_bbn_belle_meade[] = {
    {"Belle Meade Bourbon",  90,  2, 0},
    {"Belle Meade Reserve",   0,  3, 0},
    {"Belle Meade SiB",       0,  3, 0},
    {"Belle Meade Cask Str",  0,  3, 0},
};
static const LibraryEntry p_bbn_ky_owl[] = {
    {"KY Owl Bourbon",        0,  4, 0},
    {"KY Owl Confiscated",    0,  4, 0},
    {"KY Owl Takumi",         0,  4, 0},
};
static const LibraryEntry p_bbn_widow_jane[] = {
    {"Widow Jane 10yr",      91,  3, 10},
    {"Widow Jane Baby Jane",  0,  3, 0},
    {"Widow Jane The Vaults", 0,  4, 0},
    {"Widow Jane Oak&Apple",  0,  3, 0},
};
static const LibraryEntry p_bbn_yellowstone[] = {
    {"Yellowstone Select",   93,  2, 0},
    {"Yellowstone Ltd Ed",    0,  3, 0},
    {"Yellowstone 150th",     0,  4, 0},
};
static const LibraryEntry p_bbn_pinhook[] = {
    {"Pinhook Bourbon War",   0,  2, 0},
};
static const LibraryEntry p_bbn_frey_ranch[] = {
    {"Frey Ranch Bourbon",   90,  3, 0},
    {"Frey Ranch SiB",        0,  3, 0},
};
static const LibraryEntry p_bbn_blue_run[] = {
    {"Blue Run High Rye",     0,  4, 0},
    {"Blue Run Reflection",   0,  4, 0},
};
static const LibraryEntry p_bbn_mb_roland[] = {
    {"MB Roland Bourbon",     0,  2, 0},
    {"MB Roland Dark Fired",  0,  2, 0},
};
static const LibraryEntry p_bbn_redemption[] = {
    {"Redemption Wheated",    0,  2, 0},
};

#define DISTILLERY(name, arr) { name, arr, sizeof(arr)/sizeof(arr[0]) }

static const WhiskeyDistillery d_bourbon[] = {
    DISTILLERY("Buffalo Trace",       p_bbn_buffalo_trace),
    DISTILLERY("Heaven Hill",         p_bbn_heaven_hill),
    DISTILLERY("Jim Beam",            p_bbn_jim_beam),
    DISTILLERY("Wild Turkey",         p_bbn_wild_turkey),
    DISTILLERY("Woodford Reserve",    p_bbn_woodford),
    DISTILLERY("Maker's Mark",        p_bbn_makers),
    DISTILLERY("Four Roses",          p_bbn_four_roses),
    DISTILLERY("Old Forester",        p_bbn_old_forester),
    DISTILLERY("Angel's Envy",        p_bbn_angels_envy),
    DISTILLERY("Michter's",           p_bbn_michters),
    DISTILLERY("Bulleit",             p_bbn_bulleit),
    DISTILLERY("Barton 1792",         p_bbn_1792),
    DISTILLERY("Bardstown Bourbon",   p_bbn_bardstown),
    DISTILLERY("Willet",              p_bbn_willet),
    DISTILLERY("New Riff",            p_bbn_new_riff),
    DISTILLERY("Wilderness Trail",    p_bbn_wilderness),
    DISTILLERY("KY Peerless",         p_bbn_peerless),
    DISTILLERY("Rabbit Hole",         p_bbn_rabbit_hole),
    DISTILLERY("Castle & Key",        p_bbn_castle_key),
    DISTILLERY("Lux Row",             p_bbn_lux_row),
    DISTILLERY("Garrison Brothers",   p_bbn_garrison),
    DISTILLERY("Stellum",             p_bbn_stellum),
    DISTILLERY("Smoke Wagon",         p_bbn_smoke_wagon),
    DISTILLERY("Starlight",           p_bbn_starlight),
    DISTILLERY("Penelope",            p_bbn_penelope),
    DISTILLERY("Jefferson's",         p_bbn_jeffersons),
    DISTILLERY("Town Branch",         p_bbn_town_branch),
    DISTILLERY("Calumet Farm",        p_bbn_calumet),
    DISTILLERY("Belle Meade",         p_bbn_belle_meade),
    DISTILLERY("Kentucky Owl",        p_bbn_ky_owl),
    DISTILLERY("Widow Jane",          p_bbn_widow_jane),
    DISTILLERY("Yellowstone",         p_bbn_yellowstone),
    DISTILLERY("Pinhook",             p_bbn_pinhook),
    DISTILLERY("Frey Ranch",          p_bbn_frey_ranch),
    DISTILLERY("Blue Run",            p_bbn_blue_run),
    DISTILLERY("MB Roland",           p_bbn_mb_roland),
    DISTILLERY("Redemption",          p_bbn_redemption),
};

// ============================================================
// TENNESSEE WHISKEY
// ============================================================

static const LibraryEntry p_tn_jack[] = {
    {"Old No. 7",            80,  1, 0},
    {"Gentleman Jack",       80,  2, 0},
    {"Single Barrel Sel",   100,  3, 0},
    {"Single Barrel BP",      0,  3, 0},
    {"Sinatra Select",       90,  4, 0},
    {"Tennessee Rye",        90,  1, 0},
    {"Bonded",              100,  2, 0},
};
static const LibraryEntry p_tn_dickel[] = {
    {"Dickel No. 8",         80,  1, 0},
    {"Dickel No. 12",        90,  1, 0},
    {"Dickel BiB",          100,  2, 0},
    {"Dickel Barrel Select",  0,  2, 0},
    {"Dickel Single Barrel",  0,  3, 0},
};
static const LibraryEntry p_tn_uncle_nearest[] = {
    {"Uncle Nearest 1856",   93,  3, 0},
    {"Uncle Nearest 1884",   93,  2, 0},
    {"Uncle Nearest SiB",     0,  3, 0},
    {"Uncle Nearest Master",  0,  4, 0},
};
static const LibraryEntry p_tn_nelsons[] = {
    {"Nelson's First 108",  108,  2, 0},
    {"Nelson's Reserve",      0,  2, 0},
    {"Nelson's Classic",      0,  2, 0},
};
static const LibraryEntry p_tn_chattanooga[] = {
    {"Chattanooga 91",       91,  2, 0},
    {"Chattanooga 111",     111,  2, 0},
    {"Chattanooga Cask",      0,  3, 0},
    {"Chattanooga SiB",       0,  3, 0},
};
static const LibraryEntry p_tn_corsair[] = {
    {"Corsair Triple Smoke",  0,  2, 0},
    {"Corsair Ryemaggedon",   0,  2, 0},
};
static const LibraryEntry p_tn_popcorn[] = {
    {"Popcorn Sutton SmB",    0,  2, 0},
    {"Popcorn Sutton SiB",    0,  3, 0},
};

static const WhiskeyDistillery d_tennessee[] = {
    DISTILLERY("Jack Daniel's",      p_tn_jack),
    DISTILLERY("George Dickel",      p_tn_dickel),
    DISTILLERY("Uncle Nearest",      p_tn_uncle_nearest),
    DISTILLERY("Nelson's Green Br",  p_tn_nelsons),
    DISTILLERY("Chattanooga",        p_tn_chattanooga),
    DISTILLERY("Corsair",            p_tn_corsair),
    DISTILLERY("Popcorn Sutton",     p_tn_popcorn),
};

// ============================================================
// AMERICAN RYE
// ============================================================

static const LibraryEntry p_rye_whistlepig[] = {
    {"WhistlePig 6yr",      100,  3, 6},
    {"WhistlePig 10yr",     100,  3, 10},
    {"WhistlePig 12yr",      86,  4, 12},
    {"WhistlePig 15yr",      92,  4, 15},
    {"WhistlePig 18yr",      92,  4, 18},
    {"WhistlePig PiggyBack",96,  2, 0},
    {"WhistlePig Boss Hog",   0,  4, 0},
};
static const LibraryEntry p_rye_sazerac[] = {
    {"Sazerac Rye 6yr",      90,  1, 6},
    {"Thomas H. Handy",       0,  4, 0},
};
static const LibraryEntry p_rye_high_west[] = {
    {"High West Double Rye", 92,  2, 0},
    {"High West Rendezvous",92,  3, 0},
    {"High West Bourye",      0,  3, 0},
    {"High West Midwinter",   0,  4, 0},
};
static const LibraryEntry p_rye_pikesville[] = {
    {"Pikesville 110",      110,  2, 0},
};
static const LibraryEntry p_rye_rittenhouse[] = {
    {"Rittenhouse BiB",     100,  1, 0},
};
static const LibraryEntry p_rye_old_overholt[] = {
    {"Old Overholt",         80,  1, 0},
    {"Old Overholt BiB",    100,  1, 0},
    {"Old Overholt 114",    114,  1, 0},
};
static const LibraryEntry p_rye_sagamore[] = {
    {"Sagamore Signature",   83,  2, 0},
    {"Sagamore Cask Str",     0,  3, 0},
    {"Sagamore Double Oak",  83,  2, 0},
    {"Sagamore Barrel Sel",   0,  3, 0},
};
static const LibraryEntry p_rye_templeton[] = {
    {"Templeton 4yr",        80,  2, 4},
    {"Templeton 6yr",        91,  2, 6},
    {"Templeton 10yr",      104,  3, 10},
    {"Templeton Barrel Str",  0,  3, 0},
};
static const LibraryEntry p_rye_few[] = {
    {"FEW Straight Rye",     93,  2, 0},
    {"FEW Single Barrel",     0,  3, 0},
};
static const LibraryEntry p_rye_barrell[] = {
    {"Barrell Rye",           0,  3, 0},
    {"Barrell Seagrass",      0,  3, 0},
    {"Barrell Dovetail",      0,  3, 0},
    {"Barrell Vantage",       0,  3, 0},
    {"Barrell Gold Label",    0,  4, 0},
};
static const LibraryEntry p_rye_willet[] = {
    {"Willet Family Rye",     0,  4, 0},
    {"Willet 4yr Rye",      110,  3, 4},
};
static const LibraryEntry p_rye_dads_hat[] = {
    {"Dad's Hat Rye",        90,  2, 0},
    {"Dad's Hat Vermouth",   94,  2, 0},
};
static const LibraryEntry p_rye_lock_stock[] = {
    {"Lock Stock 13yr",       0,  4, 13},
    {"Lock Stock 16yr",       0,  4, 16},
    {"Lock Stock 18yr",       0,  4, 18},
    {"Lock Stock 20yr",       0,  4, 20},
};
// Rye products moved from bourbon distilleries:
static const LibraryEntry p_rye_wild_turkey[] = {
    {"Wild Turkey 101 Rye", 101,  1, 0},
    {"Russell's Res Rye",    90,  2, 0},
};
static const LibraryEntry p_rye_woodford[] = {
    {"Woodford Reserve Rye", 90,  2, 0},
};
static const LibraryEntry p_rye_bulleit[] = {
    {"Bulleit Rye",          90,  1, 0},
};
static const LibraryEntry p_rye_angels_envy[] = {
    {"Angel's Envy Rye",    100,  4, 0},
};
static const LibraryEntry p_rye_michters[] = {
    {"Michter's US*1 Rye",   85,  2, 0},
};
static const LibraryEntry p_rye_knob_creek[] = {
    {"Knob Creek Rye 7yr",  100,  2, 7},
    {"Knob Creek SiB Rye",  115,  3, 0},
};
static const LibraryEntry p_rye_old_forester[] = {
    {"Old Forester Rye",    100,  1, 0},
};
static const LibraryEntry p_rye_new_riff[] = {
    {"New Riff Rye",        100,  2, 0},
};
static const LibraryEntry p_rye_peerless[] = {
    {"Peerless Rye",          0,  3, 0},
};
static const LibraryEntry p_rye_rabbit_hole[] = {
    {"Boxergrail Rye",       95,  3, 0},
};
static const LibraryEntry p_rye_castle_key[] = {
    {"Restoration Rye",       0,  2, 0},
};
static const LibraryEntry p_rye_redemption[] = {
    {"Redemption High Rye",   0,  2, 0},
    {"Redemption BP Rye",     0,  3, 0},
};
static const LibraryEntry p_rye_wilderness[] = {
    {"WT Rye BiB",          100,  2, 0},
};
static const LibraryEntry p_rye_town_branch[] = {
    {"Town Branch Rye",      0,  2, 0},
};
static const LibraryEntry p_rye_frey_ranch[] = {
    {"Frey Ranch Rye",       90,  3, 0},
};
static const LibraryEntry p_rye_pinhook[] = {
    {"Pinhook Rye Humor",     0,  2, 0},
};
static const LibraryEntry p_rye_blue_run[] = {
    {"Blue Run Golden Rye",   0,  4, 0},
    {"Blue Run Emerald Rye",  0,  4, 0},
};
static const LibraryEntry p_rye_four_roses[] = {
    {"Four Roses SiB Rye",    0,  3, 0},
};

static const WhiskeyDistillery d_rye[] = {
    DISTILLERY("WhistlePig",        p_rye_whistlepig),
    DISTILLERY("Sazerac",           p_rye_sazerac),
    DISTILLERY("High West",         p_rye_high_west),
    DISTILLERY("Pikesville",        p_rye_pikesville),
    DISTILLERY("Rittenhouse",       p_rye_rittenhouse),
    DISTILLERY("Old Overholt",      p_rye_old_overholt),
    DISTILLERY("Sagamore Spirit",   p_rye_sagamore),
    DISTILLERY("Templeton",         p_rye_templeton),
    DISTILLERY("FEW Spirits",       p_rye_few),
    DISTILLERY("Barrell Craft",     p_rye_barrell),
    DISTILLERY("Willet",            p_rye_willet),
    DISTILLERY("Dad's Hat",         p_rye_dads_hat),
    DISTILLERY("Lock Stock&Barrel", p_rye_lock_stock),
    DISTILLERY("Wild Turkey",       p_rye_wild_turkey),
    DISTILLERY("Woodford Reserve",  p_rye_woodford),
    DISTILLERY("Bulleit",           p_rye_bulleit),
    DISTILLERY("Angel's Envy",      p_rye_angels_envy),
    DISTILLERY("Michter's",         p_rye_michters),
    DISTILLERY("Knob Creek",        p_rye_knob_creek),
    DISTILLERY("Old Forester",      p_rye_old_forester),
    DISTILLERY("New Riff",          p_rye_new_riff),
    DISTILLERY("KY Peerless",       p_rye_peerless),
    DISTILLERY("Rabbit Hole",       p_rye_rabbit_hole),
    DISTILLERY("Castle & Key",      p_rye_castle_key),
    DISTILLERY("Redemption",        p_rye_redemption),
    DISTILLERY("Wilderness Trail",  p_rye_wilderness),
    DISTILLERY("Town Branch",       p_rye_town_branch),
    DISTILLERY("Frey Ranch",        p_rye_frey_ranch),
    DISTILLERY("Pinhook",           p_rye_pinhook),
    DISTILLERY("Blue Run",          p_rye_blue_run),
    DISTILLERY("Four Roses",        p_rye_four_roses),
};

// ============================================================
// SCOTCH — SINGLE MALT
// ============================================================

static const LibraryEntry p_sc_glenfiddich[] = {
    {"Glenfiddich 12yr",     80,  2, 12},
    {"Glenfiddich 14yr",     86,  3, 14},
    {"Glenfiddich 15yr",     80,  3, 15},
    {"Glenfiddich 18yr",     80,  3, 18},
    {"Glenfiddich 21yr",     80,  4, 21},
    {"Glenfiddich Fire&Cane",86,  3, 0},
    {"Glenfiddich IPA Cask", 86,  2, 0},
};
static const LibraryEntry p_sc_glenlivet[] = {
    {"Glenlivet 12yr",       80,  2, 12},
    {"Glenlivet 14yr",       80,  2, 14},
    {"Glenlivet 15yr",       80,  3, 15},
    {"Glenlivet 18yr",       86,  3, 18},
    {"Glenlivet 21yr",       86,  4, 21},
    {"Glenlivet Nadurra",     0,  3, 0},
    {"Glenlivet Caribbean",  80,  2, 0},
};
static const LibraryEntry p_sc_macallan[] = {
    {"Macallan 12 Sherry",   86,  3, 12},
    {"Macallan 12 Dbl Cask", 86,  3, 12},
    {"Macallan 15 Dbl Cask", 86,  4, 15},
    {"Macallan 18 Sherry",   86,  4, 18},
    {"Macallan 18 Dbl Cask", 86,  4, 18},
    {"Macallan Rare Cask",   86,  4, 0},
    {"Macallan Classic Cut",  0,  4, 0},
};
static const LibraryEntry p_sc_balvenie[] = {
    {"Balvenie 12 DblWood",  86,  3, 12},
    {"Balvenie 14 Carib",    86,  3, 14},
    {"Balvenie 17 DblWood",  86,  4, 17},
    {"Balvenie 21 PortWood", 86,  4, 21},
    {"Balvenie 25yr",        86,  4, 25},
};
static const LibraryEntry p_sc_aberlour[] = {
    {"Aberlour 12yr",        86,  2, 12},
    {"Aberlour 12 NCF",      96,  3, 12},
    {"Aberlour 16yr",        80,  3, 16},
    {"Aberlour A'Bunadh",     0,  3, 0},
    {"Aberlour Casg Annamh", 96,  3, 0},
};
static const LibraryEntry p_sc_glenfarclas[] = {
    {"Glenfarclas 10yr",     80,  2, 10},
    {"Glenfarclas 12yr",     86,  2, 12},
    {"Glenfarclas 15yr",     92,  3, 15},
    {"Glenfarclas 17yr",     86,  3, 17},
    {"Glenfarclas 21yr",     86,  4, 21},
    {"Glenfarclas 25yr",     86,  4, 25},
    {"Glenfarclas 105",     120,  3, 0},
};
static const LibraryEntry p_sc_glenmorangie[] = {
    {"Glenmorangie Original",86,  2, 10},
    {"Glenmorangie Lasanta", 86,  3, 12},
    {"Glenmorangie Quinta",  92,  3, 14},
    {"Glenmorangie Nectar",  92,  3, 0},
    {"Glenmorangie 18yr",    86,  3, 18},
    {"Glenmorangie Signet",  92,  4, 0},
};
static const LibraryEntry p_sc_dalmore[] = {
    {"Dalmore 12yr",         80,  3, 12},
    {"Dalmore 15yr",         80,  3, 15},
    {"Dalmore 18yr",         86,  4, 18},
    {"Dalmore Cigar Malt",   88,  4, 0},
    {"Dalmore King Alex",    80,  4, 0},
};
static const LibraryEntry p_sc_highland_park[] = {
    {"Highland Park 12yr",   86,  3, 12},
    {"Highland Park 15yr",   86,  3, 15},
    {"Highland Park 18yr",   86,  4, 18},
    {"Highland Park 25yr",   92,  4, 25},
    {"Highland Park CS",      0,  4, 0},
};
static const LibraryEntry p_sc_laphroaig[] = {
    {"Laphroaig 10yr",       86,  2, 10},
    {"Laphroaig 10 CS",       0,  3, 10},
    {"Laphroaig Quarter",    96,  2, 0},
    {"Laphroaig Select",     80,  2, 0},
    {"Laphroaig Lore",       96,  3, 0},
    {"Laphroaig 25yr",       97,  4, 25},
};
static const LibraryEntry p_sc_ardbeg[] = {
    {"Ardbeg 10yr",          92,  2, 10},
    {"Ardbeg Uigeadail",    108,  3, 0},
    {"Ardbeg Corryvreckan", 114,  3, 0},
    {"Ardbeg An Oa",         93,  3, 0},
    {"Ardbeg Wee Beastie",   94,  2, 5},
};
static const LibraryEntry p_sc_lagavulin[] = {
    {"Lagavulin 8yr",        96,  3, 8},
    {"Lagavulin 16yr",       86,  3, 16},
    {"Lagavulin DE",         86,  3, 0},
    {"Lagavulin 12 CS",       0,  4, 12},
};
static const LibraryEntry p_sc_bruichladdich[] = {
    {"Bruichladdich Classic",100,  3, 0},
    {"Port Charlotte 10yr", 100,  3, 10},
    {"Octomore",              0,  4, 0},
};
static const LibraryEntry p_sc_bowmore[] = {
    {"Bowmore 12yr",         80,  2, 12},
    {"Bowmore 15yr",         86,  3, 15},
    {"Bowmore 18yr",         86,  3, 18},
    {"Bowmore 25yr",         86,  4, 25},
    {"Bowmore No. 1",        80,  2, 0},
};
static const LibraryEntry p_sc_bunnahabhain[] = {
    {"Bunnahabhain 12yr",    92,  3, 12},
    {"Bunnahabhain 18yr",    92,  4, 18},
    {"Bunnahabhain 25yr",    92,  4, 25},
    {"Bunnahabhain Stiuir",  92,  3, 0},
    {"Bunnahabhain Toiteach",92,  3, 0},
};
static const LibraryEntry p_sc_talisker[] = {
    {"Talisker 10yr",        91,  3, 10},
    {"Talisker Storm",       91,  2, 0},
    {"Talisker 18yr",        91,  4, 18},
    {"Talisker 25yr",        91,  4, 25},
    {"Talisker DE",          91,  3, 0},
    {"Talisker Port Ruighe", 91,  3, 0},
};
static const LibraryEntry p_sc_oban[] = {
    {"Oban 14yr",            86,  3, 14},
    {"Oban Little Bay",      86,  3, 0},
    {"Oban 18yr",            86,  4, 18},
    {"Oban DE",              86,  3, 0},
};
static const LibraryEntry p_sc_springbank[] = {
    {"Springbank 10yr",      92,  3, 10},
    {"Springbank 12 CS",      0,  4, 12},
    {"Springbank 15yr",      92,  4, 15},
    {"Springbank 18yr",      92,  4, 18},
    {"Springbank 21yr",      92,  4, 21},
    {"Hazelburn 10yr",       92,  3, 10},
    {"Longrow Peated",       92,  3, 0},
};
static const LibraryEntry p_sc_dalwhinnie[] = {
    {"Dalwhinnie 15yr",      86,  3, 15},
    {"Dalwhinnie DE",        86,  3, 0},
    {"Dalwhinnie Winter",    86,  3, 0},
};
static const LibraryEntry p_sc_cragganmore[] = {
    {"Cragganmore 12yr",     80,  2, 12},
    {"Cragganmore DE",       80,  3, 0},
};
static const LibraryEntry p_sc_clynelish[] = {
    {"Clynelish 14yr",       92,  3, 14},
    {"Clynelish DE",          0,  3, 0},
};
static const LibraryEntry p_sc_jura[] = {
    {"Jura 10yr",            80,  2, 10},
    {"Jura 12yr",            80,  2, 12},
    {"Jura 18yr",            88,  3, 18},
    {"Jura Seven Wood",      84,  3, 0},
};
static const LibraryEntry p_sc_tomatin[] = {
    {"Tomatin 12yr",         86,  2, 12},
    {"Tomatin 14yr Port",    92,  2, 14},
    {"Tomatin 18yr",         92,  3, 18},
    {"Tomatin Legacy",       86,  2, 0},
    {"Tomatin Cu Bocan",     92,  2, 0},
};
static const LibraryEntry p_sc_benriach[] = {
    {"BenRiach Original",    86,  2, 10},
    {"BenRiach Smoky 10",    92,  2, 10},
    {"BenRiach The Twelve",  92,  3, 12},
    {"BenRiach 21yr",        92,  4, 21},
    {"BenRiach Curiositas",  92,  3, 0},
};
static const LibraryEntry p_sc_glendronach[] = {
    {"GlenDronach 12yr",     86,  3, 12},
    {"GlenDronach 15yr",     92,  3, 15},
    {"GlenDronach 18yr",     92,  4, 18},
    {"GlenDronach 21yr",     96,  4, 21},
    {"GlenDronach CS",        0,  4, 0},
};
static const LibraryEntry p_sc_caol_ila[] = {
    {"Caol Ila 12yr",        86,  3, 12},
    {"Caol Ila 18yr",        86,  4, 18},
    {"Caol Ila DE",          86,  3, 0},
    {"Caol Ila Moch",        86,  2, 0},
};
static const LibraryEntry p_sc_glenkinchie[] = {
    {"Glenkinchie 12yr",     86,  2, 12},
    {"Glenkinchie DE",       86,  3, 0},
};
static const LibraryEntry p_sc_auchentoshan[] = {
    {"Auchentoshan 12yr",    80,  2, 12},
    {"Auchentoshan ThreeWd", 86,  2, 0},
    {"Auchentoshan American",80,  1, 0},
};
static const LibraryEntry p_sc_edradour[] = {
    {"Edradour 10yr",        80,  3, 10},
    {"Edradour 12yr CS",      0,  3, 12},
    {"Edradour Caledonia",   92,  3, 0},
};
static const LibraryEntry p_sc_craigellachie[] = {
    {"Craigellachie 13yr",   92,  3, 13},
    {"Craigellachie 17yr",   92,  3, 17},
    {"Craigellachie 23yr",   92,  4, 23},
};
static const LibraryEntry p_sc_glen_moray[] = {
    {"Glen Moray Classic",   80,  1, 0},
    {"Glen Moray 12yr",      80,  2, 12},
    {"Glen Moray 15yr",      80,  2, 15},
    {"Glen Moray 18yr",      94,  3, 18},
    {"Glen Moray Sherry",    80,  2, 0},
};
static const LibraryEntry p_sc_cardhu[] = {
    {"Cardhu 12yr",          80,  2, 12},
    {"Cardhu Gold Reserve",  80,  2, 0},
    {"Cardhu 15yr",          80,  3, 15},
};
static const LibraryEntry p_sc_deanston[] = {
    {"Deanston 12yr",        92,  2, 12},
    {"Deanston 18yr",        92,  3, 18},
    {"Deanston Virgin Oak",  92,  2, 0},
};
static const LibraryEntry p_sc_tobermory[] = {
    {"Tobermory 12yr",       92,  3, 12},
    {"Tobermory 20yr",        0,  4, 20},
    {"Ledaig 10yr",          92,  3, 10},
    {"Ledaig 18yr",          92,  4, 18},
};
static const LibraryEntry p_sc_kilchoman[] = {
    {"Kilchoman Machir Bay", 92,  3, 0},
    {"Kilchoman Sanaig",     92,  3, 0},
    {"Kilchoman 100% Islay", 100, 3, 0},
    {"Kilchoman Loch Gorm",  92,  3, 0},
};
static const LibraryEntry p_sc_pulteney[] = {
    {"Old Pulteney 12yr",    80,  2, 12},
    {"Old Pulteney 15yr",    92,  3, 15},
    {"Old Pulteney 18yr",    92,  3, 18},
    {"Old Pulteney Huddart", 92,  2, 0},
};
static const LibraryEntry p_sc_benromach[] = {
    {"Benromach 10yr",       86,  2, 10},
    {"Benromach 15yr",       86,  3, 15},
    {"Benromach Peat Smoke", 92,  3, 0},
    {"Benromach CS",          0,  3, 0},
};
static const LibraryEntry p_sc_mortlach[] = {
    {"Mortlach 12yr",        86,  3, 12},
    {"Mortlach 16yr",        86,  4, 16},
    {"Mortlach 20yr",        86,  4, 20},
};
static const LibraryEntry p_sc_glen_scotia[] = {
    {"Glen Scotia 15yr",     92,  3, 15},
    {"Glen Scotia 18yr",     92,  4, 18},
    {"Glen Scotia 25yr",     92,  4, 25},
    {"Glen Scotia Victoriana", 0, 3, 0},
    {"Glen Scotia Dbl Cask", 92,  2, 0},
};
static const LibraryEntry p_sc_tomintoul[] = {
    {"Tomintoul 10yr",       80,  2, 10},
    {"Tomintoul 14yr",       92,  2, 14},
    {"Tomintoul 16yr",       80,  3, 16},
    {"Tomintoul Peaty Tang", 80,  2, 0},
};
static const LibraryEntry p_sc_royal_brackla[] = {
    {"Royal Brackla 12yr",   80,  3, 12},
    {"Royal Brackla 16yr",   80,  3, 16},
    {"Royal Brackla 21yr",   80,  4, 21},
};
static const LibraryEntry p_sc_speyburn[] = {
    {"Speyburn 10yr",        80,  2, 10},
    {"Speyburn 15yr",        92,  3, 15},
    {"Speyburn 18yr",        92,  3, 18},
};
static const LibraryEntry p_sc_ancnoc[] = {
    {"anCnoc 12yr",          80,  2, 12},
    {"anCnoc 18yr",          92,  3, 18},
    {"anCnoc 24yr",          92,  4, 24},
    {"anCnoc Peatheart",      0,  3, 0},
};
static const LibraryEntry p_sc_balblair[] = {
    {"Balblair 12yr",        92,  3, 12},
    {"Balblair 15yr",        92,  3, 15},
    {"Balblair 18yr",        92,  4, 18},
    {"Balblair 25yr",        92,  4, 25},
};
static const LibraryEntry p_sc_glencadam[] = {
    {"Glencadam 10yr",       92,  2, 10},
    {"Glencadam 13yr",       92,  3, 13},
    {"Glencadam 15yr",       92,  3, 15},
    {"Glencadam 21yr",       92,  4, 21},
};
static const LibraryEntry p_sc_aberfeldy[] = {
    {"Aberfeldy 12yr",       80,  2, 12},
    {"Aberfeldy 16yr",       80,  3, 16},
    {"Aberfeldy 21yr",       80,  4, 21},
};

static const WhiskeyDistillery d_scotch_sm[] = {
    DISTILLERY("Glenfiddich",     p_sc_glenfiddich),
    DISTILLERY("The Glenlivet",   p_sc_glenlivet),
    DISTILLERY("The Macallan",    p_sc_macallan),
    DISTILLERY("Balvenie",        p_sc_balvenie),
    DISTILLERY("Aberlour",        p_sc_aberlour),
    DISTILLERY("Glenfarclas",     p_sc_glenfarclas),
    DISTILLERY("Glenmorangie",    p_sc_glenmorangie),
    DISTILLERY("Dalmore",         p_sc_dalmore),
    DISTILLERY("Highland Park",   p_sc_highland_park),
    DISTILLERY("Laphroaig",       p_sc_laphroaig),
    DISTILLERY("Ardbeg",          p_sc_ardbeg),
    DISTILLERY("Lagavulin",       p_sc_lagavulin),
    DISTILLERY("Bruichladdich",   p_sc_bruichladdich),
    DISTILLERY("Bowmore",         p_sc_bowmore),
    DISTILLERY("Bunnahabhain",    p_sc_bunnahabhain),
    DISTILLERY("Talisker",        p_sc_talisker),
    DISTILLERY("Oban",            p_sc_oban),
    DISTILLERY("Springbank",      p_sc_springbank),
    DISTILLERY("Dalwhinnie",      p_sc_dalwhinnie),
    DISTILLERY("Cragganmore",     p_sc_cragganmore),
    DISTILLERY("Clynelish",       p_sc_clynelish),
    DISTILLERY("Isle of Jura",    p_sc_jura),
    DISTILLERY("Tomatin",         p_sc_tomatin),
    DISTILLERY("BenRiach",        p_sc_benriach),
    DISTILLERY("GlenDronach",     p_sc_glendronach),
    DISTILLERY("Caol Ila",        p_sc_caol_ila),
    DISTILLERY("Glenkinchie",     p_sc_glenkinchie),
    DISTILLERY("Auchentoshan",    p_sc_auchentoshan),
    DISTILLERY("Edradour",        p_sc_edradour),
    DISTILLERY("Craigellachie",   p_sc_craigellachie),
    DISTILLERY("Glen Moray",      p_sc_glen_moray),
    DISTILLERY("Cardhu",          p_sc_cardhu),
    DISTILLERY("Deanston",        p_sc_deanston),
    DISTILLERY("Tobermory",       p_sc_tobermory),
    DISTILLERY("Kilchoman",       p_sc_kilchoman),
    DISTILLERY("Old Pulteney",    p_sc_pulteney),
    DISTILLERY("Benromach",       p_sc_benromach),
    DISTILLERY("Mortlach",        p_sc_mortlach),
    DISTILLERY("Glen Scotia",     p_sc_glen_scotia),
    DISTILLERY("Tomintoul",       p_sc_tomintoul),
    DISTILLERY("Royal Brackla",   p_sc_royal_brackla),
    DISTILLERY("Speyburn",        p_sc_speyburn),
    DISTILLERY("anCnoc",          p_sc_ancnoc),
    DISTILLERY("Balblair",        p_sc_balblair),
    DISTILLERY("Glencadam",       p_sc_glencadam),
    DISTILLERY("Aberfeldy",       p_sc_aberfeldy),
};

// ============================================================
// SCOTCH — BLENDED
// ============================================================

static const LibraryEntry p_sb_jw[] = {
    {"JW Red Label",         80,  1, 0},
    {"JW Black Label",       80,  2, 12},
    {"JW Double Black",      80,  2, 0},
    {"JW Green Label",       86,  3, 15},
    {"JW Gold Label Res",    80,  3, 0},
    {"JW Blue Label",        80,  4, 0},
    {"JW 18yr",              80,  4, 18},
};
static const LibraryEntry p_sb_chivas[] = {
    {"Chivas 12yr",          80,  2, 12},
    {"Chivas 18yr",          80,  3, 18},
    {"Chivas 25yr",          80,  4, 25},
    {"Chivas XV",            80,  2, 15},
    {"Chivas Mizunara",      80,  3, 0},
    {"Chivas Extra 13yr",    80,  2, 13},
};
static const LibraryEntry p_sb_dewars[] = {
    {"Dewar's White Label",  80,  1, 0},
    {"Dewar's 12yr",         80,  2, 12},
    {"Dewar's 15yr",         80,  2, 15},
    {"Dewar's 18yr",         80,  3, 18},
    {"Dewar's 25yr",         80,  4, 25},
    {"Dewar's Caribbean",    80,  2, 0},
};
static const LibraryEntry p_sb_monkey[] = {
    {"Monkey Shoulder",      86,  2, 0},
    {"Smokey Monkey",        80,  2, 0},
};
static const LibraryEntry p_sb_compass[] = {
    {"CB Oak Cross",         86,  3, 0},
    {"CB Spice Tree",        92,  3, 0},
    {"CB Peat Monster",      92,  3, 0},
    {"CB Hedonism",          86,  4, 0},
    {"CB Great King",        86,  2, 0},
    {"CB Glasgow",           80,  2, 0},
};
static const LibraryEntry p_sb_grouse[] = {
    {"Famous Grouse",        80,  1, 0},
    {"Famous Grouse Smoky",  80,  1, 0},
    {"Naked Malt",           80,  2, 0},
};
static const LibraryEntry p_sb_ballantines[] = {
    {"Ballantine's Finest",  80,  1, 0},
    {"Ballantine's 12yr",    80,  2, 12},
    {"Ballantine's 17yr",    80,  3, 17},
    {"Ballantine's 21yr",    86,  4, 21},
};
static const LibraryEntry p_sb_cutty[] = {
    {"Cutty Sark Original",  80,  1, 0},
    {"Cutty Sark Prohib",   100,  2, 0},
};
static const LibraryEntry p_sb_grants[] = {
    {"Grant's Triple Wood",  80,  1, 0},
    {"Grant's 12yr",         80,  2, 12},
    {"Grant's 18yr",         80,  3, 18},
};
static const LibraryEntry p_sb_buchanans[] = {
    {"Buchanan's 12yr",      80,  2, 12},
    {"Buchanan's 15yr",      80,  3, 15},
    {"Buchanan's 18yr",      80,  3, 18},
};
static const LibraryEntry p_sb_teachers[] = {
    {"Teacher's Highland",   80,  1, 0},
};

static const WhiskeyDistillery d_scotch_bl[] = {
    DISTILLERY("Johnnie Walker",  p_sb_jw),
    DISTILLERY("Chivas Regal",    p_sb_chivas),
    DISTILLERY("Dewar's",         p_sb_dewars),
    DISTILLERY("Monkey Shoulder",  p_sb_monkey),
    DISTILLERY("Compass Box",     p_sb_compass),
    DISTILLERY("Famous Grouse",   p_sb_grouse),
    DISTILLERY("Ballantine's",    p_sb_ballantines),
    DISTILLERY("Cutty Sark",      p_sb_cutty),
    DISTILLERY("Grant's",         p_sb_grants),
    DISTILLERY("Buchanan's",      p_sb_buchanans),
    DISTILLERY("Teacher's",       p_sb_teachers),
};

// ============================================================
// IRISH WHISKEY
// ============================================================

static const LibraryEntry p_ir_midleton[] = {
    {"Jameson Original",     80,  1, 0},
    {"Jameson Black Barrel", 80,  2, 0},
    {"Jameson 18yr",         80,  3, 18},
    {"Redbreast 12yr",       80,  3, 12},
    {"Redbreast 12 CS",       0,  3, 12},
    {"Redbreast 15yr",       92,  4, 15},
    {"Redbreast 21yr",       92,  4, 21},
    {"Redbreast Lustau",     92,  3, 0},
    {"Green Spot",           80,  3, 0},
    {"Yellow Spot",          92,  3, 12},
    {"Blue Spot",             0,  4, 7},
    {"Red Spot",              0,  4, 15},
    {"Powers Gold Label",    86,  1, 0},
    {"Powers John's Lane",   92,  3, 12},
    {"Midleton Very Rare",   80,  4, 0},
    {"Method & Madness",     92,  3, 0},
};
static const LibraryEntry p_ir_bushmills[] = {
    {"Bushmills Original",   80,  1, 0},
    {"Black Bush",           80,  2, 0},
    {"Bushmills 10yr",       80,  2, 10},
    {"Bushmills 12yr",       80,  2, 12},
    {"Bushmills 16yr",       80,  3, 16},
    {"Bushmills 21yr",       80,  4, 21},
};
static const LibraryEntry p_ir_tullamore[] = {
    {"Tullamore D.E.W.",     80,  1, 0},
    {"Tullamore 12yr",       80,  2, 12},
    {"Tullamore 14yr SM",    82,  3, 14},
    {"Tullamore 18yr SM",    82,  4, 18},
    {"Tullamore XO",         86,  2, 0},
};
static const LibraryEntry p_ir_teeling[] = {
    {"Teeling Small Batch",  92,  2, 0},
    {"Teeling Single Grain", 92,  2, 0},
    {"Teeling Single Malt",  92,  3, 0},
    {"Teeling Single Pot",   92,  3, 0},
    {"Teeling Blackpitts",   92,  3, 0},
};
static const LibraryEntry p_ir_dingle[] = {
    {"Dingle Single Malt",    0,  3, 0},
    {"Dingle Single Pot",     0,  3, 0},
};
static const LibraryEntry p_ir_kilbeggan[] = {
    {"Kilbeggan Original",   80,  1, 0},
    {"Kilbeggan SiG",        86,  2, 0},
    {"Kilbeggan SiM",         0,  2, 0},
};
static const LibraryEntry p_ir_connemara[] = {
    {"Connemara Original",   80,  2, 0},
    {"Connemara 12yr",       80,  3, 12},
    {"Connemara Cask Str",    0,  3, 0},
    {"Connemara Turf Mor",   92,  3, 0},
};
static const LibraryEntry p_ir_tyrconnell[] = {
    {"Tyrconnell SiM",       80,  2, 0},
    {"Tyrconnell 10 Sherry", 92,  3, 10},
    {"Tyrconnell 10 Port",   92,  3, 10},
    {"Tyrconnell 16yr",      92,  4, 16},
};
static const LibraryEntry p_ir_west_cork[] = {
    {"West Cork Original",    0,  1, 0},
    {"West Cork 8yr",         0,  2, 8},
    {"West Cork 10yr",        0,  2, 10},
    {"West Cork 12yr",        0,  2, 12},
    {"West Cork Bog Oak",     0,  2, 0},
    {"West Cork CS",          0,  2, 0},
};
static const LibraryEntry p_ir_glendalough[] = {
    {"Glendalough Double",    0,  2, 0},
    {"Glendalough 7yr",       0,  2, 7},
    {"Glendalough 13yr",      0,  3, 13},
    {"Glendalough Pot Stl",   0,  2, 0},
};
static const LibraryEntry p_ir_writers[] = {
    {"Writers' Tears Cppr",  80,  2, 0},
    {"Writers' Tears DblOk", 80,  3, 0},
    {"Writers' Tears CS",     0,  3, 0},
};
static const LibraryEntry p_ir_proper12[] = {
    {"Proper No. Twelve",    80,  1, 0},
};
static const LibraryEntry p_ir_knappogue[] = {
    {"Knappogue 12yr",       80,  2, 12},
    {"Knappogue 14yr",       92,  3, 14},
    {"Knappogue 16yr",       80,  3, 16},
};
static const LibraryEntry p_ir_keepers[] = {
    {"Keeper's Heart Irish",  0,  2, 0},
    {"Keeper's Heart Rye",    0,  2, 0},
};

static const WhiskeyDistillery d_irish[] = {
    DISTILLERY("Midleton",         p_ir_midleton),
    DISTILLERY("Bushmills",        p_ir_bushmills),
    DISTILLERY("Tullamore",        p_ir_tullamore),
    DISTILLERY("Teeling",          p_ir_teeling),
    DISTILLERY("Dingle",           p_ir_dingle),
    DISTILLERY("Kilbeggan",        p_ir_kilbeggan),
    DISTILLERY("Connemara",        p_ir_connemara),
    DISTILLERY("Tyrconnell",       p_ir_tyrconnell),
    DISTILLERY("West Cork",        p_ir_west_cork),
    DISTILLERY("Glendalough",      p_ir_glendalough),
    DISTILLERY("Writers' Tears",   p_ir_writers),
    DISTILLERY("Proper No. 12",    p_ir_proper12),
    DISTILLERY("Knappogue Castle", p_ir_knappogue),
    DISTILLERY("Keeper's Heart",   p_ir_keepers),
};

// ============================================================
// JAPANESE WHISKY
// ============================================================

static const LibraryEntry p_jp_yamazaki[] = {
    {"Yamazaki 12yr",        86,  4, 12},
    {"Yamazaki 18yr",        86,  4, 18},
    {"Yamazaki 25yr",        86,  4, 25},
    {"Yamazaki Distiller's", 86,  3, 0},
};
static const LibraryEntry p_jp_hakushu[] = {
    {"Hakushu 12yr",         86,  4, 12},
    {"Hakushu 18yr",         86,  4, 18},
    {"Hakushu 25yr",         86,  4, 25},
    {"Hakushu Distiller's",  86,  3, 0},
};
static const LibraryEntry p_jp_hibiki[] = {
    {"Hibiki Harmony",       86,  3, 0},
    {"Hibiki 17yr",          86,  4, 17},
    {"Hibiki 21yr",          86,  4, 21},
    {"Hibiki 30yr",          86,  4, 30},
    {"Hibiki Blender's Ch",  86,  4, 0},
};
static const LibraryEntry p_jp_suntory_other[] = {
    {"Suntory Toki",         86,  2, 0},
    {"The Chita SiG",        86,  3, 0},
};
static const LibraryEntry p_jp_yoichi[] = {
    {"Yoichi Single Malt",   90,  3, 0},
    {"Yoichi 10yr",          90,  4, 10},
    {"Yoichi 15yr",          90,  4, 15},
};
static const LibraryEntry p_jp_miyagikyo[] = {
    {"Miyagikyo SiM",        90,  3, 0},
    {"Miyagikyo 12yr",       90,  4, 12},
    {"Miyagikyo 15yr",       90,  4, 15},
};
static const LibraryEntry p_jp_nikka_other[] = {
    {"Nikka From Barrel",   102,  3, 0},
    {"Nikka Coffey Grain",   90,  3, 0},
    {"Nikka Coffey Malt",    90,  3, 0},
    {"Nikka Taketsuru",      86,  3, 0},
    {"Nikka Days",           80,  2, 0},
    {"Nikka Session",        86,  2, 0},
};
static const LibraryEntry p_jp_mars[] = {
    {"Mars Iwai Tradition",  80,  2, 0},
    {"Mars Iwai 45",         90,  2, 0},
    {"Mars Komagatake",       0,  3, 0},
    {"Mars Cosmo",            0,  2, 0},
};
static const LibraryEntry p_jp_chichibu[] = {
    {"Ichiro's Malt&Grain",  92,  3, 0},
    {"Ichiro's Malt Double", 92,  4, 0},
    {"Ichiro's Chichibu",     0,  4, 0},
};
static const LibraryEntry p_jp_akkeshi[] = {
    {"Akkeshi Peated SiM",    0,  4, 0},
    {"Akkeshi Blended",       0,  4, 0},
};
static const LibraryEntry p_jp_white_oak[] = {
    {"Akashi White Oak",     80,  2, 0},
    {"Akashi Single Malt",    0,  3, 0},
    {"Akashi Sherry Cask",    0,  3, 0},
};
static const LibraryEntry p_jp_kaiyo[] = {
    {"Kaiyo Whisky",         86,  2, 0},
    {"Kaiyo Peated",         92,  2, 0},
    {"Kaiyo Cask Strength",   0,  3, 0},
    {"Kaiyo The Single 7yr", 96,  3, 7},
};
static const LibraryEntry p_jp_kurayoshi[] = {
    {"Kurayoshi Pure Malt",  86,  2, 0},
    {"Kurayoshi 8yr",        86,  3, 8},
    {"Kurayoshi 12yr",       86,  3, 12},
    {"Kurayoshi 18yr",       86,  4, 18},
};
static const LibraryEntry p_jp_ohishi[] = {
    {"Ohishi Sherry Cask",    0,  3, 0},
    {"Ohishi Brandy Cask",    0,  3, 0},
    {"Ohishi Sakura Cask",    0,  3, 0},
};
static const LibraryEntry p_jp_togouchi[] = {
    {"Togouchi Premium",     80,  2, 0},
    {"Togouchi 9yr",         80,  3, 9},
    {"Togouchi 15yr",        86,  3, 15},
    {"Togouchi Peated",      80,  2, 0},
};

static const WhiskeyDistillery d_japanese[] = {
    DISTILLERY("Yamazaki",         p_jp_yamazaki),
    DISTILLERY("Hakushu",          p_jp_hakushu),
    DISTILLERY("Hibiki",           p_jp_hibiki),
    DISTILLERY("Suntory Other",    p_jp_suntory_other),
    DISTILLERY("Yoichi",           p_jp_yoichi),
    DISTILLERY("Miyagikyo",        p_jp_miyagikyo),
    DISTILLERY("Nikka Blends",     p_jp_nikka_other),
    DISTILLERY("Mars Shinshu",     p_jp_mars),
    DISTILLERY("Chichibu",         p_jp_chichibu),
    DISTILLERY("Akkeshi",          p_jp_akkeshi),
    DISTILLERY("White Oak",        p_jp_white_oak),
    DISTILLERY("Kaiyo",            p_jp_kaiyo),
    DISTILLERY("Kurayoshi",        p_jp_kurayoshi),
    DISTILLERY("Ohishi",           p_jp_ohishi),
    DISTILLERY("Togouchi",         p_jp_togouchi),
};

// ============================================================
// CANADIAN WHISKY
// ============================================================

static const LibraryEntry p_ca_crown[] = {
    {"Crown Royal Deluxe",   80,  1, 0},
    {"Crown Royal Black",    90,  2, 0},
    {"Crown Royal Res",      80,  2, 0},
    {"Crown Royal XR",       80,  4, 0},
    {"Crown Royal XO",       80,  3, 0},
    {"Crown Royal Northern", 90,  2, 0},
    {"Crown Royal Peach",    70,  1, 0},
    {"Crown Royal Apple",    70,  1, 0},
};
static const LibraryEntry p_ca_cc[] = {
    {"Canadian Club 1858",   80,  1, 0},
    {"CC 100% Rye",          80,  1, 0},
    {"CC 12yr",              80,  2, 12},
    {"CC 20yr",              80,  3, 20},
};
static const LibraryEntry p_ca_lot40[] = {
    {"Lot No. 40",           86,  2, 0},
    {"Lot No. 40 CS",         0,  3, 0},
    {"Lot No. 40 Dark Oak",  86,  3, 0},
};
static const LibraryEntry p_ca_pike[] = {
    {"Pike Creek 10yr",      84,  2, 10},
    {"Pike Creek 21yr",      84,  4, 21},
};
static const LibraryEntry p_ca_wisers[] = {
    {"JP Wiser's Deluxe",    80,  1, 0},
    {"JP Wiser's 15yr",      80,  2, 15},
    {"JP Wiser's 18yr",      80,  3, 18},
    {"JP Wiser's 23yr",      80,  4, 23},
};
static const LibraryEntry p_ca_alberta[] = {
    {"Alberta Premium",      80,  1, 0},
    {"Alberta Premium CS",    0,  3, 0},
    {"Alberta Prem 20yr",     0,  4, 20},
};
static const LibraryEntry p_ca_forty_creek[] = {
    {"Forty Creek Barrel",   80,  1, 0},
    {"Forty Creek Conf Rsv",  0,  2, 0},
    {"Forty Creek Double",    0,  2, 0},
};
static const LibraryEntry p_ca_pendleton[] = {
    {"Pendleton Original",   80,  1, 0},
    {"Pendleton 1910",       80,  2, 12},
    {"Pendleton Midnight",   90,  2, 0},
};
static const LibraryEntry p_ca_collingwood[] = {
    {"Collingwood Original",  0,  2, 0},
    {"Collingwood 21yr",      0,  3, 21},
    {"Collingwood Toasted",   0,  2, 0},
};
static const LibraryEntry p_ca_glen_breton[] = {
    {"Glen Breton Rare 10",  80,  3, 10},
    {"Glen Breton Ice 10",   80,  3, 10},
    {"Glen Breton 14yr",      0,  3, 14},
    {"Glen Breton 19yr",      0,  4, 19},
};
static const LibraryEntry p_ca_caribou[] = {
    {"Caribou Crossing SiB", 80,  3, 0},
};

static const WhiskeyDistillery d_canadian[] = {
    DISTILLERY("Crown Royal",     p_ca_crown),
    DISTILLERY("Canadian Club",   p_ca_cc),
    DISTILLERY("Lot No. 40",      p_ca_lot40),
    DISTILLERY("Pike Creek",      p_ca_pike),
    DISTILLERY("JP Wiser's",      p_ca_wisers),
    DISTILLERY("Alberta Premium", p_ca_alberta),
    DISTILLERY("Forty Creek",     p_ca_forty_creek),
    DISTILLERY("Pendleton",       p_ca_pendleton),
    DISTILLERY("Collingwood",     p_ca_collingwood),
    DISTILLERY("Glen Breton",     p_ca_glen_breton),
    DISTILLERY("Caribou Crossing",p_ca_caribou),
};

// ============================================================
// WORLD / OTHER WHISKY
// ============================================================

static const LibraryEntry p_wo_kavalan[] = {
    {"Kavalan Classic",      80,  3, 0},
    {"Kavalan Concertmastr", 80,  3, 0},
    {"Kavalan Ex-Bourbon",   92,  3, 0},
    {"Kavalan Solist Shrry",  0,  4, 0},
    {"Kavalan Solist Brbn",   0,  4, 0},
};
static const LibraryEntry p_wo_amrut[] = {
    {"Amrut Single Malt",    92,  2, 0},
    {"Amrut Fusion",         100, 3, 0},
    {"Amrut Peated",         92,  3, 0},
    {"Amrut Cask Strength",   0,  3, 0},
    {"Amrut Portonova",     124,  3, 0},
};
static const LibraryEntry p_wo_paul_john[] = {
    {"Paul John Brilliance", 92,  2, 0},
    {"Paul John Edited",     92,  2, 0},
    {"Paul John Bold",      92,  3, 0},
    {"Paul John Classic",    0,  2, 0},
    {"Paul John Peated",    110,  3, 0},
};
static const LibraryEntry p_wo_starward[] = {
    {"Starward Nova",        82,  2, 0},
    {"Starward Solera",      86,  3, 0},
    {"Starward Two-Fold",    80,  2, 0},
    {"Starward Fortis",     100,  3, 0},
    {"Starward Left-Field",  80,  2, 0},
};
static const LibraryEntry p_wo_westward[] = {
    {"Westward American SM", 90,  3, 0},
    {"Westward Stout Cask",  90,  3, 0},
    {"Westward Pinot Noir",  90,  3, 0},
    {"Westward Cask Str",     0,  4, 0},
};
static const LibraryEntry p_wo_westland[] = {
    {"Westland American SM", 92,  3, 0},
    {"Westland Sherry Wood", 92,  3, 0},
    {"Westland Peated",      92,  3, 0},
    {"Westland Garryana",     0,  4, 0},
};
static const LibraryEntry p_wo_stranahans[] = {
    {"Stranahan's Original", 94,  3, 0},
    {"Stranahan's Blue Peak",86,  2, 0},
    {"Stranahan's Sherry",   94,  3, 0},
};
static const LibraryEntry p_wo_balcones[] = {
    {"Balcones Texas SiM",    0,  3, 0},
    {"Balcones Baby Blue",   92,  2, 0},
    {"Balcones True Blue",    0,  3, 0},
    {"Balcones Brimstone",   106, 3, 0},
    {"Balcones Lineage",      0,  2, 0},
};
static const LibraryEntry p_wo_virginia[] = {
    {"Courage & Conviction", 92,  3, 0},
    {"C&C Cask Strength",     0,  4, 0},
    {"C&C Sherry Cask",      92,  3, 0},
    {"C&C Bourbon Cask",     92,  3, 0},
};

static const WhiskeyDistillery d_world[] = {
    DISTILLERY("Kavalan",           p_wo_kavalan),
    DISTILLERY("Amrut",             p_wo_amrut),
    DISTILLERY("Paul John",         p_wo_paul_john),
    DISTILLERY("Starward",          p_wo_starward),
    DISTILLERY("Westward",          p_wo_westward),
    DISTILLERY("Westland",          p_wo_westland),
    DISTILLERY("Stranahan's",       p_wo_stranahans),
    DISTILLERY("Balcones",          p_wo_balcones),
    DISTILLERY("Virginia Dist Co",  p_wo_virginia),
};

// ============================================================
// TOP-LEVEL TYPE ARRAY
// ============================================================

#define TYPE_ENTRY(name, arr) { name, arr, sizeof(arr)/sizeof(arr[0]) }

const WhiskeyType WHISKEY_TYPES[] = {
    TYPE_ENTRY("Bourbon",           d_bourbon),
    TYPE_ENTRY("Tennessee",         d_tennessee),
    TYPE_ENTRY("American Rye",      d_rye),
    TYPE_ENTRY("Scotch Single Malt",d_scotch_sm),
    TYPE_ENTRY("Scotch Blended",    d_scotch_bl),
    TYPE_ENTRY("Irish Whiskey",     d_irish),
    TYPE_ENTRY("Japanese Whisky",   d_japanese),
    TYPE_ENTRY("Canadian Whisky",   d_canadian),
    TYPE_ENTRY("World Whisky",      d_world),
};

const int WHISKEY_TYPE_COUNT = sizeof(WHISKEY_TYPES) / sizeof(WHISKEY_TYPES[0]);
