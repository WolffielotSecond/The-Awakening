# Input ownership migration

## Status

Stage two is now accepted by the user's final interactive PIE A-H regression.
Stage three input migration and final diagnostic cleanup are complete. Its scope is
action permission and owner execution; physical held-input reconstruction remains
deferred. Two separately investigated residual fixes are now included: accepted
locomotion through the zero-strength Freeze entry and the puzzle monotonic clock.
User PIE accepted Walk/Sprint/active Parkour/landing -> Scan, with no startup speed
spike or animation jump; Puzzle background timing also passed the user's retest.

### Stage three work log

- Prerequisite: added the narrow ITADialogueParticipant::CanParticipateInDialogue query.
  Character checks its own active Parkour; StoryTrigger only depends on the interface.
  Both prompts and execution use CanInteract. Landing inertia does not deny dialogue.
- Dialogue successful entry explicitly calls Character::StopCurrentMovement once,
  clearing accepted movement/pending input, velocity and landing inertia. It does not
  modify HeldKeyValues, ownership or presentation. Active Parkour + forced script
  dialogue is outside the supported normal gameplay invariant; no cancellation added.
- Step 1: replace broad Gameplay/Menu capability names with action vocabulary; add
  exact winner handle/owner authorization plus external ownership at the controller.
  Remove the winner-change ClearMovementInput side effect. Character/Gameplay ingress
  and supported owner UI handlers now use action permission.

Stage one is complete. Stage two has completed code migration for the supported
keyboard/mouse and gamepad paths. Mobile/touch is not a project target; its template
widget creation entry point has been removed rather than migrated into ownership.
Stage-two interactive PIE acceptance is complete. Stage-three Scan/Freeze fixes are
PIE accepted; final cleanup does not change input consumption or gameplay behavior.
Stage four physical held/focus reconstruction has not started.

## External player input ownership lifecycle

Router winner change (`OnInputOwnerChanged`) and external ownership loss
(`OnPlayerInputOwnershipLost`) are independent events. The router stores requests;
it does not know focus, editor state or whether a behavior needs a held key.

TAInputOwnershipAdapter::GetState classifies the current external surface:

- Owned: active application/game window and focus in the viewport or its descendants.
- Transition: valid viewport in the active window, but no focused widget. This is an
  unresolved gap, not proof of loss or a grant of permission. No timeout guesses its
  cause. Presentation may resolve this gap through the existing viewport fallback.
- Lost: inactive application, another active window, editor simulation, invalid
  viewport, or concrete focus outside the game subtree. Editor checks live only here.

Pointer hit-testing remains a separate event-delivery query; moving the pointer outside
the viewport does not by itself produce a lifecycle loss. Controller polling uses the
focus-based state. The application activation callback records definitive inactivity
immediately; reactivation only samples the adapter and never synthesizes a key release.

Controller LastDefinitiveInputOwnership stores notification edge memory only. Owned ->
Lost, including an intervening Transition, broadcasts once; repeated Lost and reentrant
Release cannot broadcast again because state is committed before callbacks. Initial
Lost and Owned -> Transition -> Owned produce no loss event. Permissions still query
the adapter directly, never this memory. Observation occurs before presentation sync
and request acquisition. No new timers, frame counters or grace periods were added;
the existing InternalFocusTransitionUntil remains solely in the old held-cache path.

Scan subscribes in BeginPlay and unsubscribes in EndPlay. It responds with the existing
CancelScan(Canceled, true): stop active scan/hover and release its exact request now,
then unwind freeze through the existing fade-out. Persistent Inventory, Puzzle,
Dialogue and History do not subscribe and keep their requests. Controller activation
no longer calls Scan CancelScan or NotifyScanInputReleased. No Scan-specific external
ownership checks exist; Router preemption continues through its separate listener.

Known limitation: a physical release while unfocused may be missed. Regaining ownership
does not clear the scan release latch. In that case the player must supply an observed
release before a fresh press can start scanning. No held-input reconstruction is added.

First PIE follow-up: movement clearing on a non-Gameplay winner was removed in stage
three. Input denial never actively resets existing movement.
Background Puzzle countdown slowdown is a separate investigation; compare wall time,
widget tick count/deltas and Session TimeRemaining before deciding its cause. The timer
is unchanged. Scan -> Puzzle -> Gameplay remains the approved lifecycle.

## Touch template dependency audit

Asset Registry referencers and read-only Blueprint exports confirm:
UI_TouchSimple is referenced only by BP_ThirdPersonPlayerController's old class default;
UI_Thumbstick only by UI_TouchSimple. BPI_TouchInterface is referenced by that widget
and BP_ThirdPersonCharacter's template interface implementations (DoMove/DoLook/jump
receivers). No keyboard/gamepad system calls through this interface. Shared Character
movement functions and the inert interface implementations are retained.

Removed the controller's MobileControlsWidgetClass/MobileControlsWidget properties,
touch widget creation branch and SVirtualJoystick dependency. Assets are retained;
the Blueprint's old serialized class-default tag is no longer a reflected property or
runtime creation path. No binary asset cleanup/resave is needed for this migration.
DefaultTouchInterface=None, bUseMouseForTouch=False and bAlwaysShowTouchInterface=False
remain; the four-finger console gesture is disabled. The old bEnableTouchEvents=True
Blueprint default alone creates no widget/interface and has no bound touch input path.

MobileExcludedMappingContexts is deliberately retained under its serialized name:
the real BP default is IMC_MouseLook (Mouse2D), not a mobile mapping. It now registers
unconditionally beside IMC_Default, with editor display name Additional Input Mapping
Contexts. IMC_Default's keyboard and gamepad actions are unchanged. DualSense touchpad
button icons and OculusTouch key definitions are not mobile touch UI and are untouched.
Audit outputs are under Saved/Automation/TouchDependencies (regeneratable, not source).

## Request contract

`FTAInputRequest` requires a live weak Owner. Acquire copies priority, the exclusive
capability set and declarative presentation. Handle 0 is invalid/base. Handles are
router-local and never reused during that router's lifetime. Release must use the
issuing controller and exact handle; duplicate/expired/zero releases are harmless.
Multiple requests from the same owner remain independent. Highest priority wins;
latest handle breaks ties. Missing capabilities never fall through to lower requests.
An empty set denies all input. Invalid owners are pruned on queries and mutations.
GetWinner returns a value snapshot, not another mutable authority.

Owner and FocusTarget are independent weak references. Losing a focus target never
invalidates its owner or changes the winner. The controller resolves missing, disabled,
hidden or unbuilt targets to viewport focus, retaining that request's cursor/input mode.

## Deterministic base presentation

With no valid request: Move/Look/Interact/Parkour/Scan/InventoryToggle/OpenDebugUI capabilities, **GameOnly**, **hidden cursor**,
**viewport focus**. No previous-owner lookup, cursor snapshot or historical input mode
participates. Higher requests closing reveal the remaining winner or this exact base.

## Ownership before and after

| System | Before | Owner / handle after | Acquire / release | Declared presentation |
|---|---|---|---|---|
| Inventory | Character Begin/End menu; PC count/arrays | Inventory widget / its InputRequestHandle | NativeConstruct / RemoveFromParent and NativeDestruct | 200; GameAndUI, cursor visible, self focus |
| Dialogue | PC dialogue bool, ownerless menu release, default IMC removal/restore | Dialogue widget / its InputRequestHandle | NativeConstruct / RemoveFromParent and NativeDestruct | 200; GameAndUI, cursor visible, self focus |
| Dialogue History | Direct focus change, no independent request | History widget / its InputRequestHandle | NativeConstruct / RemoveFromParent and NativeDestruct | 250; GameAndUI, cursor visible, close button (or self) focus |
| Puzzle | Menu request plus PC singleton puzzle handle/pointer and widget bool | Puzzle widget / its InputRequestHandle | NativeConstruct for active session / RemoveFromParent and NativeDestruct | 300; GameAndUI, cursor hidden, self focus |
| Scan | PC scan bool/handle and cursor snapshot | Scanning component / its InputRequestHandle | StartScan / StopScan and EndPlay | 100; GameOnly, cursor visible, viewport focus |
| Gameplay | Implicit default permission plus manually restored presentation | No owner/handle, explicit base | Automatic when no valid requests remain | GameOnly, cursor hidden, viewport focus |

Each instance also stores a weak InputRequestController, solely to release through the
same issuer if its owning player changes. Release clears the local token before calling
PC; NativeDestruct after a normal close cannot release somebody else's request.
Direct widget removal now releases input immediately. Owner expiry without a release
is handled on the next controller synchronization by re-evaluating remaining requests.
A missing focus target falls back within the same request, never to an old mode.

## Presentation application

Runtime C++ presentation writes are centralized in
`AThe_AwakeningPlayerController::SynchronizeInputPresentation`:

- SetShowMouseCursor from the winner.
- SetCursor on the winning widget owner (UMG can answer cursor queries before PC).
  ResetCursor removes the former owner's override; it does not restore a saved cursor.
- SetInputMode for GameOnly / GameAndUI / UIOnly. Focus is carried by these engine
  input-mode operations, using the target or viewport fallback.

The routine runs on request mutation and from the existing input-preprocessor tick.
ObservedWinnerHandle is notification deduplication. PresentedRequestHandle,
PresentedFocusTarget, PresentedCursorOwner and PresentedViewport record applied output,
not saved previous presentation. They never grant input or decide what to restore.
They prevent duplicate Release/Destruct from reapplying focus. No new frame guard,
mode count, previous cursor or previous input-mode snapshot was added.

TAInputOwnershipAdapter::CanApplyPresentation defers focus/input-mode application when
an editor control or another window owns focus. With no focused widget it may recover
focus only inside the active viewport window. PIE simulation checks remain in this
adapter. Cursor/permission data remain derived from the request while focus is deferred.
Stage-two presentation/ownership behavior was accepted by the user's PIE A-H pass;
automation alone does not establish actual mouse capture or editor focus behavior.

## Removed legacy ownership

Removed from PC: MenuInputHandles, MenuInputOwners, DialogueInputOwner, ScanInputHandle,
PuzzleInputHandle, ActiveUIModeCount, bUIInputModeActive, bDialogueModeActive,
bPreviousShowMouseCursor, bMouseCursorBeforeScan, bScanCursorModeActive and PuzzleScopeWidget.
Removed from Puzzle: bOwnsInputMode, its cursor query override and per-tick cursor writes.
Removed from Dialogue: bHistoryOpen; visible history is queried from the actual widget.
Removed interfaces: Begin/EndUIInputMode, SetDialogueModeActive, SetUIFocusWidget,
Begin/EndScanCursorMode, SetPuzzleScopeWidget and unused FocusChoice.

IsUIInputModeActive and IsScanCursorModeActive compatibility queries are removed.
PC forwards authorized commands to the winner owner's narrow ITAPlayerInputReceiver;
Puzzle itself interprets its commands and configurations.
Scan listens to OnInputOwnerChanged and cancels when Scan permission is lost; PC does
not contain the former menu-opening Scan cancellation branch.

Mapping-pushed flags remain resource guards for each widget's own Enhanced Input
contexts, not UI ownership. Default contexts are no longer removed/restored by Dialogue.
Existing HeldKeyValues, virtual cursor axes and consumed-key sets
and InternalFocusTransitionUntil remain for the later input/held migration. None were
added to solve stage-two lifecycle problems.

## Remaining presentation writers

- Target-platform runtime: the controller routine above is the sole presentation writer.
- Mobile Blueprint UI_TouchSimple: Construct -> bShowMouseCursor ->
  SetInputMode_GameAndUIEx remains inside the unused asset, with no runtime creation
  entry point. It is not a supported input path and receives no ownership request.
- TALocalizationEditor.cpp: editor-tool keyboard focus when saving its text editor;
  belongs to the editor surface, outside player input ownership.
- Virtual cursor SetCursorPos remains pointer motion, not presentation ownership.

## Historical stage-two verification (superseded by accepted PIE A-H)

Build The_AwakeningEditor Win64 Development; run TheAwakening.Input and
TheAwakening.Puzzle.Scope. OwnershipLifecycle exercises real Inventory/Dialogue/History
lifecycle hooks, out-of-order close, RemoveFromParent followed by NativeDestruct, stale
release, owner expiry, focus expiry and deterministic base cursor restoration. Router
unit tests cover priority, ties, copied requests and exclusive capabilities. Held tests
retain their previous movement/scan-stick expectations through explicit request handles.
Headless tests do not verify actual Slate focus transfer or mouse capture.

2026-10-03 post-touch-cleanup verification: Win64 Development Editor build succeeded;
TheAwakening.Input passed 6/6 (three parkour tests retain test-world destruction warnings,
no errors); TheAwakening.Puzzle.Scope passed 1/1. git diff --check passed. The initial
link attempt was blocked by the completed audit commandlet retaining the DLL; ending
only that audit process and rebuilding resolved it without code changes. Interactive
At that historical checkpoint PIE was outstanding; the later user A-H pass accepted
cursor, focus, capture and InputMode behavior.

2026-10-03 external-ownership follow-up: Editor build succeeded; Input passed 7/7
including ExternalOwnership, and Puzzle.Scope passed 1/1. ExternalOwnership exercises
the real scan subscription, cancel/release, fade-out freeze removal, release latch,
application activation callback, persistent Puzzle request and separate winner event.
OwnershipLifecycle also verifies persistent History/Dialogue/Inventory requests survive
application loss. Edge tests cover Transition, repeated Lost, reentry and teardown.
The headless scan test has three test-world DestroyActor warnings, alongside the four
existing parkour-test warnings; all tests report zero errors. git diff --check passed.

These tests inject Adapter outcomes for external/editor loss; they do NOT simulate
actual F8, Alt+Tab or Slate focus transfers. The later user A-H pass covered:
scan while ejecting/returning; scan while switching applications or focusing editor
controls; ordinary widget focus transfers; persistent UI loss/recovery; Scan -> Puzzle
-> Gameplay; and Stop PIE.

## Stage-three execution contract

UI authorization is the conjunction of external Player Input Ownership, exact current
winner handle AND owner, and the corresponding action capability. PC::AllowsInputFor
provides it. Old widgets cannot borrow a new winner's capability. Router stores only
requests, never receivers, and performs no Slate operations. Presentation remains
exclusively applied by PC::SynchronizeInputPresentation.

ITAPlayerInputReceiver has three methods: GetPlayerInputRequestHandle,
ResolvePlayerInput(Key), ExecutePlayerInput(Key, Capability). PC::RoutePlayerInputKey
resolves exactly one command from the winner owner, authorizes, then forwards it.
Repeated keys are consumed without re-execution. Resolution before execution prevents
one Dialogue Confirm key from also advancing the newly selected line.

| Owner | Priority | Capabilities |
|---|---:|---|
| Gameplay base | base | Move, Look, Interact, Parkour, Scan, InventoryToggle, OpenDebugUI |
| Scan component | 100 | Scan, Look, Cursor, OpenDebugUI |
| Inventory widget | 200 | Navigate, Cursor, Confirm, InventoryToggle, Close, ToggleDrag |
| Dialogue widget | 200 | Navigate, Cursor, Confirm, Advance, ToggleHistory, Close |
| History widget | 250 | Navigate, Cursor, Confirm, Close |
| Puzzle widget | 300 | Confirm, Undo, Pan |

Capabilities are action permissions, not mode names. A receiver resolves only commands
it actually implements; a capability does not itself bind a key or execute an action.

| Player action | Authorized execution path |
|---|---|
| Move/sprint | Physical Held observation -> Character held-intent update -> Move gate -> DoMove; no parallel EI state writer |
| Look | EI Look or cursor-look calculation -> SubmitPlayerLook -> Look gate -> DoLook |
| Interact | TryPlayerInteract -> Interact gate -> TryInteract / domain CanInteract |
| Parkour start/preview | CanPlayerParkourToMarker -> Parkour gate + domain CanParkourToMarker -> existing execution |
| Scan press | Character ingress -> Scan gate -> StartScan; Release/Cancel remains lifecycle cleanup |
| Inventory toggle | Character opens; winning Inventory receiver closes -> InventoryToggle gate -> open/remove |
| Inventory pages/tabs | Receiver Q/E/shoulders or UMG click -> Navigate-authorized widget handlers |
| Inventory Confirm | Receiver or prompt -> HandleConfirm gate -> existing synthetic pointer click -> target's authorized handler |
| Inventory drag | Slot mouse/DragDetected/Drop or owner ToggleDrag handler -> exact Inventory authorization -> mutation |
| Dialogue keys/gamepad | Receiver selects Confirm/Advance/Navigate/ToggleHistory -> authorized widget handler |
| Dialogue pointer | UMG choice/button/prompt handler; unhandled background click bubbles to authorized Advance |
| History close | Receiver shortcut or close button -> authorized HandleCloseClicked -> parent closes exact History instance |
| Puzzle | Receiver or node/Undo button -> authorized Puzzle handler -> its own reticle, inverse Pan, Confirm/Undo and session command |
| Debug Puzzle | G Character ingress or B Blueprint player opener -> OpenDebugUI gate -> public domain OpenPuzzleWithSettings |

Dialogue mouse buttons are left to UMG hit testing, so History/button clicks cannot
be consumed as raw background Advance. Choice click carries its own index. Inventory
synthetic click retains its preexisting recursion guard, not a new frame guard.
Nested clothing Slots receive their own Inventory panel, never the current winner.
Drop checks source and destination ownership. Drag cancel remains callable while covered.

Actual exported WBP graphs have no independent player behavior callbacks bypassing
these handlers. The Character B debug graph did directly call the public Puzzle opener;
the editor-only TAInputBlueprintMigration commandlet migrated that single call to
OpenPuzzleWithSettingsFromPlayerInput, verified unchanged pin defaults/links, compiled
and saved it. This is an asset migration tool, not a runtime manager or registry.

Public domain functions (DoMove/DoLook/TryInteract, Parkour/Scan domain calls, Inventory
data commands, Puzzle SelectNode/Undo/OpenPuzzleWithSettings) remain script/system/test
APIs. Real player events must use the authorized ingress, not those APIs directly.

## State boundaries and retained channels

Permission denial is a no-op for existing gameplay state: no ClearMovementInput,
StopMovementImmediately, landing cancellation or fabricated zero-input update.
The UI -> Movement Stop branch was removed; IsMoveInputIgnored keeps its explicit
engine stop semantics. Dialogue successful start alone explicitly calls the generic
StopCurrentMovement interface once. Active Parkour eligibility is queried through
ITADialogueParticipant, with shared StoryTrigger CanInteract for prompt and execution.

Freeze remains owner CustomTimeDilation; no animation/movement snapshots or algorithms
were changed. Progressive slowing is intended. An accepted locomotion command advances
while a Freeze request is live, including its zero-strength entry, without accepting
unauthorized direction/sprint changes. Request presence is not derived from strength.
The user verified Walk/Sprint/active Parkour/landing -> Scan after this fix.

Slate retains physical Held/device observation and owner command routing. EI retains
Gameplay ingress/axes and mapped key definitions. UMG retains pointer hit testing and
drag lifecycle. Virtual cursor retains pointer motion. Inventory/Dialogue EI behavior
callbacks were removed; their contexts remain for receiver key lookup and prompt icons.
Actual UI mappings have no hold/chord triggers/modifiers. Future custom triggers need
an explicit supported execution design; raw key lookup does not evaluate them.

HeldKeyValues, consumed presses, virtual axes, inventory press latch and
InternalFocusTransitionUntil/GFrameCounter remain existing delivery/physical state,
deferred to stage four. Inventory drag visuals are business state. No new registry,
mode bool, state snapshot, frame/time dedup or grace period was added.
Removed LastConfirmClickFrame/AdvancePressedFrame/ChoiceConfirmPressedFrame, old UI
gates, PC Puzzle type/configuration logic, broad Gameplay/Menu capabilities, UI EI
behavior bindings and duplicate Character EI movement writers.

## Final stage-three verification and PIE acceptance

2026-10-03 before final trace cleanup: Editor Win64 Development build succeeded;
Input (9), Puzzle (6), Freeze (2) passed 17/17, including UIPlayerIngress,
DialogueParticipation, stage-two OwnershipLifecycle/ExternalOwnership, Parkour
held/facing/momentum, zero-strength command continuity and monotonic timer regressions.
Test-world destruction and GameplayCue configuration warnings remain; no test errors.
Headless automation does not prove actual Slate capture, hit testing, focus transitions,
device feel, animation appearance or real F8/Alt+Tab behavior.

Manual final acceptance, using reachable gameplay combinations:

1. Gameplay: held WASD/LS, Shift/L3, look, interaction and held Space/Ctrl Parkour
   retain their feel; preview/start share eligibility and release still works.
2. Inventory: Tab/View opens/closes once when held. Q/E and LB/RB switch pages;
   mouse buttons and LS cursor+A select the hovered target. Drag/drop/cancel pocket
   items by mouse and controller; each mutation occurs once. Close with movement held
   resumes movement. Opening does not actively cancel landing inertia.
3. Dialogue: walking and landing entry stop immediately. Active Parkour Story prompt
   and start are unavailable, then restore after Parkour. Other interactables unaffected.
   Background/Continue click advances once; History button opens History without Advance.
   Choice mouse/A/Enter selects once without advancing the new line.
4. History: configured shortcut/button closes; A also activates its focused close
   button via existing Slate Accept behavior. Covered Dialogue cannot respond.
   Close restores current remaining Dialogue. Do not invent unsupported UI combinations.
5. Puzzle: G/B from Gameplay opens; WASD/LS inverse pan, left/A reticle selection and
   enabled right/B Undo execute once. Cursor hidden. B inside Dialogue remains History,
   and player debug input cannot bypass denied OpenDebugUI permission.
6. Scan: Walk, Sprint, active Parkour and landing progressively slow without a startup
   input gap/speed spike. Hover/LS/RS still work. Scan->G
   ->completion returns Gameplay, not held Scan; release then fresh press starts Scan.
7. Stage-two regression: F8/Alt+Tab cancels Scan without revival. Persistent Inventory,
   Puzzle, Dialogue/History survive external loss and resume correct owner presentation.
   Internal focus changes do not produce ownership loss. Stop PIE leaves no residue.

For anomalies record initial owner, exact press/hold/release sequence, device, cursor,
capture/focus, whether one or two commands executed, and resulting UI/state. Artificial
cover/out-of-order/expired-owner cases are automated, not assumed normal UI combinations.

## Residual Freeze locomotion and puzzle clock fixes

Move permission governs accepting changes to the locomotion command. Character stores
the command's coordinate basis during authorized input, not on Scan entry. During any
live Freeze request it continues advancing that accepted command in the same world
direction, using existing safety checks and engine-scaled time. Camera Look cannot
redirect it. Full Freeze suspends advancement; thaw accepts current Held input again.
Non-freezing UI still leaves velocity to existing movement rules. No velocity/state
restore, Scan-specific branch, extra time scaling or Parkour algorithm change is used.
Request presence is derived from the existing Freeze source/participant registry,
including strength zero. Scan retains it from FadeIn entry through FadeOut completion;
ReleaseFreeze ends it. Strength zero alone is not release and must not skip command
submission at the transition's first tick.

Session.UpdateTimer consumes FPlatformTime's monotonic elapsed time once per update
opportunity. Widget delta remains for visual updates only. First Start anchors the
clock; Retry/reselect does not re-anchor; Ready and terminal states do not count down.
There is currently no separate Playing-but-paused timer state/API. Background focus
loss is not a timer pause. AdvanceTime remains explicit deterministic simulation input;
live widgets use UpdateTimer exclusively, not both clocks.

Verification: Editor Win64 Development build passed; Input + Puzzle + Freeze passed
17/17 (including stage-two lifecycle regression). Existing world-destruction and
GameplayCue configuration warnings remain. Report: Saved/Automation/ScanTimerFix.
User PIE retest passed Walk/Run/active Parkour/landing Freeze visuals and background
timer behavior. The animation blueprint's user-adjusted 505 threshold is unchanged.

## Final cleanup and retained follow-up scope

Deleted TAStageThreeTrace.cpp/.h, ta.Stage3Trace, ta.Stage3Trace.Status, the log category,
Slate debug listener, counters/prebuffer/statistics and all tagged runtime call sites.
No diagnostic field remains in Character, PC, Movement, Freeze, Scan or Puzzle.
UpdateTimerAt and its test friendship remain deterministic regression support, not
instrumentation; LastTimerUpdateSeconds is the live timer's consumption position.

Stage four is not started: HeldKeyValues, HeldInputModifiers, ConsumedPresses,
ConsumedInventoryKeys, virtual cursor axes and InternalFocusTransitionUntil/GFrameCounter
retain their existing delivery/physical-state responsibilities and known focus/release
limitations. The frame-based focus grace is not action execution deduplication.

For stage-five review (not implementation or a newly approved framework), PC still
contains virtual-cursor/Character cursor-speed and Look plumbing plus the synthetic
click recursion guard. These are real behavior, not trace leftovers; any extraction
must preserve device feel. The editor-only one-time B-key Blueprint migration tool
also remains, with its BlueprintGraph dependency, for explicit/reproducible migration.
Saved audit scripts/logs are offline artifacts, not runtime input paths.

Mapping contexts/resource guards stay for mapped key lookup and prompt icons. Inventory
drag visuals stay as business lifecycle state. G/B debug puzzle opening is preexisting
gameplay testing functionality, capability-gated, not Stage3 instrumentation. Public
domain APIs stay callable by scripts/tests/systems; only real player ingress is gated.
These retained items are not a second ownership authority or a reason to remove
healthy domain code for cleanliness.

Post-cleanup verification (2026-10-03): Editor Win64 Development build succeeded;
Input + Puzzle + Freeze passed 17/17 (12 clean, 5 with existing warnings, zero failures).
Final report: Saved/Automation/ScanTimerFix/index.json. git diff --check passed.
The new test log contains no Stage3 trace output or console diagnostics.

Final read-only audit: runtime source contains no old IsUIInputModeActive/IsUIInputActive
gate, ownerless UI counter/stack, PC Puzzle type/Pan/Confirm/Undo logic, or
LastConfirmClickFrame/AdvancePressedFrame/ChoiceConfirmPressedFrame state. Inventory,
Dialogue, History and Puzzle have no UI Enhanced Input behavior callbacks; supported
key actions execute through the winner receiver, and pointer/drag handlers authorize
their exact widget owner before business mutations. Dialogue raw pointer input is
deliberately not resolved by the receiver, preserving UMG button/background ordering.
History's existing Slate Accept path reaches the same authorized Close handler.
Presentation writes occur only in PC::SynchronizeInputPresentation. Gameplay domain
APIs remain callable independently, but supported player ingress uses capability gates.
No new snapshot, delay, frame deduplication, mode exception or second receiver/owner
registry was added during cleanup. This is source-path audit plus existing ingress
regression, not a guarantee about arbitrary future Blueprint event wiring.
