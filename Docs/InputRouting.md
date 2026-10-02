# Input routing migration

The router is a policy object owned by each local PlayerController. It contains only weak owner requests, handles, priorities and capability decisions. It does not evaluate keys, execute actions, modify mapping contexts, move actors, or change Slate focus/cursors.

## Conflict rules

| Request | Priority | Allowed capabilities |
|---|---:|---|
| Base, no request | 0 | Gameplay, Look, Scan |
| Scan | 100 | Scan, Look, Cursor |
| Menu/dialogue | 200 | Menu, Cursor |
| Puzzle | 300 | Puzzle |

Highest priority valid owner wins. Equal priority: latest handle wins. The winning request's set is exclusive: a missing capability is denied, never inherited from a lower request. Release is idempotent and may occur out of order. Invalid weak owners do not participate. Scan deliberately permits camera and cursor together.

## Responsibilities

- `TAInputRouter.h`: conflict resolution and action permission only.
- `TAInputOwnershipAdapter.cpp`: application, local player, Slate viewport/focus/hit path, and PIE simulation ownership. All editor-specific ownership checks are here and guarded by WITH_EDITOR; no UnrealEd dependency is added to runtime.
- PlayerController: compatibility facade, physical held-key cache, action evaluation, request registration, and existing focus/cursor side effects. `AllowsInput` combines adapter ownership with router capability permission.
- Slate preprocessor: observes admitted input, tracks releases, consumes only operations handled by the current game owner. External toolbar clicks are passed through. Releases of consumed presses are paired even after the action closes its screen.
- Character/scan: query capabilities, retain gameplay feasibility checks. No editor/Slate checks added.

Held intent continues using existing Enhanced Input mappings/modifiers; it intentionally does not reproduce Pressed/Hold trigger timing. Normal Enhanced Input events remain in place. Movement/look/scan execution and held-intent reads use the common permission gate.

Internal UI transitions preserve physical state. Focus can settle asynchronously; for two frames after Begin/EndUIInputMode physical-cache clearing is deferred, but permission remains denied while the adapter reports no ownership. External loss clears held state, modifier state and virtual cursor axes. Returning from external ownership requires fresh input; UI close retains the existing held-to-resume behavior. This grace is not permission to consume editor events.

## Stages delivered

1. Added independent policy and ownership adapter; compiled before connecting callers.
2. Connected global ingress, held intent and puzzle/cursor paths; compiled.
3. Added source-aware menu release, paired consumption, gameplay/scan capability checks and regression tests; compiled.

BeginUIInputMode remains a compatibility API; callers with widgets release with EndUIInputMode(theSameWidget). Legacy no-argument End releases the most recent menu request. Focus restoration remains in PlayerController, not Router. Existing UI widgets and Enhanced Input mapping/trigger behavior are not replaced wholesale. UI cursor presentation and synthetic-click behavior also stay with their existing owners in this migration.

## Verification

Run `TheAwakening.Input` automation tests. Router test covers priority, exclusive capabilities, equal-priority ordering, out-of-order/idempotent release, invalid owners and restoring lower requests. Existing tests cover held keyboard/stick resume across menus, held parkour, facing and momentum.

Headless automation may exercise the gate without a viewport; this allowance exists only under WITH_DEV_AUTOMATION_TESTS in the adapter. It does not establish that interactive editor behavior was tested.

Manual PIE acceptance: held movement across inventory close; held parkour entry; scan LS/RS camera+cursor; puzzle left/A confirm and right/B undo; F8 eject/repossess; click Stop/toolbars while puzzle is open; separate PIE window focus loss; release keys outside the game; confirm synthetic gamepad clicks do not switch icons to keyboard.
