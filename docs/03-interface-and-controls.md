# Touchscreen and physical controls

## Agreed interaction

The interface is a **new, simple drag-and-drop design for this instrument — agreed**, built with LVGL and laid out natively for the 800 × 480 display.

- Each module is a **square showing its name**.
- Squares are **dragged** to arrange them on the screen.
- **Entering** a module opens a page with its internal parameters (for example drive, gain, volume, time).
- Modules are **patched in series or in parallel** with virtual cables.

Physical knobs and stomp switches control assigned parameters.

## Beebo as reference

Beebo's interface (1280 × 720, with many fixed coordinates) shows that a modular patch bay works on a pedal touchscreen. Concepts carried over: modules connected by visible cables, a separate parameter page per module, a categorized module browser, and assignments linking physical controls to parameters.

Screens, layouts, styling, icons, and code are our own. The new interface is deliberately simpler than Beebo's.

The user's five reference photos also illustrate why parameter pages need not all have the same layout: the tuner emphasizes a note and cents, the oscillator emphasizes model selection, and the granular page groups continuous controls. Keep a consistent navigation frame while adapting the content to the task. The photos' branching graphs reinforce visible split/merge routing; their cables alone do not establish engine summing behavior.

## Casette as an additional reference

Casette's published source uses LVGL framebuffer output, evdev touch, tabs, level meters, a tuner, and a CPU/temperature display for an advertised 800 × 480 screen. These are useful implementation observations, not proof of usability on our 5-inch HDMI display. Retain our square-module canvas and module pages with a generic fallback. See [the source audit](09-casette-and-nam.md).

## Visual identity — open

The basic form is agreed: named squares and cables. The instrument is named **Peepo**, agreed 14 October 2026. Colour palette, typography, and square and cable styling remain open. Design for legibility on a small screen viewed from standing height on a pedalboard, and review on the physical display.

## Patch view (main screen)

```
┌──────────────────────────────────────────────────────────────┐
│ Patch name ●            audio ok        [+]  [patches]  [⚙]  │  header
├──────────────────────────────────────────────────────────────┤
│                         ┌───────┐                            │
│  ┌──────┐   ┌───────┐ ┌─┤ DELAY ├─┐   ┌───────┐   ┌───────┐  │
│  │  IN  ├───┤ DRIVE ├─┤ └───────┘ ├───┤  EQ   ├───┤  OUT  │  │
│  └──────┘   └───────┘ │ ┌───────┐ │   └───────┘   └───────┘  │
│                       └─┤CLOUDS ├─┘                          │
│                         └───────┘                            │
└──────────────────────────────────────────────────────────────┘
          series ─────────── parallel ─────────── series
```

*Illustrative layout, not a final design.*

- **IN** and **OUT** are fixed squares representing the audio interface's input and output. They cannot be deleted.
- Each square has an **input jack on its left edge** and an **output jack on its right edge**. Sound sources without an audio input show only an output jack.
- Squares **snap to a grid** so patches stay tidy and touch targets stay predictable.
- If a patch does not fit, dragging empty space pans the view. Zoom is not required for the first version.
- The header shows the patch name, an unsaved-changes marker, audio readiness, and buttons to add a module, open the patch manager, and open settings.

### Gestures — proposed

All gestures work with a single finger. The panel reports five touch points, so two-finger gestures such as pinch zoom are possible later; none is required.

| Gesture | Action |
| --- | --- |
| Tap a square | Enter the module's parameter page |
| Press and drag a square | Move it |
| Drag from an output jack to a square's input jack | Create a cable |
| Tap a cable | Select it; a delete button appears |
| Long-press a square | Menu: bypass, duplicate, delete, assign to switch |
| Tap **+** | Open the module browser; the new square appears in a free grid position |

A drag starts only after a small movement threshold, so a tap is never mistaken for a move.

### Series and parallel routing

- **Series:** one module's output connects to the next module's input.
- **Parallel (split):** one output may connect to several inputs.
- **Parallel (merge):** several outputs may connect to one input; the engine mixes them.

Every audio port has its own connector, and **every cable is one channel from one connector to one connector** (agreed 13 October 2026, replacing the earlier single logical stereo cable). A stereo module shows two connectors on each side, left above right; they are patched separately. Any output may go to any input:

| Wanted | How it is patched |
| --- | --- |
| Stereo chain | Two cables: left to left, right to right |
| Left and right through different effects | One cable from each output to a different module |
| Mono source into a stereo effect | One cable to one input, or two cables from the same output to both |
| Stereo into a mono effect | One cable from the side wanted, or both into the one input (the engine sums them) |
| Swap left and right | Cross the two cables |

**Measured:** Ingen sums several cables into one input at unity, with no compensation ([evidence](14-ingen-engine-evidence.md)); two equal signals are +6 dB. Whether to attenuate automatically or offer a mixer module is still to be decided.

The desktop simulator's **Patches → Parallel demo** starts with IN → Drive, splits into Clouds and Delay, and reconnects both branches at OUT. Branches use ordinary editable cables, one per channel; loading the demo uses the unsaved-replacement guard. This demonstrates graph editing, with no audio processing.

Feedback loops (a cable that leads back to an earlier module) are refused in the first version.

### Control (CV) cables

Agreed 13 October 2026 (D16 revised). Modulation sources such as envelopes, LFOs, random and rhythm generators have control outputs; most Poly modules have a control input beside each knob (the granular module has twelve).

- A module with control outputs has one **square connector on its top edge**; a module with control inputs has one **on its bottom edge**. Control connectors and cables are amber, audio ones green.
- Drag from a top connector to another module's bottom connector. A list then asks which output and which input; a module with one control output skips the first question.
- Tapping a cable names it in the footer (module / port > module / port) and offers Delete. Several control cables between the same two modules lie on top of each other and are picked one at a time.
- Audio connects to audio and control to control. The engine itself would accept either into the other; the interface does not offer it.
- Feedback loops are refused for control cables too.

### MIDI cables

Agreed 20 October 2026 (D16 revised again; [MIDI](39-midi.md)). The look was chosen from three sketches: side jacks in a colour of their own.

- A MIDI connector is a **diamond on the side edges**, inputs on the left and outputs on the right, below the module's audio jacks. Diamonds and MIDI cables are teal (`0x2dd4bf`), a colour no module group uses.
- **IN has a MIDI output** (every MIDI device, merged) and **OUT a MIDI input** (to every device), below their two audio jacks.
- Drag from a MIDI output to a MIDI input, as for audio. MIDI never joins audio or control; the drop is refused with a reason. Holding two modules also offers their MIDI ports in the cable list.
- A plugin's other atom ports, such as the one a Cabinet or NAM takes its file on, are not connectors.

A visual cable must always correspond to a valid engine connection; a refused connection shows a short reason.

### MIDI pages

Added 20 October 2026 ([MIDI](39-midi.md)), in the rows-and-buttons style of the Controls page:

- **Settings → MIDI**, instrument-wide: clock out on or off, Start / Stop on or off, tempo from the patch or from incoming clock (with the clock heard), program change off, on one channel or any; then the MIDI devices plugged in, each with In and Out switches. A chosen option has a green border.
- **MIDI learn**: hold a bar on a module page. The page names the control and what sets it; **Learn** waits for a control moved on a MIDI device, **Forget** removes the binding, Cancel or Back ends a wait. The bar shows the binding under its name (`CC 7 / 3`), beside any encoder or stomp pinned to it.
- **Patches → MIDI** (*MIDI on load*): the control changes this patch sends to every device as it loads, up to six, each a channel stepper, a controller slider, a value slider and a remove button; **+ Add** in the header.
- The **Tempo page** says *FOLLOWING MIDI CLOCK* when the tempo comes from incoming clock, and has **Restart clock** (Stop, then Start) to line another device's pattern up by hand.

## Module page

Opened by tapping a square. It fills the screen with the module's parameters and a **back** button to the patch view. Redone on 17 October 2026 in the direction of Beebo's module pages ([record](31-module-page-and-two-encoders.md)):

- **Every control is a bar** with its name, its value and a fill in the module's group colour, two columns of six: twelve controls on one screen, more scroll. There are no pages.
- **Drag a bar sideways** to set it (relative: the bar's width is the whole range, so a touch never jumps the value). **Tap** a switch to flip it or a mode to step it.
- Logarithmic parameters (frequency, time) move logarithmically; whole numbers in whole steps; a time in beats by note value from an encoder.
- The overlay's groups are headings on the page; a module's file is its first bar.
- The header holds the module's name, an **ON** / **BYPASSED** pill and **ACTIONS**.
- Encoder 1 moves a white highlight between bars, encoder 2 turns the highlighted one (see below).

Every module uses this same page, generated from plugin metadata and the project catalogue overlay. Only the Tuner adds something: its meter, above its bars. The earlier custom Clouds and Delay pages are gone.

### NAM and cabinet pages — proposed

A tested NAM module needs a local `.nam` file selector, current model name, input gain/calibration, output level, and supported quality control. An amp-only capture normally needs a separate cabinet IR module; a capture that already includes a cabinet may not. Make asset and loading errors readable and retain the last usable state or show an explicit failed state according to verified engine behaviour.

Model and IR paths are not ordinary numeric sliders. The engine client must send the selected plugin's supported file-path messages and read back the accepted state. Preserve asset references and quality with the patch. Show loading progress without blocking drawing or audio, and prevent unsupported quality controls from suggesting that every model can change size.

Expose DSP load/xrun status and a settings view for CPU, memory, and temperature using verified engine/system measurements. Do not label a process CPU estimate as total system load or spare audio capacity. Automatic quality changes are not agreed; any later policy must make the change visible and preserve user intent.

## Visual style

Every page uses one look, taken from a design sketch the user chose (15 October 2026):

- **Black background.** Colour appears only where it means something: a module's group, a state, the one main action on a page.
- **Header.** On the left, a dot (green when the engine answers, orange when it does not), a `PEEPO // SECTION` line in JetBrains Mono, and the page title in bold capitals. On the right, the page's buttons as pills.
- **Footer.** A thin line, then `< BACK` on the left (every page except the patch canvas), the latest message in mono, and the BPM and CPU readings.
- **Buttons.** Dark with a grey outline, bold capital labels. The main action on a page is white with black text (Add, Save, Create, Replace, Load, Learn, Tap, the confirming button).
- **Every page is built like a module page** (21 October 2026). The page's actions are pills in the header, the main one white and first. Below, one list of compact bars (300 by 40), two to a row, with the rail on the left once it is longer than the screen. Sections are headed in small green capitals, and explanations are short grey lines. Nothing pages. A bar has its name on the left, a line under it, and its value on the right. A switch is filled green when on. Choices are segmented bars. A number is a bar you drag sideways, as on a module, and encoder 2 turns it. Confirmations put their two choices in the header (`Replace patch`, `Cancel`). The code is `page_list`, `list_bar`, `choice_bar`, `value_bar` and `header_pills` in `src/ui/application.cpp`. The browser, the patch canvas, the naming keyboard and the custom module pages (Loo-pi, Euclidean) keep their own layouts.
- **Fonts.** Montserrat Bold for titles, labels and buttons, Montserrat Medium for body text, JetBrains Mono for status lines (see [notices](../THIRD_PARTY_NOTICES.md)).
- **Module colours.** A module takes the colour of its group, the same on the browser pill and tag, the square on the canvas, and the sliders on its parameter page. Audio cables are light grey; control cables and sockets are the CV group yellow; orange marks a fault or a selection.

## Module browser

A full-screen page with three bands:

- **Header:** the group pills on the right, each as wide as its name: ALL, GAIN, MOD, TIME, TEXTURE, CV + UTIL. The open group is white. A group with no modules in the catalogue gets no pill.
- **Middle:** inside a group with more than one finer category, a row of category chips (ALL, then each category; the open one in the group colour) above the cards. Then a scrolling grid of large module cards, two per row. Each card shows the module name, a tag for its finer category in the group colour, and a two-line description, with black space below.
- **Footer:** Back, a `GROUP // N MODULES` line, and **Add module**.

The groups merge the catalogue's categories the way the Beebo firmware does:

| Group | Categories |
|---|---|
| GAIN | Drive, Amp, Cabinet, Dynamics |
| MOD | Modulation, Filter |
| TIME | Delay, Reverb |
| TEXTURE | Texture, Resonator, Pitch, Sound source |
| CV + UTIL | Control, Utility, and any category not listed |

Adding a module:

- The first tap on a card picks it (white border). Tapping it again, or pressing Add module, adds it.
- With no pin on them, encoder 1 steps through the cards and encoder 2 through the groups. Pushing an encoder adds the picked card.
- The chosen group is remembered between visits, but the page always opens scrolled to the top with nothing picked.

Only plugins that are installed and passed compatibility checks can be added. The others are dimmed, and their description is replaced by the reason they cannot be added.

## Patch manager

Full-screen list of saved patches: load, save, save as, rename, delete, and set as the startup patch. A **Revert** pill in its header reloads the last saved revision of the patch being played. Text entry uses an on-screen keyboard sized for fingers.

## Settings, diagnostics and power

Settings shows the engine connection, patch storage and network; its header pills lead to **Controls**, **Diagnostics** (engine, power supply, temperature, memory, storage, uptime, read from the kernel) and **Power** (shut down, restart). See [the record](28-screens-and-diagnostics.md).

## Initial sizing targets — proposed

- Module squares: about 96 × 96 pixels, so roughly seven columns by four rows fit below the header.
- Jacks: drawn small but with a touch area of at least 40 × 40 pixels.
- Normal labels: approximately 18–22 pixels; module names on squares: up to two lines, abbreviated if needed.
- Primary interactive controls: approximately 44–48 pixels minimum in each usable dimension.
- Cables: at least 3 pixels wide, with a wider invisible touch area.

These are starting values, not proven acceptance results. Test them on the 5-inch screen and adjust based on readability and touch errors. Do not claim usability from a desktop simulator or screenshot.

## Physical controls

Two push encoders and three momentary SPST footswitches (17 October 2026; four encoders before). Encoder 1 moves through the page, encoder 2 turns values; one encoder alone can do both. The behaviour, mapping rules, assignment screen and wiring are recorded in [physical controls](18-physical-controls.md) and [the two-encoder record](31-module-page-and-two-encoders.md).

| Control type | Behaviour | Design question |
| --- | --- | --- |
| Encoder 1 | Move the highlight: modules, bars, cards, buttons | Acceleration and fine adjustment |
| Encoder 2 | Turn the highlighted value, or a pinned parameter | Relative increments, acceleration, fine adjustment |
| Potentiometer | Set an absolute parameter value | Pickup/soft takeover to avoid jumps after patch changes |
| Encoder push | Open, flip or step what is highlighted; hold encoder 1 for Back | Must not conflict with value adjustment |
| Stomp switch | Musical action chosen in the patch | Toggle, momentary, tap, and hold behaviour |
| Expression pedal | Optional continuous control | Hardware input and range calibration are not yet scoped |

Parameter mappings must preserve ranges and types: frequency may benefit from logarithmic movement, mode selection requires discrete values, and freeze requires a defined binary state. Where smoothing is needed, apply it without delaying urgent switch actions.

A parameter can be pinned to a knob, so changing the displayed module does not unexpectedly change a knob's role. The interface shows each knob's current assignment.

## Suggested stomp actions

- Toggle one effect or a group of effects.
- Hold or toggle Clouds freeze.
- Tap tempo for modules that accept a usable tempo reference.
- Move to the next/previous patch.
- Trigger supported looper actions once the looper is validated.

Distinguish software effect bypass from physical guitar bypass. A direct unprocessed signal path while Linux is unavailable requires suitable audio/bypass hardware; an on-screen bypass button alone cannot provide it.

## Assignment persistence

Save each mapping with its patch: control identity, target module instance, parameter symbol or action, range, mode, and any inversion. Detect mappings whose module or parameter no longer exists and show them for repair rather than silently controlling a different target.
