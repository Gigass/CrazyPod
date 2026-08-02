import React, { useState } from "react";
import {
  Pressable,
  SafeAreaView,
  StyleSheet,
  Text,
  View,
} from "react-native";

export default function App() {
  const [count, setCount] = useState(0);

  return (
    <View style={styles.screen}>
      <SafeAreaView style={styles.content}>
        <Text style={styles.eyebrow}>CRAZYPOD · NATIVE ABI 1</Text>
        <Text style={styles.title}>React Profile AOT</Text>
        <Text style={styles.copy}>This screen contains no JavaScript runtime.</Text>
        <Pressable
          style={styles.button}
          onPress={() => setCount((value) => value + 1)}
        >
          <Text style={styles.buttonLabel}>SELECT TO INCREMENT</Text>
        </Pressable>
        <Text style={styles.count}>{count}</Text>
      </SafeAreaView>
    </View>
  );
}

const styles = StyleSheet.create({
  screen: {
    width: 320,
    height: 240,
    backgroundColor: "#111318",
  },
  content: {
    width: 320,
    height: 208,
    paddingLeft: 18,
    paddingRight: 18,
    paddingTop: 8,
    alignItems: "center",
  },
  eyebrow: {
    width: 284,
    height: 16,
    color: "#ff9f43",
    fontSize: 12,
    textAlign: "center",
  },
  title: {
    width: 284,
    height: 32,
    marginTop: 8,
    color: "#ffffff",
    fontSize: 24,
    textAlign: "center",
  },
  copy: {
    width: 270,
    height: 38,
    marginTop: 6,
    color: "#aeb4c0",
    fontSize: 14,
    textAlign: "center",
  },
  button: {
    width: 236,
    height: 38,
    marginTop: 12,
    paddingTop: 10,
    borderRadius: 10,
    backgroundColor: "#ff9f43",
  },
  buttonLabel: {
    width: 236,
    height: 18,
    color: "#111318",
    fontSize: 14,
    textAlign: "center",
  },
  count: {
    width: 284,
    height: 40,
    marginTop: 10,
    color: "#ffffff",
    fontSize: 32,
    textAlign: "center",
  },
});
