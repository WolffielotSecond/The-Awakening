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
Stage four (4.1-4.4, including the synthetic click surface fix) is PIE accepted.
Stage 5.1 scoped Slate navigation is implemented, automated and user PIE accepted.
Stage 5.2 verified migration/source cleanup is accepted. Stage 5.3's redundant
Inventory Preview repeat interception removal passed automation and user PIE smoke.
Stage 5.4 final audit changes documentation only; no new runtime refactoring.
Stages 1-5 are complete. Final compile, regression and diff checks passed; this is
a stable checkpoint with the explicitly accepted ownership-recovery limitation.

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

Controller LastDefinitiveInputOwnership stores lifecycle edge memory only. Owned ->
Lost, including an intervening Transition, broadcasts once; repeated Lost and reentrant
Release cannot broadcast again because state is committed before callbacks. Initial
Lost and Owned -> Transition -> Owned produce no loss event. Permissions still query
the adapter directly, never this memory. Observation occurs before presentation sync
and request acquisition. Stage 4.2 also uses this existing edge memory for once-only
physical observation invalidation on entering Lost (including initial Lost, which
does not broadcast loss). Transition retains the last definitive state. The old
InternalFocusTransitionUntil/GFrameCounter focus grace has been removed; no timeout
or replacement state was added.

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
Puzzle background countdown uses the accepted monotonic-clock fix described below.
Scan -> Puzzle -> Gameplay remains the approved lifecycle.

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
HeldKeyValues and consumed-key sets retain the distinct stage-four responsibilities
documented below; none is a second ownership authority.
Stage 4.2 removed the old focus frame grace; observation now follows adapter lifecycle.
Stage 4.3 removed separate virtual-axis storage; cursor axes derive from Held.

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
| Move/sprint | Physical Held observation -> Move-authorized UpdateHeldGameplayInput -> UpdateMovementInput -> ExecuteMovementCommand; no parallel EI state writer. A live Freeze request advances the already accepted command without accepting new intent |
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
the one-time editor commandlet migrated that single call to
OpenPuzzleWithSettingsFromPlayerInput, verified unchanged pin defaults/links, compiled
and saved it. The completed migration commandlet was removed in Stage 5.2; Git
retains its history. The player-input entry and migrated Blueprint remain intact.

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

Stage four removed focus frame grace and separate virtual-axis storage. Held observations, consumed delivery pairing and the Inventory press latch retain distinct responsibilities. Inventory drag visuals are business state. No new registry,
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

Stage four is accepted. Its final observation, pairing, cursor-axis and Parkour consumption contracts are documented below; no focus frame grace remains.

After stage-five review, PC deliberately retains
contains virtual-cursor/Character cursor-speed and Look plumbing plus the synthetic
click recursion guard. These are real behavior, not trace leftovers; any extraction
must preserve device feel. The one-time Blueprint migration tool was retained at
the Stage 4 checkpoint, then removed after asset verification in Stage 5.2 together
with its otherwise-unused direct BlueprintGraph module dependency.
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

## Stage four: accepted physical observation and consumption contract

Stages 4.1-4.4 and the synthetic pointer surface fix passed user PIE acceptance.
This is the final stage-four implementation contract; stage-five changes below do
not change its observation or Gameplay semantics.

### Physical observation and invalidation

PC HeldKeyValues is the single physical observation truth. Slate key/button and
analog observations update it; paired analog axes are derived there. Permission
checks decide whether consumers may act, never whether a physical key is Released.
ReadHeldKey/ReadHeldAction retain continuous-value semantics (missing reads zero).
FindHeldKeyObservation exposes validity to release-sensitive consumers: missing
means Unknown; a present zero means an observed Up/analog zero.

- Owned accepts observations and permits commands subject to Router authorization.
- Transition permits no new commands, retains Held/modifiers/latches/accepted
  locomotion and does not generate a zero, Release, or ownership-loss edge.
- Definitive Lost enters InvalidatePhysicalObservationForExternalOwnership once.
  It clears HeldKeyValues, HeldInputModifiers and ConsumedInventoryKeys, retaining
  the existing external-loss Character ClearMovementInput behavior. It does not
  stop velocity, consume pending movement, synthesize Release or mutate requests.
- Initial Lost invalidates without broadcasting loss. Owned -> Lost (including
  an intervening Transition) broadcasts the independent loss edge once. Repeated
  Lost does neither again. Permissions query the adapter, not edge memory.
- Activation, polling, request acquisition and analog ingress use that lifecycle.
  Analog Transition drops the sample without writing zero; Lost invalidates.
  Recovery samples ownership but never reconstructs Held or synthesizes events.
- InvalidateGamepadObservationOnDisconnect uses the existing matching local-user
  filter and removes gamepad observations/Inventory latches only. Keyboard/mouse,
  modifiers and accepted locomotion remain. Disconnect is not physical Release.
  Scope remains FKey-based gamepad input, not per-device-ID tracking.
- Real Up updates only its observation/paired axis and Inventory press latch.
  Processor ConsumedPresses pairs consumed Down/Up delivery and survives both
  invalidation paths until the matching Up or processor teardown. It is separate
  from the Inventory long-press latch ConsumedInventoryKeys.

Capability denial invokes none of these invalidation paths. Scan cancellation
belongs to its ownership-lost listener; persistent UI requests survive. No focus
grace, timer, delayed zero, snapshot or second observation registry is present.
The private explicit-state analog seam tests the production lifecycle policy;
it does not override runtime ownership. Regression friends/fixtures remain used.

### Continuous cursor axes

ReadCursorStick derives from the existing paired Held axes with the original
per-component 0.18 dead zone and float remap. GetCursorInputAxis uses LS, adds RS
when the winner allows Cursor+Look, and clamps combined magnitude to one. Cursor
speed, Y inversion, Slate delta and Look delta scaling retain their existing feel.
IsCursorStickLookActive checks either stick independently, so opposed sticks still
suppress mouse-warp Look even when their combined direction cancels.

There is no separate virtual-axis storage, analog second write or winner-change
reseed. Consumer permission changes do not change physical axes. Puzzle keeps its
own radial dead zone, keyboard precedence and inverse Pan interpretation.

### Synthetic pointer surface boundary

The generic PC synthetic left-click helper requires external ownership and Confirm
permission. Slate's actual hit path must contain the exact game viewport; sharing
the active host window is insufficient. An existing pointer captor must also be
inside that surface. Rejected targets emit no Down/Up or focus/cursor changes.
Accepted targets use the local Slate user and hit-path window, preserve the
recursion guard, and always pair emitted Down with Up even if the UI closes.
Inventory's authorized receiver still calls this helper; Dialogue retains direct
receiver execution. No Inventory/B/edge-coordinate special case is introduced.

### Parkour Held and consumption

Held is a continuous request. ConsumedHeldMarkers belongs to the current Marker
overlap/execution opportunity, not an entire physical hold. Character calls
UpdatePlayerHeldRequests with the local PC and Jump/Drop actions. Startup uses the
existing Parkour capability and business feasibility rules, with Jump priority.

A successful launch stores only the participating physical Key identities for
that marker. GetObservedHeldKeysForAction obtains nonzero observations from the
currently resolved mappings without reevaluating modifiers. Current Jump/Drop
mappings are digital Space/B and Ctrl/A with no modifiers. Parkour stores no
physical values and never adds later presses to an existing consumption.

There are two independent ways to regain an execution opportunity:

1. Real Exit immediately removes that marker's registration and consumption,
   even during Parkour. Later Enter can execute with the key still Held:
   A -> B -> A -> B is supported. Duplicate Register without Exit never rearms.
2. Within the same overlap, all sources participating in that consumption must
   have valid zero observations before release-based rearm. A subsequent press
   can execute again. A Held or Unknown source blocks this release path; an unused
   alternate binding does not. No global Action latch exists.

Lost/Unknown, Transition, mapping shadow/restoration and capability denial do not
prove Release or generate an entry. Source identity remains usable when a mapping
is shadowed. No release history is cached: invalidating a formerly zero source
makes it Unknown again; the same-overlap release path needs observed evidence.

Marker OnEndOverlap checks TriggerBox->IsOverlappingActor before unregistering.
UE removes the ending pair before broadcasting, so a remaining character component
means no actor Exit. No additional overlap cache, counter or epoch ID is needed.
FinishParkour prunes only invalid markers; it does not defer overlap-exit cleanup.
The held update also prunes invalid entries. Component destruction ends the map's
lifetime. Source-less scripted launches can rearm through real Exit or marker
lifecycle termination. The bool-based Parkour Held/rearm API and Character copies
are removed; regression fixtures submit physical observations through PC instead.

### Accepted low-priority limitation

After definitive ownership loss (Alt+Tab/F8), Held observation is invalidated.
Ownership recovery does not rebuild it. A Parkour key held continuously across
that loss may therefore require a real Release/Re-Press to resume traversal.
The user explicitly accepts this limitation. Do not add reacquire snapshots,
synthetic Held/Release, grace periods or guesses about missed physical events.
A fresh observed repeat is still not Release of an already-consumed same-overlap
request. This limitation does not apply a gameplay permission change as zero input.

### Regression and handoff

Input regression covers invalidation idempotence/scope, Down/Up pairing, analog
Transition/Lost/recovery, derived LS/RS axes, synthetic pointer surface ownership,
Parkour continuous A-B-A-B traversal, same-overlap duplicate prevention, true
Release/Re-Press, partial multi-source release, mapping shadow, duplicate Register,
invalid-marker cleanup and multiple character components exiting separately.
Puzzle and Freeze regressions remain part of the full suite. The revised traversal
assertions replace the discarded cross-overlap physical-hold restriction.

Editor Win64 Development and full Input / Puzzle / Freeze regression (including
Parkour) are the checkpoint checks. Headless tests do not replace live Slate/device
validation; user PIE acceptance is recorded above with the explicit limitation.
No temporary diagnostic code or console variables remain. No assets are modified.
For stage five, retain the existing virtual-cursor/Look wiring, click recursion
protection and Mapping Context uses. Stage 5 asset/caller review confirmed these
still have real responsibilities; only the completed one-time migration tool was
removed in Stage 5.2.

Final stage-four cleanup verification (2026-10-04): Editor Win64 Development
build succeeded; Input / Puzzle / Freeze (including Parkour) passed 21/21,
with 16 clean and 5 existing warning-bearing tests, zero failures/not-run.
ParkourPhysicalRelease passed without warnings/errors. Final report:
Saved/Automation/StageFourFour/index.json. git diff --check passed. Source/docs
contain no temporary stage-four diagnostic identifiers; retired runtime Held
bool paths, focus grace and virtual-axis copies have no remaining source matches.
The accepted ownership-recovery limitation is not a commit blocker. This is a
stable historical stage-four checkpoint. Subsequent stage-five work is recorded below.

## Stage 5.1: game-scoped Slate focus navigation

DefaultEngine.ini selects UTAGameViewportClient. It overrides the existing
UGameViewportClient::HandleNavigation hook and returns handled without changing
focus. The controller no longer writes application-global NavigationConfig flags.
No configuration snapshot, delegate registry, mode branch or timer is added.

UE 5.6 converts unhandled focusable-widget key/analog input into an FReply navigation
request in SWidget::OnKeyDown/OnAnalogValueChanged. ProcessReply resolves a candidate;
ExecuteNavigation consults the window's ISlateViewport only when the source focus
path contains that viewport's widget. FSceneViewport forwards to its viewport client
before Slate sets navigation focus. RegisterGameViewport/RegisterViewport establish
this chain for standalone and Selected Viewport PIE. The policy therefore belongs to
the game viewport's lifetime and applies to its local users, not the application.
Editor siblings in the same host window do not pass the source-path containment test;
other PIE windows have their own viewport. No Editor-specific code is needed here.

This consumes Slate navigation focus requests, including explicit navigation
destinations, while leaving direct SetUserFocus/Presentation available. Current UI
uses receiver actions and pointer interactions, not intentional Slate focus travel.
New UI that intentionally needs automatic/explicit Slate navigation must revise this
viewport policy explicitly rather than adding a PC mode exception. Detached popup
windows outside the game viewport path are outside this hook's scope.

RoutePlayerInputKey runs earlier in InputPreProcessors; authorized receiver Navigate
does not become a Slate navigation request. Cursor/Pan observation and consumer
algorithms are unchanged. Scan focuses the viewport and retains its existing
SceneViewport axis delivery and cursor-look path. Button Accept/Back classification,
UMG pointer clicks/drag/drop and synthetic-click validation are not handled by this
hook. History's Accept -> Close callback remains intact.

UE's GetRelevantNavConfig already selects a separate EditorNavigationConfig for the
keyboard user outside registered game viewports. The old writes cannot be described
as disabling all Editor navigation; they still modified shared application game
configuration without a viewport lifetime. The scoped replacement modifies neither
configuration and installs no BeginPlay/EndPlay restore logic.

Validation (2026-10-05): Editor Win64 Development succeeded. Input/Puzzle/Freeze,
including Parkour, passed 22/22 (17 clean, 5 existing warning-bearing tests).
ScopedNavigation uses a virtual Slate window, actual ProcessReply/FSceneViewport
routing and a deterministic candidate to verify game rejection versus a same-window
non-game source, raw Tab/Arrow/D-pad conversion, controller navigation, direct focus,
Enter/Space/Virtual Accept and Back classification, and unchanged global flags.
It does not replace interactive PIE, device, popup or concurrent-PIE validation.
Report: Saved/Automation/StageFiveOne/index.json.

Restart the Editor before PIE acceptance: the configured viewport-client class is
selected at engine startup; hot reload does not replace an existing viewport client.
Verify Editor Tab/Arrow before/during/after PIE; Inventory/Dialogue/History/Puzzle
Tab/Shift+Tab/Arrow/D-pad/LS without automatic focus travel; custom receiver Navigate;
History Accept; legal/edge synthetic clicks; Scan Cursor+Look; F8/Alt+Tab and Stop PIE.
Stage 5.1 subsequently passed the user's interactive PIE acceptance.

## Stage 5.2: verified migration/source cleanup

Removed the completed input Blueprint migration commandlet (.h/.cpp). No other
source uses its BlueprintGraph API, so the editor module's direct dependency is
removed too. KismetCompiler, AssetRegistry and the other editor dependencies remain;
the puzzle-layout and story-editor tools are not part of this cleanup.

Removed unused PC Button/WidgetTree includes and the duplicate SlateApplication
include, plus Inventory's unused EnhancedInputComponent include. Removed the obsolete
stick > 0.5 Sprint comment and two commented-out Scan blocks (debug print and an old
Invalid case). No executable gameplay statements or assets change. Scan duration's
intentionally disabled decrement, Puzzle Retry placeholder, mapping contexts, public
business APIs and Inventory repeat interception remain unchanged.

Stage 5.2 validation (2026-10-05): Editor Win64 Development succeeded after removing
the direct BlueprintGraph dependency. Full Input/Puzzle/Freeze regression (including
Parkour and scoped navigation) passed 22/22: 17 clean, 5 existing warning-bearing,
zero failures/not-run. git diff --check passed. Source/config/docs contain no removed
commandlet-name references. Report: Saved/Automation/StageFiveTwo/index.json.
This is behavior-neutral source/dependency cleanup; no additional PIE pass is required.
Stage 5.2 is code-accepted under the user's criteria.

## Stage 5.3: Inventory Preview repeat interception

Removed only the hardcoded Tab/Gamepad_Special_Left repeat Handled branch from
Inventory NativeOnPreviewKeyDown. The exact owner/handle/Navigate authorization
defense still returns Handled before Blueprint Super for stale or unauthorized UI.

The physical processor observes Down before routing. In base Gameplay, the first
Inventory Toggle reaches Enhanced Input Started and the Character opening entry.
ConsumeInventoryTogglePress latches the currently held mapped sources. With an
Inventory receiver winner, RoutePlayerInputKey consumes mapped Repeat without
executing Close; Slate therefore never delivers that Repeat to UMG Preview.
Duplicate Toggle attempts during the same hold are rejected by ConsumedInventoryKeys.
After closing, the latch still prevents reopening until physical Up clears it;
processor ConsumedPresses independently pairs the matching Up even after owner release.
These policies use Action mappings, not the Tab/SpecialLeft key names.

Authorized Preview may observe an unmapped Repeat; observation grants no behavior
permission. The current Inventory Blueprint has no Preview behavior handler. Future
Blueprint player behavior must use authorized receiver/entry paths; a raw public
business call is intentionally also available to scripts and is not a player gate.

InventoryRepeatRouting tests actual Slate routing and the Inventory WBP with Tab,
SpecialLeft and a transient K rebind. Opening uses the existing Toggle-latch fixture
seam; closing routes the actual receiver. Tests cover repeated/duplicate delivery,
release and re-press, held close/no reopen, pairing after owner release, mapped Repeat
not reaching Preview, and stale/non-winner Preview and behavior rejection.

Validation (2026-10-05): Editor Win64 Development succeeded. Full Input/Puzzle/Freeze
regression including Parkour and scoped navigation passed 23/23: 18 clean, 5 existing
warning-bearing, zero failures/not-run; the new test has no warnings or errors.
Report: Saved/Automation/StageFiveThree/index.json. git diff --check passed.
The user subsequently passed the 5.3 Inventory PIE smoke and formally accepted it.
No mapping assets or other runtime paths changed.

## Stage 5.4: final responsibilities and extension boundary

Final source audit found no remaining legacy ownerless request/release API, UI
ownership bool/count, historical Presentation snapshot, focus frame grace, separate
virtual axes/reseed, PC Puzzle behavior, retired Parkour bool-rearm path, temporary
Stage3/P44 trace/CVar, removed migration commandlet reference or runtime global
NavigationConfig write. Tests deliberately set/restore fixture focus; the localization
editor tool focuses its own editor surface. Neither is a player Presentation writer.
Unused Touch assets remain outside the supported runtime input path. Saved audit
artifacts and the capability-gated debug puzzle opener are not migration execution.

| Layer | Owns / may know | Does not own |
|---|---|---|
| Router | Live owner requests, exact handles, exclusive capability winner, declarative Presentation | Slate, editor state, receivers, Gameplay execution |
| Ownership Adapter | External game surface Owned/Transition/Lost; centralized PIE simulation/focus evidence | Router arbitration, Held reconstruction, Gameplay cancellation |
| Input Processor | Device observation, physical Down/Up/analog delivery, consumed delivery pairing, forwarding to PC | UI rules, independent permissions, Presentation restoration |
| PlayerController | Sole Held physical truth, ownership edges, authorized winner forwarding, applying Presentation, cursor/synthetic-pointer adapter | Puzzle rules, UI mode gates, domain feasibility, global navigation settings |
| Character | Accepted locomotion, player Gameplay ingress, SubmitPlayerLook, explicit StopCurrentMovement, owned movement coordination | Slate/editor focus, UI ownership, independent physical cache |
| Widget / receiver | Own request/issuer token, action resolution, exact authorization of keys/pointers, domain UI behavior and teardown | Borrowing another winner's capabilities or restoring previous Presentation |
| Enhanced Input | Resolved mappings/modifiers, prompts and retained Gameplay callbacks/Look axes | Parallel UI behavior callbacks or parallel accepted Move writes |
| Parkour | Marker feasibility/movement, overlap opportunities, consumed source identities | Physical Held values, inferred Release from Unknown, UI ownership |
| GameViewportClient | Consume default Slate focus Navigation originating inside its game viewport | Accept/Back behavior, receiver Navigate, global config or editor sibling paths |

Runtime Cursor/Focus/InputMode authority is exclusively SynchronizeInputPresentation:
winner declaration -> cursor output and input-mode focus application. Applied-output
bookkeeping avoids redundant operations; it never determines winner or restoration.
No request means deterministic GameOnly/hidden cursor/viewport focus. Missing focus
target falls back within the same winning request. Owner expiry recomputes requests.
GameAndUI/UIOnly currently use DoNotLock; GameAndUI preserves cursor during capture.
Pointer motion and guarded game-surface synthetic clicks are separate from ownership.

### Read-only extension checks

| New UI | Request declaration | Receiver / execution |
|---|---|---|
| Cursor + Confirm | Widget owner, suitable priority; Cursor/Confirm, GameAndUI, visible cursor, self focus; omit Move/Look | Owner resolves its mapped Confirm and exact handle is authorized before execution |
| Skip-only, no cursor | Widget owner; Confirm used as the sole skip command, hidden cursor, target or viewport focus; omit Gameplay capabilities | Owner resolves its Skip binding to Confirm and implements Skip locally. A separately distinguished Skip permission would require a vocabulary addition, not a mode branch |
| Cursor + Look, no Move | Widget owner; Cursor/Look plus required UI actions, visible cursor, GameAndUI and target focus; omit Move | Existing cursor+Look plumbing calls SubmitPlayerLook; UI receiver handles its own commands |

All three fit current arbitration, Presentation and forwarding without Inventory,
Dialogue or Puzzle branches in PC/Character. Receiver implementations are currently
C++ (CannotImplementInterfaceInBlueprint); derived WBP can customize exposed UI.
New custom hold/chord triggers or intentional default Slate focus navigation require
an explicit design review; raw key resolution does not silently implement them.

### Minimum future UI integration rules

1. Request with a live owner and declarative requirements; retain exact handle and
   weak issuing PC. Release idempotently on close and destruct; never restore a snapshot.
2. Resolve/execute through the winner receiver. Authorize every real player pointer,
   button and drag ingress using external ownership + exact owner/handle + capability.
   Blueprint player events must not bypass these entry points via public business APIs.
3. Keep domain feasibility in Gameplay. Permission denial submits no command; it is
   neither Release nor a Stop. Explicit domain lifecycle Stop remains separate.
4. Read physical observations from PC. Unknown is not Up; do not rebuild Held on
   ownership recovery, change it on winner transitions or duplicate continuous axes.
5. Keep mapping contexts/prompts and cleanup guards where they have real duties.
   Do not add mode-specific PC/Character gates, global Slate policy or frame grace.

Accepted limitation remains unchanged: Lost invalidates Held without reconstructing
it on recovery. A Parkour hold across Alt+Tab/F8 may need Release/Re-Press. No repair
is planned or required for this checkpoint. Detached popup navigation and arbitrary
future Blueprint bypasses are not automatically protected by the viewport/ingress
contracts; integrate such surfaces deliberately rather than adding hidden exceptions.

Final validation (2026-10-05): Editor Win64 Development build succeeded (target
up to date). Full Input/Puzzle/Freeze regression, including Parkour, passed 23/23:
18 clean, 5 existing warning-bearing, zero failures/not-run. Report:
Saved/Automation/StageFiveFour/index.json. The first sandboxed launch could not
access UE's Zen utility; the approved unrestricted rerun completed with exit code 0.
git diff --check passed. The three untracked files are the intended stage-5 viewport
client header/source and scoped-navigation regression; include them in the checkpoint.
Stage 5.4 changed documentation only. Stages 5.1 and 5.3 already have user PIE acceptance;
no new runtime behavior needs additional PIE validation for this documentation audit.
