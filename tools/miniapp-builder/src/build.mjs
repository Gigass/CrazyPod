import { execFile } from "node:child_process";
import { mkdir, readFile } from "node:fs/promises";
import path from "node:path";
import process from "node:process";
import { promisify } from "node:util";

import {
  generateNativeProject,
  packageNativeProject,
} from "./aot.mjs";

const execute = promisify(execFile);
const sdkDirectory = path.resolve(import.meta.dirname, "../../../miniapps/sdk");

async function buildSimulatorBinary(project) {
  const artifactDirectory = path.join(project, ".crazypod/native");
  const source = path.join(artifactDirectory, "app.c");
  const binary = path.join(artifactDirectory, "app.dylib");
  await mkdir(artifactDirectory, { recursive: true });
  await generateNativeProject(project, { output: source });
  const linkFlag = process.platform === "darwin" ? "-dynamiclib" : "-shared";
  await execute(process.env.CC ?? "cc", [
    "-std=c11",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-fPIC",
    "-fvisibility=hidden",
    "-DCRAZYPOD_MINIAPP_PACKAGE",
    "-DCRAZYPOD_MINIAPP_STANDALONE_SIM",
    `-I${sdkDirectory}`,
    linkFlag,
    source,
    "-o",
    binary,
  ]);
  return { binary, source };
}

export async function buildProject(
  projectDirectory, {
    output,
    binary,
    target = "simulator",
  } = {},
) {
  const project = path.resolve(projectDirectory);
  const config = JSON.parse(
    await readFile(path.join(project, "crazypod.config.json"), "utf8"),
  );
  if (config.runtime !== "native-aot") {
    throw new Error(
      "CrazyPod supports only runtime: native-aot (CPK5)",
    );
  }
  let nativeBinary = binary;
  let generatedSource;
  if (!nativeBinary && target === "simulator") {
    const built = await buildSimulatorBinary(project);
    nativeBinary = built.binary;
    generatedSource = built.source;
  }
  if (!nativeBinary) {
    throw new Error(
      "ipod6g build requires --binary app.arm from the firmware toolchain",
    );
  }
  return packageNativeProject(project, {
    binary: nativeBinary,
    target,
    output,
    generatedSource,
  });
}
