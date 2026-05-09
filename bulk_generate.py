#!/usr/bin/env python3
"""bulk_generate.py — Run spotify2beat.py for a curated list of ~250 songs to
populate the WillyBeat library across many genres and niche subgenres.

Each entry is (search_query, comma-separated genre tags).  The runner calls
spotify2beat.py with --auto --stems --genre <tags>, logs progress, and
continues on per-song failures so a single bad query won't kill the batch.

Usage:
    .venv/bin/python bulk_generate.py            # run all queries
    .venv/bin/python bulk_generate.py --skip 50  # skip the first 50 entries
"""

import argparse
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
PRESETS_DIR = Path.home() / "Library/Application Support/WillyBeat/Presets"
LOG_FILE = ROOT / "bulk_generate.log"
PYTHON = ROOT / ".venv/bin/python"


# ─── Curated song list (~250 entries) ─────────────────────────────────────────
# Format: (spotify search query, "Tag1, Tag2, ...")
# Tags become the .beat file's `genres:` field — these drive WillyBeat's
# tag-based pattern selection and density-augmentation pool.
QUERIES = [
    # ── Classic / arena rock ──────────────────────────────────────────────
    ("Smells Like Teen Spirit Nirvana",          "Rock, Grunge, 90s"),
    ("Stairway to Heaven Led Zeppelin",          "Rock, Classic Rock"),
    ("Bohemian Rhapsody Queen",                  "Rock, Classic Rock"),
    ("Sweet Child o Mine Guns N Roses",          "Rock, Hard Rock"),
    ("Hotel California Eagles",                  "Rock, Soft Rock"),
    ("Back in Black AC/DC",                      "Rock, Hard Rock"),
    ("Born to Run Bruce Springsteen",            "Rock, Heartland Rock"),
    ("Whole Lotta Love Led Zeppelin",            "Rock, Hard Rock, Blues Rock"),
    ("Tom Sawyer Rush",                          "Rock, Prog Rock"),
    ("Don't Stop Believin Journey",              "Rock, Arena Rock"),
    ("Walk This Way Aerosmith",                  "Rock, Hard Rock"),
    ("Mr Brightside The Killers",                "Rock, Indie Rock, Alternative"),
    ("Use Somebody Kings of Leon",               "Rock, Indie Rock"),
    ("Seven Nation Army White Stripes",          "Rock, Garage Rock"),
    ("Smoke on the Water Deep Purple",           "Rock, Classic Rock"),
    ("Crazy Train Ozzy Osbourne",                "Rock, Heavy Metal"),
    ("Run to the Hills Iron Maiden",             "Rock, Heavy Metal, NWOBHM"),
    ("Roxanne The Police",                       "Rock, New Wave"),
    ("Just What I Needed The Cars",              "Rock, New Wave"),
    ("Welcome to the Jungle Guns N Roses",       "Rock, Hard Rock"),
    ("More Than a Feeling Boston",               "Rock, Classic Rock"),
    ("Black Dog Led Zeppelin",                   "Rock, Hard Rock"),
    ("Highway to Hell ACDC",                     "Rock, Hard Rock"),
    ("The Boys Are Back in Town Thin Lizzy",     "Rock, Classic Rock"),
    ("Like a Rolling Stone Bob Dylan",           "Rock, Folk Rock"),

    # ── Mainstream pop ────────────────────────────────────────────────────
    ("Billie Jean Michael Jackson",              "Pop, R&B, Disco"),
    ("Like a Prayer Madonna",                    "Pop, Dance Pop"),
    ("I Want It That Way Backstreet Boys",       "Pop, Boy Band"),
    ("Baby One More Time Britney Spears",        "Pop, Dance Pop"),
    ("Bad Romance Lady Gaga",                    "Pop, Electropop"),
    ("Shake it Off Taylor Swift",                "Pop, Synth-Pop"),
    ("Rolling in the Deep Adele",                "Pop, Soul, Blue-Eyed Soul"),
    ("Uptown Funk Mark Ronson",                  "Pop, Funk, Disco"),
    ("Watermelon Sugar Harry Styles",            "Pop, Indie Pop"),
    ("As It Was Harry Styles",                   "Pop, Indie Pop, Synth-Pop"),
    ("Blinding Lights The Weeknd",               "Pop, Synth-Pop, 80s"),
    ("Bad Guy Billie Eilish",                    "Pop, Alt Pop, Electropop"),
    ("Levitating Dua Lipa",                      "Pop, Disco, Dance Pop"),
    ("Drivers License Olivia Rodrigo",           "Pop, Indie Pop, Bedroom Pop"),
    ("Shape of You Ed Sheeran",                  "Pop, Tropical House"),
    ("Closer The Chainsmokers",                  "Pop, EDM, Future Bass"),
    ("Counting Stars OneRepublic",               "Pop, Pop Rock"),
    ("Happier Than Ever Billie Eilish",          "Pop, Alt Pop"),
    ("good 4 u Olivia Rodrigo",                  "Pop, Pop Punk"),
    ("Espresso Sabrina Carpenter",               "Pop, Dance Pop"),

    # ── Hip-Hop / rap (broad) ─────────────────────────────────────────────
    ("Funky Drummer James Brown",                "Funk, Soul, Hip-Hop Sample"),
    ("Nuthin But a G Thang Dr Dre",              "Hip-Hop, G-Funk"),
    ("Juicy Notorious BIG",                      "Hip-Hop, Boom Bap, East Coast"),
    ("C.R.E.A.M. Wu Tang Clan",                  "Hip-Hop, Boom Bap, East Coast"),
    ("Lose Yourself Eminem",                     "Hip-Hop"),
    ("In Da Club 50 Cent",                       "Hip-Hop, Gangsta Rap"),
    ("Hey Ya Outkast",                           "Hip-Hop, Funk, Pop"),
    ("Empire State of Mind Jay-Z",               "Hip-Hop"),
    ("All of the Lights Kanye West",             "Hip-Hop, Maximalist Pop"),
    ("Sicko Mode Travis Scott",                  "Hip-Hop, Trap"),
    ("HUMBLE Kendrick Lamar",                    "Hip-Hop, West Coast"),
    ("Money Trees Kendrick Lamar",               "Hip-Hop, Conscious Hip-Hop"),
    ("Gold Digger Kanye West",                   "Hip-Hop, Soul Sample"),
    ("Mask Off Future",                          "Hip-Hop, Trap"),
    ("Bodak Yellow Cardi B",                     "Hip-Hop, Trap"),
    ("Mo Bamba Sheck Wes",                       "Hip-Hop, Trap"),
    ("Bad and Boujee Migos",                     "Hip-Hop, Trap"),
    ("Look At Me XXXTentacion",                  "Hip-Hop, Trap, Punk Rap"),
    ("POWER Kanye West",                         "Hip-Hop"),
    ("Drop It Like Its Hot Snoop Dogg",          "Hip-Hop, West Coast"),
    ("Get Ur Freak On Missy Elliott",            "Hip-Hop"),
    ("Hot in Herre Nelly",                       "Hip-Hop, Pop Rap"),
    ("Industry Baby Lil Nas X",                  "Hip-Hop, Trap"),
    ("Knuck If You Buck Crime Mob",              "Hip-Hop, Crunk"),
    ("Get Low Lil Jon",                          "Hip-Hop, Crunk"),

    # ── UK rap / drill ────────────────────────────────────────────────────
    ("Headie One Both",                          "Hip-Hop, UK Drill"),
    ("Pop Smoke Welcome to the Party",           "Hip-Hop, Drill, Brooklyn Drill"),
    ("Central Cee Doja",                         "Hip-Hop, UK Drill"),
    ("Sheff G No Suburban",                      "Hip-Hop, Brooklyn Drill"),
    ("Russ Millions Body",                       "Hip-Hop, UK Drill"),

    # ── R&B / Soul / Funk ────────────────────────────────────────────────
    ("Superstition Stevie Wonder",               "Funk, Soul"),
    ("Got to Give it Up Marvin Gaye",            "Funk, Soul, Disco"),
    ("Pick Up the Pieces AWB",                   "Funk, Disco"),
    ("I Got You I Feel Good James Brown",        "Funk, Soul"),
    ("Cold Sweat James Brown",                   "Funk, Soul"),
    ("Brick House Commodores",                   "Funk, Soul, Disco"),
    ("Le Freak Chic",                            "Funk, Disco"),
    ("Good Times Chic",                          "Funk, Disco"),
    ("Flash Light Parliament",                   "Funk, P-Funk"),
    ("Give Up the Funk Parliament",              "Funk, P-Funk"),
    ("Atomic Dog George Clinton",                "Funk, P-Funk, Electro"),
    ("Crazy in Love Beyonce",                    "R&B, Pop, Funk"),
    ("No Scrubs TLC",                            "R&B, Pop"),
    ("Say My Name Destinys Child",               "R&B, Pop"),
    ("Adorn Miguel",                             "R&B, Alt R&B"),
    ("Earned It The Weeknd",                     "R&B, Alt R&B"),
    ("Ye D'Angelo",                              "R&B, Neo-Soul"),
    ("Tyrone Erykah Badu",                       "R&B, Neo-Soul"),
    ("Cranes in the Sky Solange",                "R&B, Neo-Soul"),
    ("That's Life Frank Sinatra",                "Vocal Jazz, Big Band"),

    # ── Electronic / Dance: classic + modern ─────────────────────────────
    ("Smack My Bitch Up Prodigy",                "Electronic, Big Beat, Breakbeat"),
    ("Around the World Daft Punk",               "Electronic, French House"),
    ("One More Time Daft Punk",                  "Electronic, French House, Disco"),
    ("Sandstorm Darude",                         "Electronic, Trance"),
    ("Levels Avicii",                            "Electronic, EDM, Progressive House"),
    ("Don't You Worry Child Swedish House Mafia","Electronic, EDM, Progressive House"),
    ("Strobe deadmau5",                          "Electronic, Progressive House"),
    ("Animals Martin Garrix",                    "Electronic, EDM, Big Room"),
    ("Cinema Skrillex",                          "Electronic, Dubstep"),
    ("Bangarang Skrillex",                       "Electronic, Dubstep"),
    ("Lyla Bonobo",                              "Electronic, Downtempo"),
    ("Teardrop Massive Attack",                  "Electronic, Trip-Hop"),
    ("Angel Massive Attack",                     "Electronic, Trip-Hop"),
    ("Resonance Home",                           "Electronic, Synthwave, Vaporwave"),
    ("Idioteque Radiohead",                      "Electronic, Glitch, IDM"),
    ("Reckless Daft Punk",                       "Electronic, French House"),
    ("Strings of Life Derrick May",              "Electronic, Detroit Techno"),
    ("Plastic Dreams Jaydee",                    "Electronic, Deep House"),
    ("Dominator Human Resource",                 "Electronic, Hardcore"),
    ("Belfast Orbital",                          "Electronic, Ambient, IDM"),
    ("Windowlicker Aphex Twin",                  "Electronic, IDM"),
    ("Come to Daddy Aphex Twin",                 "Electronic, IDM"),
    ("Bonus Round Justice",                      "Electronic, French Touch"),
    ("D.A.N.C.E. Justice",                       "Electronic, French Touch"),
    ("Music Sounds Better with You Stardust",    "Electronic, French House"),
    ("On The Run Pink Floyd",                    "Rock, Prog Rock, Electronic"),

    # ── House subgenres ──────────────────────────────────────────────────
    ("Show Me Love Robin S",                     "Electronic, House, 90s House"),
    ("Pacific State 808 State",                  "Electronic, Acid House"),
    ("Sueño Latino Sueño Latino",                "Electronic, House, Italo House"),
    ("Strings of Life Derrick May Mayday",       "Electronic, Detroit Techno"),
    ("Music Is the Answer Celeda",               "Electronic, House"),
    ("Let Me Show You CamelPhat",                "Electronic, Tech House"),
    ("Cola CamelPhat",                           "Electronic, Tech House"),
    ("Inspector Norse Todd Terje",               "Electronic, Nu-Disco"),
    ("I Feel Love Donna Summer",                 "Disco, Electronic, Hi-NRG"),
    ("Move Your Feet Junior Senior",             "Electronic, Dance"),

    # ── Techno subgenres ─────────────────────────────────────────────────
    ("Spastik Plastikman",                       "Electronic, Techno, Acid Techno"),
    ("Energy Flash Joey Beltram",                "Electronic, Techno"),
    ("LFO LFO",                                  "Electronic, Bleep, Techno"),
    ("Hot On The Heels of Love Throbbing Gristle","Electronic, Industrial"),
    ("Konichiwa Bitches Robyn",                  "Pop, Electropop"),

    # ── Trance / Hardstyle ───────────────────────────────────────────────
    ("Children Robert Miles",                    "Electronic, Trance, Dream Trance"),
    ("Adagio for Strings Tiesto",                "Electronic, Trance"),
    ("For an Angel Paul van Dyk",                "Electronic, Trance"),
    ("Brain Power Noma",                         "Electronic, Hardstyle"),

    # ── Drum & Bass / Jungle ─────────────────────────────────────────────
    ("Watercolour Pendulum",                     "Electronic, DnB"),
    ("Out the Blue Sub Focus",                   "Electronic, DnB, Liquid DnB"),
    ("Inner City Life Goldie",                   "Electronic, DnB, Atmospheric DnB"),
    ("Original Nuttah Shy FX",                   "Electronic, DnB, Jungle"),
    ("Atlantis LTJ Bukem",                       "Electronic, DnB, Atmospheric"),
    ("Heartbeat Loud Andy C",                    "Electronic, DnB"),

    # ── UK Garage / 2-Step / Future Garage ───────────────────────────────
    ("Archangel Burial",                         "Electronic, Future Garage, Dubstep"),
    ("Latch Disclosure",                         "Electronic, UK Garage, House"),
    ("Hyph Mngo Joy Orbison",                    "Electronic, UK Garage, House"),
    ("Re-Rewind Artful Dodger",                  "Electronic, 2-Step, UK Garage"),
    ("Flowers Sweet Female Attitude",            "Electronic, UK Garage"),

    # ── IDM / Glitch / Experimental ──────────────────────────────────────
    ("Avril 14th Aphex Twin",                    "Electronic, IDM, Ambient"),
    ("Xtal Aphex Twin",                          "Electronic, IDM, Ambient Techno"),
    ("Polynomial-C Aphex Twin",                  "Electronic, Acid, IDM"),
    ("Roygbiv Boards of Canada",                 "Electronic, IDM, Ambient"),
    ("Telephasic Workshop Boards of Canada",     "Electronic, IDM"),
    ("Glaroon FM Belew",                         "Electronic, IDM, Glitch"),
    ("Squarepusher Beep Street",                 "Electronic, IDM, Drill n Bass"),
    ("Birthday Squarepusher",                    "Electronic, IDM"),
    ("Autechre Gantz Graf",                      "Electronic, IDM, Glitch"),

    # ── Footwork / Juke ──────────────────────────────────────────────────
    ("DJ Rashad Show U How",                     "Electronic, Footwork, Juke"),
    ("Traxman Footworkin On Air",                "Electronic, Footwork"),
    ("RP Boo Heat from Us",                      "Electronic, Footwork"),

    # ── Phonk / Memphis / Drift ──────────────────────────────────────────
    ("Murder in My Mind Kordhell",               "Electronic, Phonk, Drift Phonk"),
    ("Close Eyes DVRST",                         "Electronic, Phonk"),
    ("Mercury Ghostemane",                       "Hip-Hop, Phonk, Trap"),
    ("Sahara Hensonn",                           "Electronic, Phonk"),

    # ── Vaporwave ────────────────────────────────────────────────────────
    ("Lisa Frank 420 Macintosh Plus",            "Electronic, Vaporwave"),
    ("Strawberries Saint Pepsi",                 "Electronic, Vaporwave, Future Funk"),
    ("News at 11 ECO Virtual",                   "Electronic, Vaporwave"),
    ("Yung Lean Hurt",                           "Hip-Hop, Cloud Rap, Vaporwave"),

    # ── Synthwave / Outrun / Darksynth ───────────────────────────────────
    ("Interceptor Mitch Murder",                 "Electronic, Synthwave, Outrun"),
    ("Turbo Killer Carpenter Brut",              "Electronic, Synthwave, Darksynth"),
    ("Sunset The Midnight",                      "Electronic, Synthwave, Outrun"),
    ("Tech Noir Gunship",                        "Electronic, Synthwave"),
    ("Galactic Sundown Lazerhawk",               "Electronic, Synthwave"),

    # ── Future Bass / Future Funk ────────────────────────────────────────
    ("Shelter Porter Robinson",                  "Electronic, Future Bass"),
    ("All My Friends Snakehips",                 "Electronic, Future Bass"),
    ("Faded Alan Walker",                        "Electronic, Future Bass"),
    ("Night Tempo Diskette",                     "Electronic, Future Funk"),

    # ── Lo-fi / Chillhop ─────────────────────────────────────────────────
    ("Memories Nujabes",                         "Hip-Hop, Lo-fi, Chillhop"),
    ("Aruarian Dance Nujabes",                   "Hip-Hop, Lo-fi, Chillhop"),
    ("Comfort Idealism Tomppabeats",             "Hip-Hop, Lo-fi"),
    ("Cyber Wishes BSD.U",                       "Hip-Hop, Lo-fi"),

    # ── Big Beat / Breakbeat ─────────────────────────────────────────────
    ("Block Rockin Beats Chemical Brothers",     "Electronic, Big Beat, Breakbeat"),
    ("Galvanize Chemical Brothers",              "Electronic, Big Beat"),
    ("Praise You Fatboy Slim",                   "Electronic, Big Beat"),
    ("Right Here Right Now Fatboy Slim",         "Electronic, Big Beat"),

    # ── Dubstep / Bass / Wonky ───────────────────────────────────────────
    ("Eastern Jam Chase Status",                 "Electronic, Dubstep"),
    ("Pretty Lights Hot Like Sauce",             "Electronic, Glitch Hop"),
    ("Untrue Burial",                            "Electronic, Dubstep, Future Garage"),
    ("Spongebob Skrillex",                       "Electronic, Dubstep, Brostep"),

    # ── Trip-Hop / Downtempo ─────────────────────────────────────────────
    ("Glory Box Portishead",                     "Electronic, Trip-Hop"),
    ("Roads Portishead",                         "Electronic, Trip-Hop"),
    ("Black Steel Tricky",                       "Electronic, Trip-Hop"),
    ("Karma Coma Massive Attack",                "Electronic, Trip-Hop"),
    ("Outsider DJ Shadow",                       "Electronic, Instrumental Hip-Hop"),
    ("Building Steam with a Grain of Salt DJ Shadow","Electronic, Instrumental Hip-Hop"),

    # ── Jazz (broad) ─────────────────────────────────────────────────────
    ("Take Five Dave Brubeck",                   "Jazz, Cool Jazz"),
    ("So What Miles Davis",                      "Jazz, Modal Jazz"),
    ("A Love Supreme John Coltrane",             "Jazz, Spiritual Jazz"),
    ("Birdland Weather Report",                  "Jazz, Fusion"),
    ("Mercy Mercy Mercy Cannonball Adderley",    "Jazz, Soul Jazz"),
    ("Caravan Duke Ellington",                   "Jazz, Latin Jazz, Big Band"),
    ("Sing Sing Sing Benny Goodman",             "Jazz, Big Band, Swing"),
    ("Cantaloop Us3",                            "Jazz, Acid Jazz"),
    ("Watermelon Man Herbie Hancock",            "Jazz, Funk"),
    ("Chameleon Herbie Hancock",                 "Jazz, Funk, Fusion"),

    # ── Latin (broad) ────────────────────────────────────────────────────
    ("Oye Como Va Santana",                      "Latin, Latin Rock"),
    ("Smooth Santana",                           "Latin, Latin Rock"),
    ("Conga Gloria Estefan",                     "Latin, Pop"),
    ("Despacito Luis Fonsi",                     "Latin, Reggaeton"),
    ("Bailando Enrique Iglesias",                "Latin, Reggaeton"),
    ("Tusa Karol G",                             "Latin, Reggaeton"),
    ("Bichota Karol G",                          "Latin, Reggaeton"),
    ("Mi Gente J Balvin",                        "Latin, Reggaeton"),
    ("Hawai Maluma",                             "Latin, Reggaeton"),
    ("Garota de Ipanema",                        "Latin, Bossa Nova"),
    ("Mas Que Nada Sergio Mendes",               "Latin, Bossa Nova, Samba"),
    ("Aguas de Marco Tom Jobim",                 "Latin, Bossa Nova"),
    ("Cumbia Sobre el Mar Quantic",              "Latin, Cumbia"),
    ("Llorando Se Fue Los Kjarkas",              "Latin, Bolivian Folk"),
    ("La Bamba Ritchie Valens",                  "Latin, Rock and Roll"),

    # ── Reggae / Dub ─────────────────────────────────────────────────────
    ("No Woman No Cry Bob Marley",               "Reggae, Roots Reggae"),
    ("Three Little Birds Bob Marley",            "Reggae, Roots Reggae"),
    ("Stir It Up Bob Marley",                    "Reggae"),
    ("I Shot the Sheriff Bob Marley",            "Reggae"),
    ("Pass the Dutchie Musical Youth",           "Reggae, Pop"),

    # ── Hyperpop / Avant Pop ─────────────────────────────────────────────
    ("Money Machine 100 gecs",                   "Hyperpop, Pop"),
    ("Sticky 100 gecs",                          "Hyperpop"),
    ("Speed Drive Charli XCX",                   "Pop, Hyperpop, Electropop"),
    ("Vroom Vroom Charli XCX",                   "Pop, Hyperpop, Electropop"),
    ("Immaterial SOPHIE",                        "Hyperpop, Avant Pop"),
    ("Suffocate Glaive",                         "Hyperpop, Emo"),
    ("Trampoline Kero Kero Bonito",              "Indie Pop, Hyperpop"),

    # ── Bedroom Pop / Indie Pop ──────────────────────────────────────────
    ("Apocalypse Cigarettes After Sex",          "Dream Pop, Bedroom Pop"),
    ("Chamber of Reflection Mac DeMarco",        "Indie, Bedroom Pop"),
    ("Pretty Girl Clairo",                       "Bedroom Pop, Indie Pop"),
    ("Pristine Snail Mail",                      "Indie Rock, Bedroom Pop"),
    ("Sleep Apnea Beach Fossils",                "Dream Pop, Indie"),
    ("Death Bed Powfu",                          "Lo-fi, Bedroom Pop"),

    # ── Dream Pop / Shoegaze ─────────────────────────────────────────────
    ("Space Song Beach House",                   "Dream Pop, Shoegaze"),
    ("Sugar for the Pill Slowdive",              "Shoegaze, Dream Pop"),
    ("Only Shallow My Bloody Valentine",         "Shoegaze"),
    ("Heaven or Las Vegas Cocteau Twins",        "Dream Pop, Ethereal Wave"),
    ("Fade Into You Mazzy Star",                 "Dream Pop, Slowcore"),

    # ── Indie / Alt / Post-Punk ──────────────────────────────────────────
    ("Where Is My Mind Pixies",                  "Indie Rock, Alt Rock"),
    ("This Charming Man The Smiths",             "Indie, Jangle Pop"),
    ("Pumped Up Kicks Foster the People",        "Indie Pop, Indie"),
    ("All My Friends LCD Soundsystem",           "Indie, Dance Punk"),
    ("Daft Punk Is Playing at My House LCDS",    "Indie, Dance Punk"),
    ("Wake Up Arcade Fire",                      "Indie, Indie Rock"),
    ("Float On Modest Mouse",                    "Indie, Indie Rock"),
    ("Time to Pretend MGMT",                     "Indie, Synth-Pop"),
    ("Electric Feel MGMT",                       "Indie, Synth-Pop"),
    ("Such Great Heights Postal Service",        "Indie, Electronic, Indietronica"),

    # ── Disco / Nu-Disco ─────────────────────────────────────────────────
    ("Stayin Alive Bee Gees",                    "Disco, Pop"),
    ("I Will Survive Gloria Gaynor",             "Disco"),
    ("Daddy Cool Boney M",                       "Disco, Eurodisco"),
    ("Rasputin Boney M",                         "Disco, Eurodisco"),
    ("Get Lucky Daft Punk",                      "Disco, Funk, Nu-Disco"),
    ("Don't Stop Til You Get Enough Michael Jackson","Disco, Funk"),

    # ── Country / Folk / Blues ───────────────────────────────────────────
    ("Ring of Fire Johnny Cash",                 "Country"),
    ("Friends in Low Places Garth Brooks",       "Country"),
    ("Wagon Wheel Old Crow Medicine Show",       "Country, Folk"),
    ("Take Me Home Country Roads John Denver",   "Folk, Country"),
    ("Folsom Prison Blues Johnny Cash",          "Country"),
    ("Boom Boom John Lee Hooker",                "Blues"),
    ("Born Under a Bad Sign Albert King",        "Blues, Soul Blues"),
    ("House of the Rising Sun Animals",          "Folk Rock, Blues Rock"),
    ("Tangled Up in Blue Bob Dylan",             "Folk, Folk Rock"),
    ("Pride in the Name of Love U2",             "Rock, Heartland Rock"),

    # ── Metal subgenres ──────────────────────────────────────────────────
    ("Master of Puppets Metallica",              "Metal, Thrash"),
    ("Holy Wars Megadeth",                       "Metal, Thrash"),
    ("Painkiller Judas Priest",                  "Metal, Power Metal"),
    ("Domination Pantera",                       "Metal, Groove Metal"),
    ("Toxicity System of a Down",                "Metal, Nu-Metal, Alt Metal"),
    ("Down with the Sickness Disturbed",         "Metal, Nu-Metal"),
    ("Hammer Smashed Face Cannibal Corpse",      "Metal, Death Metal"),
    ("Raining Blood Slayer",                     "Metal, Thrash"),

    # ── Punk / Pop Punk / Emo / Hardcore ─────────────────────────────────
    ("Anarchy in the UK Sex Pistols",            "Punk, Punk Rock"),
    ("London Calling The Clash",                 "Punk, Post-Punk"),
    ("Should I Stay or Should I Go The Clash",   "Punk, Rock"),
    ("Smells Like Teen Spirit Nirvana",          "Rock, Grunge, 90s"),
    ("Basket Case Green Day",                    "Pop Punk"),
    ("All The Small Things blink-182",           "Pop Punk"),
    ("Misery Business Paramore",                 "Pop Punk, Emo"),
    ("My Chemical Romance Helena",               "Emo, Pop Punk"),
    ("I Write Sins Not Tragedies Panic at the Disco","Emo, Pop Punk"),

    # ── Afrobeat / Afropop / World ───────────────────────────────────────
    ("Water No Get Enemy Fela Kuti",             "Afrobeat"),
    ("Zombie Fela Kuti",                         "Afrobeat"),
    ("Last Last Burna Boy",                      "Afrobeats, Pop"),
    ("Essence Wizkid",                           "Afrobeats, Pop"),
    ("Fall Davido",                              "Afrobeats"),
    ("Synchro System King Sunny Ade",            "Juju, Afrobeat"),

    # ── New Wave / Post-Punk revival ─────────────────────────────────────
    ("Tainted Love Soft Cell",                   "New Wave, Synth-Pop"),
    ("Don't You Want Me Human League",           "New Wave, Synth-Pop"),
    ("Just Like Heaven The Cure",                "Post-Punk, New Wave"),
    ("Love Will Tear Us Apart Joy Division",     "Post-Punk"),
    ("Blue Monday New Order",                    "Post-Punk, Synth-Pop, House"),

    # ── Other niches ─────────────────────────────────────────────────────
    ("She Bop Cyndi Lauper",                     "Pop, New Wave"),
    ("Africa Toto",                              "Soft Rock, Yacht Rock"),
    ("Sailing Christopher Cross",                "Soft Rock, Yacht Rock"),
    ("Fool Me Once Pinegrove",                   "Indie, Emo Revival"),
    ("Cigarettes Ramble Jess Shoman",            "Indie, Slowcore"),

    # ── Gospel / contemporary ────────────────────────────────────────────
    ("Oh Happy Day Edwin Hawkins",               "Gospel, Soul"),
    ("Take Me to the King Tamela Mann",          "Gospel"),

    # ── Trap subgenres / cloud rap ───────────────────────────────────────
    ("Lucid Dreams Juice WRLD",                  "Hip-Hop, Trap, Emo Rap"),
    ("All Girls Are the Same Juice WRLD",        "Hip-Hop, Trap, Emo Rap"),
    ("XO Tour Llif3 Lil Uzi Vert",               "Hip-Hop, Trap, Emo Rap"),
    ("Magnolia Playboi Carti",                   "Hip-Hop, Trap"),
    ("Goosebumps Travis Scott",                  "Hip-Hop, Trap"),

    # ── 80s synth pop / Italo / Hi-NRG ───────────────────────────────────
    ("Take On Me a-ha",                          "Synth-Pop, 80s"),
    ("Sweet Dreams Eurythmics",                  "Synth-Pop, 80s"),
    ("Self Control Laura Branigan",              "Italo Disco, 80s"),
    ("Tarzan Boy Baltimora",                     "Italo Disco, 80s"),

    # ── Acid / Italo House ───────────────────────────────────────────────
    ("French Kiss Lil Louis",                    "Electronic, House, Acid"),
    ("Voodoo Ray A Guy Called Gerald",           "Electronic, Acid House"),

    # ── Math rock / experimental rock ────────────────────────────────────
    ("Atlas American Football",                  "Math Rock, Emo, Indie"),
    ("Map of the Problematique Muse",            "Rock, Alt Rock"),
    ("Knights of Cydonia Muse",                  "Rock, Alt Rock, Prog"),
]

# ─── Runner ───────────────────────────────────────────────────────────────────


def main():
    ap = argparse.ArgumentParser(description="Bulk-generate WillyBeat presets via Spotify.")
    ap.add_argument("--skip", type=int, default=0, help="Skip the first N entries")
    ap.add_argument("--max",  type=int, default=None,
                    help="Stop after running this many queries")
    ap.add_argument("--no-stems", action="store_true",
                    help="Skip Demucs stem separation (faster, less accurate)")
    args = ap.parse_args()

    queries = QUERIES[args.skip:]
    if args.max is not None:
        queries = queries[:args.max]

    PRESETS_DIR.mkdir(parents=True, exist_ok=True)
    total = len(queries)
    successes = failures = 0
    start = time.time()

    log = open(LOG_FILE, "w")
    print(f"Starting bulk-generate: {total} queries", flush=True)
    log.write(f"Starting bulk-generate at {time.strftime('%Y-%m-%d %H:%M:%S')} — {total} queries\n")
    log.flush()

    for i, (query, tags) in enumerate(queries, 1):
        elapsed = time.time() - start
        rate = i / max(elapsed, 1.0)
        eta_min = (total - i) / max(rate, 1e-3) / 60.0
        prefix = f"[{i}/{total}] {query[:55]:55s} | tags: {tags[:40]:40s} | ETA {eta_min:5.0f} min"
        log.write(f"\n{prefix}\n"); log.flush()
        print(prefix, flush=True)

        cmd = [str(PYTHON), "spotify2beat.py", query, "--auto", "--genre", tags]
        if not args.no_stems:
            cmd.append("--stems")

        try:
            result = subprocess.run(cmd, capture_output=True, text=True,
                                    timeout=420, cwd=ROOT)
            if result.returncode == 0:
                successes += 1
                log.write("  ✓ success\n")
            else:
                failures += 1
                log.write(f"  ✗ failure (rc={result.returncode})\n")
                if result.stderr:
                    log.write(f"    stderr: {result.stderr[:400]}\n")
        except subprocess.TimeoutExpired:
            failures += 1
            log.write("  ✗ timeout (>7 min)\n")
        except Exception as e:
            failures += 1
            log.write(f"  ✗ exception: {e}\n")
        log.flush()

    total_min = (time.time() - start) / 60.0
    summary = f"\nDone in {total_min:.1f} min — {successes} ok, {failures} failed (of {total})\n"
    print(summary, flush=True)
    log.write(summary)
    log.close()


if __name__ == "__main__":
    main()
