import React, {
  useFixedArray,
  useInterval,
  useOnMount,
  useState,
} from "react";
import {
  Image,
  Modal,
  StyleSheet,
  Text,
  View,
} from "react-native";
import {
  adjustVolume,
  MarqueeText,
  nextTrack,
  NowPlayingArtwork,
  playQueueItem,
  previousTrack,
  refreshLyricsWindow,
  refreshNowPlaying,
  refreshQueueItem,
  refreshQueueState,
  seekPlayback,
  setFavorite,
  setPlaybackMode,
  togglePlayback,
} from "@crazypod/now-playing";

function progressWidth(elapsed: number, length: number) {
  return length <= 0 ? 0
    : elapsed >= length ? 236
      : length > 9000000
        ? (elapsed / 1000) * 236 / (length / 1000)
        : elapsed * 236 / length;
}

function audioLevel(left: number, right: number) {
  return left > right ? left : right;
}

function cycleForward(value: number, steps: number, count: number) {
  return (value + steps) % count;
}

function cycleBackward(value: number, steps: number, count: number) {
  return (value + count - (steps % count)) % count;
}

function queueForward(value: number, steps: number, count: number) {
  return count <= 0 ? 0 : value + steps >= count ? count - 1 : value + steps;
}

function queueBackward(value: number, steps: number) {
  return value <= steps ? 0 : value - steps;
}

function seekForward(value: number, length: number, steps: number) {
  return length <= 0 ? 0
    : value + 5000 * steps >= length ? length - 1 : value + 5000 * steps;
}

function seekBackward(value: number, steps: number) {
  return value <= 5000 * steps ? 0 : value - 5000 * steps;
}

function playbackMode(repeat: number, shuffle: number) {
  return shuffle === 1 ? 1 : repeat === 1 ? 2 : repeat === 2 ? 3 : 0;
}

export default function App() {
  const [title, setTitle] = useState<string>("");
  const [artist, setArtist] = useState<string>("");
  const [album, setAlbum] = useState<string>("");
  const [info, setInfo] = useFixedArray(10, 0);
  const [result, setResult] = useState(0);
  const [panel, setPanel] = useState(0);
  const [action, setAction] = useState(0);
  const [mode, setMode] = useState(0);
  const [queueSelected, setQueueSelected] = useState(0);
  const [seekDraft, setSeekDraft] = useState(0);
  const [queueInfo, setQueueInfo] = useFixedArray(3, 0);

  const [queueTitle0, setQueueTitle0] = useState<string>("");
  const [queueArtist0, setQueueArtist0] = useState<string>("");
  const [queueAlbum0, setQueueAlbum0] = useState<string>("");
  const [queueItem0, setQueueItem0] = useFixedArray(3, 0);
  const [queueTitle1, setQueueTitle1] = useState<string>("");
  const [queueArtist1, setQueueArtist1] = useState<string>("");
  const [queueAlbum1, setQueueAlbum1] = useState<string>("");
  const [queueItem1, setQueueItem1] = useFixedArray(3, 0);
  const [queueTitle2, setQueueTitle2] = useState<string>("");
  const [queueArtist2, setQueueArtist2] = useState<string>("");
  const [queueAlbum2, setQueueAlbum2] = useState<string>("");
  const [queueItem2, setQueueItem2] = useFixedArray(3, 0);

  const [lyricPrevious, setLyricPrevious] = useState<string>("");
  const [lyricCurrent, setLyricCurrent] = useState<string>("");
  const [lyricNext, setLyricNext] = useState<string>("");
  const [lyricInfo, setLyricInfo] = useFixedArray(3, 0);

  useOnMount(() => {
    refreshNowPlaying(setTitle, setArtist, setAlbum, setInfo, setResult);
    refreshQueueState(setQueueInfo, setResult);
  });
  useInterval(100, () => {
    refreshNowPlaying(setTitle, setArtist, setAlbum, setInfo, setResult);
  });
  useInterval(250, () => {
    if (panel === 2) {
      refreshQueueState(setQueueInfo, setResult);
      if (queueInfo[1] > 0) {
        if (queueSelected > 0) {
          refreshQueueItem(
            queueSelected - 1,
            setQueueTitle0, setQueueArtist0, setQueueAlbum0,
            setQueueItem0, setResult,
          );
        } else {
          setQueueTitle0("");
          setQueueArtist0("");
          setQueueAlbum0("");
        }
        refreshQueueItem(
          queueSelected,
          setQueueTitle1, setQueueArtist1, setQueueAlbum1,
          setQueueItem1, setResult,
        );
        if (queueSelected + 1 < queueInfo[1]) {
          refreshQueueItem(
            queueSelected + 1,
            setQueueTitle2, setQueueArtist2, setQueueAlbum2,
            setQueueItem2, setResult,
          );
        } else {
          setQueueTitle2("");
          setQueueArtist2("");
          setQueueAlbum2("");
        }
      }
    }
    if (panel === 4) {
      refreshLyricsWindow(
        info[1], setLyricPrevious, setLyricCurrent, setLyricNext,
        setLyricInfo, setResult,
      );
    }
  });

  return (
    <View
      style={styles.screen}
      onWheelClockwise={(event) => {
        if (panel === 0) adjustVolume(event.steps, setResult);
        else if (panel === 1) setAction((value) => cycleForward(value, event.steps, 6));
        else if (panel === 2) setQueueSelected((value) =>
          queueForward(value, event.steps, queueInfo[1]));
        else if (panel === 3) setMode((value) => cycleForward(value, event.steps, 4));
        else if (panel === 5) setSeekDraft((value) =>
          seekForward(value, info[2], event.steps));
      }}
      onWheelCounterClockwise={(event) => {
        if (panel === 0) adjustVolume(-event.steps, setResult);
        else if (panel === 1) setAction((value) => cycleBackward(value, event.steps, 6));
        else if (panel === 2) setQueueSelected((value) =>
          queueBackward(value, event.steps));
        else if (panel === 3) setMode((value) => cycleBackward(value, event.steps, 4));
        else if (panel === 5) setSeekDraft((value) =>
          seekBackward(value, event.steps));
      }}
      onLeft={() => {
        if (panel === 0) previousTrack(setResult);
        else if (panel === 1) setAction((value) => value <= 0 ? 5 : value - 1);
        else if (panel === 2) setQueueSelected((value) =>
          value <= 0 ? 0 : value - 1);
        else if (panel === 3) setMode((value) => value <= 0 ? 3 : value - 1);
        else if (panel === 4) setPanel(1);
        else if (panel === 5) setSeekDraft((value) =>
          value <= 5000 ? 0 : value - 5000);
      }}
      onRight={() => {
        if (panel === 0) nextTrack(setResult);
        else if (panel === 1) setAction((value) => value >= 5 ? 0 : value + 1);
        else if (panel === 2) setQueueSelected((value) =>
          value + 1 >= queueInfo[1] ? value : value + 1);
        else if (panel === 3) setMode((value) => value >= 3 ? 0 : value + 1);
        else if (panel === 4) setPanel(1);
        else if (panel === 5) setSeekDraft((value) => seekForward(value, info[2], 1));
      }}
      onMenu={() => setPanel((value) => value === 1 ? 0 : 1)}
      onPlay={() => togglePlayback(setResult)}
      onPress={() => {
        if (panel === 0) {
          setAction(0);
          setPanel(1);
        } else if (panel === 1) {
          if (action === 0) {
            setQueueSelected(queueInfo[2]);
            setPanel(2);
          } else if (action === 1) {
            setFavorite(info[7] === 0, setResult);
          } else if (action === 2) {
            setMode(playbackMode(info[5], info[6]));
            setPanel(3);
          } else if (action === 3) {
            refreshLyricsWindow(
              info[1], setLyricPrevious, setLyricCurrent, setLyricNext,
              setLyricInfo, setResult,
            );
            setPanel(4);
          } else if (action === 4) {
            setSeekDraft(info[1]);
            setPanel(5);
          } else setPanel(0);
        } else if (panel === 2) {
          if (queueInfo[1] > 0) playQueueItem(queueSelected, setResult);
          setPanel(0);
        } else if (panel === 3) {
          if (mode === 0) setPlaybackMode("normal", setResult);
          else if (mode === 1) setPlaybackMode("shuffle", setResult);
          else if (mode === 2) setPlaybackMode("repeat-all", setResult);
          else setPlaybackMode("repeat-one", setResult);
          setPanel(1);
        } else if (panel === 4) setPanel(1);
        else if (panel === 5) {
          seekPlayback(seekDraft, setResult);
          setPanel(1);
        }
      }}
    >
      <Image style={styles.console} source="atelier-console" />
      <View style={styles.coverShadow} />
      <NowPlayingArtwork style={styles.artwork} variant={2} />
      <View style={styles.coverBezel} />

      <View style={styles.metadataPanel}>
        <View style={styles.pilotLight} />
        <Text style={styles.kicker}>ATELIER</Text>
        <MarqueeText style={styles.title}>
          {title === "" ? "No Track" : title}
        </MarqueeText>
        <MarqueeText style={styles.artist}>
          {artist === "" ? "Unknown Artist" : artist}
        </MarqueeText>
        <MarqueeText style={styles.album}>
          {album === "" ? "Local Library" : album}
        </MarqueeText>
        <Text style={styles.favorite}>{info[7] === 1 ? "FAV" : "---"}</Text>
        <Text style={styles.modeText}>
          {info[6] === 1 ? "SHUF" : info[5] === 2 ? "RPT1" :
            info[5] === 1 ? "RPTA" : "NORM"}
        </Text>
        <View style={styles.statusPlate}>
          <Text style={styles.statusText}>
            {info[3] === 2 ? "PLAY" : info[3] === 1 ? "PAUSE" : "STOP"}
          </Text>
        </View>
        <Text style={styles.volume}>VOL {info[4]}</Text>
      </View>

      <View style={styles.transportDeck}>
        <View style={styles.progressGroove}>
          <View style={[styles.progressFill, {
            width: progressWidth(info[1], info[2]),
          }]} />
        </View>
        <View style={styles.meterRow}>
          <View style={[styles.meterSegment, info[3] === 2 &&
            audioLevel(info[8], info[9]) >= 25 ? styles.meterOn : styles.meterOff]} />
          <View style={[styles.meterSegment, info[3] === 2 &&
            audioLevel(info[8], info[9]) >= 55 ? styles.meterOn : styles.meterOff]} />
          <View style={[styles.meterSegment, info[3] === 2 &&
            audioLevel(info[8], info[9]) >= 90 ? styles.meterOn : styles.meterOff]} />
          <View style={[styles.meterSegment, info[3] === 2 &&
            audioLevel(info[8], info[9]) >= 140 ? styles.meterOn : styles.meterOff]} />
          <View style={[styles.meterSegment, info[3] === 2 &&
            audioLevel(info[8], info[9]) >= 210 ? styles.meterOn : styles.meterOff]} />
          <View style={[styles.meterSegment, info[3] === 2 &&
            audioLevel(info[8], info[9]) >= 320 ? styles.meterOn : styles.meterOff]} />
          <View style={[styles.meterSegment, info[3] === 2 &&
            audioLevel(info[8], info[9]) >= 470 ? styles.meterOn : styles.meterOff]} />
          <View style={[styles.meterSegment, info[3] === 2 &&
            audioLevel(info[8], info[9]) >= 650 ? styles.meterHot : styles.meterHotOff]} />
          <View style={[styles.meterSegment, info[3] === 2 &&
            audioLevel(info[8], info[9]) >= 850 ? styles.meterHot : styles.meterHotOff]} />
        </View>
        <Text style={styles.previousHint}>PREV</Text>
        <Text style={styles.centerHint}>SELECT / OPTIONS</Text>
        <Text style={styles.nextHint}>NEXT</Text>
      </View>

      {panel === 1 ? (
        <Modal style={styles.overlay}>
          <View style={styles.panel}>
            <Text style={styles.panelTitle}>NOW PLAYING OPTIONS</Text>
            <View style={action === 0 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 0 ? styles.menuTextOn : styles.menuTextOff}>QUEUE</Text>
            </View>
            <View style={action === 1 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 1 ? styles.menuTextOn : styles.menuTextOff}>
                {info[7] === 1 ? "REMOVE FAVORITE" : "ADD FAVORITE"}
              </Text>
            </View>
            <View style={action === 2 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 2 ? styles.menuTextOn : styles.menuTextOff}>PLAYBACK MODE</Text>
            </View>
            <View style={action === 3 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 3 ? styles.menuTextOn : styles.menuTextOff}>LYRICS</Text>
            </View>
            <View style={action === 4 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 4 ? styles.menuTextOn : styles.menuTextOff}>SEEK</Text>
            </View>
            <View style={action === 5 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 5 ? styles.menuTextOn : styles.menuTextOff}>CLOSE</Text>
            </View>
          </View>
        </Modal>
      ) : <View style={styles.hidden} />}

      {panel === 2 ? (
        <Modal style={styles.overlay}>
          <View style={styles.panel}>
            <Text style={styles.panelTitle}>
              QUEUE {queueInfo[1] > 0 ? queueSelected + 1 : 0}/{queueInfo[1]}
            </Text>
            <View style={styles.queueRow}>
              <MarqueeText style={styles.queueTitle}>{queueTitle0}</MarqueeText>
              <MarqueeText style={styles.queueArtist}>{queueArtist0}</MarqueeText>
            </View>
            <View style={styles.queueRowOn}>
              <MarqueeText style={styles.queueTitleOn}>{queueTitle1}</MarqueeText>
              <MarqueeText style={styles.queueArtistOn}>{queueArtist1}</MarqueeText>
            </View>
            <View style={styles.queueRow}>
              <MarqueeText style={styles.queueTitle}>{queueTitle2}</MarqueeText>
              <MarqueeText style={styles.queueArtist}>{queueArtist2}</MarqueeText>
            </View>
            <Text style={styles.panelHint}>SELECT PLAYS</Text>
          </View>
        </Modal>
      ) : <View style={styles.hidden} />}

      {panel === 3 ? (
        <Modal style={styles.overlay}>
          <View style={styles.panelSmall}>
            <Text style={styles.panelTitle}>PLAYBACK MODE</Text>
            <View style={mode === 0 ? styles.menuOn : styles.menuOff}>
              <Text style={mode === 0 ? styles.menuTextOn : styles.menuTextOff}>NORMAL</Text>
            </View>
            <View style={mode === 1 ? styles.menuOn : styles.menuOff}>
              <Text style={mode === 1 ? styles.menuTextOn : styles.menuTextOff}>SHUFFLE</Text>
            </View>
            <View style={mode === 2 ? styles.menuOn : styles.menuOff}>
              <Text style={mode === 2 ? styles.menuTextOn : styles.menuTextOff}>REPEAT ALL</Text>
            </View>
            <View style={mode === 3 ? styles.menuOn : styles.menuOff}>
              <Text style={mode === 3 ? styles.menuTextOn : styles.menuTextOff}>REPEAT ONE</Text>
            </View>
          </View>
        </Modal>
      ) : <View style={styles.hidden} />}

      {panel === 4 ? (
        <Modal style={styles.overlay}>
          <View style={styles.lyricsPanel}>
            <Text style={styles.panelTitle}>LYRICS</Text>
            <MarqueeText style={styles.lyricDim}>
              {lyricInfo[0] === 1 ? lyricPrevious : ""}
            </MarqueeText>
            <MarqueeText style={styles.lyricCurrent}>
              {lyricInfo[0] === 1 ? lyricCurrent : "No lyrics available"}
            </MarqueeText>
            <MarqueeText style={styles.lyricDim}>
              {lyricInfo[0] === 1 ? lyricNext : ""}
            </MarqueeText>
            <Text style={styles.panelHint}>SELECT RETURNS</Text>
          </View>
        </Modal>
      ) : <View style={styles.hidden} />}

      {panel === 5 ? (
        <Modal style={styles.overlay}>
          <View style={styles.panelSmall}>
            <Text style={styles.panelTitle}>SEEK</Text>
            <View style={styles.seekTrack}>
              <View style={[styles.seekFill, {
                width: progressWidth(seekDraft, info[2]) * 180 / 236,
              }]} />
            </View>
            <Text style={styles.seekValue}>{seekDraft / 1000}s / {info[2] / 1000}s</Text>
            <Text style={styles.panelHint}>SCROLL 5s / SELECT APPLIES</Text>
          </View>
        </Modal>
      ) : <View style={styles.hidden} />}
    </View>
  );
}

const styles = StyleSheet.create({
  screen: { width: 320, height: 240, backgroundColor: "#090806" },
  console: { position: "absolute", left: 0, top: 0, width: 320, height: 240 },
  coverShadow: { position: "absolute", left: 25, top: 48, width: 114, height: 114,
    backgroundColor: "#070605", borderColor: "#050403", borderWidth: 2,
    borderRadius: 14, shadowColor: "#000000", shadowRadius: 8, opacity: 1 },
  artwork: { position: "absolute", left: 30, top: 53, width: 104, height: 104,
    borderRadius: 10 },
  coverBezel: { position: "absolute", left: 28, top: 51, width: 108, height: 108,
    borderColor: "#c58a4e", borderWidth: 1, borderRadius: 12, opacity: 0.72 },
  metadataPanel: { position: "absolute", left: 145, top: 48, width: 147, height: 114,
    backgroundColor: "#11100e", borderColor: "#514333", borderWidth: 1,
    borderRadius: 9, shadowColor: "#000000", shadowRadius: 5, opacity: 1 },
  pilotLight: { position: "absolute", left: 132, top: 9, width: 5, height: 5,
    backgroundColor: "#f4b45f", borderColor: "#6c3e18", borderWidth: 1,
    borderRadius: 3, shadowColor: "#f4a340", shadowRadius: 4 },
  kicker: { position: "absolute", left: 10, top: 8, width: 78, height: 13,
    color: "#d9a15d", fontSize: 8, fontFamily: "system" },
  title: { position: "absolute", left: 10, top: 27, width: 127, height: 15,
    color: "#f6ecd8", fontSize: 14, fontFamily: "serif", fontWeight: "700" },
  artist: { position: "absolute", left: 10, top: 47, width: 127, height: 14,
    color: "#d0b99d", fontSize: 12 },
  album: { position: "absolute", left: 10, top: 65, width: 127, height: 14,
    color: "#8f8376", fontSize: 10 },
  favorite: { position: "absolute", left: 10, top: 86, width: 32, height: 12,
    color: "#ff657f", fontSize: 8 },
  modeText: { position: "absolute", left: 45, top: 86, width: 34, height: 12,
    color: "#55d6e7", fontSize: 8 },
  statusPlate: { position: "absolute", left: 88, top: 6, width: 40, height: 11,
    backgroundColor: "#7b441d", borderColor: "#d59b59", borderWidth: 1,
    borderRadius: 3 },
  statusText: { width: 38, height: 9, color: "#ffe1ac", fontSize: 7,
    textAlign: "center" },
  volume: { position: "absolute", left: 65, top: 101, width: 67, height: 9,
    color: "#897665", fontSize: 7, textAlign: "right" },
  transportDeck: { position: "absolute", left: 27, top: 174, width: 266, height: 49,
    backgroundColor: "#0b0a09", borderColor: "#594a39", borderWidth: 1,
    borderRadius: 8, shadowColor: "#000000", shadowRadius: 5, opacity: 1 },
  progressGroove: { position: "absolute", left: 14, top: 9, width: 236, height: 4,
    backgroundColor: "#31291f", borderRadius: 2 },
  progressFill: { height: 4, backgroundColor: "#d89142", borderRadius: 2 },
  meterRow: { position: "absolute", left: 14, top: 18, width: 236, height: 5,
    flexDirection: "row" },
  meterSegment: { width: 23, height: 4, marginRight: 3, borderRadius: 2 },
  meterOn: { backgroundColor: "#d89142", opacity: 1 },
  meterOff: { backgroundColor: "#3a2e23", opacity: 0.58 },
  meterHot: { backgroundColor: "#e9683e", opacity: 1 },
  meterHotOff: { backgroundColor: "#4d2420", opacity: 0.58 },
  previousHint: { position: "absolute", left: 14, top: 31, width: 50, height: 9,
    color: "#8d7962", fontSize: 7 },
  centerHint: { position: "absolute", left: 70, top: 31, width: 126, height: 9,
    color: "#bca384", fontSize: 7, textAlign: "center" },
  nextHint: { position: "absolute", left: 201, top: 31, width: 49, height: 9,
    color: "#8d7962", fontSize: 7, textAlign: "right" },
  hidden: { position: "absolute", left: 0, top: 0, width: 0, height: 0,
    backgroundColor: "#000000", opacity: 0 },
  overlay: { backgroundColor: "#050403", opacity: 1 },
  panel: { position: "absolute", left: 35, top: 38, width: 250, height: 172,
    backgroundColor: "#15120f", borderColor: "#d89142", borderWidth: 1,
    borderRadius: 10, padding: 12 },
  panelSmall: { position: "absolute", left: 35, top: 58, width: 250, height: 132,
    backgroundColor: "#15120f", borderColor: "#d89142", borderWidth: 1,
    borderRadius: 10, padding: 12 },
  lyricsPanel: { position: "absolute", left: 24, top: 54, width: 272, height: 138,
    backgroundColor: "#15120f", borderColor: "#d89142", borderWidth: 1,
    borderRadius: 10, padding: 12 },
  panelTitle: { width: 226, height: 14, color: "#d9a15d", fontSize: 10,
    fontFamily: "system", textAlign: "center", marginBottom: 5 },
  menuOn: { width: 226, height: 21, backgroundColor: "#d89142",
    borderRadius: 4, marginBottom: 2, justifyContent: "center" },
  menuOff: { width: 226, height: 21, backgroundColor: "#231c16",
    borderRadius: 4, marginBottom: 2, justifyContent: "center" },
  menuTextOn: { width: 226, height: 12, color: "#130f0b", fontSize: 10,
    textAlign: "center" },
  menuTextOff: { width: 226, height: 12, color: "#d8c6aa", fontSize: 10,
    textAlign: "center" },
  queueRow: { width: 226, height: 38, backgroundColor: "#211b16",
    borderColor: "#3e3025", borderWidth: 1, borderRadius: 5, marginBottom: 3 },
  queueRowOn: { width: 226, height: 38, backgroundColor: "#6f4828",
    borderColor: "#d89142", borderWidth: 1, borderRadius: 5, marginBottom: 3 },
  queueTitle: { width: 212, height: 14, color: "#cdbda4", fontSize: 10,
    marginLeft: 7, marginTop: 3 },
  queueArtist: { width: 212, height: 12, color: "#817566", fontSize: 8,
    marginLeft: 7, marginTop: 1 },
  queueTitleOn: { width: 212, height: 14, color: "#fff1d4", fontSize: 10,
    marginLeft: 7, marginTop: 3 },
  queueArtistOn: { width: 212, height: 12, color: "#e2bf91", fontSize: 8,
    marginLeft: 7, marginTop: 1 },
  panelHint: { width: 226, height: 12, color: "#867665", fontSize: 8,
    textAlign: "center", marginTop: 5 },
  lyricDim: { width: 248, height: 18, color: "#867665", fontSize: 10,
    textAlign: "center", marginBottom: 7 },
  lyricCurrent: { width: 248, height: 22, color: "#fff0d0", fontSize: 12,
    textAlign: "center", marginBottom: 7 },
  seekTrack: { width: 180, height: 10, backgroundColor: "#30271e",
    borderColor: "#d89142", borderWidth: 1, borderRadius: 5,
    marginLeft: 23, marginTop: 18 },
  seekFill: { height: 8, backgroundColor: "#d89142", borderRadius: 4 },
  seekValue: { width: 226, height: 18, color: "#f4dfbc", fontSize: 10,
    textAlign: "center", marginTop: 12 },
});
