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
