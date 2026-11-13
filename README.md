# Well fellas (and fellettes), that's a wrap.

MCC map support is locked in at 1.0 and completed. The final refactor is done and we now have full support across desktop, mobile devices and even web browsers.

For fork maintainers, the branch you'd want to fork the code from for yourself is under the aptly-named **GET-YOUR-MCC-HERE** branch (YOU ARE HERE!). It's based against OpenCE's main branch and due to the new networking messages for MCC-specific netcode, the version protocol sits at 42.

## What about crossplay on my fork?!
OpenCE will remain at 41 until the upstream merge is completed, so if you play early adopter but want to keep crossplay, you may need to add a temporary network guard to the MCC-specific netcode. 

This functionality is already present on the **mcc-code-refactor** branch, so you can fork the network guards from this as well. You would need to make sure to remove said guards and sync the MCC-specific netcode back into OpenCE's network protocol once it's merged upstream.

## Do you support every MCC map?
There are some known issues with the map support at the moment, such as unsupported scripts that haven't been reverse-engineered yet. MCC map support does not support skulls or maps that require skulls as part of their scripting, for example.

These unsupported areas are in the very small minority of maps as we've tested over 112+ MCC mods to cover as much ground as possible.

## What if I want to contribute?
**GET-YOUR-MCC-HERE** comes with full MCC map support documentation as well as an easy "reader's guide" for easy maintenance, fixes, and alterations by either yourself or your chosen clanker(s).

All I ask is that if you do fix anything or add support for unsupported scripts that I haven't got around to covering, open a lil PR against Gulch's TESTING branch so I can enjoy it too.

**And just for the avoidance of any doubt, I authorise people to relicense the code (currently AGPL) under their fork's own license (whether permissive or copyleft), e.g. Apache or GPL, my work has and always will be open-source and free to the masses.**

My DMs are open and I will act in a support capacity for those who are eager to get the MCC stuff integrated onto their fork. Just shoot me a message.

Beyond that, enjoy your MCC maps.


## MCC package fixes from OpenCE PR #658

Copy intact Workshop folders into `mcc_maps`; local package names and previews
work without Steam discovery. Map Sharing automatically offers eligible packages,
while required base resources stay local and are validated before installation.
Dedicated servers accept MCC rotation entries, including package names with
spaces. Run `sv_reload` and refresh the page in pregame to discover copied maps.
See [MCC installation and sharing](docs/mcc_maps.md) and the
[dedicated-server guide](docs/dedicated_server.md).

This branch includes current OpenCE main and the complete MCC fixes from PR #658.
It uses OpenCE network version **44**, including the preceding shared-engine
agreement, so fork maintainers can update to the same protocol.
