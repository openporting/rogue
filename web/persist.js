/* persist.js — make Rogue's save survive a page reload (IDBFS + FS.syncfs).
 *
 * The engine runs on Emscripten's in-memory filesystem (MEMFS), so a refresh
 * normally wipes the save. This module mounts an IndexedDB-backed filesystem
 * (IDBFS) at the save directory and syncs it both ways:
 *   - on boot:  FS.syncfs(true)   IndexedDB -> MEMFS   (load the saved game)
 *   - on exit:  FS.syncfs(false)  MEMFS -> IndexedDB    (persist it)
 *
 * Where Rogue writes the save (verified against the 5.4.4 source):
 *   main.c:  file_name = md_gethomedir() + "rogue.save"
 *   md_gethomedir() (mdport.c) uses getpwuid()'s home, else $HOME. Under
 *   Emscripten that resolves to $HOME, so we point HOME at the IDBFS mount and
 *   the save lands at /save/rogue.save. (headless-test.js already relies on
 *   $HOME being the effective home dir.) The score file is compile-time
 *   disabled — SCOREFILE is undefined in webcurses/config.h, so scoreboard is
 *   NULL and nothing else touches disk; rogue.save is the whole story.
 *
 * Resume: Rogue restores a saved game when it is given the save file as argv[1]
 *   (main.c: argc==2 -> restore()). restore() then unlinks the save (classic
 *   anti-save-scum), and the resumed game runs until you die/quit, which calls
 *   exit() — so the unlink is flushed too and a fresh game starts next time.
 *   That means the only moments rogue.save changes are immediately before an
 *   exit() (save_file() on 'S', or restore()'s unlink), so flushing on exit
 *   captures every state change.
 *
 * Load order (index.html):  bridge.js -> persist.js -> rogue.js .
 * Engine runtime symbols (FS/IDBFS/ENV/addRunDependency/...) are exported onto
 * Module by build.sh's EXPORTED_RUNTIME_METHODS; we resolve them defensively so
 * this also works as a plain global script and under the Node smoke test.
 */
var Module =
  (typeof Module !== "undefined" && Module) ||
  (typeof globalThis !== "undefined" && globalThis.Module) ||
  {};
if (typeof globalThis !== "undefined") globalThis.Module = Module;

(function () {
  var G = typeof globalThis !== "undefined" ? globalThis : this;

  var SAVE_DIR = "/save";
  var SAVE_FILE = SAVE_DIR + "/rogue.save"; // <HOME>/rogue.save with HOME=/save

  // Resolve an Emscripten runtime symbol whether it lives on the global scope
  // (default non-modularized build) or on Module (EXPORTED_RUNTIME_METHODS).
  function rt(name) {
    if (G && G[name] != null) return G[name];
    return Module[name];
  }

  // MEMFS -> IndexedDB flush, with a re-entrancy guard so overlapping requests
  // (exit + visibility change) coalesce into one trailing sync.
  var syncing = false,
    pending = false;
  function persist(reason) {
    var FS = rt("FS");
    if (!FS) return;
    if (syncing) {
      pending = true;
      return;
    }
    syncing = true;
    FS.syncfs(false, function (err) {
      syncing = false;
      if (err) console.warn("[persist] save (" + reason + ") failed:", err);
      if (pending) {
        pending = false;
        persist(reason);
      }
    });
  }

  Module.preRun = Module.preRun || [];
  Module.preRun.push(function () {
    var FS = rt("FS"),
      IDBFS = rt("IDBFS"),
      ENV = rt("ENV"),
      addRunDependency = rt("addRunDependency"),
      removeRunDependency = rt("removeRunDependency");

    if (!FS || !IDBFS) {
      console.warn("[persist] IDBFS unavailable — saves will not persist");
      return;
    }

    // Point Rogue's home (and thus its save path) at the IDBFS mount.
    if (ENV) {
      ENV.HOME = SAVE_DIR;
      if (!ENV.USER) ENV.USER = "rogue";
    }

    try {
      FS.mkdir(SAVE_DIR);
    } catch (e) {
      /* already exists */
    }
    FS.mount(IDBFS, {}, SAVE_DIR);

    // Block startup until the persisted filesystem is pulled in from IndexedDB.
    addRunDependency("idbfs-load");
    FS.syncfs(true, function (err) {
      if (err) console.warn("[persist] load failed:", err);
      try {
        var st = FS.analyzePath(SAVE_FILE);
        if (st.exists && st.object && FS.isFile(st.object.mode)) {
          // Resume the saved game: feed it as argv[1] (main.c -> restore()).
          Module.arguments = (Module.arguments || []).concat([SAVE_FILE]);
        }
      } catch (e) {
        console.warn("[persist] restore check failed:", e);
      }
      removeRunDependency("idbfs-load");
    });
  });

  // Classic Rogue exits the program when you save ('S') or die — the save file
  // is written/unlinked right before exit(), so flush here to persist it.
  Module.onExit = (function (prev) {
    return function (code) {
      try {
        persist("exit");
      } catch (e) {}
      if (prev) prev(code);
      announceExit();
    };
  })(Module.onExit);

  // The C program has ended (there's no in-place "continue"). Invite a reload,
  // which resumes from the just-written save.
  function announceExit() {
    if (typeof document === "undefined" || !document.body) return;
    if (document.getElementById("persist-exit")) return;
    var d = document.createElement("div");
    d.id = "persist-exit";
    d.textContent = "게임이 종료되었습니다. 화면을 눌러 새로고침하면 이어서 플레이합니다.";
    d.style.cssText =
      "position:fixed;left:0;right:0;bottom:0;padding:14px;text-align:center;" +
      "background:#2b2118;color:#e9d8b8;font:600 15px system-ui,sans-serif;" +
      "border-top:1px solid #5a4630;z-index:9999;cursor:pointer";
    d.addEventListener("click", function () {
      location.reload();
    });
    document.body.appendChild(d);
  }

  // Safety net: flush if the tab is backgrounded/closed. Usually a no-op (Rogue
  // only writes at exit), but cheap insurance and forward-compatible with any
  // future autosave.
  if (typeof document !== "undefined") {
    var flush = function () {
      try {
        if (rt("FS")) persist("hide");
      } catch (e) {}
    };
    document.addEventListener("visibilitychange", function () {
      if (document.hidden) flush();
    });
    if (typeof window !== "undefined") window.addEventListener("pagehide", flush);
  }

  // Console / UI escape hatch: wipe the save (e.g. to start over, or recover
  // from an incompatible save that fails to restore) and persist the deletion.
  G.RoguePersist = {
    flush: function () {
      persist("manual");
    },
    reset: function () {
      var FS = rt("FS");
      try {
        FS.unlink(SAVE_FILE);
      } catch (e) {}
      persist("reset");
    },
  };
})();
