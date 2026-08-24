import assert from "node:assert/strict";
import { mkdtemp, readFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import test from "node:test";

import {
  compileNativeSource,
  generateNativeProject,
  semanticFontSetFromGeneratedSource,
} from "../src/aot.mjs";

const manifest = {
  id: "counter",
  name: "Counter",
  version: "1.0.0",
};

test("React Profile emits native C without a JavaScript runtime", () => {
  const source = `
    import React, { useState } from "react";
    import { Pressable, StyleSheet, Text, View } from "react-native";
    export default function App() {
      const [count, setCount] = useState(1);
      return <View style={styles.root}>
        <Pressable onPress={() => {
          setCount(value => value + 1);
          setCount(value => value + 1);
        }}>
          <Text>{count}</Text>
        </Pressable>
      </View>;
    }
    const styles = StyleSheet.create({
      root: { width: 320, height: 240, backgroundColor: "#101010" },
    });
  `;
  const output = compileNativeSource(source, { manifest });
  assert.match(output, /CP_NATIVE_MINIAPP_HEADER/);
  assert.match(output, /ui->create\(CP_UI_OBJECT_SCREEN\)/);
  assert.equal(
    output.match(/state_count = \(state_count \+ 1\);/g)?.length,
    2,
  );
  assert.match(output, /static cp_ui_handle_t native_handles\[3\]/);
  assert.match(
    output,
    /structure_key == rendered_structure_key\)\s+return cp_update_dynamic\(\)/,
  );
  assert.match(
    output,
    /ui->set_string\(native_handles\[2\], CP_UI_PROP_TEXT, text_buffer\)/,
  );
  assert.doesNotMatch(output, /QuickJS|JSRuntime|Solid|Virtual DOM/);
});

test("repository native reference generates deterministic C", async () => {
  const directory = await mkdtemp(path.join(os.tmpdir(), "cp-aot-"));
  const repository = path.resolve(import.meta.dirname, "../../..");
  const project = path.join(repository, "miniapps/apps/native-reference");
  const first = await generateNativeProject(project, {
    output: path.join(directory, "first.c"),
  });
  const second = await generateNativeProject(project, {
    output: path.join(directory, "second.c"),
  });
  assert.equal(
    await readFile(first.output, "utf8"),
    await readFile(second.output, "utf8"),
  );
});

test("React Profile lowers native input, control events and conditions", () => {
  const source = `
    import React, { useState } from "react";
    import { Slider, StyleSheet, Text, View } from "react-native";
    export default function App() {
      const [route, setRoute] = useState(0);
      const [value, setValue] = useState(10);
      return <View
        style={styles.root}
        onLeft={() => setValue(previous => previous - 1)}
        onRight={() => setValue(previous => previous + 1)}
      >
        <Slider
          minimumValue={0}
          maximumValue={100}
          value={value}
          onValueChange={(event) => {
            setValue(event.value);
            setRoute(1);
          }}
        />
        {route === 0
          ? <Text>READY</Text>
          : <Text>{value * 2}</Text>}
      </View>;
    }
    const styles = StyleSheet.create({
      root: { width: 320, height: 240, flexDirection: "column" },
    });
  `;
  const output = compileNativeSource(source, { manifest });
  assert.match(output, /case CP_INPUT_LEFT:/);
  assert.match(output, /case CP_INPUT_RIGHT:/);
  assert.match(output, /ui->listen\(h1, CP_UI_EVENT_CHANGE, 1u\)/);
  assert.match(output, /state_value = value;/);
  assert.match(output, /if\(state_route == 0\)/);
  assert.match(output, /cp_i32_text\(\(state_value \* 2\)/);
});

test("React Profile lowers wrap, absolute positioning and animated images", () => {
  const source = `
    import React from "react";
    import { AnimatedImage, StyleSheet, View } from "react-native";
    export default function App() {
      return <View style={styles.root}>
        <AnimatedImage source="pulse" style={styles.overlay} />
      </View>;
    }
    const styles = StyleSheet.create({
      root: {
        width: 320,
        height: 240,
        flexDirection: "row",
        flexWrap: "wrap",
        overflow: "hidden",
      },
      overlay: {
        position: "absolute",
        left: 0,
        top: 0,
        width: 24,
        height: 24,
      },
    });
  `;
  const output = compileNativeSource(source, { manifest });
  assert.match(output, /ui->create\(CP_UI_OBJECT_ANIMATED_IMAGE\)/);
  assert.match(output, /CP_UI_FLEX_ROW_WRAP/);
  assert.match(output, /CP_UI_PROP_POSITION, CP_UI_POSITION_ABSOLUTE/);
  assert.match(output, /CP_UI_PROP_OVERFLOW, 0/);
});

test("React Profile gives containers React Native column layout by default", () => {
  const source = `
    import React from "react";
    import { Pressable, Text, View } from "react-native";
    export default function App() {
      return <View><Pressable><Text>OK</Text></Pressable></View>;
    }
  `;
  const output = compileNativeSource(source, { manifest });
  assert.equal(
    output.match(/CP_UI_PROP_LAYOUT, CP_UI_LAYOUT_FLEX/g)?.length,
    2,
  );
  assert.equal(
    output.match(/CP_UI_PROP_FLEX_FLOW, CP_UI_FLEX_COLUMN/g)?.length,
    2,
  );
});

test("React Profile lowers SafeAreaView and Modal to device-safe root layers", () => {
  const source = `
    import React from "react";
    import { Modal, SafeAreaView, Text, View } from "react-native";
    export default function App() {
      return <View><SafeAreaView><Text>PAGE</Text><Modal><Text>DIALOG</Text></Modal></SafeAreaView></View>;
    }
  `;
  const output = compileNativeSource(source, { manifest });
  assert.match(
    output,
    /ui->set_i32\(h1, CP_UI_PROP_Y, 32\).*ui->set_i32\(h1, CP_UI_PROP_HEIGHT, 208\)/s,
  );
  assert.match(output, /ui->insert\(h3, h0, 0\)/);
  assert.match(
    output,
    /ui->set_i32\(h3, CP_UI_PROP_WIDTH, 320\).*ui->set_i32\(h3, CP_UI_PROP_HEIGHT, 240\)/s,
  );
});

test("React Profile preserves requested system font sizes", () => {
  const source = `
    import React from "react";
    import { StyleSheet, Text, View } from "react-native";
    export default function App() {
      return <View>
        <Text style={styles.small}>12</Text>
        <Text style={styles.body}>14</Text>
        <Text style={styles.title}>19</Text>
        <Text style={styles.number}>24</Text>
        <Text style={styles.display}>40</Text>
      </View>;
    }
    const styles = StyleSheet.create({
      small: { fontSize: 12 },
      body: { fontSize: 14 },
      title: { fontSize: 19 },
      number: { fontSize: 24 },
      display: { fontSize: 40 },
    });
  `;
  const output = compileNativeSource(source, { manifest });
  for (const size of [12, 14, 19, 24, 40]) {
    assert.match(output,
      new RegExp(`CP_UI_PROP_FONT_SIZE, ${size}\\)`, "g"));
  }
  assert.equal((output.match(/CP_UI_FONT_SYSTEM/g) ?? []).length, 5);
});

test("React Profile maps semantic families and typography to Noto", () => {
  const source = `
    import React from "react";
    import { StyleSheet, Text, View } from "react-native";
    export default function App() {
      return <View>
        <Text style={styles.system}>SYSTEM</Text>
        <Text style={styles.serif}>SERIF</Text>
        <Text style={styles.mono}>MONO</Text>
      </View>;
    }
    const styles = StyleSheet.create({
      system: { fontFamily: "system", fontSize: 22 },
      serif: { fontFamily: "serif", fontSize: 28, fontWeight: "600" },
      mono: { fontFamily: "mono", fontSize: 14, lineHeight: 18 },
    });
  `;
  const output = compileNativeSource(source, { manifest });
  for (const font of ["SYSTEM", "SERIF", "MONO"]) {
    assert.match(output, new RegExp(`CP_UI_FONT_${font}`));
  }
  assert.match(output, /CP_UI_PROP_FONT_WEIGHT, 600/);
  assert.match(output, /CP_UI_PROP_LINE_HEIGHT, 18/);
  assert.deepEqual(semanticFontSetFromGeneratedSource(output), [
    "mono:400:14",
    "serif:600:28",
    "system:400:22",
  ]);
});

test("React Profile rejects removed fixed host font families", () => {
  assert.throws(() => compileNativeSource(`
    import React from "react";
    import { Text, View } from "react-native";
    export default function App() {
      return <View><Text style={{ fontFamily: "helvetica" }}>Font</Text></View>;
    }
  `, { manifest }), /unsupported fontFamily helvetica/);
});

test("React Profile binds declared package fonts by resource id", () => {
  const source = `
    import React from "react";
    import { StyleSheet, Text, View } from "react-native";
    export default function App() {
      return <View><Text style={styles.title}>PACKAGE FONT</Text></View>;
    }
    const styles = StyleSheet.create({
      title: { fontFamily: "asset:headline", fontSize: 18 },
    });
  `;
  const output = compileNativeSource(source, {
    manifest,
    fontAssets: new Set(["headline"]),
  });
  assert.match(
    output,
    /set_string\(h1, CP_UI_PROP_FONT_SOURCE, "headline"\)/,
  );
  assert.throws(
    () => compileNativeSource(source, {
      manifest,
      fontAssets: new Set(),
    }),
    /font asset headline is not declared/,
  );
});

test("direct setters keep render-snapshot semantics within one handler", () => {
  const source = `
    import React, { useState } from "react";
    import { Pressable, Text, View } from "react-native";
    export default function App() {
      const [count, setCount] = useState(0);
      return <View>
        <Pressable onPress={() => {
          setCount(count + 1);
          setCount(count + 1);
        }}><Text>{count}</Text></Pressable>
      </View>;
    }
  `;
  const output = compileNativeSource(source, { manifest });
  assert.equal(
    output.match(/state_count = \(previous_state_count \+ 1\);/g)?.length,
    2,
  );
});

test("React Profile rejects components without a complete device contract", () => {
  for (const component of ["FlatList", "TextInput", "Picker"]) {
    const source = `
      import React from "react";
      import { ${component}, View } from "react-native";
      export default function App() { return <View><${component} /></View>; }
    `;
    assert.throws(
      () => compileNativeSource(source, { manifest }),
      new RegExp(`unsupported React Native component ${component}`),
    );
  }
});

test("React Profile requires a View root", () => {
  const source = `
    import React from "react";
    import { Text } from "react-native";
    export default function App() { return <Text>invalid root</Text>; }
  `;
  assert.throws(
    () => compileNativeSource(source, { manifest }),
    /App JSX root must be View/,
  );
});
