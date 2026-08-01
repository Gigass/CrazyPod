#!/usr/bin/env node

import {
  access,
  mkdir,
  writeFile,
} from "node:fs/promises";
import path from "node:path";
import process from "node:process";

function projectId(name) {
  const value = name.toLowerCase()
    .replace(/[^a-z0-9_-]+/g, "-")
    .replace(/^-+|-+$/g, "");
  if (!/^[a-z][a-z0-9_-]{0,31}$/.test(value)) {
    throw new Error(
      "project name must produce a 1-32 character lowercase app id",
    );
  }
  return value;
}

async function ensureEmpty(directory) {
  try {
    await access(directory);
    throw new Error(`destination already exists: ${directory}`);
  } catch (error) {
    if (error.code !== "ENOENT") throw error;
  }
}

async function main() {
  const name = process.argv[2];
  if (!name || name === "--help" || name === "-h") {
    process.stdout.write("Usage: create-crazypod-app <project-name>\n");
    return;
  }
  const id = projectId(path.basename(name));
  const directory = path.resolve(name);
  await ensureEmpty(directory);
  await mkdir(path.join(directory, "src"), { recursive: true });
  await mkdir(path.join(directory, "test"), { recursive: true });
  await writeFile(
    path.join(directory, "crazypod.config.json"),
    `${JSON.stringify({
      manifest: {
        id,
        name: path.basename(name),
        version: "1.0.0",
        versionCode: 1,
        symbol: "APP",
        summary: "CrazyPod Mini App",
        accent: "#ff9f43",
      },
      runtime: "native-aot",
      source: "src/App.tsx",
      iconColor: "#ff9f43",
    }, null, 2)}\n`,
  );
  await writeFile(
    path.join(directory, "package.json"),
    `${JSON.stringify({
      name: id,
      version: "1.0.0",
      private: true,
      type: "module",
      scripts: {
        dev: "crazypod dev",
        build: "crazypod build",
        test: "crazypod test",
      },
      devDependencies: {
        "create-crazypod-app": "^5.0.0",
        "typescript": "^5.9.2",
      },
    }, null, 2)}\n`,
  );
  await writeFile(
    path.join(directory, "tsconfig.json"),
    `${JSON.stringify({
      compilerOptions: {
        target: "ES2020",
        module: "ESNext",
        moduleResolution: "Bundler",
        strict: true,
        lib: ["ES2020"],
        jsx: "preserve",
        types: ["create-crazypod-app/types"],
        noEmit: true,
      },
      include: ["src", "test"],
    }, null, 2)}\n`,
  );
  await writeFile(
    path.join(directory, "src/App.tsx"),
    `import React, { useState } from "react";
import { Pressable, SafeAreaView, StyleSheet, Text, View } from "react-native";

export default function App() {
  const [count, setCount] = useState(0);
  return (
    <View style={styles.screen}>
      <SafeAreaView style={styles.content}>
        <Text style={styles.title}>CrazyPod</Text>
        <Pressable
          style={styles.button}
          onPress={() => setCount((previous) => previous + 1)}
        >
          <Text style={styles.buttonText}>{count}</Text>
        </Pressable>
      </SafeAreaView>
    </View>
  );
}

const styles = StyleSheet.create({
  screen: {
    width: 320,
    height: 240,
    backgroundColor: "#16181d",
  },
  content: {
    width: 320,
    height: 208,
    paddingTop: 12,
    alignItems: "center",
  },
  title: {
    width: 280,
    height: 40,
    color: "#f9f6f2",
    fontSize: 24,
    textAlign: "center",
  },
  button: {
    width: 200,
    height: 44,
    marginTop: 24,
    paddingTop: 10,
    backgroundColor: "#ff9f43",
    borderRadius: 8,
  },
  buttonText: {
    width: 200,
    height: 24,
    color: "#16181d",
    fontSize: 19,
    textAlign: "center",
  },
});
`,
  );
  await writeFile(
    path.join(directory, "test/app.test.mjs"),
    `import assert from "node:assert/strict";
import test from "node:test";

test("application tests are wired", () => {
  assert.equal(2 + 2, 4);
});
`,
  );
  process.stdout.write(`${directory}\n`);
}

main().catch((error) => {
  process.stderr.write(`${error.stack ?? error}\n`);
  process.exitCode = 1;
});
