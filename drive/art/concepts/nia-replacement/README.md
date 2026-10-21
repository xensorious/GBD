# Nia replacement: character art draft

Art review only. These sheets are outside the live story asset catalog and build
inputs. No character name, dialogue, family photograph or game sprite has been
replaced.

The design is an original fictional adult garage owner: dark wavy hair,
blue-green eyes, a fitted opaque white cropped tank with no buttons and a clean
sewn hem, worn denim shorts, a dark brown belt, small hoop earrings and a pendant
necklace. The revised adult figure has a fuller, supported bust. Her final name
is undecided. Previous coral-shirt drafts remain in git history and the original
generated-image directory.

Both transparent PNGs show the same four variants in a single row:

1. Neutral / confident — proposed DEFAULT.
2. Friendly / smiling — proposed CASUAL.
3. Focused / mechanic with wrench — proposed GEAR.
4. Quiet / reflective — proposed QUIET.

`outfit-poses.png` shows the design to mid-thigh, including the shorts.
`dialogue-portraits.png` provides closer waist-up framing for readable faces.
These are source illustrations, not indexed DS exports or emulator captures.

The outfit sheet has clean quarter-cell gutters. The closer dialogue sheet has
some arms extending across the nominal quarter-cell boundaries; extract complete
individual silhouettes rather than blindly clipping equal quarters. The tiny
colored alpha fringes in the source are below alpha 8 and do not survive the
existing alpha-96 sprite cutoff. All heads remain inside the image bounds.

The existing game uses 128x128 compiled character cutouts. Conversion, individual
sprite extraction, any family-photo revisions and story integration are a later
step. Original generated source images are retained in /workspace/generated_images.
