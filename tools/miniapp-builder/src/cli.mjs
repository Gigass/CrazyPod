#!/usr/bin/env node

import {
  cp,
  mkdir,
  stat,
} from "node:fs/promises";
import path from "node:path";
import process from "node:process";
import { watch } from "node:fs";
import { spawn } from "node:child_process";
import { buildProject } from "./build.mjs";
import { generateNativeProject } from "./aot.mjs";

function usage() {
  process.stdout.write(
    "Usage: crazypod <generate|build|test|install|dev> [project] [options]\n" +
    "  generate [project] --out FILE\n" +
    "  build [project] [--target simulator|ipod6g] [--binary FILE]\n" +
    "  test [project]\n" +
    "  install [project|CPK] IPOD_VOLUME\n" +
    "  dev [project]\n",
  );
}

function option(name) {
  const index = process.argv.indexOf(name);
  return index >= 0 ? process.argv[index + 1] : undefined;
}

async function exists(file) {
  try {
    await stat(file);
    return true;
  } catch {
    return false;
  }
}

async function waitForChild(child) {
  const code = await new Promise((resolve, reject) => {
    child.once("error", reject);
    child.once("exit", resolve);
  });
  return code ?? 1;
}

async function main() {
  const command = process.argv[2];
  const positional = process.argv.slice(3)
    .filter((value, index, all) =>
      !value.startsWith("--") &&
      (index === 0 || !all[index - 1].startsWith("--")));
  if (!command || command === "--help" || command === "-h") {
    usage();
    return;
  }
  const project = path.resolve(positional[0] ?? process.cwd());
  if (command === "generate") {
    const result = await generateNativeProject(project, {
      output: option("--out"),
    });
    process.stdout.write(`${result.output}\n`);
    return;
  }
  if (command === "build") {
    const result = await buildProject(project, {
      output: option("--out"),
      development: process.argv.includes("--development"),
      binary: option("--binary"),
      target: option("--target"),
    });
    process.stdout.write(`${result.output}\n`);
    return;
  }
  if (command === "test") {
    const typeScript = path.join(
      project, "node_modules/typescript/bin/tsc");
    if (!await exists(typeScript)) {
      throw new Error(
        "crazypod test requires project dependencies; run npm install",
      );
    }
    const typeCheck = spawn(
      process.execPath, [typeScript, "--noEmit"],
      { cwd: project, stdio: "inherit" },
    );
    const typeCode = await waitForChild(typeCheck);
    if (typeCode !== 0) {
      process.exitCode = typeCode;
      return;
    }
    await generateNativeProject(project);
    const tests = spawn(
      process.execPath, ["--test"],
      { cwd: project, stdio: "inherit" },
    );
    const testCode = await waitForChild(tests);
    if (testCode !== 0)
      process.exitCode = testCode;
    return;
  }
  if (command === "install") {
    const volume = positional[1]
      ? path.resolve(positional[1])
      : null;
    if (!volume) throw new Error("install requires an iPod volume path");
    const source = project.endsWith(".cpk") && await exists(project)
      ? project : (await buildProject(project, {
        target: option("--target") ?? "ipod6g",
        binary: option("--binary"),
      })).output;
    const destinationDirectory = path.join(volume, "MiniApps", "Install");
    await mkdir(destinationDirectory, { recursive: true });
    const destination = path.join(destinationDirectory, path.basename(source));
    await cp(source, destination);
    process.stdout.write(`${destination}\n`);
    return;
  }
  if (command === "dev") {
    const repository = path.resolve(import.meta.dirname, "../../..");
    const packageDirectory =
      process.env.CRAZYPOD_DEV_PACKAGE_DIRECTORY ??
      path.join(
        repository,
        "build-sim/simdisk/.rockbox/crazypod/miniapps/packages",
      );
    const simulator =
      process.env.CRAZYPOD_DEV_SIMULATOR ??
      path.join(repository, "build-sim/rockboxui");
    if (!await exists(simulator)) {
      throw new Error("build the simulator first with ./build-sim.sh --incremental");
    }
    let child = null;
    let rebuilding = false;
    let rebuildAgain = false;
    let debounce = null;
    const launch = async () => {
      if (rebuilding) {
        rebuildAgain = true;
        return;
      }
      rebuilding = true;
      try {
        const result = await buildProject(project, { development: true });
        await mkdir(packageDirectory, { recursive: true });
        await cp(
          result.output,
          path.join(packageDirectory, path.basename(result.output)),
        );
        if (child && child.exitCode === null) {
          const exited = new Promise((resolve) =>
            child.once("exit", resolve));
          child.kill("SIGTERM");
          await exited;
        }
        child = spawn(simulator, [], {
          cwd: path.join(repository, "build-sim"),
          stdio: "inherit",
          env: {
            ...process.env,
            CRAZYPOD_SIM_DUMP: "1",
            CRAZYPOD_SIM_SCREEN: result.manifest.id,
          },
        });
        process.stdout.write(
          `CrazyPod reloaded ${result.manifest.id}\n`,
        );
      } finally {
        rebuilding = false;
        if (rebuildAgain) {
          rebuildAgain = false;
          void launch();
        }
      }
    };
    await launch();
    const watcher = watch(
      project, { recursive: true },
      (_event, filename) => {
        const normalized = String(filename ?? "");
        if (normalized.startsWith(".crazypod/") ||
            normalized.startsWith("dist/") ||
            normalized.startsWith("node_modules/")) return;
        clearTimeout(debounce);
        debounce = setTimeout(() => void launch(), 80);
      },
    );
    const shutdown = () => {
      watcher.close();
      if (child) child.kill("SIGTERM");
    };
    process.once("SIGINT", shutdown);
    process.once("SIGTERM", shutdown);
    return;
  }
  usage();
  process.exitCode = 2;
}

main().catch((error) => {
  process.stderr.write(`${error.stack ?? error}\n`);
  process.exitCode = 1;
});
