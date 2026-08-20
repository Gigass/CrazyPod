import React, { useState } from "react";
import {
  ActivityIndicator,
  AnimatedImage,
  CheckBox,
  Image,
  Modal,
  Pressable,
  ProgressBar,
  SafeAreaView,
  Slider,
  StyleSheet,
  Switch,
  Text,
  View,
} from "react-native";

export default function App() {
  const [route, setRoute] = useState(0);
  const [value, setValue] = useState(42);
  const [enabled, setEnabled] = useState(1);
  const [events, setEvents] = useState(0);
  const [modal, setModal] = useState(0);

  return (
    <View
      style={styles.screen}
      onMenu={() => {
        setModal(0);
        setRoute(0);
      }}
    >
      {route === 0 && (
        <SafeAreaView style={styles.page}>
          <Text style={styles.eyebrow}>CRAZYPOD · NATIVE ABI 1</Text>
          <Text style={styles.title}>Capability Lab</Text>
          <Text style={styles.subtitle}>
            React TSX · AOT C · host-owned LVGL
          </Text>
          <Pressable style={styles.menuButton} onPress={() => setRoute(1)}>
            <Text style={styles.menuLabel}>01  CONTROLS & EVENTS</Text>
          </Pressable>
          <Pressable style={styles.menuButton} onPress={() => setRoute(2)}>
            <Text style={styles.menuLabel}>02  ASSETS & LAYOUT</Text>
          </Pressable>
          <Pressable style={styles.menuButton} onPress={() => setRoute(3)}>
            <Text style={styles.menuLabel}>03  LIFECYCLE MODAL</Text>
          </Pressable>
          <Text style={styles.footer}>MENU returns here from every page</Text>
        </SafeAreaView>
      )}

      {route === 1 && (
        <SafeAreaView style={styles.page}>
          <Text style={styles.title}>Native Controls</Text>
          <ProgressBar
            style={styles.progress}
            minimumValue={0}
            maximumValue={100}
            value={value}
          />
          <View style={styles.controlRow}>
            <ActivityIndicator
              style={styles.arc}
              minimumValue={0}
              maximumValue={100}
              value={value}
            />
            <Switch
              style={styles.toggle}
              checked={enabled !== 0}
              onValueChange={(event) => setEnabled(event.value)}
            />
            <CheckBox
              style={styles.checkbox}
              checked={enabled !== 0}
              onValueChange={(event) => setEnabled(event.value)}
            />
          </View>
          <Slider
            style={styles.slider}
            minimumValue={0}
            maximumValue={100}
            value={value}
            onFocus={() => setEvents((previous) => previous + 1)}
            onBlur={() => setEvents((previous) => previous + 1)}
            onLongPress={() => setEvents((previous) => previous + 1)}
            onValueChange={(event) => setValue(event.value)}
          />
          <View style={styles.infoRow}>
            <Text style={styles.readout}>{value}</Text>
            <Text style={styles.status}>
              {enabled ? "STATE ON" : "STATE OFF"}
            </Text>
            <Text style={styles.eventCount}>{events}</Text>
          </View>
          <Pressable style={styles.backButton} onPress={() => setRoute(0)}>
            <Text style={styles.buttonLabel}>BACK</Text>
          </Pressable>
        </SafeAreaView>
      )}

      {route === 2 && (
        <SafeAreaView style={styles.page}>
          <Text style={styles.title}>Assets & Flex Layout</Text>
          <View style={styles.assetCard}>
            <Image style={styles.image} source="logo" />
            <View style={styles.assetCopy}>
              <Text style={styles.cardTitle}>CPK5 RESOURCE</Text>
              <Text style={styles.cardBody}>RGB565 decoded by the host</Text>
              <Text style={styles.cardBody}>No JS · no virtual DOM</Text>
            </View>
          </View>
          <View style={styles.row}>
            <AnimatedImage style={styles.animation} source="pulse" />
            <View style={styles.greenBlock} />
            <View style={styles.orangeBlock} />
            <View style={styles.blueBlock} />
          </View>
          <Pressable style={styles.backButton} onPress={() => setRoute(0)}>
            <Text style={styles.buttonLabel}>BACK</Text>
          </Pressable>
        </SafeAreaView>
      )}

      {route === 3 && (
        <SafeAreaView style={styles.page}>
          <Text style={styles.title}>Lifecycle & Modal</Text>
          <Text style={styles.modalDescription}>
            Open and close this host-owned subtree repeatedly.
          </Text>
          <Pressable style={styles.menuButton} onPress={() => setModal(1)}>
            <Text style={styles.menuLabel}>OPEN MODAL</Text>
          </Pressable>
          <Pressable style={styles.backButton} onPress={() => setRoute(0)}>
            <Text style={styles.buttonLabel}>BACK</Text>
          </Pressable>
          {modal === 1 && (
            <Modal style={styles.modalBackdrop}>
              <View style={styles.modalCard}>
                <Text style={styles.cardTitle}>HOST-OWNED OVERLAY</Text>
                <Text style={styles.cardBody}>
                  Removing it releases every LVGL child.
                </Text>
                <Pressable
                  style={styles.modalClose}
                  onPress={() => setModal(0)}
                >
                  <Text style={styles.buttonLabel}>CLOSE</Text>
                </Pressable>
              </View>
            </Modal>
          )}
        </SafeAreaView>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  screen: {
    width: 320,
    height: 240,
    backgroundColor: "#15181e",
  },
  page: {
    width: 320,
    height: 208,
    paddingLeft: 16,
    paddingRight: 16,
    paddingTop: 4,
    flexDirection: "column",
    alignItems: "center",
  },
  eyebrow: {
    width: 288,
    height: 14,
    color: "#3dc483",
    fontSize: 12,
    textAlign: "center",
  },
  title: {
    width: 288,
    height: 28,
    marginTop: 2,
    color: "#f5f5f7",
    fontSize: 24,
    textAlign: "center",
  },
  subtitle: {
    width: 288,
    height: 18,
    marginTop: 1,
    marginBottom: 4,
    color: "#9aa5b5",
    fontSize: 12,
    textAlign: "center",
  },
  menuButton: {
    width: 284,
    height: 30,
    marginTop: 4,
    justifyContent: "center",
    alignItems: "center",
    backgroundColor: "#232934",
    borderColor: "#465164",
    borderWidth: 2,
    borderRadius: 8,
  },
  menuLabel: {
    width: 280,
    height: 16,
    color: "#f5f5f7",
    fontSize: 14,
    textAlign: "center",
  },
  footer: {
    width: 288,
    height: 16,
    marginTop: 4,
    color: "#9aa5b5",
    fontSize: 12,
    textAlign: "center",
  },
  progress: {
    width: 190,
    height: 10,
    marginTop: 4,
  },
  controlRow: {
    width: 250,
    height: 40,
    marginTop: 4,
    flexDirection: "row",
    justifyContent: "space-around",
    alignItems: "center",
  },
  arc: {
    width: 36,
    height: 36,
  },
  slider: {
    width: 250,
    height: 18,
    marginTop: 4,
    borderColor: "#3dc483",
    borderWidth: 2,
  },
  toggle: {
    width: 44,
    height: 22,
  },
  checkbox: {
    width: 28,
    height: 22,
  },
  infoRow: {
    width: 250,
    height: 26,
    marginTop: 4,
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
  },
  readout: {
    width: 62,
    height: 26,
    color: "#ff9f43",
    fontSize: 24,
    textAlign: "center",
  },
  status: {
    width: 112,
    height: 16,
    color: "#f5f5f7",
    fontSize: 14,
    textAlign: "center",
  },
  eventCount: {
    width: 62,
    height: 16,
    color: "#9aa5b5",
    fontSize: 12,
    textAlign: "center",
  },
  backButton: {
    width: 96,
    height: 26,
    marginTop: 4,
    justifyContent: "center",
    alignItems: "center",
    backgroundColor: "#303846",
    borderColor: "#3dc483",
    borderWidth: 2,
    borderRadius: 8,
  },
  buttonLabel: {
    width: 92,
    height: 16,
    color: "#f5f5f7",
    fontSize: 14,
    textAlign: "center",
  },
  assetCard: {
    width: 286,
    height: 72,
    marginTop: 6,
    padding: 4,
    backgroundColor: "#232934",
    borderRadius: 10,
    flexDirection: "row",
    alignItems: "center",
  },
  image: {
    width: 60,
    height: 60,
  },
  assetCopy: {
    width: 210,
    height: 60,
    marginLeft: 8,
    flexDirection: "column",
    justifyContent: "center",
  },
  cardTitle: {
    width: 190,
    height: 18,
    color: "#3dc483",
    fontSize: 15,
    textAlign: "center",
  },
  cardBody: {
    width: 190,
    height: 16,
    color: "#f5f5f7",
    fontSize: 12,
    textAlign: "center",
  },
  row: {
    width: 286,
    height: 38,
    marginTop: 6,
    flexDirection: "row",
    justifyContent: "space-between",
  },
  greenBlock: {
    width: 70,
    height: 38,
    backgroundColor: "#3dc483",
    borderRadius: 8,
  },
  orangeBlock: {
    width: 70,
    height: 38,
    backgroundColor: "#ff9f43",
    borderRadius: 8,
  },
  blueBlock: {
    width: 70,
    height: 38,
    backgroundColor: "#5ba9ff",
    borderRadius: 8,
  },
  animation: {
    width: 24,
    height: 24,
    marginTop: 7,
  },
  modalDescription: {
    width: 270,
    height: 34,
    marginTop: 10,
    color: "#9aa5b5",
    fontSize: 14,
    textAlign: "center",
  },
  modalBackdrop: {
    position: "absolute",
    left: 0,
    top: 0,
    width: 320,
    height: 240,
    justifyContent: "center",
    alignItems: "center",
    backgroundColor: "#111318",
  },
  modalCard: {
    position: "absolute",
    left: 53,
    top: 72,
    width: 214,
    height: 96,
    padding: 12,
    backgroundColor: "#303846",
    borderColor: "#3dc483",
    borderWidth: 2,
    borderRadius: 12,
    flexDirection: "column",
    alignItems: "center",
  },
  modalClose: {
    width: 96,
    height: 26,
    marginTop: 12,
    justifyContent: "center",
    alignItems: "center",
    backgroundColor: "#232934",
    borderRadius: 8,
  },
});
