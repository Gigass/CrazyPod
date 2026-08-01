import assert from "node:assert/strict";
import {
  chmod,
  mkdir,
  mkdtemp,
  readFile,
  readdir,
  rm,
  symlink,
  writeFile,
} from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { spawn } from "node:child_process";
import test from "node:test";

const builder = path.resolve(import.meta.dirname, "..");

async function run(
  script, args, cwd = builder, environment = process.env,
) {
  const child = spawn(
    process.execPath, [path.join(builder, "src", script), ...args],
    { cwd, stdio: "pipe", env: environment },
  );
  let output = "";
  let error = "";
  child.stdout.on("data", (data) => {
    output += data;
  });
  child.stderr.on("data", (data) => {
    error += data;
  });
  const code = await new Promise((resolve, reject) => {
    child.once("error", reject);
    child.once("exit", resolve);
  });
  return { code, output, error };
}

async function linkProjectDependencies(project) {
  await mkdir(path.join(project, "node_modules"), { recursive: true });
  for (const dependency of [
    "create-crazypod-app",
    "typescript",
  ]) {
    const target = dependency === "create-crazypod-app"
      ? builder : path.join(builder, "node_modules", dependency);
    await symlink(
      target, path.join(project, "node_modules", dependency),
      "dir",
    );
  }
}

async function waitFor(predicate, message, timeout = 10000) {
  const deadline = Date.now() + timeout;

  while (Date.now() < deadline) {
    if (await predicate()) return;
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  throw new Error(message);
}

test("crazypod test performs strict TypeScript checking", async (context) => {
  const temporary = await mkdtemp(
    path.join(os.tmpdir(), "crazypod-cli-"));
  const project = path.join(temporary, "typed-app");
  context.after(() => rm(temporary, { recursive: true, force: true }));

  const created = await run("create-app.mjs", [project]);
  assert.equal(created.code, 0, created.error);
  await linkProjectDependencies(project);

  const valid = await run("cli.mjs", ["test", project]);
  assert.equal(
    valid.code, 0, `${valid.output}\n${valid.error}`);

  const sourcePath = path.join(project, "src", "App.tsx");
  const source = await readFile(sourcePath, "utf8");
  await writeFile(
    sourcePath,
    `${source}\nconst invalidType: number = "not a number";\n`,
  );
  const invalid = await run("cli.mjs", ["test", project]);
  assert.notEqual(invalid.code, 0);
  assert.match(
    `${invalid.output}\n${invalid.error}`,
    /Type 'string' is not assignable to type 'number'/,
  );

  await writeFile(
    sourcePath,
    `${source}\ndocument.title = "browser-only";\n`,
  );
  const browserOnly = await run("cli.mjs", ["test", project]);
  assert.notEqual(browserOnly.code, 0);
  assert.match(
    `${browserOnly.output}\n${browserOnly.error}`,
    /Cannot find name 'document'/,
  );
});

test("crazypod install builds into the target volume", async (context) => {
  const temporary = await mkdtemp(
    path.join(os.tmpdir(), "crazypod-install-"));
  const project = path.join(temporary, "install-app");
  const volume = path.join(temporary, "IPOD");
  context.after(() => rm(temporary, { recursive: true, force: true }));

  const created = await run("create-app.mjs", [project]);
  assert.equal(created.code, 0, created.error);
  await linkProjectDependencies(project);
  const packagePath = path.join(temporary, "install-app.cpk");
  const built = await run(
    "cli.mjs", ["build", project, "--out", packagePath]);
  assert.equal(built.code, 0, built.error);
  const installed = await run(
    "cli.mjs", ["install", packagePath, volume]);
  assert.equal(installed.code, 0, installed.error);
  const packages = await readdir(
    path.join(volume, "MiniApps", "Install"));
  assert.deepEqual(packages, ["install-app.cpk"]);
});

test("crazypod dev rebuilds and restarts the simulator", async (context) => {
  const temporary = await mkdtemp(
    path.join(os.tmpdir(), "crazypod-dev-"));
  const project = path.join(temporary, "watch-app");
  const packageDirectory = path.join(temporary, "packages");
  const simulator = path.join(temporary, "fake-simulator.mjs");
  const launchLog = path.join(temporary, "launch.log");
  context.after(() => rm(temporary, { recursive: true, force: true }));

  const created = await run("create-app.mjs", [project]);
  assert.equal(created.code, 0, created.error);
  await linkProjectDependencies(project);
  await writeFile(
    simulator,
    "#!/usr/bin/env node\n" +
    "import { appendFileSync } from 'node:fs';\n" +
    "appendFileSync(process.env.FAKE_SIM_LOG, 'launch\\n');\n" +
    "setInterval(() => {}, 1000);\n",
  );
  await chmod(simulator, 0o755);

  const child = spawn(
    process.execPath,
    [path.join(builder, "src/cli.mjs"), "dev", project],
    {
      cwd: builder,
      stdio: "pipe",
      env: {
        ...process.env,
        CRAZYPOD_DEV_PACKAGE_DIRECTORY: packageDirectory,
        CRAZYPOD_DEV_SIMULATOR: simulator,
        FAKE_SIM_LOG: launchLog,
      },
    },
  );
  let output = "";
  let error = "";
  child.stdout.on("data", (data) => {
    output += data;
  });
  child.stderr.on("data", (data) => {
    error += data;
  });
  context.after(async () => {
    if (child.exitCode === null && child.signalCode === null) {
      child.kill("SIGTERM");
      await waitFor(
        () => child.exitCode !== null || child.signalCode !== null,
        "forced dev cleanup timed out",
        2000,
      ).catch(() => child.kill("SIGKILL"));
    }
  });

  await waitFor(
    () => output.includes("CrazyPod reloaded watch-app"),
    `initial dev reload missing\n${output}\n${error}`,
  );
  assert.deepEqual(
    await readdir(packageDirectory),
    ["watch-app-1.0.0-simulator.cpk"],
  );
  await waitFor(
    async () => {
      try {
        return (await readFile(launchLog, "utf8"))
          .includes("launch");
      } catch {
        return false;
      }
    },
    "initial simulator was not launched",
  );
  const sourcePath = path.join(project, "src", "App.tsx");
  const source = await readFile(sourcePath, "utf8");
  await writeFile(sourcePath, `${source}\n// trigger reload\n`);
  await waitFor(
    () => (
      output.match(/CrazyPod reloaded watch-app/g) ?? []
    ).length >= 2,
    `watched dev reload missing\n${output}\n${error}`,
  );
  await waitFor(
    async () => {
      try {
        const launches = await readFile(launchLog, "utf8");
        return launches.trim().split("\n").length >= 2;
      } catch {
        return false;
      }
    },
    "simulator was not restarted",
  );
  child.kill("SIGTERM");
  await waitFor(
    () => child.exitCode !== null,
    "crazypod dev did not stop cleanly",
  );
});
