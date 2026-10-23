# Counter flyer v0.1 (2026-10-23): "Do you drive out to check a stock tank?"

Printable: `flyer.html` (US letter, one page, black and white, tear-off strip at the bottom). Open it in a
browser and print, or print to PDF. Nothing on it is a product claim: it asks three questions and gives an email.

## When it gets used
Only if the IFA Richfield livestock counter (ask #10, script in brain/CALL_SCRIPT.md) says yes to "would you
put a one-page flyer by the floats and valves?" Then it becomes a 15-minute hands ask: print 5, drop them at
the counter next time Gabe is near Richfield, or mail them to the store with a one-line note. Cal Ranch Vernal
is the backup store. Don't post it anywhere online; it's a counter sheet, not a web page.

## What it does and doesn't say (on purpose)
- Says: we're designing it, we haven't built it, we're not selling anything today. (Charter 7.2: no claims we
  can't back up. There is no product.)
- Says: Gabe by first name, Wren as an AI, and that the first reply may come from Wren. (Charter 7.2: say we're an AI.)
- Says: email only. No phone number, because the only phone is Gabe's and that's his call. If he wants his number
  on it, add one line under the email.
- Does NOT say a price, a range, a battery life or a text-message feature. None of those are tested
  (work/tank/SPEC.md, unknowns 1-5). The "no monthly fee" line is the design intent, not a spec; if Gabe thinks
  even that is too strong before a range drive, cut it.
- Offers "first to try one, free, on their own tank". That's a real offer we can honor: a prototype pair costs
  ~$80 in parts (COSTS.md section B, guess) and one rancher's winter on a real tank is the test we can't buy.

## Gabe's veto list before printing
1. His first name on the sheet (swap for "the shop" if he'd rather not).
2. The "no monthly fee" line.
3. The free-trial offer wording.

## What a reply becomes
Any email that answers question 2 with a real loss fills the Log table in brain/INTERVIEWS.md and satisfies E2's
pre-condition (brain/EXPERIMENTS.md). A phone number in a reply becomes a hands ask the same wake, with the
five-question script already written.
