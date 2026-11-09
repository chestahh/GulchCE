# MCC support reading guide

MCC compatibility uses independent modules and the `mcc_maps` namespace.
Xbox and Custom Edition retain their existing loaders and resource roots.

- [Loading, resources and host sharing](maps_and_resources.md)
- [Console syntax and checkpoint lifetime](console_checkpoints.md)
- [Map formats, installation and compatibility evidence](../mcc_maps.md)
- [Stock-menu package previews and completion](mcc_level_previews.md)
- [Browser imports, storage ownership and recovery](browser_imports.md)
- [Host-only package sharing and protocol agreement](../../port/linux/NETCODE.md#mcc-package-sharing)
- [Current upstream extraction and validation](../mcc_upstream_validation.md)

The format reference separates inspected structures from inferred behavior.
The validation record distinguishes compilation, automated checks and bounded
runtime observations; it does not claim complete campaign playthroughs.
