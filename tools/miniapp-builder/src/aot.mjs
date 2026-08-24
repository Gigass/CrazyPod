import { mkdir, readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import ts from "typescript";
import {
  buildAssets,
  NATIVE_ABI_MAJOR,
  NATIVE_ABI_MINOR,
  REACT_PROFILE,
  nativeProfile,
  solidIcon,
  writeNativePackage,
} from "./package.mjs";

const objectTypes = Object.freeze({
  View: "CP_UI_OBJECT_VIEW",
  SafeAreaView: "CP_UI_OBJECT_VIEW",
  Text: "CP_UI_OBJECT_TEXT",
  Pressable: "CP_UI_OBJECT_BUTTON",
  Button: "CP_UI_OBJECT_BUTTON",
  ScrollView: "CP_UI_OBJECT_SCROLL_VIEW",
  Image: "CP_UI_OBJECT_IMAGE",
  AnimatedImage: "CP_UI_OBJECT_ANIMATED_IMAGE",
  ActivityIndicator: "CP_UI_OBJECT_ARC",
  ProgressBar: "CP_UI_OBJECT_PROGRESS",
  Slider: "CP_UI_OBJECT_SLIDER",
  Switch: "CP_UI_OBJECT_SWITCH",
  CheckBox: "CP_UI_OBJECT_CHECKBOX",
  Checkbox: "CP_UI_OBJECT_CHECKBOX",
  Modal: "CP_UI_OBJECT_MODAL",
});

const eventProps = Object.freeze({
  onPress: "CP_UI_EVENT_SELECT",
  onSelect: "CP_UI_EVENT_SELECT",
  onLongPress: "CP_UI_EVENT_LONG_PRESS",
  onFocus: "CP_UI_EVENT_FOCUS",
  onBlur: "CP_UI_EVENT_BLUR",
  onValueChange: "CP_UI_EVENT_CHANGE",
  onChange: "CP_UI_EVENT_CHANGE",
  onScroll: "CP_UI_EVENT_SCROLL",
});

const inputProps = Object.freeze({
  onWheelClockwise: "CP_INPUT_WHEEL_CLOCKWISE",
  onWheelCounterClockwise: "CP_INPUT_WHEEL_COUNTERCLOCKWISE",
  onLeft: "CP_INPUT_LEFT",
  onRight: "CP_INPUT_RIGHT",
  onPress: "CP_INPUT_SELECT",
  onSelect: "CP_INPUT_SELECT",
  onPlay: "CP_INPUT_PLAY",
  onMenu: "CP_INPUT_MENU",
});

const integerProps = Object.freeze({
  disabled: "CP_UI_PROP_DISABLED",
  minimumValue: "CP_UI_PROP_MINIMUM",
  minimum: "CP_UI_PROP_MINIMUM",
  maximumValue: "CP_UI_PROP_MAXIMUM",
  maximum: "CP_UI_PROP_MAXIMUM",
  value: "CP_UI_PROP_VALUE",
  checked: "CP_UI_PROP_CHECKED",
  focusable: "CP_UI_PROP_FOCUSABLE",
  scrollX: "CP_UI_PROP_SCROLL_X",
  scrollY: "CP_UI_PROP_SCROLL_Y",
  variant: "CP_UI_PROP_VARIANT",
  phase: "CP_UI_PROP_PHASE",
  playing: "CP_UI_PROP_PLAYING",
  waveStyle: "CP_UI_PROP_WAVE_STYLE",
  adaptiveLyrics: "CP_UI_PROP_ADAPTIVE_LYRICS",
});

const colorProps = Object.freeze({
  primaryColor: "CP_UI_PROP_WAVE_PRIMARY_COLOR",
  secondaryColor: "CP_UI_PROP_WAVE_SECONDARY_COLOR",
  highlightColor: "CP_UI_PROP_WAVE_HIGHLIGHT_COLOR",
});

const nativeConstants = Object.freeze({
  "SoundWaveStyle.Torrent": "CP_UI_SOUND_WAVE_TORRENT",
  "SoundWaveStyle.RadialSpectrum": "CP_UI_SOUND_WAVE_RADIAL_SPECTRUM",
  "SoundWaveStyle.LiquidRibbon": "CP_UI_SOUND_WAVE_LIQUID_RIBBON",
  "SoundWaveStyle.VinylGroove": "CP_UI_SOUND_WAVE_VINYL_GROOVE",
  "SoundWaveStyle.MiniLEDMeter": "CP_UI_SOUND_WAVE_MINI_LED_METER",
  "SoundWaveStyle.ParticlePulse": "CP_UI_SOUND_WAVE_PARTICLE_PULSE",
  "SoundWaveVariant.Bar": "CP_UI_SOUND_WAVE_BAR",
  "SoundWaveVariant.Ball": "CP_UI_SOUND_WAVE_BALL",
});

const stringProps = Object.freeze({
  placeholder: "CP_UI_PROP_PLACEHOLDER",
  source: "CP_UI_PROP_IMAGE_SOURCE",
  text: "CP_UI_PROP_TEXT",
});

const numericStyles = Object.freeze({
  left: "CP_UI_PROP_X",
  top: "CP_UI_PROP_Y",
  width: "CP_UI_PROP_WIDTH",
  height: "CP_UI_PROP_HEIGHT",
  minWidth: "CP_UI_PROP_MIN_WIDTH",
  minHeight: "CP_UI_PROP_MIN_HEIGHT",
  maxWidth: "CP_UI_PROP_MAX_WIDTH",
  maxHeight: "CP_UI_PROP_MAX_HEIGHT",
  flexGrow: "CP_UI_PROP_FLEX_GROW",
  padding: "CP_UI_PROP_PADDING",
  paddingLeft: "CP_UI_PROP_PADDING_LEFT",
  paddingRight: "CP_UI_PROP_PADDING_RIGHT",
  paddingTop: "CP_UI_PROP_PADDING_TOP",
  paddingBottom: "CP_UI_PROP_PADDING_BOTTOM",
  margin: "CP_UI_PROP_MARGIN",
  marginLeft: "CP_UI_PROP_MARGIN_LEFT",
  marginRight: "CP_UI_PROP_MARGIN_RIGHT",
  marginTop: "CP_UI_PROP_MARGIN_TOP",
  marginBottom: "CP_UI_PROP_MARGIN_BOTTOM",
  borderWidth: "CP_UI_PROP_BORDER_WIDTH",
  borderRadius: "CP_UI_PROP_RADIUS",
  shadowRadius: "CP_UI_PROP_SHADOW_WIDTH",
});

const colorStyles = Object.freeze({
  backgroundColor: "CP_UI_PROP_BACKGROUND_COLOR",
  borderColor: "CP_UI_PROP_BORDER_COLOR",
  shadowColor: "CP_UI_PROP_SHADOW_COLOR",
  color: "CP_UI_PROP_TEXT_COLOR",
});

function fail(node, message) {
  const file = node.getSourceFile();
  const point = file.getLineAndCharacterOfPosition(node.getStart());
  throw new Error(
    `${file.fileName}:${point.line + 1}:${point.character + 1}: ${message}`,
  );
}

function propertyName(node) {
  if (ts.isIdentifier(node) || ts.isStringLiteral(node)) return node.text;
  fail(node, "computed properties are not supported by React Profile v1");
}

function literal(node) {
  if (ts.isNumericLiteral(node)) return Number(node.text);
  if (ts.isStringLiteral(node) || ts.isNoSubstitutionTemplateLiteral(node)) {
    return node.text;
  }
  if (node.kind === ts.SyntaxKind.TrueKeyword) return true;
  if (node.kind === ts.SyntaxKind.FalseKeyword) return false;
  if (ts.isPrefixUnaryExpression(node) &&
      node.operator === ts.SyntaxKind.MinusToken &&
      ts.isNumericLiteral(node.operand)) {
    return -Number(node.operand.text);
  }
  fail(node, "style values must be compile-time literals");
}

function objectLiteral(node) {
  if (!ts.isObjectLiteralExpression(node)) {
    fail(node, "expected an object literal");
  }
  return Object.fromEntries(node.properties.map((property) => {
    if (!ts.isPropertyAssignment(property)) {
      fail(property, "style spreads and methods are not supported");
    }
    return [propertyName(property.name), literal(property.initializer)];
  }));
}

function cString(value) {
  return JSON.stringify(String(value))
    .replaceAll("\u2028", "\\u2028")
    .replaceAll("\u2029", "\\u2029");
}

function color(value, node) {
  if (typeof value !== "string" || !/^#[0-9a-f]{6}$/i.test(value)) {
    fail(node, `color ${JSON.stringify(value)} must be #RRGGBB`);
  }
  return `0x${value.slice(1).toLowerCase()}u`;
}

function initializerNumber(node) {
  const value = literal(node);
  if (!Number.isInteger(value) || value < -2147483648 ||
      value > 2147483647) {
    fail(node, "useState currently requires a signed 32-bit integer");
  }
  return value;
}

function jsxName(node) {
  if (ts.isIdentifier(node)) return node.text;
  fail(node, "namespaced and member-expression JSX tags are unsupported");
}

function unwrapExpression(node) {
  let current = node;
  while (ts.isParenthesizedExpression(current) ||
         ts.isAsExpression(current) ||
         ts.isNonNullExpression(current)) {
    current = current.expression;
  }
  return current;
}

function usesState(node, states) {
  let found = false;
  const visit = (current) => {
    if (found) return;
    if (ts.isIdentifier(current) && states.has(current.text)) {
      found = true;
      return;
    }
    current.forEachChild(visit);
  };
  visit(node);
  return found;
}

function attributeMap(attributes) {
  const result = new Map();
  for (const attribute of attributes.properties) {
    if (!ts.isJsxAttribute(attribute)) {
      fail(attribute, "JSX spread attributes are unsupported");
    }
    const name = attribute.name.text;
    if (result.has(name)) fail(attribute, `duplicate JSX prop ${name}`);
    result.set(name, attribute.initializer);
  }
  return result;
}

function expressionFromAttribute(initializer) {
  if (!initializer) return ts.factory.createTrue();
  if (ts.isStringLiteral(initializer)) return initializer;
  if (ts.isJsxExpression(initializer) && initializer.expression) {
    return initializer.expression;
  }
  fail(initializer, "unsupported JSX attribute value");
}

function resolveStyle(expression, styles) {
  if (ts.isPropertyAccessExpression(expression) &&
      ts.isIdentifier(expression.expression) &&
      expression.expression.text === "styles") {
    const style = styles.get(expression.name.text);
    if (!style) fail(expression, `unknown style styles.${expression.name.text}`);
    return style;
  }
  if (ts.isObjectLiteralExpression(expression)) return objectLiteral(expression);
  if (ts.isArrayLiteralExpression(expression)) {
    return Object.assign(
      {},
      ...expression.elements.map((item) => resolveStyle(item, styles)),
    );
  }
  fail(expression, "style must use StyleSheet.create, an object, or an array");
}

function fontForStyle(style, node, fontAssets) {
  const size = style.fontSize ?? 12;
  const family = style.fontFamily ?? "system";
  const weight = normalizeFontWeight(style.fontWeight, node);
  const fontStyle = style.fontStyle ?? "normal";
  const lineHeight = style.lineHeight ?? 0;
  if (!Number.isInteger(size)) fail(node, "fontSize must be an integer");
  if (size < 6 || size > 48) {
    fail(node, "fontSize must be from 6 through 48");
  }
  if (typeof family !== "string") fail(node, "fontFamily must be a string");
  if (!Number.isInteger(lineHeight) ||
      (lineHeight !== 0 && lineHeight < size)) {
    fail(node, "lineHeight must be zero or an integer >= fontSize");
  }
  if (fontStyle === "italic") {
    fail(node, "Noto CJK has no true italic; fontStyle italic is unavailable");
  }
  if (fontStyle !== "normal") fail(node, "fontStyle must be normal or italic");
  if (family.startsWith("asset:")) {
    const id = family.slice("asset:".length);
    if (!/^[a-z][a-z0-9_.-]{0,30}$/.test(id)) {
      fail(node, `invalid font asset id ${JSON.stringify(id)}`);
    }
    if (fontAssets && !fontAssets.has(id)) {
      fail(node, `font asset ${id} is not declared in assets.json`);
    }
    return { asset: id, size, weight, lineHeight };
  }
  const families = {
    system: "CP_UI_FONT_SYSTEM",
    serif: "CP_UI_FONT_SERIF",
    mono: "CP_UI_FONT_MONO",
    condensed: "CP_UI_FONT_SYSTEM",
    technical: "CP_UI_FONT_MONO",
  };
  if (!families[family]) fail(node, `unsupported fontFamily ${family}`);
  return { font: families[family], size, weight, lineHeight };
}

function normalizeFontWeight(value, node) {
  if (value === undefined || value === "normal") return 400;
  if (value === "bold") return 700;
  const weight = typeof value === "string" ? Number(value) : value;
  if (!Number.isInteger(weight) || weight < 100 || weight > 900 ||
      weight % 100 !== 0) {
    fail(node, "fontWeight must be normal, bold, or 100 through 900");
  }
  return weight;
}

export function semanticFontSetFromGeneratedSource(source) {
  const stateByHandle = new Map();
  const specs = new Set();
  const call = /ui->set_i32\(\s*([^,]+?)\s*,\s*CP_UI_PROP_(FONT_SIZE|FONT_WEIGHT|FONT)\s*,\s*(\d+)u?\s*\)|ui->set_i32\(\s*([^,]+?)\s*,\s*CP_UI_PROP_FONT\s*,\s*CP_UI_FONT_(SYSTEM|SERIF|MONO)\s*\)/g;
  let match;

  while ((match = call.exec(source)) !== null) {
    const handle = (match[1] ?? match[4]).trim();
    const state = stateByHandle.get(handle) ?? {};
    if (match[2] === "FONT_SIZE") {
      state.size = Number(match[3]);
      stateByHandle.set(handle, state);
      continue;
    }
    if (match[2] === "FONT_WEIGHT") {
      state.weight = Number(match[3]);
      stateByHandle.set(handle, state);
      continue;
    }
    const family = match[5]?.toLowerCase();
    if (!family || !Number.isInteger(state.size) ||
        !Number.isInteger(state.weight)) {
      throw new Error(
        `generated source has an incomplete semantic font for ${handle}`,
      );
    }
    if (state.size < 6 || state.size > 48 || state.weight < 100 ||
        state.weight > 900 || state.weight % 100 !== 0) {
      throw new Error(
        `generated source has an invalid semantic font for ${handle}`,
      );
    }
    specs.add(`${family}:${state.weight}:${state.size}`);
  }
  return [...specs].sort((left, right) => {
    const [leftFamily, leftWeight, leftSize] = left.split(":");
    const [rightFamily, rightWeight, rightSize] = right.split(":");
    return leftFamily.localeCompare(rightFamily) ||
      Number(leftWeight) - Number(rightWeight) ||
      Number(leftSize) - Number(rightSize);
  });
}

function styleLines(handle, style, node, component, fontAssets) {
  const lines = [];
  const flexContainers = new Set([
    "View",
    "SafeAreaView",
    "Pressable",
    "Button",
    "ScrollView",
    "Modal",
  ]);
  if (flexContainers.has(component) ||
      style.flexDirection !== undefined || style.flexWrap !== undefined) {
    const direction = style.flexDirection ?? "column";
    const wrap = style.flexWrap ?? "nowrap";
    const flows = {
      "row:nowrap": "CP_UI_FLEX_ROW",
      "column:nowrap": "CP_UI_FLEX_COLUMN",
      "row:wrap": "CP_UI_FLEX_ROW_WRAP",
      "column:wrap": "CP_UI_FLEX_COLUMN_WRAP",
    };
    const flow = flows[`${direction}:${wrap}`];
    if (!flow) {
      fail(node, `unsupported flexDirection/flexWrap ${direction}/${wrap}`);
    }
    lines.push(
      `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_LAYOUT, CP_UI_LAYOUT_FLEX));`,
      `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_FLEX_FLOW, ${flow}));`,
    );
  }
  if (component === "Text" || style.fontSize !== undefined ||
      style.fontFamily !== undefined || style.fontWeight !== undefined ||
      style.fontStyle !== undefined || style.lineHeight !== undefined) {
    const font = fontForStyle(style, node, fontAssets);
    lines.push(
      `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_FONT_SIZE, ${font.size}));`,
      `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_FONT_WEIGHT, ${font.weight}));`,
      `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_FONT_STYLE, CP_UI_FONT_STYLE_NORMAL));`,
      `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_LINE_HEIGHT, ${font.lineHeight}));`,
    );
    if (font.font) {
      lines.push(
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_FONT, ${font.font}));`,
      );
    } else {
      lines.push(
        `    UI_OK(ui->set_string(${handle}, CP_UI_PROP_FONT_SOURCE, ` +
        `${cString(font.asset)}));`,
      );
    }
  }
  for (const [name, value] of Object.entries(style)) {
    if (name === "flexDirection" || name === "flexWrap" ||
        name === "fontSize" || name === "fontWeight" ||
        name === "fontFamily" || name === "fontStyle" ||
        name === "lineHeight") continue;
    if (numericStyles[name]) {
      if (!Number.isInteger(value)) fail(node, `${name} must be an integer`);
      lines.push(
        `    UI_OK(ui->set_i32(${handle}, ${numericStyles[name]}, ${value}));`,
      );
    } else if (colorStyles[name]) {
      lines.push(
        `    UI_OK(ui->set_color(${handle}, ${colorStyles[name]}, ` +
        `${color(value, node)}));`,
      );
      if (name === "backgroundColor") {
        lines.push(
          `    UI_OK(ui->set_i32(${handle}, ` +
          `CP_UI_PROP_BACKGROUND_OPACITY, 255));`,
        );
      }
    } else if (name === "opacity" || name === "backgroundOpacity") {
      if (typeof value !== "number" || value < 0 || value > 1) {
        fail(node, `${name} must be between 0 and 1`);
      }
      const property = name === "opacity"
        ? "CP_UI_PROP_OPACITY" : "CP_UI_PROP_BACKGROUND_OPACITY";
      lines.push(
        `    UI_OK(ui->set_i32(${handle}, ${property}, ` +
        `${Math.round(value * 255)}));`,
      );
    } else if (name === "display") {
      lines.push(
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_VISIBLE, ` +
        `${value === "none" ? 0 : 1}));`,
      );
    } else if (name === "justifyContent" || name === "alignItems") {
      const values = {
        "flex-start": "CP_UI_PLACE_START",
        center: "CP_UI_PLACE_CENTER",
        "flex-end": "CP_UI_PLACE_END",
        "space-between": "CP_UI_PLACE_SPACE_BETWEEN",
        "space-around": "CP_UI_PLACE_SPACE_AROUND",
        "space-evenly": "CP_UI_PLACE_SPACE_EVENLY",
      };
      if (!values[value]) fail(node, `unsupported ${name} ${value}`);
      const property = name === "justifyContent"
        ? "CP_UI_PROP_JUSTIFY" : "CP_UI_PROP_ALIGN";
      lines.push(
        `    UI_OK(ui->set_i32(${handle}, ${property}, ${values[value]}));`,
      );
    } else if (name === "textAlign") {
      const values = {
        left: "CP_UI_TEXT_ALIGN_LEFT",
        center: "CP_UI_TEXT_ALIGN_CENTER",
        right: "CP_UI_TEXT_ALIGN_RIGHT",
      };
      if (!values[value]) fail(node, `unsupported textAlign ${value}`);
      lines.push(
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_TEXT_ALIGN, ${values[value]}));`,
      );
    } else if (name === "position") {
      const positions = {
        relative: "CP_UI_POSITION_RELATIVE",
        absolute: "CP_UI_POSITION_ABSOLUTE",
      };
      if (!positions[value]) fail(node, `unsupported position ${value}`);
      lines.push(
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_POSITION, ` +
        `${positions[value]}));`,
      );
    } else if (name === "overflow") {
      if (!["visible", "hidden", "scroll"].includes(value)) {
        fail(node, `unsupported overflow ${value}`);
      }
      lines.push(
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_OVERFLOW, ` +
        `${value === "visible" ? 1 : 0}));`,
      );
    } else {
      fail(node, `unsupported React Native style property ${name}`);
    }
  }
  return lines;
}

function compileCExpression(node, states, locals = new Map()) {
  if (ts.isParenthesizedExpression(node)) {
    return `(${compileCExpression(node.expression, states, locals)})`;
  }
  if (ts.isNumericLiteral(node)) return node.text;
  if (ts.isStringLiteral(node) || ts.isNoSubstitutionTemplateLiteral(node)) {
    return cString(node.text);
  }
  if (node.kind === ts.SyntaxKind.TrueKeyword) return "1";
  if (node.kind === ts.SyntaxKind.FalseKeyword) return "0";
  if (ts.isPropertyAccessExpression(node) &&
      ts.isIdentifier(node.expression)) {
    const constant = nativeConstants[
      `${node.expression.text}.${node.name.text}`
    ];
    if (constant) return constant;
  }
  if (ts.isIdentifier(node)) {
    if (states.has(node.text)) {
      const state = states.get(node.text);
      return state.read ?? `state_${state.name}`;
    }
    if (locals.has(node.text)) return locals.get(node.text);
    fail(node, `unknown expression ${node.text}`);
  }
  if (ts.isPropertyAccessExpression(node) &&
      ts.isIdentifier(node.expression) &&
      locals.has(node.expression.text)) {
    const base = locals.get(node.expression.text);
    if (node.name.text === "value") return base;
    fail(node, `unsupported event property ${node.name.text}`);
  }
  if (ts.isPrefixUnaryExpression(node)) {
    const operators = new Map([
      [ts.SyntaxKind.MinusToken, "-"],
      [ts.SyntaxKind.PlusToken, "+"],
      [ts.SyntaxKind.ExclamationToken, "!"],
      [ts.SyntaxKind.TildeToken, "~"],
    ]);
    const operator = operators.get(node.operator);
    if (!operator) fail(node, "unsupported unary operator");
    return `(${operator}${compileCExpression(node.operand, states, locals)})`;
  }
  if (ts.isBinaryExpression(node)) {
    const operators = new Map([
      [ts.SyntaxKind.PlusToken, "+"],
      [ts.SyntaxKind.MinusToken, "-"],
      [ts.SyntaxKind.AsteriskToken, "*"],
      [ts.SyntaxKind.SlashToken, "/"],
      [ts.SyntaxKind.PercentToken, "%"],
      [ts.SyntaxKind.EqualsEqualsEqualsToken, "=="],
      [ts.SyntaxKind.ExclamationEqualsEqualsToken, "!="],
      [ts.SyntaxKind.LessThanToken, "<"],
      [ts.SyntaxKind.LessThanEqualsToken, "<="],
      [ts.SyntaxKind.GreaterThanToken, ">"],
      [ts.SyntaxKind.GreaterThanEqualsToken, ">="],
      [ts.SyntaxKind.AmpersandAmpersandToken, "&&"],
      [ts.SyntaxKind.BarBarToken, "||"],
      [ts.SyntaxKind.AmpersandToken, "&"],
      [ts.SyntaxKind.BarToken, "|"],
      [ts.SyntaxKind.CaretToken, "^"],
      [ts.SyntaxKind.LessThanLessThanToken, "<<"],
      [ts.SyntaxKind.GreaterThanGreaterThanToken, ">>"],
    ]);
    const operator = operators.get(node.operatorToken.kind);
    if (!operator) fail(node, "unsupported expression operator");
    return `(${compileCExpression(node.left, states, locals)} ` +
      `${operator} ${compileCExpression(node.right, states, locals)})`;
  }
  if (ts.isConditionalExpression(node)) {
    return `(${compileCExpression(node.condition, states, locals)} ? ` +
      `${compileCExpression(node.whenTrue, states, locals)} : ` +
      `${compileCExpression(node.whenFalse, states, locals)})`;
  }
  if (ts.isCallExpression(node) &&
      ts.isIdentifier(node.expression) &&
      node.expression.text === "tile2048Text" &&
      node.arguments.length === 1) {
    return `cp_game2048_text(${
      compileCExpression(node.arguments[0], states, locals)})`;
  }
  fail(node, "unsupported AOT expression");
}

function compileSetter(expression, states, locals = new Map(), indent = 8) {
  if (!ts.isCallExpression(expression) ||
      !ts.isIdentifier(expression.expression) ||
      expression.arguments.length !== 1) {
    fail(expression, "event handlers must call one useState setter");
  }
  const state = [...states.values()]
    .find((candidate) => candidate.setter === expression.expression.text);
  if (!state) fail(expression, "event handler calls an unknown setter");
  let value = expression.arguments[0];
  const expressionLocals = new Map(locals);
  if (ts.isArrowFunction(value)) {
    if (value.parameters.length !== 1 ||
        !ts.isIdentifier(value.parameters[0].name) ||
        ts.isBlock(value.body)) {
      fail(value, "functional state updates require value => expression");
    }
    expressionLocals.set(
      value.parameters[0].name.text,
      `state_${state.name}`,
    );
    value = value.body;
  }
  return `${" ".repeat(indent)}state_${state.name} = ` +
    `${compileCExpression(value, states, expressionLocals)};`;
}

function game2048HandlerLine(expression, states, intrinsics) {
  if (!ts.isCallExpression(expression) ||
      !ts.isIdentifier(expression.expression) ||
      expression.arguments.length !== 0) return null;
  const calls = {
    move2048Left: "CP_GAME2048_LEFT",
    move2048Right: "CP_GAME2048_RIGHT",
    move2048Up: "CP_GAME2048_UP",
    move2048Down: "CP_GAME2048_DOWN",
  };
  if (calls[expression.expression.text]) {
    intrinsics.add("game2048");
    const call = `cp_game2048_move(${calls[expression.expression.text]});`;
    return states.has("route")
      ? `        if(previous_state_route == 1) ${call}`
      : `        ${call}`;
  }
  if (expression.expression.text === "reset2048") {
    intrinsics.add("game2048");
    return "        cp_game2048_reset();";
  }
  return null;
}

function compileHandler(expression, states, intrinsics) {
  if (!ts.isArrowFunction(expression) || expression.parameters.length > 1) {
    fail(expression, "handlers must be arrow functions with zero or one parameter");
  }
  const locals = new Map();
  if (expression.parameters.length === 1) {
    const parameter = expression.parameters[0];
    if (!ts.isIdentifier(parameter.name)) {
      fail(parameter, "event parameter must be an identifier");
    }
    locals.set(parameter.name.text, "value");
  }
  const statements = ts.isBlock(expression.body)
    ? expression.body.statements
    : [ts.factory.createExpressionStatement(expression.body)];
  if (statements.length === 0) {
    fail(expression.body, "event handler cannot be empty");
  }
  const eventStates = new Map(
    [...states].map(([name, state]) => [
      name,
      { ...state, read: `previous_state_${state.name}` },
    ]),
  );
  return statements.map((statement) => {
    if (!ts.isExpressionStatement(statement)) {
      fail(statement, "event handlers currently support setter calls only");
    }
    return game2048HandlerLine(statement.expression, eventStates, intrinsics) ??
      compileSetter(statement.expression, eventStates, locals);
  });
}

function expressionKind(node, states, locals = new Map()) {
  if (ts.isStringLiteral(node) ||
      ts.isNoSubstitutionTemplateLiteral(node)) return "string";
  if (ts.isConditionalExpression(node)) {
    const whenTrue = expressionKind(node.whenTrue, states, locals);
    const whenFalse = expressionKind(node.whenFalse, states, locals);
    return whenTrue === "string" && whenFalse === "string"
      ? "string" : "integer";
  }
  if (ts.isIdentifier(node) && states.has(node.text)) return "integer";
  if (ts.isCallExpression(node) &&
      ts.isIdentifier(node.expression) &&
      node.expression.text === "tile2048Text") return "string";
  return "integer";
}

function compileCondition(node, states, locals = new Map()) {
  const expression = compileCExpression(node, states, locals);
  return expression.startsWith("(") ? expression : `(${expression})`;
}

function game2048RuntimeSource() {
  return `
enum cp_game2048_direction {
    CP_GAME2048_LEFT = 0,
    CP_GAME2048_RIGHT,
    CP_GAME2048_UP,
    CP_GAME2048_DOWN
};

static uint32_t cp_game2048_random(uint32_t *state)
{
    uint32_t value = *state != 0 ? *state : 0x6d2b79f5u;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static int cp_game2048_index(int direction, int line, int offset)
{
    if(direction == CP_GAME2048_LEFT)
        return line * 4 + offset;
    if(direction == CP_GAME2048_RIGHT)
        return line * 4 + (3 - offset);
    if(direction == CP_GAME2048_UP)
        return offset * 4 + line;
    return (3 - offset) * 4 + line;
}

static bool cp_game2048_can_move(const int32_t board[16])
{
    int row;
    int column;
    for(row = 0; row < 4; ++row) {
        for(column = 0; column < 4; ++column) {
            int index = row * 4 + column;
            if(board[index] == 0)
                return true;
            if(column < 3 && board[index] == board[index + 1])
                return true;
            if(row < 3 && board[index] == board[index + 4])
                return true;
        }
    }
    return false;
}

static void cp_game2048_store(const int32_t board[16])
{
    state_cell0 = board[0];
    state_cell1 = board[1];
    state_cell2 = board[2];
    state_cell3 = board[3];
    state_cell4 = board[4];
    state_cell5 = board[5];
    state_cell6 = board[6];
    state_cell7 = board[7];
    state_cell8 = board[8];
    state_cell9 = board[9];
    state_cell10 = board[10];
    state_cell11 = board[11];
    state_cell12 = board[12];
    state_cell13 = board[13];
    state_cell14 = board[14];
    state_cell15 = board[15];
}

struct cp_game2048_record {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t checksum;
    int32_t board[16];
    int32_t score;
    int32_t moves;
    int32_t seed;
    int32_t won;
    int32_t game_over;
};

static uint32_t cp_game2048_checksum(
    const struct cp_game2048_record *record)
{
    const unsigned char *bytes = (const unsigned char *)record;
    uint32_t hash = 2166136261u;
    size_t index;
    for(index = 0; index < sizeof(*record); ++index) {
        unsigned char value =
            index >= offsetof(struct cp_game2048_record, checksum) &&
            index < offsetof(struct cp_game2048_record, checksum) +
                    sizeof(record->checksum)
                ? 0 : bytes[index];
        hash = (hash ^ value) * 16777619u;
    }
    return hash;
}

static void cp_game2048_persist(void)
{
    struct cp_game2048_record record = {
        .magic = 0x35473243u,
        .version = 1u,
        .size = sizeof(record),
        .board = {
            state_cell0, state_cell1, state_cell2, state_cell3,
            state_cell4, state_cell5, state_cell6, state_cell7,
            state_cell8, state_cell9, state_cell10, state_cell11,
            state_cell12, state_cell13, state_cell14, state_cell15
        },
        .score = state_score,
        .moves = state_moves,
        .seed = state_seed,
        .won = state_won,
        .game_over = state_gameOver,
    };
    if(host->state_write == NULL)
        return;
    record.checksum = cp_game2048_checksum(&record);
    (void)host->state_write(&record, sizeof(record));
}

static bool cp_game2048_load(void)
{
    struct cp_game2048_record record;
    int result;
    if(host->state_read == NULL)
        return false;
    result = host->state_read(&record, sizeof(record));
    if(result != (int)sizeof(record) ||
       record.magic != 0x35473243u ||
       record.version != 1u ||
       record.size != sizeof(record) ||
       record.checksum != cp_game2048_checksum(&record))
        return false;
    cp_game2048_store(record.board);
    state_score = record.score;
    state_moves = record.moves;
    state_seed = record.seed;
    state_won = record.won != 0;
    state_gameOver = record.game_over != 0;
    return true;
}

static void cp_game2048_spawn(int32_t board[16], uint32_t *seed)
{
    int empty[16];
    int count = 0;
    int index;
    for(index = 0; index < 16; ++index) {
        if(board[index] == 0)
            empty[count++] = index;
    }
    if(count > 0) {
        int selected = empty[cp_game2048_random(seed) % (uint32_t)count];
        board[selected] =
            cp_game2048_random(seed) % 10u == 0u ? 2 : 1;
    }
}

static void cp_game2048_reset(void)
{
    int32_t board[16] = { 0 };
    uint32_t seed = (uint32_t)host->monotonic_ms() ^ 0x2048cafeu;
    cp_game2048_spawn(board, &seed);
    cp_game2048_spawn(board, &seed);
    cp_game2048_store(board);
    state_score = 0;
    state_moves = 0;
    state_seed = (int32_t)seed;
    state_won = 0;
    state_gameOver = 0;
    cp_game2048_persist();
}

static void cp_game2048_move(int direction)
{
    int32_t board[16] = {
        state_cell0, state_cell1, state_cell2, state_cell3,
        state_cell4, state_cell5, state_cell6, state_cell7,
        state_cell8, state_cell9, state_cell10, state_cell11,
        state_cell12, state_cell13, state_cell14, state_cell15
    };
    int32_t original[16];
    int32_t gain = 0;
    uint32_t seed = (uint32_t)state_seed;
    bool changed = false;
    int line;
    int index;
    for(index = 0; index < 16; ++index)
        original[index] = board[index];
    for(line = 0; line < 4; ++line) {
        int32_t compact[4] = { 0, 0, 0, 0 };
        int32_t output[4] = { 0, 0, 0, 0 };
        int input_count = 0;
        int output_count = 0;
        int offset;
        for(offset = 0; offset < 4; ++offset) {
            int32_t value = board[
                cp_game2048_index(direction, line, offset)];
            if(value != 0)
                compact[input_count++] = value;
        }
        for(offset = 0; offset < input_count; ++offset) {
            int32_t value = compact[offset];
            if(offset + 1 < input_count &&
               compact[offset + 1] == value) {
                ++value;
                ++offset;
                if(value < 31)
                    gain += (int32_t)(1u << value);
            }
            output[output_count++] = value;
        }
        for(offset = 0; offset < 4; ++offset)
            board[cp_game2048_index(direction, line, offset)] =
                output[offset];
    }
    for(index = 0; index < 16; ++index) {
        if(board[index] != original[index]) {
            changed = true;
            break;
        }
    }
    if(!changed) {
        state_gameOver = cp_game2048_can_move(board) ? 0 : 1;
        if(state_gameOver)
            cp_game2048_persist();
        return;
    }
    cp_game2048_spawn(board, &seed);
    cp_game2048_store(board);
    state_score += gain;
    ++state_moves;
    state_seed = (int32_t)seed;
    state_won = 0;
    for(index = 0; index < 16; ++index) {
        if(board[index] >= 11)
            state_won = 1;
    }
    state_gameOver = cp_game2048_can_move(board) ? 0 : 1;
    cp_game2048_persist();
}

static const char *cp_game2048_text(int32_t exponent)
{
    static const char *const values[] = {
        "", "2", "4", "8", "16", "32", "64", "128",
        "256", "512", "1024", "2048", "4096", "8192",
        "16384", "32768", "65536"
    };
    return exponent >= 0 &&
           exponent < (int32_t)(sizeof(values) / sizeof(values[0]))
        ? values[exponent] : "MAX";
}
`;
}

export function compileNativeSource(source, {
  filename = "app.tsx",
  manifest,
  fontAssets = null,
} = {}) {
  if (!manifest) throw new TypeError("manifest is required");
  const file = ts.createSourceFile(
    filename, source, ts.ScriptTarget.ES2022, true, ts.ScriptKind.TSX,
  );
  const styles = new Map();
  let app = null;

  for (const statement of file.statements) {
    if (ts.isImportDeclaration(statement)) continue;
    if (ts.isVariableStatement(statement)) {
      for (const declaration of statement.declarationList.declarations) {
        if (!ts.isIdentifier(declaration.name) || !declaration.initializer) {
          fail(declaration, "unsupported top-level declaration");
        }
        const call = declaration.initializer;
        if (declaration.name.text !== "styles" ||
            !ts.isCallExpression(call) ||
            !ts.isPropertyAccessExpression(call.expression) ||
            call.expression.expression.getText(file) !== "StyleSheet" ||
            call.expression.name.text !== "create" ||
            call.arguments.length !== 1 ||
            !ts.isObjectLiteralExpression(call.arguments[0])) {
          fail(declaration, "only const styles = StyleSheet.create(...) is allowed");
        }
        for (const property of call.arguments[0].properties) {
          if (!ts.isPropertyAssignment(property)) {
            fail(property, "style entries must be property assignments");
          }
          styles.set(
            propertyName(property.name), objectLiteral(property.initializer),
          );
        }
      }
      continue;
    }
    if (ts.isFunctionDeclaration(statement) &&
        statement.modifiers?.some((item) =>
          item.kind === ts.SyntaxKind.DefaultKeyword)) {
      app = statement;
      continue;
    }
    if (ts.isExportAssignment(statement) &&
        ts.isIdentifier(statement.expression)) continue;
    fail(statement, "unsupported top-level statement in React Profile v1");
  }
  if (!app?.body) throw new Error(`${filename}: default App function is required`);

  const states = new Map();
  let rootJsx = null;
  for (const statement of app.body.statements) {
    if (ts.isVariableStatement(statement)) {
      for (const declaration of statement.declarationList.declarations) {
        if (!ts.isArrayBindingPattern(declaration.name) ||
            declaration.name.elements.length !== 2 ||
            !declaration.initializer ||
            !ts.isCallExpression(declaration.initializer) ||
            declaration.initializer.expression.getText(file) !== "useState" ||
            declaration.initializer.arguments.length !== 1) {
          fail(declaration, "App locals must be const [value, setValue] = useState(initial)");
        }
        const [value, setter] = declaration.name.elements;
        if (!ts.isBindingElement(value) || !ts.isIdentifier(value.name) ||
            !ts.isBindingElement(setter) || !ts.isIdentifier(setter.name)) {
          fail(declaration, "useState binding names must be identifiers");
        }
        states.set(value.name.text, {
          name: value.name.text,
          setter: setter.name.text,
          initial: initializerNumber(declaration.initializer.arguments[0]),
        });
      }
    } else if (ts.isReturnStatement(statement) && statement.expression) {
      rootJsx = unwrapExpression(statement.expression);
    } else {
      fail(statement, "App supports useState declarations followed by one return");
    }
  }
  if (!rootJsx ||
      (!ts.isJsxElement(rootJsx) && !ts.isJsxSelfClosingElement(rootJsx))) {
    fail(app, "App must return one JSX root");
  }

  const render = [];
  const dynamic = [];
  const structureConditions = [];
  const handlers = [];
  const inputHandlers = [];
  const intrinsics = new Set();
  let handleIndex = 0;
  let rootHandle = null;
  const compileNode = (node, parent, isRoot = false) => {
    const opening = ts.isJsxElement(node) ? node.openingElement : node;
    const name = jsxName(opening.tagName);
    if (isRoot && name !== "View")
      fail(opening, "App JSX root must be View");
    const type = isRoot ? "CP_UI_OBJECT_SCREEN" : objectTypes[name];
    if (!type) fail(opening, `unsupported React Native component ${name}`);
    const index = handleIndex++;
    const handle = `h${index}`;
    const retained = `native_handles[${index}]`;
    const attributes = attributeMap(opening.attributes);
    render.push(
      `    cp_ui_handle_t ${handle} = ui->create(${type});`,
      `    if(${handle} == CP_NATIVE_UI_HANDLE_NONE) return CP_NATIVE_ERROR_LIMIT;`,
      `    ${retained} = ${handle};`,
    );
    if (isRoot) {
      rootHandle = handle;
      render.push(`    native_root = ${handle};`);
    }
    const insertParent = name === "Modal" ? rootHandle : parent;
    if (insertParent) {
      render.push(`    UI_OK(ui->insert(${handle}, ${insertParent}, 0));`);
    }
    if (name === "SafeAreaView") {
      render.push(
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_POSITION, CP_UI_POSITION_ABSOLUTE));`,
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_X, 0));`,
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_Y, 32));`,
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_WIDTH, 320));`,
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_HEIGHT, 208));`,
      );
    }
    if (name === "Modal") {
      render.push(
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_POSITION, CP_UI_POSITION_ABSOLUTE));`,
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_X, 0));`,
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_Y, 0));`,
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_WIDTH, 320));`,
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_HEIGHT, 240));`,
      );
    }
    if (name === "Pressable" || name === "Button") {
      render.push(
        `    UI_OK(ui->set_i32(${handle}, CP_UI_PROP_FOCUSABLE, 1));`,
      );
    }
    const styleAttribute = attributes.get("style");
    if (styleAttribute) {
      const expression = expressionFromAttribute(styleAttribute);
      render.push(...styleLines(
        handle, resolveStyle(expression, styles), expression, name,
        fontAssets,
      ));
    } else {
      render.push(...styleLines(handle, {}, opening, name, fontAssets));
    }

    for (const [prop, property] of Object.entries(integerProps)) {
      const attribute = attributes.get(prop);
      if (!attribute) continue;
      const expression = expressionFromAttribute(attribute);
      const value = compileCExpression(expression, states);
      const line =
        `    UI_OK(ui->set_i32(${handle}, ${property}, ${value}));`;
      render.push(line);
      if (usesState(expression, states)) {
        dynamic.push(
          `    if(${retained} != CP_NATIVE_UI_HANDLE_NONE)`,
          `        UI_OK(ui->set_i32(${retained}, ${property}, ${value}));`,
        );
      }
    }
    for (const [prop, property] of Object.entries(stringProps)) {
      const attribute = attributes.get(prop);
      if (!attribute) continue;
      const expression = expressionFromAttribute(attribute);
      if (expressionKind(expression, states) !== "string") {
        fail(expression, `${prop} must be a string expression`);
      }
      const value = compileCExpression(expression, states);
      render.push(
        `    UI_OK(ui->set_string(${handle}, ${property}, ${value}));`,
      );
      if (usesState(expression, states)) {
        dynamic.push(
          `    if(${retained} != CP_NATIVE_UI_HANDLE_NONE)`,
          `        UI_OK(ui->set_string(${retained}, ${property}, ${value}));`,
        );
      }
    }
    for (const [prop, property] of Object.entries(colorProps)) {
      const attribute = attributes.get(prop);
      if (!attribute) continue;
      const expression = expressionFromAttribute(attribute);
      const value = literal(expression);
      if (typeof value !== "string" || !/^#[0-9a-f]{6}$/i.test(value)) {
        fail(expression, `${prop} must be a static #RRGGBB color`);
      }
      render.push(
        `    UI_OK(ui->set_color(${handle}, ${property}, ` +
        `0x${value.slice(1).toLowerCase()}u));`,
      );
    }
    for (const [prop, eventType] of Object.entries(eventProps)) {
      const attribute = attributes.get(prop);
      if (!attribute || (isRoot && inputProps[prop])) continue;
      const expression = expressionFromAttribute(attribute);
      const id = handlers.length + 1;
      handlers.push({
        id,
        lines: compileHandler(expression, states, intrinsics),
      });
      render.push(
        `    UI_OK(ui->listen(${handle}, ${eventType}, ${id}u));`,
      );
    }
    if (isRoot) {
      for (const [prop, inputType] of Object.entries(inputProps)) {
        const attribute = attributes.get(prop);
        if (!attribute) continue;
        const expression = expressionFromAttribute(attribute);
        inputHandlers.push({
          inputType,
          lines: compileHandler(expression, states, intrinsics),
        });
      }
    }

    const children = ts.isJsxElement(node) ? node.children : [];
    const textParts = children.filter((child) =>
      ts.isJsxText(child) || ts.isJsxExpression(child));
    if (name === "Text" && textParts.length > 0) {
      const meaningful = textParts.filter((child) =>
        !ts.isJsxText(child) || child.text.trim().length > 0);
      if (meaningful.length !== 1) {
        fail(node, "Text currently requires one literal or state expression");
      }
      const child = meaningful[0];
      if (ts.isJsxText(child)) {
        render.push(
          `    UI_OK(ui->set_string(${handle}, CP_UI_PROP_TEXT, ` +
          `${cString(child.text.trim())}));`,
        );
      } else if (child.expression) {
        const expression = unwrapExpression(child.expression);
        if (expressionKind(expression, states) === "string") {
          const value = compileCExpression(expression, states);
          render.push(
            `    UI_OK(ui->set_string(${handle}, CP_UI_PROP_TEXT, ` +
            `${value}));`,
          );
          if (usesState(expression, states)) {
            dynamic.push(
              `    if(${retained} != CP_NATIVE_UI_HANDLE_NONE)`,
              `        UI_OK(ui->set_string(${retained}, ` +
              `CP_UI_PROP_TEXT, ${value}));`,
            );
          }
        } else {
          const value = compileCExpression(expression, states);
          render.push(
            `    cp_i32_text(${value}, ` +
            `text_buffer, sizeof(text_buffer));`,
            `    UI_OK(ui->set_string(${handle}, CP_UI_PROP_TEXT, text_buffer));`,
          );
          if (usesState(expression, states)) {
            dynamic.push(
              `    if(${retained} != CP_NATIVE_UI_HANDLE_NONE) {`,
              `        cp_i32_text(${value}, text_buffer, sizeof(text_buffer));`,
              `        UI_OK(ui->set_string(${retained}, ` +
              `CP_UI_PROP_TEXT, text_buffer));`,
              "    }",
            );
          }
        }
      } else {
        fail(child, "empty Text expression is unsupported");
      }
    }

    const compileConditionalChild = (expression) => {
      const value = unwrapExpression(expression);
      if (ts.isConditionalExpression(value)) {
        const whenTrue = unwrapExpression(value.whenTrue);
        const whenFalse = unwrapExpression(value.whenFalse);
        if ((!ts.isJsxElement(whenTrue) &&
             !ts.isJsxSelfClosingElement(whenTrue)) ||
            (!ts.isJsxElement(whenFalse) &&
             !ts.isJsxSelfClosingElement(whenFalse))) {
          fail(value, "conditional children require JSX in both branches");
        }
        const condition = compileCondition(value.condition, states);
        structureConditions.push(condition);
        render.push(`    if${condition} {`);
        compileNode(whenTrue, handle);
        render.push("    } else {");
        compileNode(whenFalse, handle);
        render.push("    }");
        return;
      }
      if (ts.isBinaryExpression(value) &&
          value.operatorToken.kind === ts.SyntaxKind.AmpersandAmpersandToken) {
        const child = unwrapExpression(value.right);
        if (!ts.isJsxElement(child) &&
            !ts.isJsxSelfClosingElement(child)) {
          fail(child, "logical conditional child must be JSX");
        }
        const condition = compileCondition(value.left, states);
        structureConditions.push(condition);
        render.push(`    if${condition} {`);
        compileNode(child, handle);
        render.push("    }");
        return;
      }
      fail(value, "unsupported JSX child expression");
    };
    for (const child of children) {
      if (ts.isJsxElement(child) || ts.isJsxSelfClosingElement(child)) {
        compileNode(child, handle);
      } else if (ts.isJsxExpression(child) && child.expression &&
                 name !== "Text") {
        compileConditionalChild(child.expression);
      }
    }
  };
  compileNode(rootJsx, null, true);

  if (intrinsics.has("game2048")) {
    const required = [
      ...Array.from({ length: 16 }, (_, index) => `cell${index}`),
      "score", "moves", "seed", "won", "gameOver",
    ];
    for (const name of required) {
      if (!states.has(name)) {
        fail(app, `2048 intrinsic requires useState binding ${name}`);
      }
    }
  }

  const stateDefinitions = [...states.values()]
    .map((state) => `static int32_t state_${state.name};`).join("\n");
  const stateInitializers = [...states.values()]
    .map((state) => `    state_${state.name} = ${state.initial};`).join("\n");
  const stateSnapshotDeclarations = [...states.values()]
    .map((state) =>
      `    int32_t previous_state_${state.name} = state_${state.name};\n` +
      `    (void)previous_state_${state.name};`)
    .join("\n");
  const handlerCases = handlers.map((handler) =>
    `    case ${handler.id}u:\n${handler.lines.join("\n")}\n` +
    "        break;").join("\n");
  const inputCases = inputHandlers.map((handler) =>
    `    case ${handler.inputType}:\n${handler.lines.join("\n")}\n` +
    "        break;").join("\n");

  return `/* Generated by CrazyPod React Profile AOT. Do not edit. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "crazypod_miniapp_native.h"

#ifdef CRAZYPOD_MINIAPP_PACKAGE
CP_NATIVE_MINIAPP_HEADER;
#endif

static const struct cp_native_host_api *host;
static cp_ui_handle_t native_root;
static cp_ui_handle_t native_handles[${handleIndex}];
static uint32_t rendered_structure_key;
${stateDefinitions}
${intrinsics.has("game2048") ? game2048RuntimeSource() : ""}

#define UI_OK(call) do { int result_ = (call); if(result_ != CP_NATIVE_OK) return result_; } while(0)

static void cp_i32_text(int32_t value, char *buffer, size_t capacity)
{
    char digits[11];
    uint32_t magnitude;
    size_t count = 0;
    size_t cursor = 0;
    if(capacity == 0) return;
    if(value < 0) {
        buffer[cursor++] = '-';
        magnitude = (uint32_t)(-(value + 1)) + 1u;
    } else {
        magnitude = (uint32_t)value;
    }
    do {
        digits[count++] = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while(magnitude != 0 && count < sizeof(digits));
    while(count > 0 && cursor + 1 < capacity)
        buffer[cursor++] = digits[--count];
    buffer[cursor] = '\\0';
}

static uint32_t cp_structure_key(void)
{
    uint32_t key = 5381u;
${structureConditions.map((condition) =>
    `    key = key * 33u + (${condition} ? 1u : 0u);`).join("\n")}
    return key;
}

static int cp_update_dynamic(void)
{
    const struct cp_native_ui_api *ui = host->ui;
    char text_buffer[16];
    UI_OK(ui->begin_update());
${dynamic.join("\n")}
    UI_OK(ui->end_update());
    return CP_NATIVE_OK;
}

static int cp_render(void)
{
    const struct cp_native_ui_api *ui = host->ui;
    char text_buffer[16];
    uint32_t structure_key = cp_structure_key();
    size_t index;
    if(native_root != CP_NATIVE_UI_HANDLE_NONE &&
       structure_key == rendered_structure_key)
        return cp_update_dynamic();
    if(native_root != CP_NATIVE_UI_HANDLE_NONE) {
        UI_OK(ui->remove(native_root));
        native_root = CP_NATIVE_UI_HANDLE_NONE;
    }
    for(index = 0; index < ${handleIndex}; ++index)
        native_handles[index] = CP_NATIVE_UI_HANDLE_NONE;
    UI_OK(ui->begin_update());
${render.join("\n")}
    UI_OK(ui->end_update());
    rendered_structure_key = structure_key;
    return CP_NATIVE_OK;
}

static int app_mount(void)
{
    native_root = CP_NATIVE_UI_HANDLE_NONE;
    rendered_structure_key = UINT32_MAX;
${stateInitializers}
${intrinsics.has("game2048")
    ? "    if(!cp_game2048_load()) cp_game2048_reset();" : ""}
    return cp_render();
}

static void app_unmount(void)
{
${intrinsics.has("game2048") ? "    cp_game2048_persist();" : ""}
    if(native_root != CP_NATIVE_UI_HANDLE_NONE) {
        (void)host->ui->remove(native_root);
        native_root = CP_NATIVE_UI_HANDLE_NONE;
    }
    rendered_structure_key = UINT32_MAX;
}

static uint32_t app_input(const struct cp_input_event *event)
{
    int32_t value;
${stateSnapshotDeclarations}
    if(event == NULL || event->struct_size < sizeof(*event))
        return CP_NATIVE_UPDATE_NONE;
    value = event->steps;
    (void)value;
    switch(event->type) {
${inputCases}
    default:
        return CP_NATIVE_UPDATE_NONE;
    }
    return cp_render() == CP_NATIVE_OK
        ? CP_NATIVE_UPDATE_UI : CP_NATIVE_UPDATE_NONE;
}

static uint32_t app_ui_event(
    cp_event_handler_t handler, uint8_t event_type,
    cp_ui_handle_t target, int32_t value)
{
    (void)event_type;
    (void)target;
    (void)value;
${stateSnapshotDeclarations}
    switch(handler) {
${handlerCases}
    default:
        return CP_NATIVE_UPDATE_NONE;
    }
    return cp_render() == CP_NATIVE_OK
        ? CP_NATIVE_UPDATE_UI : CP_NATIVE_UPDATE_NONE;
}

static uint32_t app_tick(uint32_t epoch_seconds, uint32_t monotonic_ms)
{
    (void)epoch_seconds;
    (void)monotonic_ms;
    return CP_NATIVE_UPDATE_NONE;
}

static bool app_has_scheduled_work(void)
{
    return false;
}

static const struct cp_native_app_ops app_ops = {
    .abi_major = CP_NATIVE_ABI_MAJOR,
    .abi_minor = CP_NATIVE_ABI_MINOR,
    .struct_size = sizeof(struct cp_native_app_ops),
    .id = ${cString(manifest.id)},
    .name = ${cString(manifest.name)},
    .version = ${cString(manifest.version)},
    .mount = app_mount,
    .unmount = app_unmount,
    .input = app_input,
    .ui_event = app_ui_event,
    .tick = app_tick,
    .has_scheduled_work = app_has_scheduled_work,
};

const struct cp_native_app_ops *
cp_native_miniapp_entry(const struct cp_native_host_api *api)
{
    if(api == NULL ||
       api->abi_major != CP_NATIVE_ABI_MAJOR ||
       api->struct_size < CP_NATIVE_HOST_V1_SIZE ||
       api->ui == NULL ||
       api->ui->struct_size < CP_NATIVE_UI_V1_SIZE)
        return NULL;
    host = api;
    return &app_ops;
}
`;
}

export async function generateNativeProject(
  projectDirectory, { output } = {},
) {
  const project = path.resolve(projectDirectory);
  const config = JSON.parse(
    await readFile(path.join(project, "crazypod.config.json"), "utf8"),
  );
  if (config.runtime !== "native-aot") {
    throw new Error("config.runtime must be native-aot");
  }
  const sourcePath = path.resolve(project, config.source ?? "src/App.tsx");
  const source = await readFile(sourcePath, "utf8");
  const fontAssets = new Set();
  if (config.assets) {
    const descriptorPath = path.join(
      path.resolve(project, config.assets), "assets.json",
    );
    let descriptor;
    try {
      descriptor = JSON.parse(await readFile(descriptorPath, "utf8"));
    } catch (error) {
      if (error.code === "ENOENT") descriptor = [];
      else throw error;
    }
    if (!Array.isArray(descriptor)) {
      throw new Error("assets/assets.json must be an array");
    }
    for (const item of descriptor) {
      if (item?.type === "font" && typeof item.id === "string") {
        fontAssets.add(item.id);
      }
    }
    if (fontAssets.size > 4) {
      throw new Error("a package may declare at most 4 fonts");
    }
  }
  const destination = path.resolve(
    output ?? path.join(project, ".crazypod/native/app.c"),
  );
  const generated = compileNativeSource(source, {
    filename: sourcePath,
    manifest: config.manifest,
    fontAssets,
  });
  await mkdir(path.dirname(destination), { recursive: true });
  await writeFile(destination, generated);
  return { output: destination, manifest: config.manifest };
}

function nativeManifest(source, target, fontSet) {
  const required = [
    "id", "name", "version", "versionCode",
    "symbol", "summary", "accent",
  ];
  for (const name of required) {
    if (source[name] === undefined) {
      throw new Error(`manifest.${name} is required`);
    }
  }
  if (!/^[a-z][a-z0-9_-]{0,31}$/.test(source.id)) {
    throw new Error("manifest.id is invalid");
  }
  if (!Number.isInteger(source.versionCode) || source.versionCode <= 0) {
    throw new Error("manifest.versionCode must be a positive integer");
  }
  if (!/^#[0-9a-f]{6}$/i.test(source.accent)) {
    throw new Error("manifest.accent must be #RRGGBB");
  }
  if (source.kind !== undefined &&
      !["miniapp", "now-playing-theme"].includes(source.kind)) {
    throw new Error("manifest.kind must be miniapp or now-playing-theme");
  }
  if (source.artworkSourceSize !== undefined &&
      (!Number.isInteger(source.artworkSourceSize) ||
       source.artworkSourceSize < 16 || source.artworkSourceSize > 320)) {
    throw new Error(
      "manifest.artworkSourceSize must be an integer from 16 to 320",
    );
  }
  if (source.kind === "now-playing-theme" &&
      source.artworkSourceSize === undefined) {
    throw new Error(
      "manifest.artworkSourceSize is required for now-playing-theme",
    );
  }
  if (source.kind !== "now-playing-theme" &&
      source.artworkSourceSize !== undefined) {
    throw new Error(
      "manifest.artworkSourceSize requires kind now-playing-theme",
    );
  }
  if (source.statusBar !== undefined &&
      !["system", "theme"].includes(source.statusBar)) {
    throw new Error("manifest.statusBar must be system or theme");
  }
  if (source.statusBar !== undefined &&
      source.kind !== "now-playing-theme") {
    throw new Error("manifest.statusBar requires kind now-playing-theme");
  }
  if (!Array.isArray(fontSet)) {
    throw new TypeError("fontSet must be an array");
  }
  if (!["simulator", "ipod6g"].includes(target)) {
    throw new Error("native target must be simulator or ipod6g");
  }
  return {
    format: 5,
    kind: source.kind === undefined ? "miniapp" : String(source.kind),
    ...(source.kind === "now-playing-theme"
      ? {
        ...(source.statusBar === undefined
          ? {} : { statusBar: source.statusBar }),
        artworkSourceSize: source.artworkSourceSize,
      }
      : {}),
    id: source.id,
    name: String(source.name),
    version: String(source.version),
    versionCode: source.versionCode,
    runtime: "native-aot",
    abiMajor: NATIVE_ABI_MAJOR,
    abiMinor: NATIVE_ABI_MINOR,
    reactProfile: REACT_PROFILE,
    target,
    entry: target === "simulator" ? "app.dylib" : "app.arm",
    icon: "icon.bin",
    symbol: String(source.symbol),
    summary: String(source.summary),
    accent: source.accent.toLowerCase(),
    fontSet: fontSet.join(","),
  };
}

export async function packageNativeProject(
  projectDirectory, {
    binary,
    target,
    output,
    generatedSource,
  } = {},
) {
  if (!binary) throw new Error("native build requires --binary FILE");
  const project = path.resolve(projectDirectory);
  const config = JSON.parse(
    await readFile(path.join(project, "crazypod.config.json"), "utf8"),
  );
  if (config.runtime !== "native-aot") {
    throw new Error("config.runtime must be native-aot");
  }
  const generatedSourcePath = path.resolve(
    generatedSource ?? path.join(project, "generated/app.c"),
  );
  let generatedSourceText;
  try {
    generatedSourceText = await readFile(generatedSourcePath, "utf8");
  } catch (error) {
    if (error.code === "ENOENT") {
      throw new Error(
        `native build requires generated source ${generatedSourcePath}`,
      );
    }
    throw error;
  }
  const fontSet = semanticFontSetFromGeneratedSource(generatedSourceText);
  const manifest = nativeManifest(config.manifest ?? {}, target, fontSet);
  const binaryData = await readFile(path.resolve(binary));
  if (binaryData.length === 0 || binaryData.length > 8 * 1024 * 1024) {
    throw new Error("native binary must be 1 byte to 8 MiB");
  }
  const assets = await buildAssets(
    config.assets ? path.resolve(project, config.assets) : null,
  );
  const icon = solidIcon(config.iconColor ?? manifest.accent);
  const destination = path.resolve(
    output ?? path.join(
      project, "dist",
      `${manifest.id}-${manifest.version}-${target}.cpk`,
    ),
  );
  await writeNativePackage(destination, {
    manifest: Buffer.from(JSON.stringify(manifest), "utf8"),
    binaryName: manifest.entry,
    binary: binaryData,
    profile: nativeProfile(),
    assets,
    icon,
  });
  return {
    output: destination,
    manifest,
    sizes: {
      binary: binaryData.length,
      profile: 16,
      assets: assets.length,
      icon: icon.length,
    },
  };
}
