# Mantle Core

The Unreal Engine plugin for making content for [Mantle](https://mantleplatform.com): build places, avatars and items in the editor, convert them, check them, upload them, and try them with **Test locally**. Scripts and places run only in the Mantle game client, which Test locally starts.

- **Engine:** Unreal Engine 5.8
- **Platform:** Windows (Win64) only
- **Docs:** https://mantleplatform.com/docs/creators/plugin/

## Install

Put this folder in your engine's `Engine/Plugins` (or your project's `Plugins`) as `MantleCore`, then open your project.

## Shared files

Some files here are copies of files whose one home is the Mantle game repo: the avatar contract, the vehicle presets and the script API files. `SharedFiles.json` lists each copy and its home. Change the home in the game repo, never the copy here, then bring the copies up to date by running this in the game repo (it finds this folder next to the game repo, or wherever `MANTLE_CORE_DIR` points):

```
python tools/copies/check_copies.py --sync
```

Run it again without `--sync` to check that every copy matches. The game repo's CI runs that check too.

## Releasing

Fab has no publisher API, so a release is a tag here plus one step by hand in the Fab portal.

1. **Bump the version.** Set `VersionName` in `MantleCore.uplugin` (for example `0.2.0`) and merge that to `main`.
2. **Dry run.** Start the Release workflow by hand (Actions > Release > Run workflow, or `gh workflow run release.yml --ref main`). It runs every check and BuildPlugin, and uploads the zip as a workflow artifact instead of making a release. To run only the checks locally:

   ```
   python .github/scripts/fab_release.py check
   ```

3. **Tag.** Push a tag that matches the version, with a `v` in front:

   ```
   git tag v0.2.0
   git push origin v0.2.0
   ```

4. **Wait for the release.** The Release workflow checks the plugin, packages `MantleCore-<version>-UE5.8.zip` (the source under a top-level `MantleCore/` folder, without Binaries, Intermediate or Saved), checks the zip, compiles it with `RunUAT BuildPlugin` on UE 5.8 (Fab Technical Requirements 4.3.6.2.b), and attaches it to a GitHub release for the tag. It fails without making a release if the tag doesn't match `VersionName`, a uplugin field is wrong (EngineVersion 5.8.0, Win64 only on every module, CreatedBy Joshua Eger), a source file lacks the copyright line, Content holds anything but IK rig and IK retargeter assets, a third-party library lacks its LICENSE, or BuildPlugin fails.
5. **Paste the link into Fab.** In the Fab publishing portal, add a Project Version for UE 5.8, paste the link to the release's zip, and submit the update. Fab downloads the zip without signing in, so this works only once this repo is public. While the repo is private the zip stays on the release and isn't hosted anywhere else.

`FabURL` in `MantleCore.uplugin` is set to the Fab listing's launcher address (Fab Technical Requirements 4.3.6.c), and the check fails if it is missing or points elsewhere. It uses the public listing ID from the fab.com listing address, not the portal edit ID.

### The BuildPlugin runner

BuildPlugin runs on our Windows build machine, on a second GitHub Actions runner registered to this repo only. The machine's other runner serves the game repo and is separate.

- **Folder:** `C:\actions-runner-mantlecore`
- **Service:** `actions.runner.GhzGarage-MantleCore.mantlecore-buildplugin` (runs as NETWORK SERVICE, starts with Windows)
- **Label:** `mantlecore-buildplugin`
- **Engine:** the Launcher (installed) UE 5.8 at `C:\Program Files\Epic Games\UE_5.8`, set in the repo variable `MANTLECORE_UE_ROOT`. The job fails if that folder isn't an installed 5.8 engine, because Fab compiles against an installed engine.

Never trigger the Release workflow on `pull_request` once this repo is public: a fork's pull request could then run code on the build machine. Tags and manual runs need write access, so they're safe.
