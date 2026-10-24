# Reading the GulchCE code

This guide is for someone who can follow individual lines of code but wants
to understand how they fit together. Start with one visible behaviour and
follow the values it uses through a few functions. You do not need to read
the engine from beginning to end.

Read the [field-of-view walkthrough](field_of_view.md) first, keeping the
[glossary](glossary.md) nearby. It follows a setting from the menu to the
camera, reticle and first-person weapon. The glossary explains both engine
terms and the C conventions used in that example.

## Where the pieces live

GulchCE is a fork of OpenCE. Both compile the decompiled Xbox game with a
platform layer in place of the Xbox services. GulchCE also has its own
features and MCC compatibility modules. A directory's name does not always
tell you which platforms compile it: much of `port/linux/` is shared.

| Place | Read it to understand |
| --- | --- |
| [source/](../../source/) | The original engine's gameplay, cameras, rendering and data structures, with marked port changes. |
| [port/linux/game/](../../port/linux/game/) | Added game features and integration with engine types: menus, FOV, networking, map support and mods. |
| [port/linux/src/](../../port/linux/src/) | Settings and platform services, including the OpenGL implementation of the graphics calls the game makes. |
| [port/linux/include/](../../port/linux/include/) | Shared limits, compatibility headers and declarations that connect game code to the port. |
| [port/windows/](../../port/windows/), [port/android/](../../port/android/), [port/web/](../../port/web/) | Platform-specific entry points, runtime support and build details. |
| [tools/](../../tools/) | Build scripts, asset generators and tests. Some menu content is generated here rather than written directly. |
| [docs/](../) | Detailed explanations of features, formats and compatibility decisions. |

For example, changing an FOV setting travels from menu code in
`port/linux/game/` to configuration code in `port/linux/src/`. The engine
in `source/` later calls the FOV module while preparing a view. These are
connections between responsibilities, not separate copies of the game.

MCC compatibility has its own modules and boundaries. Use the boundary
inventory in [CODINGSTANDARDS.md](../../CODINGSTANDARDS.md) to find them.
Xbox, Custom Edition and MCC support are not interchangeable loading paths;
the map-family isolation rules are in [AGENTS.md](../../AGENTS.md).

## How to follow a feature

1. Start with a concrete question, such as "what changes when I select
   120 degrees?" Find the setting name or the function that handles the
   visible action.
2. Read the feature's header and opening comment. A `.h` file usually
   describes the interface other files use; the `.c` file implements it.
3. Find one caller and one consumer of the result. Establish what goes in,
   what comes out, and which state is modified before following helpers.
4. Read the early returns. They often explain when a feature must leave
   the existing behaviour alone.
5. Identify state that survives the call. Ask who updates it, who reads it,
   and when it is reset or restored. A per-player array and a single shared
   variable have different implications in split screen.
6. Read the relevant tests and their limits. A calculation checked in a
   fake world does not establish that the final picture looks correct.

Use your editor's search for the exact function name. From the repository
root, these searches also locate the first walkthrough's entry points:

```sh
rg -n 'render_fov_vertical' source port/linux/game
rg -n 'display.fov' port/linux/src/port_config.c tools/port_settings.py
```

There is no need to follow every dependency immediately. If a helper's
contract is clear, continue with the value it returns and revisit its
implementation when it matters to the question.

## Reading inherited code and port additions

Changes in the decompiled `source/` are marked with comments beginning
`port:`. These show where to look for an integration point; they do not
by themselves say whether OpenCE or GulchCE introduced it. Use file history
when authorship matters. The FOV module in the first walkthrough is an
existing port feature, not a new GulchCE implementation added by this guide.

Some source files start with long symbol lists and addresses from the
decompilation. Start at the includes and function definitions when learning
the current code. Similarly, original Xbox paths in assertions are historical
context, not paths you need to create on your computer.

Build definitions matter. Code behind a preprocessor condition may not run
in the build you are reading about. See the Defines table in
[AGENTS.md](../../AGENTS.md); in particular, `HALO_LINUX` and
`HALO_CUSTOM_EDITION` are not defined by these builds. Runtime settings and
compile-time conditions answer different questions.

## What an explanation should establish

A walkthrough should connect the visible behaviour to named functions,
trace a concrete example, explain the important constraints and identify
the checks that support it. Use a small flow diagram when the order of
calls is easier to see than to describe. Explain unfamiliar terms where
they first matter and add reusable definitions to the glossary.

Distinguish what the implementation does from why a choice was made.
Existing comments and design notes may explain the reason; otherwise label
an interpretation as an inference. A source trace, an automated test and
an observation in a running game are different kinds of evidence. Record
known gaps instead of implying that a readable explanation proves correctness.

Keep links relative to the document and name functions beside them. Line
numbers move as code changes. Quote only the small expressions needed for
the explanation, and update a guide when its described behaviour changes.
The maintenance expectations are in [AGENTS.md](../../AGENTS.md).

## Where to read next

| Question | Reference |
| --- | --- |
| How does the port compile and replace Xbox services? | [How the port operates](../../port/linux/README.md#how-the-port-operates) |
| How do settings affect the game? | [Settings](../../port/linux/README.md#settings) and the [FOV walkthrough](field_of_view.md) |
| What do the host and clients each decide? | [Distributed netcode](../../port/linux/NETCODE.md) |
| How are Custom Edition maps loaded? | [Custom Edition caches](../custom_edition_caches.md) |
| Where are GulchCE's MCC boundaries? | [Coding standards](../../CODINGSTANDARDS.md#scope-and-implementation-boundaries) |
| How do menu generators relate to the saved XML? | [Menu assets](../../port/assets/menus/README.md) |
| What can tests establish without retail assets? | [Harness](../../tools/harness/README.md) and [Stagekit](../../tools/stagekit/README.md) |
