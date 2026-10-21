# Nia replacement: character art draft

Art review only. These sheets are outside the live story asset catalog and build
inputs. No character name, dialogue, family photograph or game sprite has been
replaced.

The design is an original fictional adult garage owner: dark wavy hair,
blue-green eyes, a fitted opaque white cropped tank with no buttons and a clean
sewn hem, worn denim shorts, a dark brown belt, small hoop earrings and a pendant
necklace. The revised adult figure has a fuller, supported bust. Subtle barbell
jewelry impressions are visible through the fabric relief; the tank remains
fully opaque. Her final name is undecided. Previous drafts remain in git history
and the original generated-image directory.

The transparent sheets show the same four variants in a single row:

1. Neutral / confident — proposed DEFAULT.
2. Friendly / smiling — proposed CASUAL.
3. Focused / mechanic with wrench — proposed GEAR.
4. Quiet / reflective — proposed QUIET.

`outfit-poses.png` shows the design to mid-thigh, including the shorts.
`dialogue-portraits.png` provides closer waist-up framing for readable faces.
These are source illustrations, not indexed DS exports or emulator captures.

`bikini-outfit-poses.png` is a separate wardrobe draft with a plain opaque white
triangle bikini top and the same four poses. `bikini-dialogue-portraits.png` is
cropped directly from that exact image, with no regeneration, redrawing or
resampling. The two original tank-top sheets remain together for comparison.

`bikini-portraits/` contains the four individual 560x591 transparent portrait
PNGs. Source crops use horizontal boundaries 0, 543, 1085, 1628 and 2170 and
vertical bounds 0 to 575, then are centered in each portrait with 16 pixels of
transparent headroom. The combined sheet places these portraits in the listed
order. Crop pixels are verified identical to the source. Several tiny hair tips
already touch the source image's upper edge; the crops preserve them as supplied.

The outfit sheet has clean quarter-cell gutters. The closer dialogue sheet has
some arms extending across the nominal quarter-cell boundaries; extract complete
individual silhouettes rather than blindly clipping equal quarters. The tiny
colored alpha fringes in the source are below alpha 8 and do not survive the
existing alpha-96 sprite cutoff. All heads remain inside the image bounds.

The existing game uses 168x168 compiled character cutouts. Conversion, individual
sprite extraction, any family-photo revisions and story integration are a later
step. Original generated source images are retained in /workspace/generated_images.
