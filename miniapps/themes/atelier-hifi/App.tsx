import React, {
  boundedText,
  callHostService,
  useEffect,
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
  refreshLyricsContext,
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

function volumePercent(value: number) {
  return value <= -60 ? 0 : value >= 12 ? 100 : (value + 60) * 100 / 72;
}

function audioLevel(left: number, right: number) {
  return left > right ? left : right;
}

function cycleForward(value: number, steps: number, count: number) {
  return count <= 0 ? 0 : value + steps >= count ? count - 1 : value + steps;
}

function cycleBackward(value: number, steps: number, count: number) {
  return count <= 0 || value <= steps ? 0 : value - steps;
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
  const [systemRequest, setSystemRequest] = useFixedArray(1, 0);
  const [system, setSystem] = useFixedArray(6, 0);
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

  const [lyricPrevious, setLyricPrevious] = useState(boundedText(768, ""));
  const [lyricCurrent, setLyricCurrent] = useState(boundedText(768, ""));
  const [lyricNext, setLyricNext] = useState(boundedText(768, ""));
  const [lyricInfo, setLyricInfo] = useFixedArray(6, 0);
  const [lyricAnchor, setLyricAnchor] = useState(-1);
  const [lyricBrowseTicks, setLyricBrowseTicks] = useState(0);
  const [lyricsEnabled, setLyricsEnabled] = useState(0);
  const [lyricFrame, setLyricFrame] = useState(9);
  const [meterLevel, setMeterLevel] = useState(0);

  useOnMount(() => {
    refreshNowPlaying(setTitle, setArtist, setAlbum, setInfo, setResult);
    refreshQueueState(setQueueInfo, setResult);
    callHostService(0, 1, systemRequest, setSystem, setResult);
  });
  useInterval(100, () => {
    refreshNowPlaying(setTitle, setArtist, setAlbum, setInfo, setResult);
  });
  useInterval(50, () => {
    if (system[5] === 1) {
      setLyricFrame(9);
      setMeterLevel(info[3] === 2 ? audioLevel(info[8], info[9]) : 0);
    } else {
      setLyricFrame((value) => value >= 9 ? 9 : value + 1);
      setMeterLevel((value) => info[3] !== 2
        ? value <= 70 ? 0 : value - 70
        : audioLevel(info[8], info[9]) > value
          ? value + 180 >= audioLevel(info[8], info[9])
            ? audioLevel(info[8], info[9]) : value + 180
          : value <= 55 ? 0 : value - 55);
    }
  });
  useEffect(() => {
    setLyricFrame(0);
  }, [lyricInfo[2]]);
  useInterval(250, () => {
    callHostService(0, 1, systemRequest, setSystem, setResult);
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
    if (lyricsEnabled === 1) {
      if (lyricAnchor >= 0) {
        if (lyricBrowseTicks <= 1) {
          setLyricBrowseTicks(0);
          setLyricAnchor(-1);
        } else setLyricBrowseTicks((value) => value - 1);
      }
      refreshLyricsContext(
        info[1], lyricAnchor, setLyricPrevious, setLyricCurrent, setLyricNext,
        setLyricInfo, setResult,
      );
    }
  });

  return (
    <View
      style={styles.screen}
      onWheelClockwise={(event) => {
        if (panel === 0 && lyricsEnabled === 1 && lyricInfo[3] > 0) {
          setLyricAnchor((value) => cycleForward(
            value < 0 ? lyricInfo[2] : value, event.steps, lyricInfo[3]));
          setLyricBrowseTicks(12);
        }
        else if (panel === 0) adjustVolume(event.steps, setResult);
        else if (panel === 1) setAction((value) => cycleForward(value, event.steps, 5));
        else if (panel === 2) setQueueSelected((value) =>
          queueForward(value, event.steps, queueInfo[1]));
        else if (panel === 3) setMode((value) => cycleForward(value, event.steps, 4));
        else if (panel === 5) setSeekDraft((value) =>
          seekForward(value, info[2], event.steps));
      }}
      onWheelCounterClockwise={(event) => {
        if (panel === 0 && lyricsEnabled === 1 && lyricInfo[3] > 0) {
          setLyricAnchor((value) => cycleBackward(
            value < 0 ? lyricInfo[2] : value, event.steps, lyricInfo[3]));
          setLyricBrowseTicks(12);
        }
        else if (panel === 0) adjustVolume(-event.steps, setResult);
        else if (panel === 1) setAction((value) => cycleBackward(value, event.steps, 5));
        else if (panel === 2) setQueueSelected((value) =>
          queueBackward(value, event.steps));
        else if (panel === 3) setMode((value) => cycleBackward(value, event.steps, 4));
        else if (panel === 5) setSeekDraft((value) =>
          seekBackward(value, event.steps));
      }}
      onLeft={() => {
        if (panel === 0) previousTrack(setResult);
        else if (panel === 1) setAction((value) => cycleBackward(value, 1, 5));
        else if (panel === 2) setQueueSelected((value) =>
          value <= 0 ? 0 : value - 1);
        else if (panel === 3) setMode((value) => cycleBackward(value, 1, 4));
        else if (panel === 5) setSeekDraft((value) =>
          value <= 5000 ? 0 : value - 5000);
      }}
      onRight={() => {
        if (panel === 0) nextTrack(setResult);
        else if (panel === 1) setAction((value) => cycleForward(value, 1, 5));
        else if (panel === 2) setQueueSelected((value) =>
          value + 1 >= queueInfo[1] ? value : value + 1);
        else if (panel === 3) setMode((value) => cycleForward(value, 1, 4));
        else if (panel === 5) setSeekDraft((value) => seekForward(value, info[2], 1));
      }}
      onMenu={() => {
        if (lyricsEnabled === 1) setLyricsEnabled(0);
        else setPanel((value) => value === 1 ? 0 : 1);
      }}
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
            setLyricAnchor(-1);
            setLyricBrowseTicks(0);
            refreshLyricsContext(
              info[1], -1, setLyricPrevious, setLyricCurrent, setLyricNext,
              setLyricInfo, setResult,
            );
            setLyricsEnabled((value) => value === 0 ? 1 : 0);
            setPanel(0);
          } else {
            setSeekDraft(info[1]);
            setPanel(5);
          }
        } else if (panel === 2) {
          if (queueInfo[1] > 0) playQueueItem(queueSelected, setResult);
          setPanel(0);
        } else if (panel === 3) {
          if (mode === 0) setPlaybackMode("normal", setResult);
          else if (mode === 1) setPlaybackMode("shuffle", setResult);
          else if (mode === 2) setPlaybackMode("repeat-all", setResult);
          else setPlaybackMode("repeat-one", setResult);
          setPanel(1);
        } else if (panel === 5) {
          seekPlayback(seekDraft, setResult);
          setPanel(1);
        }
      }}
    >
      <Image style={styles.console} source="atelier-console" />
      <View style={styles.topRail} />
      <Text style={styles.topTime}>{system[0]}:{system[1]}{system[2]}</Text>
      <Text style={styles.topState}>{info[3] === 2 ? "PLAYING" : info[3] === 1 ? "PAUSED" : "STOPPED"}</Text>
      <Text style={styles.topBattery}>{system[4] === 1 ? "CHG" : "BAT"} {system[3]}%</Text>
      <View style={styles.coverShadow} />
      <NowPlayingArtwork style={styles.artwork} variant={2} />
      <View style={styles.coverBezel} />

      <View style={styles.metadataPanel}>
        <View style={[styles.pilotLight,
          info[3] === 2 ? styles.pilotLightOn : styles.pilotLightOff]} />
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
        <Text style={styles.favorite}>{info[7] === 1 ? "MEM ●" : "MEM ○"}</Text>
        <Text style={styles.modeText}>
          {info[6] === 1 ? "MIX" : info[5] === 2 ? "ONE" :
            info[5] === 1 ? "LOOP" : "DIR"}
        </Text>
        <View style={styles.statusPlate}>
          <Text style={styles.statusText}>
            {info[3] === 2 ? "PLAY" : info[3] === 1 ? "PAUSE" : "STOP"}
          </Text>
        </View>
        <Text style={styles.volume}>GAIN {volumePercent(info[4])}%</Text>
      </View>

      <View style={styles.transportDeck}>
        <View style={styles.progressGroove}>
          <View style={[styles.progressFill, {
            width: progressWidth(info[1], info[2]),
          }]} />
        </View>
        <View style={styles.meterRow}>
          <View style={[styles.meterSegment, meterLevel >= 25 ? styles.meterOn : styles.meterOff]} />
          <View style={[styles.meterSegment, meterLevel >= 55 ? styles.meterOn : styles.meterOff]} />
          <View style={[styles.meterSegment, meterLevel >= 90 ? styles.meterOn : styles.meterOff]} />
          <View style={[styles.meterSegment, meterLevel >= 140 ? styles.meterOn : styles.meterOff]} />
          <View style={[styles.meterSegment, meterLevel >= 210 ? styles.meterOn : styles.meterOff]} />
          <View style={[styles.meterSegment, meterLevel >= 320 ? styles.meterOn : styles.meterOff]} />
          <View style={[styles.meterSegment, meterLevel >= 470 ? styles.meterOn : styles.meterOff]} />
          <View style={[styles.meterSegment, meterLevel >= 650 ? styles.meterHot : styles.meterHotOff]} />
          <View style={[styles.meterSegment, meterLevel >= 850 ? styles.meterHot : styles.meterHotOff]} />
        </View>
        <Text style={styles.previousHint}>PREV</Text>
        <Text style={styles.centerHint}>SELECT / OPTIONS</Text>
        <Text style={styles.nextHint}>NEXT</Text>
      </View>

      {lyricsEnabled === 1 ? <Modal style={styles.lyricsOverlay}>
        <View style={styles.lyricsPanel}>
          <Text style={styles.lyricsHeader}>LYRICS MONITOR · {lyricInfo[5] === 1 ? "SYNC" : "TEXT"}</Text>
          <View style={styles.lyricsRows} adaptiveLyrics={true}>
            <Text style={styles.lyricDim}>{lyricInfo[0] === 1 ? lyricPrevious : ""}</Text>
            <Text style={styles.lyricCurrent}>{lyricInfo[0] === 1 ? lyricCurrent :
              lyricInfo[4] === 3 ? "NO LYRICS FILE" : lyricInfo[4] === 4 ?
                "INVALID LYRICS FILE" : lyricInfo[4] === 5 ?
                  "LYRICS FILE TOO LARGE" : "NO LYRICS AVAILABLE"}</Text>
            <Text style={styles.lyricDim}>{lyricInfo[0] === 1 ? lyricNext : ""}</Text>
          </View>
          <Text style={styles.lyricsCloseHint}>WHEEL BROWSE · {lyricInfo[2] + 1}/{lyricInfo[3]} · MENU CLOSE</Text>
        </View>
      </Modal> : <View style={styles.hidden} />}

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
              <Text style={action === 3 ? styles.menuTextOn : styles.menuTextOff}>{lyricsEnabled === 1 ? "HIDE LYRICS" : "SHOW LYRICS"}</Text>
            </View>
            <View style={action === 4 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 4 ? styles.menuTextOn : styles.menuTextOff}>SEEK</Text>
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
              <MarqueeText style={styles.queueTitleOn}>{queueInfo[1] > 0 ? queueTitle1 : "QUEUE IS EMPTY"}</MarqueeText>
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
  topRail: { position: "absolute", left: 20, top: 5, width: 280, height: 18,
    backgroundColor: "#16110d", borderColor: "#6f5132", borderWidth: 1,
    borderRadius: 4, opacity: 0.94 },
  topTime: { position: "absolute", left: 29, top: 8, width: 52, height: 13,
    color: "#d9a15d", fontSize: 11 },
  topState: { position: "absolute", left: 112, top: 8, width: 96, height: 13,
    color: "#ffe1ac", fontSize: 11, textAlign: "center" },
  topBattery: { position: "absolute", left: 229, top: 8, width: 62, height: 13,
    color: "#d4c2aa", fontSize: 11, textAlign: "right" },
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
    borderColor: "#6c3e18", borderWidth: 1, borderRadius: 3 },
  pilotLightOn: { backgroundColor: "#f4b45f", shadowColor: "#f4a340", shadowRadius: 4 },
  pilotLightOff: { backgroundColor: "#5d4327", shadowColor: "#5d4327", shadowRadius: 0 },
  kicker: { position: "absolute", left: 10, top: 7, width: 78, height: 13,
    color: "#d9a15d", fontFamily: "system", fontSize: 11 },
  title: { position: "absolute", left: 10, top: 27, width: 127, height: 15,
    color: "#f6ecd8", fontFamily: "serif", fontSize: 14, fontWeight: "700" },
  artist: { position: "absolute", left: 10, top: 47, width: 127, height: 14,
    color: "#d0b99d", fontSize: 12 },
  album: { position: "absolute", left: 10, top: 65, width: 127, height: 14,
    color: "#8f8376", fontSize: 11 },
  favorite: { position: "absolute", left: 10, top: 84, width: 88, height: 14,
    color: "#ff8fa2", fontSize: 11 },
  modeText: { position: "absolute", left: 102, top: 84, width: 35, height: 14,
    color: "#55d6e7", fontSize: 11 },
  statusPlate: { position: "absolute", left: 88, top: 6, width: 40, height: 11,
    backgroundColor: "#7b441d", borderColor: "#d59b59", borderWidth: 1,
    borderRadius: 3 },
  statusText: { width: 38, height: 11, color: "#ffe1ac", fontSize: 11,
    textAlign: "center" },
  volume: { position: "absolute", left: 65, top: 99, width: 67, height: 13,
    color: "#b8a38c", fontSize: 11, textAlign: "right" },
  lyricsOverlay: { backgroundColor: "#050403", opacity: 1 },
  lyricsHeader: { width: 260, height: 14, color: "#d9a15d", fontSize: 11,
    fontFamily: "mono", textAlign: "center", marginBottom: 4 },
  lyricsCloseHint: { width: 260, height: 13, color: "#bca384", fontSize: 11,
    textAlign: "center", marginTop: 2 },
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
    color: "#8d7962", fontSize: 11 },
  centerHint: { position: "absolute", left: 70, top: 31, width: 126, height: 9,
    color: "#bca384", fontSize: 11, textAlign: "center" },
  nextHint: { position: "absolute", left: 201, top: 31, width: 49, height: 9,
    color: "#8d7962", fontSize: 11, textAlign: "right" },
  hidden: { position: "absolute", left: 0, top: 0, width: 0, height: 0,
    backgroundColor: "#000000", opacity: 0 },
  overlay: { backgroundColor: "#050403", opacity: 1 },
  panel: { position: "absolute", left: 35, top: 38, width: 250, height: 172,
    backgroundColor: "#15120f", borderColor: "#d89142", borderWidth: 1,
    borderRadius: 10, padding: 12 },
  panelSmall: { position: "absolute", left: 35, top: 58, width: 250, height: 132,
    backgroundColor: "#15120f", borderColor: "#d89142", borderWidth: 1,
    borderRadius: 10, padding: 12 },
  lyricsPanel: { position: "absolute", left: 16, top: 36, width: 288, height: 190,
    backgroundColor: "#15120f", borderColor: "#d89142", borderWidth: 1,
    borderRadius: 10, padding: 12 },
  lyricsRows: { width: 260, height: 96 },
  panelTitle: { width: 226, height: 14, color: "#d9a15d", fontFamily: "system", fontSize: 11,
    textAlign: "center", marginBottom: 5 },
  menuOn: { width: 226, height: 21, backgroundColor: "#d89142",
    borderRadius: 4, marginBottom: 2, justifyContent: "center" },
  menuOff: { width: 226, height: 21, backgroundColor: "#231c16",
    borderRadius: 4, marginBottom: 2, justifyContent: "center" },
  menuTextOn: { width: 226, height: 12, color: "#130f0b", fontSize: 11,
    textAlign: "center" },
  menuTextOff: { width: 226, height: 12, color: "#d8c6aa", fontSize: 11,
    textAlign: "center" },
  queueRow: { width: 226, height: 38, backgroundColor: "#211b16",
    borderColor: "#3e3025", borderWidth: 1, borderRadius: 5, marginBottom: 3 },
  queueRowOn: { width: 226, height: 38, backgroundColor: "#6f4828",
    borderColor: "#d89142", borderWidth: 1, borderRadius: 5, marginBottom: 3 },
  queueTitle: { width: 212, height: 14, color: "#cdbda4", fontSize: 11,
    marginLeft: 7, marginTop: 3 },
  queueArtist: { width: 212, height: 13, color: "#a99a87", fontSize: 11,
    marginLeft: 7, marginTop: 1 },
  queueTitleOn: { width: 212, height: 14, color: "#fff1d4", fontSize: 11,
    marginLeft: 7, marginTop: 3 },
  queueArtistOn: { width: 212, height: 13, color: "#f0c894", fontSize: 11,
    marginLeft: 7, marginTop: 1 },
  panelHint: { width: 226, height: 13, color: "#a99a87", fontSize: 11,
    textAlign: "center", marginTop: 5 },
  lyricDim: { color: "#867665", fontSize: 12, lineHeight: 16,
    textAlign: "center" },
  lyricCurrent: { color: "#fff0d0", fontSize: 16, lineHeight: 20,
    fontWeight: "700", textAlign: "center" },
  seekTrack: { width: 180, height: 10, backgroundColor: "#30271e",
    borderColor: "#d89142", borderWidth: 1, borderRadius: 5,
    marginLeft: 23, marginTop: 18 },
  seekFill: { height: 8, backgroundColor: "#d89142", borderRadius: 4 },
  seekValue: { width: 226, height: 18, color: "#f4dfbc", fontSize: 11,
    textAlign: "center", marginTop: 12 },
});
