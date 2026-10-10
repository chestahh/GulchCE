# GulchCE versions

`VERSION` owns GulchCE's version, initially `0.1`. Players see `v0.1`.
Use `major.minor` or `major.minor.patch`, for example `0.2` or `0.2.1`.
Components are integers from 0 to 999, without leading zeroes. `0.1` and
`0.1.0` are equivalent; each new release must increase the numeric version.
Workflow run numbers and OpenCE's versions have no role in update checks.

## Publishing a release

1. Change `VERSION` on a branch and review/merge its PR into main.
2. Create and push the matching tag on that reviewed main commit, for
   example `v0.1` for `VERSION` containing `0.1`.
3. The Build workflow validates the tag, builds all three platforms, and
   publishes `GulchCE v0.1` with the existing asset filenames only after all
   builds pass. Do not create an empty GitHub release before the workflow.

Branch pushes still build downloadable Actions artifacts, but do not
publish releases or enable automatic update checks. A mismatched `v*` tag
fails the build. Existing releases and tags are retained.

Only version-tag builds from `chestahh/GulchCE` enable the updater. It checks
that repository's latest release, accepts stable version tags only, and
compares components numerically (`v0.10` is newer than `v0.9`). Legacy
`build-*` tags and prerelease suffixes are ignored. Do not mark an older
version as GitHub's latest release when republishing historical tags.

Android requires a numeric package version internally. It is derived from
the version (`major * 1000000 + minor * 1000 + patch`), not a workflow run.
Keep the existing Android signing key to preserve update compatibility.

## Moving from the old updater

Install the first fixed GulchCE release manually. The previous updater
points at OpenCE and cannot discover this migration safely. Choose **No**
on its prompt, or turn off `auto` in the configuration's `[update]` section.
If it was turned off, re-enable it after installing a fixed tagged release
to receive future GulchCE version updates. Maps and saves need not change.
