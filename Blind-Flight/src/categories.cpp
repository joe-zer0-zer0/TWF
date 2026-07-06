#include "categories.h"

// ============================================================
// Blind Flight — Whiskey Category Data (Session 12)
// ============================================================
// ~120 distilleries, ~650+ products across 9 whiskey types.
// All rye products consolidated under American Rye.
// ============================================================

// ============================================================
// BOURBON
// ============================================================

static const char* p_bbn_buffalo_trace[] = {
    "Buffalo Trace", "Eagle Rare 10yr", "Blanton's Original",
    "W.L. Weller Special", "W.L. Weller 107", "W.L. Weller 12yr",
    "E.H. Taylor SmB", "E.H. Taylor SiB", "Stagg Jr",
    "Elmer T. Lee SiB", "Benchmark No. 8"
};
static const char* p_bbn_heaven_hill[] = {
    "Elijah Craig SmB", "Elijah Craig BP", "Elijah Craig Toasted",
    "Evan Williams Black", "Evan Williams BiB", "Evan Williams 1783",
    "Evan Williams SiB", "Larceny SmB", "Larceny BP",
    "Henry McKenna 10yr", "Old Fitzgerald BiB", "Fighting Cock",
    "Heaven Hill BiB"
};
static const char* p_bbn_jim_beam[] = {
    "Jim Beam White", "Jim Beam Black", "Jim Beam Double Oak",
    "Knob Creek 9yr", "Knob Creek 12yr", "Knob Creek SiB",
    "Booker's", "Baker's 7yr SiB", "Basil Hayden's",
    "Old Grand-Dad 114", "Old Grand-Dad BiB", "Little Book", "Legent"
};
static const char* p_bbn_wild_turkey[] = {
    "Wild Turkey 101", "Wild Turkey 81", "Rare Breed",
    "Russell's Res 10yr", "Russell's Res SiB", "Kentucky Spirit",
    "Longbranch"
};
static const char* p_bbn_woodford[] = {
    "Woodford Reserve", "Woodford Double Oak", "Woodford Batch Proof",
    "Woodford Wheat", "Woodford Malt"
};
static const char* p_bbn_makers[] = {
    "Maker's Mark", "Maker's 46", "Maker's Cask Str",
    "Maker's Private Sel", "Maker's Cellar Aged",
    "Maker's FAE-01", "Maker's FAE-02"
};
static const char* p_bbn_four_roses[] = {
    "Four Roses Yellow", "Four Roses SmB", "Four Roses SmB Sel",
    "Four Roses SiB", "Four Roses SiB BS"
};
static const char* p_bbn_old_forester[] = {
    "Old Forester 86", "Old Forester 100", "Old Forester 1870",
    "Old Forester 1897", "Old Forester 1910", "Old Forester 1920",
    "Old Forester SiB"
};
static const char* p_bbn_angels_envy[] = {
    "Angel's Envy Bourbon", "Angel's Envy Cask Str"
};
static const char* p_bbn_michters[] = {
    "Michter's US*1 Brbn", "Michter's Toasted",
    "Michter's US*1 SrMsh", "Michter's 10yr Brbn"
};
static const char* p_bbn_bulleit[] = {
    "Bulleit Bourbon", "Bulleit 10yr", "Bulleit Barrel Str",
    "Bulleit BiB"
};
static const char* p_bbn_1792[] = {
    "1792 Small Batch", "1792 Full Proof", "1792 Single Barrel",
    "1792 BiB", "1792 Sweet Wheat", "1792 Port Finish"
};
static const char* p_bbn_bardstown[] = {
    "Bardstown Discovery", "Bardstown Fusion",
    "Bardstown Origin", "Bardstown Collab"
};
static const char* p_bbn_willet[] = {
    "Willet Pot Still", "Noah's Mill", "Rowan's Creek",
    "Johnny Drum"
};
static const char* p_bbn_new_riff[] = {
    "New Riff Bourbon", "New Riff SiB",
    "New Riff Backsetter", "New Riff Maltster"
};
static const char* p_bbn_wilderness[] = {
    "WT Bourbon BiB", "WT SiB Bourbon", "WT Wheated BiB"
};
static const char* p_bbn_peerless[] = {
    "Peerless Bourbon", "Peerless SmB",
    "Peerless SiB", "Peerless Double Oak"
};
static const char* p_bbn_rabbit_hole[] = {
    "Dareringer Bourbon", "Cavehill Bourbon", "Heigold Bourbon"
};
static const char* p_bbn_castle_key[] = {
    "Castle & Key SmB", "Roots of Ruin"
};
static const char* p_bbn_lux_row[] = {
    "Ezra Brooks 99", "Ezra Brooks SiB", "Rebel 100", "Rebel SiB",
    "Blood Oath", "David Nicholson 1843", "David Nicholson Res"
};
static const char* p_bbn_garrison[] = {
    "Garrison SmB", "Garrison SiB", "Garrison Balmorhea",
    "Garrison Cowboy", "Garrison HoneyDew"
};
static const char* p_bbn_stellum[] = {
    "Stellum Bourbon", "Stellum Equinox", "Stellum SiB"
};
static const char* p_bbn_smoke_wagon[] = {
    "Smoke Wagon SmB", "Smoke Wagon Uncut", "Smoke Wagon Str Brbn"
};
static const char* p_bbn_starlight[] = {
    "Starlight Bourbon", "Starlight Double Oak", "Starlight Carl T."
};
static const char* p_bbn_penelope[] = {
    "Penelope Bourbon", "Penelope Barrel Str", "Penelope Four Grain",
    "Penelope Architect", "Penelope Toasted"
};
static const char* p_bbn_jeffersons[] = {
    "Jefferson's SmB", "Jefferson's Reserve",
    "Jefferson's Ocean", "Jefferson's Grand Sel"
};
static const char* p_bbn_town_branch[] = {
    "Town Branch Bourbon", "Town Branch SiB"
};
static const char* p_bbn_calumet[] = {
    "Calumet Farm Bourbon", "Calumet Farm SmB", "Calumet Srk Black"
};
static const char* p_bbn_belle_meade[] = {
    "Belle Meade Bourbon", "Belle Meade Reserve",
    "Belle Meade SiB", "Belle Meade Cask Str"
};
static const char* p_bbn_ky_owl[] = {
    "KY Owl Bourbon", "KY Owl Confiscated", "KY Owl Takumi"
};
static const char* p_bbn_widow_jane[] = {
    "Widow Jane 10yr", "Widow Jane Baby Jane",
    "Widow Jane The Vaults", "Widow Jane Oak&Apple"
};
static const char* p_bbn_yellowstone[] = {
    "Yellowstone Select", "Yellowstone Ltd Ed", "Yellowstone 150th"
};
static const char* p_bbn_pinhook[] = {
    "Pinhook Bourbon War"
};
static const char* p_bbn_frey_ranch[] = {
    "Frey Ranch Bourbon", "Frey Ranch SiB"
};
static const char* p_bbn_blue_run[] = {
    "Blue Run High Rye", "Blue Run Reflection"
};
static const char* p_bbn_mb_roland[] = {
    "MB Roland Bourbon", "MB Roland Dark Fired"
};
static const char* p_bbn_redemption[] = {
    "Redemption Wheated"
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

static const char* p_tn_jack[] = {
    "Old No. 7", "Gentleman Jack", "Single Barrel Sel",
    "Single Barrel BP", "Sinatra Select", "Tennessee Rye", "Bonded"
};
static const char* p_tn_dickel[] = {
    "Dickel No. 8", "Dickel No. 12", "Dickel BiB",
    "Dickel Barrel Select", "Dickel Single Barrel"
};
static const char* p_tn_uncle_nearest[] = {
    "Uncle Nearest 1856", "Uncle Nearest 1884",
    "Uncle Nearest SiB", "Uncle Nearest Master"
};
static const char* p_tn_nelsons[] = {
    "Nelson's First 108", "Nelson's Reserve", "Nelson's Classic"
};
static const char* p_tn_chattanooga[] = {
    "Chattanooga 91", "Chattanooga 111",
    "Chattanooga Cask", "Chattanooga SiB"
};
static const char* p_tn_corsair[] = {
    "Corsair Triple Smoke", "Corsair Ryemaggedon"
};
static const char* p_tn_popcorn[] = {
    "Popcorn Sutton SmB", "Popcorn Sutton SiB"
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

static const char* p_rye_whistlepig[] = {
    "WhistlePig 6yr", "WhistlePig 10yr", "WhistlePig 12yr",
    "WhistlePig 15yr", "WhistlePig 18yr", "WhistlePig PiggyBack",
    "WhistlePig Boss Hog"
};
static const char* p_rye_sazerac[] = {
    "Sazerac Rye 6yr", "Thomas H. Handy"
};
static const char* p_rye_high_west[] = {
    "High West Double Rye", "High West Rendezvous",
    "High West Bourye", "High West Midwinter"
};
static const char* p_rye_pikesville[] = {
    "Pikesville 110"
};
static const char* p_rye_rittenhouse[] = {
    "Rittenhouse BiB"
};
static const char* p_rye_old_overholt[] = {
    "Old Overholt", "Old Overholt BiB", "Old Overholt 114"
};
static const char* p_rye_sagamore[] = {
    "Sagamore Signature", "Sagamore Cask Str",
    "Sagamore Double Oak", "Sagamore Barrel Sel"
};
static const char* p_rye_templeton[] = {
    "Templeton 4yr", "Templeton 6yr",
    "Templeton 10yr", "Templeton Barrel Str"
};
static const char* p_rye_few[] = {
    "FEW Straight Rye", "FEW Single Barrel"
};
static const char* p_rye_barrell[] = {
    "Barrell Rye", "Barrell Seagrass", "Barrell Dovetail",
    "Barrell Vantage", "Barrell Gold Label"
};
static const char* p_rye_willet[] = {
    "Willet Family Rye", "Willet 4yr Rye"
};
static const char* p_rye_dads_hat[] = {
    "Dad's Hat Rye", "Dad's Hat Vermouth"
};
static const char* p_rye_lock_stock[] = {
    "Lock Stock 13yr", "Lock Stock 16yr",
    "Lock Stock 18yr", "Lock Stock 20yr"
};
// Rye products moved from bourbon distilleries:
static const char* p_rye_wild_turkey[] = {
    "Wild Turkey 101 Rye", "Russell's Res Rye"
};
static const char* p_rye_woodford[] = {
    "Woodford Reserve Rye"
};
static const char* p_rye_bulleit[] = {
    "Bulleit Rye"
};
static const char* p_rye_angels_envy[] = {
    "Angel's Envy Rye"
};
static const char* p_rye_michters[] = {
    "Michter's US*1 Rye"
};
static const char* p_rye_knob_creek[] = {
    "Knob Creek Rye 7yr", "Knob Creek SiB Rye"
};
static const char* p_rye_old_forester[] = {
    "Old Forester Rye"
};
static const char* p_rye_new_riff[] = {
    "New Riff Rye"
};
static const char* p_rye_peerless[] = {
    "Peerless Rye"
};
static const char* p_rye_rabbit_hole[] = {
    "Boxergrail Rye"
};
static const char* p_rye_castle_key[] = {
    "Restoration Rye"
};
static const char* p_rye_redemption[] = {
    "Redemption High Rye", "Redemption BP Rye"
};
static const char* p_rye_wilderness[] = {
    "WT Rye BiB"
};
static const char* p_rye_town_branch[] = {
    "Town Branch Rye"
};
static const char* p_rye_frey_ranch[] = {
    "Frey Ranch Rye"
};
static const char* p_rye_pinhook[] = {
    "Pinhook Rye Humor"
};
static const char* p_rye_blue_run[] = {
    "Blue Run Golden Rye", "Blue Run Emerald Rye"
};
static const char* p_rye_four_roses[] = {
    "Four Roses SiB Rye"
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

static const char* p_sc_glenfiddich[] = {
    "Glenfiddich 12yr", "Glenfiddich 14yr", "Glenfiddich 15yr",
    "Glenfiddich 18yr", "Glenfiddich 21yr", "Glenfiddich Fire&Cane",
    "Glenfiddich IPA Cask"
};
static const char* p_sc_glenlivet[] = {
    "Glenlivet 12yr", "Glenlivet 14yr", "Glenlivet 15yr",
    "Glenlivet 18yr", "Glenlivet 21yr", "Glenlivet Nadurra",
    "Glenlivet Caribbean"
};
static const char* p_sc_macallan[] = {
    "Macallan 12 Sherry", "Macallan 12 Dbl Cask",
    "Macallan 15 Dbl Cask", "Macallan 18 Sherry",
    "Macallan 18 Dbl Cask", "Macallan Rare Cask",
    "Macallan Classic Cut"
};
static const char* p_sc_balvenie[] = {
    "Balvenie 12 DblWood", "Balvenie 14 Carib",
    "Balvenie 17 DblWood", "Balvenie 21 PortWood", "Balvenie 25yr"
};
static const char* p_sc_aberlour[] = {
    "Aberlour 12yr", "Aberlour 12 NCF", "Aberlour 16yr",
    "Aberlour A'Bunadh", "Aberlour Casg Annamh"
};
static const char* p_sc_glenfarclas[] = {
    "Glenfarclas 10yr", "Glenfarclas 12yr", "Glenfarclas 15yr",
    "Glenfarclas 17yr", "Glenfarclas 21yr", "Glenfarclas 25yr",
    "Glenfarclas 105"
};
static const char* p_sc_glenmorangie[] = {
    "Glenmorangie Original", "Glenmorangie Lasanta",
    "Glenmorangie Quinta", "Glenmorangie Nectar",
    "Glenmorangie 18yr", "Glenmorangie Signet"
};
static const char* p_sc_dalmore[] = {
    "Dalmore 12yr", "Dalmore 15yr", "Dalmore 18yr",
    "Dalmore Cigar Malt", "Dalmore King Alex"
};
static const char* p_sc_highland_park[] = {
    "Highland Park 12yr", "Highland Park 15yr",
    "Highland Park 18yr", "Highland Park 25yr",
    "Highland Park CS"
};
static const char* p_sc_laphroaig[] = {
    "Laphroaig 10yr", "Laphroaig 10 CS",
    "Laphroaig Quarter", "Laphroaig Select",
    "Laphroaig Lore", "Laphroaig 25yr"
};
static const char* p_sc_ardbeg[] = {
    "Ardbeg 10yr", "Ardbeg Uigeadail", "Ardbeg Corryvreckan",
    "Ardbeg An Oa", "Ardbeg Wee Beastie"
};
static const char* p_sc_lagavulin[] = {
    "Lagavulin 8yr", "Lagavulin 16yr",
    "Lagavulin DE", "Lagavulin 12 CS"
};
static const char* p_sc_bruichladdich[] = {
    "Bruichladdich Classic", "Port Charlotte 10yr",
    "Octomore"
};
static const char* p_sc_bowmore[] = {
    "Bowmore 12yr", "Bowmore 15yr", "Bowmore 18yr",
    "Bowmore 25yr", "Bowmore No. 1"
};
static const char* p_sc_bunnahabhain[] = {
    "Bunnahabhain 12yr", "Bunnahabhain 18yr",
    "Bunnahabhain 25yr", "Bunnahabhain Stiuir",
    "Bunnahabhain Toiteach"
};
static const char* p_sc_talisker[] = {
    "Talisker 10yr", "Talisker Storm", "Talisker 18yr",
    "Talisker 25yr", "Talisker DE", "Talisker Port Ruighe"
};
static const char* p_sc_oban[] = {
    "Oban 14yr", "Oban Little Bay", "Oban 18yr", "Oban DE"
};
static const char* p_sc_springbank[] = {
    "Springbank 10yr", "Springbank 12 CS", "Springbank 15yr",
    "Springbank 18yr", "Springbank 21yr",
    "Hazelburn 10yr", "Longrow Peated"
};
static const char* p_sc_dalwhinnie[] = {
    "Dalwhinnie 15yr", "Dalwhinnie DE", "Dalwhinnie Winter"
};
static const char* p_sc_cragganmore[] = {
    "Cragganmore 12yr", "Cragganmore DE"
};
static const char* p_sc_clynelish[] = {
    "Clynelish 14yr", "Clynelish DE"
};
static const char* p_sc_jura[] = {
    "Jura 10yr", "Jura 12yr", "Jura 18yr", "Jura Seven Wood"
};
static const char* p_sc_tomatin[] = {
    "Tomatin 12yr", "Tomatin 14yr Port", "Tomatin 18yr",
    "Tomatin Legacy", "Tomatin Cu Bocan"
};
static const char* p_sc_benriach[] = {
    "BenRiach Original", "BenRiach Smoky 10",
    "BenRiach The Twelve", "BenRiach 21yr",
    "BenRiach Curiositas"
};
static const char* p_sc_glendronach[] = {
    "GlenDronach 12yr", "GlenDronach 15yr", "GlenDronach 18yr",
    "GlenDronach 21yr", "GlenDronach CS"
};
static const char* p_sc_caol_ila[] = {
    "Caol Ila 12yr", "Caol Ila 18yr", "Caol Ila DE", "Caol Ila Moch"
};
static const char* p_sc_glenkinchie[] = {
    "Glenkinchie 12yr", "Glenkinchie DE"
};
static const char* p_sc_auchentoshan[] = {
    "Auchentoshan 12yr", "Auchentoshan ThreeWd",
    "Auchentoshan American"
};
static const char* p_sc_edradour[] = {
    "Edradour 10yr", "Edradour 12yr CS", "Edradour Caledonia"
};
static const char* p_sc_craigellachie[] = {
    "Craigellachie 13yr", "Craigellachie 17yr", "Craigellachie 23yr"
};
static const char* p_sc_glen_moray[] = {
    "Glen Moray Classic", "Glen Moray 12yr", "Glen Moray 15yr",
    "Glen Moray 18yr", "Glen Moray Sherry"
};
static const char* p_sc_cardhu[] = {
    "Cardhu 12yr", "Cardhu Gold Reserve", "Cardhu 15yr"
};
static const char* p_sc_deanston[] = {
    "Deanston 12yr", "Deanston 18yr", "Deanston Virgin Oak"
};
static const char* p_sc_tobermory[] = {
    "Tobermory 12yr", "Tobermory 20yr", "Ledaig 10yr", "Ledaig 18yr"
};
static const char* p_sc_kilchoman[] = {
    "Kilchoman Machir Bay", "Kilchoman Sanaig",
    "Kilchoman 100% Islay", "Kilchoman Loch Gorm"
};
static const char* p_sc_pulteney[] = {
    "Old Pulteney 12yr", "Old Pulteney 15yr",
    "Old Pulteney 18yr", "Old Pulteney Huddart"
};
static const char* p_sc_benromach[] = {
    "Benromach 10yr", "Benromach 15yr",
    "Benromach Peat Smoke", "Benromach CS"
};
static const char* p_sc_mortlach[] = {
    "Mortlach 12yr", "Mortlach 16yr", "Mortlach 20yr"
};
static const char* p_sc_glen_scotia[] = {
    "Glen Scotia 15yr", "Glen Scotia 18yr", "Glen Scotia 25yr",
    "Glen Scotia Victoriana", "Glen Scotia Dbl Cask"
};
static const char* p_sc_tomintoul[] = {
    "Tomintoul 10yr", "Tomintoul 14yr", "Tomintoul 16yr",
    "Tomintoul Peaty Tang"
};
static const char* p_sc_royal_brackla[] = {
    "Royal Brackla 12yr", "Royal Brackla 16yr", "Royal Brackla 21yr"
};
static const char* p_sc_speyburn[] = {
    "Speyburn 10yr", "Speyburn 15yr", "Speyburn 18yr"
};
static const char* p_sc_ancnoc[] = {
    "anCnoc 12yr", "anCnoc 18yr", "anCnoc 24yr", "anCnoc Peatheart"
};
static const char* p_sc_balblair[] = {
    "Balblair 12yr", "Balblair 15yr", "Balblair 18yr", "Balblair 25yr"
};
static const char* p_sc_glencadam[] = {
    "Glencadam 10yr", "Glencadam 13yr", "Glencadam 15yr",
    "Glencadam 21yr"
};
static const char* p_sc_aberfeldy[] = {
    "Aberfeldy 12yr", "Aberfeldy 16yr", "Aberfeldy 21yr"
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

static const char* p_sb_jw[] = {
    "JW Red Label", "JW Black Label", "JW Double Black",
    "JW Green Label", "JW Gold Label Res", "JW Blue Label", "JW 18yr"
};
static const char* p_sb_chivas[] = {
    "Chivas 12yr", "Chivas 18yr", "Chivas 25yr",
    "Chivas XV", "Chivas Mizunara", "Chivas Extra 13yr"
};
static const char* p_sb_dewars[] = {
    "Dewar's White Label", "Dewar's 12yr", "Dewar's 15yr",
    "Dewar's 18yr", "Dewar's 25yr", "Dewar's Caribbean"
};
static const char* p_sb_monkey[] = {
    "Monkey Shoulder", "Smokey Monkey"
};
static const char* p_sb_compass[] = {
    "CB Oak Cross", "CB Spice Tree", "CB Peat Monster",
    "CB Hedonism", "CB Great King", "CB Glasgow"
};
static const char* p_sb_grouse[] = {
    "Famous Grouse", "Famous Grouse Smoky", "Naked Malt"
};
static const char* p_sb_ballantines[] = {
    "Ballantine's Finest", "Ballantine's 12yr",
    "Ballantine's 17yr", "Ballantine's 21yr"
};
static const char* p_sb_cutty[] = {
    "Cutty Sark Original", "Cutty Sark Prohib"
};
static const char* p_sb_grants[] = {
    "Grant's Triple Wood", "Grant's 12yr", "Grant's 18yr"
};
static const char* p_sb_buchanans[] = {
    "Buchanan's 12yr", "Buchanan's 15yr", "Buchanan's 18yr"
};
static const char* p_sb_teachers[] = {
    "Teacher's Highland"
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

static const char* p_ir_midleton[] = {
    "Jameson Original", "Jameson Black Barrel", "Jameson 18yr",
    "Redbreast 12yr", "Redbreast 12 CS", "Redbreast 15yr",
    "Redbreast 21yr", "Redbreast Lustau",
    "Green Spot", "Yellow Spot", "Blue Spot", "Red Spot",
    "Powers Gold Label", "Powers John's Lane",
    "Midleton Very Rare", "Method & Madness"
};
static const char* p_ir_bushmills[] = {
    "Bushmills Original", "Black Bush",
    "Bushmills 10yr", "Bushmills 12yr",
    "Bushmills 16yr", "Bushmills 21yr"
};
static const char* p_ir_tullamore[] = {
    "Tullamore D.E.W.", "Tullamore 12yr",
    "Tullamore 14yr SM", "Tullamore 18yr SM", "Tullamore XO"
};
static const char* p_ir_teeling[] = {
    "Teeling Small Batch", "Teeling Single Grain",
    "Teeling Single Malt", "Teeling Single Pot",
    "Teeling Blackpitts"
};
static const char* p_ir_dingle[] = {
    "Dingle Single Malt", "Dingle Single Pot"
};
static const char* p_ir_kilbeggan[] = {
    "Kilbeggan Original", "Kilbeggan SiG", "Kilbeggan SiM"
};
static const char* p_ir_connemara[] = {
    "Connemara Original", "Connemara 12yr",
    "Connemara Cask Str", "Connemara Turf Mor"
};
static const char* p_ir_tyrconnell[] = {
    "Tyrconnell SiM", "Tyrconnell 10 Sherry",
    "Tyrconnell 10 Port", "Tyrconnell 16yr"
};
static const char* p_ir_west_cork[] = {
    "West Cork Original", "West Cork 8yr", "West Cork 10yr",
    "West Cork 12yr", "West Cork Bog Oak", "West Cork CS"
};
static const char* p_ir_glendalough[] = {
    "Glendalough Double", "Glendalough 7yr",
    "Glendalough 13yr", "Glendalough Pot Stl"
};
static const char* p_ir_writers[] = {
    "Writers' Tears Cppr", "Writers' Tears DblOk",
    "Writers' Tears CS"
};
static const char* p_ir_proper12[] = {
    "Proper No. Twelve"
};
static const char* p_ir_knappogue[] = {
    "Knappogue 12yr", "Knappogue 14yr", "Knappogue 16yr"
};
static const char* p_ir_keepers[] = {
    "Keeper's Heart Irish", "Keeper's Heart Rye"
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

static const char* p_jp_yamazaki[] = {
    "Yamazaki 12yr", "Yamazaki 18yr", "Yamazaki 25yr",
    "Yamazaki Distiller's"
};
static const char* p_jp_hakushu[] = {
    "Hakushu 12yr", "Hakushu 18yr", "Hakushu 25yr",
    "Hakushu Distiller's"
};
static const char* p_jp_hibiki[] = {
    "Hibiki Harmony", "Hibiki 17yr", "Hibiki 21yr",
    "Hibiki 30yr", "Hibiki Blender's Ch"
};
static const char* p_jp_suntory_other[] = {
    "Suntory Toki", "The Chita SiG"
};
static const char* p_jp_yoichi[] = {
    "Yoichi Single Malt", "Yoichi 10yr", "Yoichi 15yr"
};
static const char* p_jp_miyagikyo[] = {
    "Miyagikyo SiM", "Miyagikyo 12yr", "Miyagikyo 15yr"
};
static const char* p_jp_nikka_other[] = {
    "Nikka From Barrel", "Nikka Coffey Grain",
    "Nikka Coffey Malt", "Nikka Taketsuru",
    "Nikka Days", "Nikka Session"
};
static const char* p_jp_mars[] = {
    "Mars Iwai Tradition", "Mars Iwai 45",
    "Mars Komagatake", "Mars Cosmo"
};
static const char* p_jp_chichibu[] = {
    "Ichiro's Malt&Grain", "Ichiro's Malt Double",
    "Ichiro's Chichibu"
};
static const char* p_jp_akkeshi[] = {
    "Akkeshi Peated SiM", "Akkeshi Blended"
};
static const char* p_jp_white_oak[] = {
    "Akashi White Oak", "Akashi Single Malt", "Akashi Sherry Cask"
};
static const char* p_jp_kaiyo[] = {
    "Kaiyo Whisky", "Kaiyo Peated", "Kaiyo Cask Strength",
    "Kaiyo The Single 7yr"
};
static const char* p_jp_kurayoshi[] = {
    "Kurayoshi Pure Malt", "Kurayoshi 8yr",
    "Kurayoshi 12yr", "Kurayoshi 18yr"
};
static const char* p_jp_ohishi[] = {
    "Ohishi Sherry Cask", "Ohishi Brandy Cask", "Ohishi Sakura Cask"
};
static const char* p_jp_togouchi[] = {
    "Togouchi Premium", "Togouchi 9yr",
    "Togouchi 15yr", "Togouchi Peated"
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

static const char* p_ca_crown[] = {
    "Crown Royal Deluxe", "Crown Royal Black", "Crown Royal Res",
    "Crown Royal XR", "Crown Royal XO", "Crown Royal Northern",
    "Crown Royal Peach", "Crown Royal Apple"
};
static const char* p_ca_cc[] = {
    "Canadian Club 1858", "CC 100% Rye", "CC 12yr", "CC 20yr"
};
static const char* p_ca_lot40[] = {
    "Lot No. 40", "Lot No. 40 CS", "Lot No. 40 Dark Oak"
};
static const char* p_ca_pike[] = {
    "Pike Creek 10yr", "Pike Creek 21yr"
};
static const char* p_ca_wisers[] = {
    "JP Wiser's Deluxe", "JP Wiser's 15yr",
    "JP Wiser's 18yr", "JP Wiser's 23yr"
};
static const char* p_ca_alberta[] = {
    "Alberta Premium", "Alberta Premium CS", "Alberta Prem 20yr"
};
static const char* p_ca_forty_creek[] = {
    "Forty Creek Barrel", "Forty Creek Conf Rsv",
    "Forty Creek Double"
};
static const char* p_ca_pendleton[] = {
    "Pendleton Original", "Pendleton 1910", "Pendleton Midnight"
};
static const char* p_ca_collingwood[] = {
    "Collingwood Original", "Collingwood 21yr", "Collingwood Toasted"
};
static const char* p_ca_glen_breton[] = {
    "Glen Breton Rare 10", "Glen Breton Ice 10",
    "Glen Breton 14yr", "Glen Breton 19yr"
};
static const char* p_ca_caribou[] = {
    "Caribou Crossing SiB"
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

static const char* p_wo_kavalan[] = {
    "Kavalan Classic", "Kavalan Concertmastr",
    "Kavalan Ex-Bourbon", "Kavalan Solist Shrry",
    "Kavalan Solist Brbn"
};
static const char* p_wo_amrut[] = {
    "Amrut Single Malt", "Amrut Fusion", "Amrut Peated",
    "Amrut Cask Strength", "Amrut Portonova"
};
static const char* p_wo_paul_john[] = {
    "Paul John Brilliance", "Paul John Edited",
    "Paul John Bold", "Paul John Classic",
    "Paul John Peated"
};
static const char* p_wo_starward[] = {
    "Starward Nova", "Starward Solera", "Starward Two-Fold",
    "Starward Fortis", "Starward Left-Field"
};
static const char* p_wo_westward[] = {
    "Westward American SM", "Westward Stout Cask",
    "Westward Pinot Noir", "Westward Cask Str"
};
static const char* p_wo_westland[] = {
    "Westland American SM", "Westland Sherry Wood",
    "Westland Peated", "Westland Garryana"
};
static const char* p_wo_stranahans[] = {
    "Stranahan's Original", "Stranahan's Blue Peak",
    "Stranahan's Sherry"
};
static const char* p_wo_balcones[] = {
    "Balcones Texas SiM", "Balcones Baby Blue",
    "Balcones True Blue", "Balcones Brimstone",
    "Balcones Lineage"
};
static const char* p_wo_virginia[] = {
    "Courage & Conviction", "C&C Cask Strength",
    "C&C Sherry Cask", "C&C Bourbon Cask"
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
