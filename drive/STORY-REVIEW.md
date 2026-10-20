# Rising Tide: continuity and scene direction

Reviewed all 125 events, in order, including their journal summaries. There are
still 628 messages. This pass keeps the story-only progression: scenes unlock
the next event without simulating a race or changing the shared garage economy.

## Continuity changes

- **Saltblock:** the five-heat training block follows Kade's D.06 exhibition.
  D.09 now refers to the two practice heats already completed. The courier
  duel provides the first payoff before Calder's final. D.19 refers to the
  marker post actually depicted in the painting, rather than an unseen lamp.
- **Lantern Mile:** C.23 introduces Voss's role and continuing racing career.
  C.25 separates Marisol's club recommendation from the attached commercial
  offer, so the sponsorship document cannot be mistaken for the sporting rules.
- **Foundry:** B.02 is one exhibition lap together, matching the depicted loss,
  rather than two separate runs. B.14 identifies the Meridian cost guarantee.
  Kade's rematch, Rook's guarantee and the independent application remain
  distinct steps. Voss's concern about wages stays credible without making him
  the sole owner of every sporting decision.
- **Meridian:** A.09 is a paid support heat that covers travel, rather than a
  claim that ordinary private practice pays the driver. A.15 distinguishes the
  funded Meridian stage from the larger future Cup budget. A.25 grants a
  sporting invitation, with a separate financial approval still outstanding.
- **Cup:** S.01 acknowledges provisional entry. S.11 establishes Nia's proposal
  before Sable signs it; S.13 explains why the Cup guarantee exceeds Meridian's.
  S.20 publishes the independent terms, then S.21 confirms the financial review.
  S.24 puts both drivers under Aster's marshals. S.25 explicitly resolves the
  race ahead of Voss, whose contract cannot change with the result.

Night races now retain night backgrounds for their immediate conversations;
the ending moves from sunset preparation to the floodlit finish and then dawn
at Sunline. The D.06 film review is explicitly over breakfast. These cuts no
longer imply the sun moved backward between a finish and its handshake.

The throughline remains a garage becoming a crew: keys, the notebook, borrowed
equipment, ordinary paid work, and the folding chairs recur for a reason. Nia
learns that independence must include her colleagues' choices. Jules's need for
reliable wages and Mina's own paid work are part of the decision, rather than
obstacles that disappear after a speech. Scripted defeats lead to specific
practice goals; the eventual races must deliver those goals, not force an
unexplained loss or take money from the save.

## Visual subjects

All 378 shot cues now name a subject and a physical cast. Twelve new paintings
provide close subjects instead of forcing every conversation onto a wide car
meet. The artwork was inspected before calibrating the camera coordinates;
for example the jack is at X235 in its 512-pixel painting, not at the empty
doorway in the right-hand quarter.

| Events | Direction and purpose |
| --- | --- |
| D.01 | Pan from the wandering jack to the small keys, then meet Nia and Mina. |
| D.04, D.09, D.21 | Show the lease and bills when they are discussed; return to Nia for the decisions. |
| D.06, B.02 | A corner-exit montage illustrates the exhibition gap; the subsequent review focuses on onboard film. |
| D.07, D.15, B.04, B.08 | The reference study, new notebook and comparison film become visible subjects. |
| D.12, D.14, D.24, D.25 | Marshal's barrier, courier map, family photograph and chairs support the dialogue and chapter ending. |
| D.19, B.07 | Move from the reference marker to the corner exit, then hold. |
| C.02, C.04, C.07, C.17, C.21 | Harbor workers' lights, borrowed awning, actual rain and the food van match the waterfront exchanges. |
| B.14–B.16, B.24 | Financial paperwork is visible when needed; the offer and crew reactions use physical character staging. |
| A.01, A.04, A.05 | The pit lane and marked apron are the objects of the explanation. |
| A.07 | Kade answers over the radio; he is not placed beside Ren on the apron. |
| A.13–A.15 | A contract meeting, a return to Sunline after hours, and the actual budget frame the crew's decision. |
| A.21, A.25 | The commercial folder and independent application occupy different parts of the same office painting. |
| S.06, S.08, S.09, S.19 | Weather equipment and timing sheets support practical race discussions. |
| S.13, S.20, S.21 | The office and specific documents support negotiation and approval, not a random row of cars. |
| S.18 | A floodlit circuit establishes the interrupted race before the pit-wall radio discussion. |
| S.23–S.25 | The old sign and chairs, shared terms, floodlit finish, jack and open door carry the ending's callbacks. |

Face conversations use only the actors explicitly present in that shot. Inserts
have no physical actors; the small speaker portrait identifies the voice.
Radio portraits identify callers without implying they are standing at the
depicted location. Object inserts can recall a place while the people remain
elsewhere, such as remembering the Sunline sign from the circuit paddock.

Camera motion is an eased, one-way move to the named target, followed by a hold.
There is no idle oscillation or speaker-number offset. New locations use soft
cuts; changes of emphasis inside one painting can pan between its subjects.
The overall framebuffer and shared 256,000-byte art scratch budget stay the
same; extra paintings are streamed from the ROM.

## Verification scope

Native tests play all 125 scenes, render all 628 messages, check text fit and
memory guards, and test that a real jack-to-keys pan reaches its target, stays
there past the old oscillation period, and freezes while paused. Cartridge
checks earn the opening six scenes through buttons, verify immediate autosave
and independent profiles, and review later story beats using explicitly
labelled save fixtures. Later fixtures are not claimed as earned progression.
The archive audit compares every compiled message, cast and focus against the
authored directions, checks that named subjects land inside their final frame,
and preserves the existing RAM limits. The source and this editorial review
remain available for revising future race implementations.

The paintings illustrate dialogue rather than simulate its events. They do not
claim to show the player's equipped car or an actual raced result. Existing
Free Roam frame-time limitations are reported separately in VALIDATION.json.
