declare module "react" {
  export type Dispatch<T> = (value: T | ((previous: T) => T)) => void;

  export function useState<T>(initial: T): [T, Dispatch<T>];

  const React: Record<string, never>;
  export default React;
}

declare module "react-native" {
  export namespace JSX {
    type Element = any;
    interface ElementChildrenAttribute {
      children: {};
    }
  }

  export interface NativeEvent {
    readonly value: number;
    readonly steps: number;
    readonly repeated: boolean;
  }

  export interface ViewStyle {
    position?: "relative" | "absolute";
    display?: "flex" | "none";
    overflow?: "visible" | "hidden" | "scroll";
    left?: number;
    top?: number;
    width?: number;
    height?: number;
    minWidth?: number;
    minHeight?: number;
    maxWidth?: number;
    maxHeight?: number;
    flexDirection?: "row" | "column";
    flexWrap?: "nowrap" | "wrap";
    flexGrow?: number;
    justifyContent?: "flex-start" | "center" | "flex-end" |
      "space-between" | "space-around" | "space-evenly";
    alignItems?: "flex-start" | "center" | "flex-end" |
      "space-between" | "space-around" | "space-evenly";
    padding?: number;
    paddingLeft?: number;
    paddingRight?: number;
    paddingTop?: number;
    paddingBottom?: number;
    margin?: number;
    marginLeft?: number;
    marginRight?: number;
    marginTop?: number;
    marginBottom?: number;
    backgroundColor?: `#${string}`;
    opacity?: number;
    borderColor?: `#${string}`;
    borderWidth?: number;
    borderRadius?: number;
    shadowColor?: `#${string}`;
    shadowRadius?: number;
  }

  export interface TextStyle extends ViewStyle {
    color?: `#${string}`;
    fontSize?: number;
    fontWeight?: "normal" | "bold" | number | string;
    textAlign?: "left" | "center" | "right";
  }

  export type StyleProp<T> = T | readonly StyleProp<T>[];

  export interface ViewProps {
    children?: JSX.Element;
    style?: StyleProp<ViewStyle>;
    disabled?: boolean;
    focusable?: boolean;
    onPress?: (event: NativeEvent) => void;
    onSelect?: (event: NativeEvent) => void;
    onLongPress?: (event: NativeEvent) => void;
    onFocus?: (event: NativeEvent) => void;
    onBlur?: (event: NativeEvent) => void;
    onChange?: (event: NativeEvent) => void;
    onValueChange?: (event: NativeEvent) => void;
    onScroll?: (event: NativeEvent) => void;
    onWheelClockwise?: (event: NativeEvent) => void;
    onWheelCounterClockwise?: (event: NativeEvent) => void;
    onLeft?: (event: NativeEvent) => void;
    onRight?: (event: NativeEvent) => void;
    onPlay?: (event: NativeEvent) => void;
    onMenu?: (event: NativeEvent) => void;
  }

  export interface TextProps extends ViewProps {
    children?: JSX.Element | string | number;
  }

  export interface ValueProps extends ViewProps {
    value?: number;
    minimum?: number;
    minimumValue?: number;
    maximum?: number;
    maximumValue?: number;
    checked?: boolean;
  }

  export interface ImageProps extends ViewProps {
    source: string;
  }

  export type Component<Props> = (props: Props) => JSX.Element;

  export const View: Component<ViewProps>;
  export const SafeAreaView: Component<ViewProps>;
  export const Text: Component<TextProps>;
  export const Pressable: Component<ViewProps>;
  export const Button: Component<ViewProps>;
  export const ScrollView: Component<ViewProps & {
    scrollX?: number;
    scrollY?: number;
  }>;
  export const Image: Component<ImageProps>;
  export const AnimatedImage: Component<ImageProps>;
  export const ActivityIndicator: Component<ValueProps>;
  export const ProgressBar: Component<ValueProps>;
  export const Slider: Component<ValueProps>;
  export const Switch: Component<ValueProps>;
  export const CheckBox: Component<ValueProps>;
  export const Checkbox: Component<ValueProps>;
  export const Modal: Component<ViewProps>;

  export const StyleSheet: {
    create<T extends Record<string, ViewStyle | TextStyle>>(styles: T): T;
  };
}

declare module "@crazypod/game2048" {
  export function move2048Left(): void;
  export function move2048Right(): void;
  export function move2048Up(): void;
  export function move2048Down(): void;
  export function reset2048(): void;
  export function tile2048Text(exponent: number): string;
}
