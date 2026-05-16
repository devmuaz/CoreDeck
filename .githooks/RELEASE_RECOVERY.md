# Release recovery: when the pre-push hook blocked your tag

The `pre-push` hook in this directory refuses to push a version tag
(e.g. `v0.7.0`, `v0.7.0-beta.5`) unless `CHANGELOG.md` has a non-empty
section for it. This is the same matcher used by
`.github/workflows/release.yml`, so passing the hook means the release
notes will populate correctly.

If you've already created the tag locally and the push was blocked,
here are the two recovery flows.

## When to pick which

**Option A — separate "Update CHANGELOG" commit.** Use this most of the
time. It's the lowest-risk option and leaves a clean history. Pick this
if you've already pushed the commit the tag points at, or if anyone
else might have pulled it.

**Option B — amend the changelog into the existing commit.** Use this
only if the commit the tag points at has **not** been pushed yet
(or you're the sole user of the branch). Cleaner history, but requires
a force-push.

---

## Option A — separate commit (recommended)

```bash
# 1. Edit CHANGELOG.md, add the ## [v0.7.0](...) — YYYY-MM-DD section.

# 2. Commit it.
git add CHANGELOG.md
git commit -m "Update CHANGELOG for v0.7.0"

# 3. Move the tag to point at this new commit.
git tag -f v0.7.0

# 4. Push the branch first, then the tag.
git push
git push origin v0.7.0
```

Notes:

- `git tag -f` is needed because `v0.7.0` already exists locally; it
  re-points the tag at the new commit.
- If the tag was never successfully pushed to the remote (the usual
  case after a blocked push), step 4's `git push origin v0.7.0` does
  not need `--force` — the remote tag doesn't exist yet.
- If the tag *was* somehow already on the remote, you'll need
  `git push origin v0.7.0 --force` to overwrite it.

## Option B — amend into the previous commit

```bash
# 1. Edit CHANGELOG.md.

# 2. Amend it into the previous commit.
git add CHANGELOG.md
git commit --amend --no-edit

# 3. Re-point the tag (the amend changed the commit's SHA).
git tag -f v0.7.0

# 4. Push. The branch was rewritten, so this needs a force.
git push --force-with-lease
git push origin v0.7.0
```

Notes:

- Only safe when nobody else has pulled the branch yet.
- `--force-with-lease` is safer than `--force` for branches because it
  refuses to overwrite if the remote has moved since your last fetch.
- The branch push needs `--force-with-lease` because amending rewrote
  the commit. The tag push needs `--force` only if the tag was already
  on the remote.

---

## If the bad tag already triggered a GitHub Release

Force-pushing a tag does **not** automatically update or delete the
GitHub Release that was created from the original push. Release
workflows typically trigger on tag *creation*, not on tag updates, so
the workflow may not re-run on a force-pushed tag.

If a Release was already published:

1. Delete the Release in the GitHub UI (Releases → the bad release →
   "Delete").
2. Force-push the corrected tag.
3. If the workflow still doesn't re-run, delete the remote tag too
   (`git push origin :refs/tags/v0.7.0`) and push it fresh.

## Sanity check before pushing

To confirm the tag you're about to push has a real changelog entry at
that exact commit:

```bash
git show v0.7.0:CHANGELOG.md | grep -A 1 "## \[v0.7.0\]"
```

If that prints the heading plus at least one bullet, the hook will let
the push through.

## Bypassing the hook

For the rare case where you really do want to push without a changelog
entry (e.g. testing the release workflow itself):

```bash
git push --no-verify origin v0.7.0
# or
SKIP_CHANGELOG_CHECK=1 git push origin v0.7.0
```
