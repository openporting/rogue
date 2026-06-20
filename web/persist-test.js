/* persist-test.js — headless smoke test for web/persist.js (IDBFS save flow).
 *
 * No browser, no wasm: it mocks the Emscripten runtime symbols persist.js uses
 * (FS / IDBFS / ENV / addRunDependency / removeRunDependency) and drives the
 * lifecycle to assert the save is loaded on boot, the saved game is resumed via
 * argv, and the state is flushed on exit / reset.
 *
 *   run:  node web/persist-test.js
 */
const path = require("path");

let failures = 0;
function check(name, cond) {
  console.log((cond ? "  PASS  " : "  FAIL  ") + name);
  if (!cond) failures++;
}

// Build a fake Emscripten runtime + filesystem and load a fresh persist.js
// against it. `saveExists` seeds /save/rogue.save (a prior session's save).
function load(saveExists) {
  const calls = { syncIn: 0, syncOut: 0, mounts: [], deps: [], unlinks: [] };
  const files = {};
  if (saveExists) files["/save/rogue.save"] = "ENCRYPTEDSAVE";

  const FS = {
    mkdir() {},
    mount(type, opts, dir) {
      calls.mounts.push({ type, dir });
    },
    syncfs(populate, cb) {
      if (populate) calls.syncIn++;
      else calls.syncOut++;
      setImmediate(() => cb(null));
    },
    analyzePath(p) {
      const exists = Object.prototype.hasOwnProperty.call(files, p);
      return { exists, object: exists ? { mode: 0o100644 } : null };
    },
    isFile(mode) {
      return (mode & 0o170000) === 0o100000;
    },
    unlink(p) {
      calls.unlinks.push(p);
      delete files[p];
    },
  };

  // Fresh globals for each scenario.
  global.FS = FS;
  global.IDBFS = { name: "IDBFS" };
  global.ENV = {};
  global.addRunDependency = (id) => calls.deps.push("+" + id);
  global.removeRunDependency = (id) => calls.deps.push("-" + id);
  delete globalThis.Module;
  delete globalThis.RoguePersist;

  delete require.cache[require.resolve("./persist.js")];
  require(path.join(__dirname, "persist.js"));

  return { calls, files, Module: globalThis.Module, ENV: global.ENV };
}

// Run all queued preRun callbacks, then wait for the async syncfs(true) cb.
function boot(ctx) {
  return new Promise((resolve) => {
    (ctx.Module.preRun || []).forEach((fn) => fn());
    setImmediate(resolve); // let the syncfs(true) callback fire
  });
}

(async () => {
  console.log("=== persist.js: fresh game (no prior save) ===");
  {
    const ctx = load(false);
    await boot(ctx);
    check("HOME points at the IDBFS mount", ctx.ENV.HOME === "/save");
    check("IDBFS mounted at /save",
      ctx.calls.mounts.some((m) => m.dir === "/save" && m.type === global.IDBFS));
    check("loaded from IndexedDB on boot (syncfs(true))", ctx.calls.syncIn === 1);
    check("startup gated on the load (run dependency add+remove)",
      ctx.calls.deps.join(",") === "+idbfs-load,-idbfs-load");
    check("no resume argv when there is no save",
      !ctx.Module.arguments || ctx.Module.arguments.length === 0);

    ctx.Module.onExit(0);
    await new Promise((r) => setImmediate(r));
    check("flushed to IndexedDB on exit (syncfs(false))", ctx.calls.syncOut === 1);
  }

  console.log("=== persist.js: prior save present (resume) ===");
  {
    const ctx = load(true);
    await boot(ctx);
    check("resumes by passing the save as argv[1]",
      Array.isArray(ctx.Module.arguments) &&
        ctx.Module.arguments[ctx.Module.arguments.length - 1] === "/save/rogue.save");

    ctx.Module.onExit(0);
    await new Promise((r) => setImmediate(r));
    check("persists state on exit", ctx.calls.syncOut === 1);
  }

  console.log("=== persist.js: RoguePersist.reset() wipes the save ===");
  {
    const ctx = load(true);
    await boot(ctx);
    globalThis.RoguePersist.reset();
    await new Promise((r) => setImmediate(r));
    check("save file unlinked", ctx.calls.unlinks.includes("/save/rogue.save"));
    check("deletion flushed to IndexedDB", ctx.calls.syncOut === 1);
  }

  console.log("");
  if (failures === 0) {
    console.log("PASS: persist.js IDBFS save flow verified");
    process.exit(0);
  } else {
    console.log("FAIL: " + failures + " check(s) failed");
    process.exit(1);
  }
})();
