import React, { useState } from "react";
import {
  Pressable,
  SafeAreaView,
  StyleSheet,
  Text,
  View,
} from "react-native";
import {
  move2048Down,
  move2048Left,
  move2048Right,
  move2048Up,
  reset2048,
  tile2048Text,
} from "@crazypod/game2048";

export default function App() {
  const [route, setRoute] = useState(0);
  const [cell0, setCell0] = useState(1);
  const [cell1, setCell1] = useState(0);
  const [cell2, setCell2] = useState(0);
  const [cell3, setCell3] = useState(0);
  const [cell4, setCell4] = useState(0);
  const [cell5, setCell5] = useState(1);
  const [cell6, setCell6] = useState(0);
  const [cell7, setCell7] = useState(0);
  const [cell8, setCell8] = useState(0);
  const [cell9, setCell9] = useState(0);
  const [cell10, setCell10] = useState(0);
  const [cell11, setCell11] = useState(0);
  const [cell12, setCell12] = useState(0);
  const [cell13, setCell13] = useState(0);
  const [cell14, setCell14] = useState(0);
  const [cell15, setCell15] = useState(0);
  const [score, setScore] = useState(0);
  const [moves, setMoves] = useState(0);
  const [seed, setSeed] = useState(1);
  const [won, setWon] = useState(0);
  const [gameOver, setGameOver] = useState(0);

  return (
    <View
      style={styles.screen}
      onLeft={() => move2048Left()}
      onRight={() => move2048Right()}
      onMenu={() => move2048Up()}
      onPlay={() => move2048Down()}
    >
      {route === 0 && (
        <SafeAreaView style={styles.home}>
          <Text style={styles.brand}>CRAZYPOD · NATIVE GAME</Text>
          <Text style={styles.logo}>2048</Text>
          <Text style={styles.tagline}>
            React TSX compiled to native ARM
          </Text>
          <Pressable
            style={styles.primaryButton}
            onPress={() => {
              reset2048();
              setRoute(1);
            }}
          >
            <Text style={styles.primaryLabel}>NEW GAME</Text>
          </Pressable>
          <Pressable
            style={styles.secondaryButton}
            onPress={() => setRoute(1)}
          >
            <Text style={styles.secondaryLabel}>CONTINUE</Text>
          </Pressable>
          <Text style={styles.help}>MENU · PLAY · LEFT · RIGHT</Text>
        </SafeAreaView>
      )}

      {route === 1 && (
        <SafeAreaView style={styles.game}>
          <View style={styles.stats}>
            <Text style={styles.statLabel}>SCORE</Text>
            <Text style={styles.statValue}>{score}</Text>
            <Text style={styles.statLabel}>MOVES</Text>
            <Text style={styles.statValue}>{moves}</Text>
          </View>
          <View style={styles.board}>
            <View style={styles.row}>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell0)}</Text></View>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell1)}</Text></View>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell2)}</Text></View>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell3)}</Text></View>
            </View>
            <View style={styles.row}>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell4)}</Text></View>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell5)}</Text></View>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell6)}</Text></View>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell7)}</Text></View>
            </View>
            <View style={styles.row}>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell8)}</Text></View>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell9)}</Text></View>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell10)}</Text></View>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell11)}</Text></View>
            </View>
            <View style={styles.row}>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell12)}</Text></View>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell13)}</Text></View>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell14)}</Text></View>
              <View style={styles.tile}><Text style={styles.tileText}>{tile2048Text(cell15)}</Text></View>
            </View>
          </View>
          <Text style={styles.directionHelp}>
            MENU UP · PLAY DOWN · PREV/NEXT LEFT/RIGHT
          </Text>
          <Text style={styles.gameState}>
            {gameOver ? "NO MOVES · START A NEW GAME" :
              won ? "2048 REACHED · KEEP GOING" : "PLAYING"}
          </Text>
          <Pressable
            style={styles.pauseButton}
            onPress={() => setRoute(2)}
          >
            <Text style={styles.pauseLabel}>PAUSE</Text>
          </Pressable>
        </SafeAreaView>
      )}

      {route === 2 && (
        <SafeAreaView style={styles.pause}>
          <Text style={styles.pauseTitle}>PAUSED</Text>
          <Text style={styles.pauseCopy}>
            The native state is already persisted.
          </Text>
          <Pressable
            style={styles.primaryButton}
            onPress={() => setRoute(1)}
          >
            <Text style={styles.primaryLabel}>CONTINUE</Text>
          </Pressable>
          <Pressable
            style={styles.secondaryButton}
            onPress={() => {
              reset2048();
              setRoute(1);
            }}
          >
            <Text style={styles.secondaryLabel}>RESTART</Text>
          </Pressable>
          <Pressable
            style={styles.secondaryButton}
            onPress={() => setRoute(0)}
          >
            <Text style={styles.secondaryLabel}>HOME</Text>
          </Pressable>
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
  home: {
    width: 320,
    height: 208,
    paddingLeft: 18,
    paddingRight: 18,
    paddingTop: 6,
    flexDirection: "column",
    alignItems: "center",
  },
  brand: {
    width: 284,
    height: 14,
    color: "#edc53f",
    fontSize: 12,
    textAlign: "center",
  },
  logo: {
    width: 284,
    height: 50,
    marginTop: 2,
    color: "#f5f5f7",
    fontSize: 40,
    textAlign: "center",
  },
  tagline: {
    width: 284,
    height: 16,
    color: "#9aa5b5",
    fontSize: 12,
    textAlign: "center",
  },
  primaryButton: {
    width: 210,
    height: 32,
    marginTop: 5,
    justifyContent: "center",
    alignItems: "center",
    backgroundColor: "#edc53f",
    borderRadius: 9,
  },
  primaryLabel: {
    width: 210,
    height: 18,
    color: "#15181e",
    fontSize: 15,
    textAlign: "center",
  },
  secondaryButton: {
    width: 210,
    height: 34,
    marginTop: 5,
    justifyContent: "center",
    alignItems: "center",
    backgroundColor: "#303846",
    borderColor: "#edc53f",
    borderWidth: 2,
    borderRadius: 9,
  },
  secondaryLabel: {
    width: 206,
    height: 18,
    color: "#f5f5f7",
    fontSize: 14,
    textAlign: "center",
  },
  help: {
    width: 284,
    height: 14,
    marginTop: 6,
    color: "#9aa5b5",
    fontSize: 12,
    textAlign: "center",
  },
  game: {
    width: 320,
    height: 208,
    paddingTop: 2,
    flexDirection: "column",
    alignItems: "center",
  },
  stats: {
    width: 200,
    height: 18,
    flexDirection: "row",
    justifyContent: "space-between",
    alignItems: "center",
  },
  statLabel: {
    width: 46,
    height: 14,
    color: "#9aa5b5",
    fontSize: 12,
    textAlign: "center",
  },
  statValue: {
    width: 44,
    height: 18,
    color: "#f5f5f7",
    fontSize: 15,
    textAlign: "center",
  },
  board: {
    width: 136,
    height: 136,
    padding: 4,
    backgroundColor: "#303846",
    borderRadius: 9,
    flexDirection: "column",
    justifyContent: "space-between",
  },
  row: {
    width: 128,
    height: 30,
    flexDirection: "row",
    justifyContent: "space-between",
  },
  tile: {
    width: 30,
    height: 30,
    paddingTop: 6,
    backgroundColor: "#4a5361",
    borderRadius: 5,
  },
  tileText: {
    width: 30,
    height: 18,
    color: "#f5f5f7",
    fontSize: 15,
    textAlign: "center",
  },
  directionHelp: {
    width: 300,
    height: 14,
    marginTop: 2,
    color: "#9aa5b5",
    fontSize: 12,
    textAlign: "center",
  },
  gameState: {
    width: 260,
    height: 14,
    color: "#edc53f",
    fontSize: 12,
    textAlign: "center",
  },
  pauseButton: {
    width: 76,
    height: 18,
    justifyContent: "center",
    alignItems: "center",
    backgroundColor: "#303846",
    borderRadius: 5,
  },
  pauseLabel: {
    width: 76,
    height: 14,
    color: "#f5f5f7",
    fontSize: 12,
    textAlign: "center",
  },
  pause: {
    width: 320,
    height: 208,
    paddingTop: 14,
    flexDirection: "column",
    alignItems: "center",
  },
  pauseTitle: {
    width: 284,
    height: 32,
    color: "#f5f5f7",
    fontSize: 28,
    textAlign: "center",
  },
  pauseCopy: {
    width: 270,
    height: 22,
    marginTop: 4,
    color: "#9aa5b5",
    fontSize: 12,
    textAlign: "center",
  },
});
