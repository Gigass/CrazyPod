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
    : elapsed >= length ? 228
      : length > 9000000
        ? (elapsed / 1000) * 228 / (length / 1000)
        : elapsed * 228 / length;
}

function volumePercent(value: number) {
  return value <= -60 ? 0 : value >= 12 ? 100 : (value + 60) * 100 / 72;
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
    } else {
      setLyricFrame((value) => value >= 9 ? 9 : value + 1);
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
        else if (panel === 2) setQueueSelected((value) => value <= 0 ? 0 : value - 1);
        else if (panel === 3) setMode((value) => cycleBackward(value, 1, 4));
        else if (panel === 5) setSeekDraft((value) => value <= 5000 ? 0 : value - 5000);
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
      <Image style={styles.console} source="signal-console" />
      <View style={styles.topRail} />
      <Text style={styles.topTime}>{system[0]}:{system[1]}{system[2]}</Text>
      <Text style={styles.topState}>{info[3] === 2 ? "PLAYING" : info[3] === 1 ? "PAUSED" : "STOPPED"}</Text>
      <Text style={styles.topBattery}>{system[4] === 1 ? "CHG" : "BAT"} {system[3]}%</Text>

      <NowPlayingArtwork style={styles.artwork} variant={2} />
      <View style={styles.artworkFrame} />
      <Text style={styles.coverLabel}>SIGNAL / SOURCE</Text>

      <Text style={styles.brand}>SIGNAL ONE</Text>
      <View style={styles.metadataPanel}>
        <Text style={styles.sourceLabel}>NOW PLAYING</Text>
        <View style={styles.liveDot} />
        <MarqueeText style={styles.title}>{title === "" ? "No Track" : title}</MarqueeText>
        <MarqueeText style={styles.artist}>{artist === "" ? "Unknown Artist" : artist}</MarqueeText>
        <MarqueeText style={styles.album}>{album === "" ? "Local Library" : album}</MarqueeText>
        <View style={styles.rule} />
        <Text style={styles.statusText}>
          {info[3] === 2 ? "PLAY" : info[3] === 1 ? "PAUSE" : "STOP"}
        </Text>
        <Text style={styles.modeText}>
          {info[6] === 1 ? "×" : info[5] === 2 ? "1×" :
            info[5] === 1 ? "∞" : "→"}
        </Text>
        <Text style={styles.favorite}>{info[7] === 1 ? "BANK ●" : "BANK ○"}</Text>
        <Text style={styles.volume}>L{volumePercent(info[4])}</Text>
      </View>

      <View style={styles.progressTrack}>
        <View style={[styles.progressFill, {
          width: progressWidth(info[1], info[2]),
        }]} />
      </View>
      <Text style={styles.progressLabel}>POSITION</Text>
      <Text style={styles.progressValue}>{info[1] / 1000}s / {info[2] / 1000}s</Text>
      <Text style={styles.transportHint}>MENU / OPTIONS       PLAY / PAUSE       WHEEL / VOLUME</Text>

      {lyricsEnabled === 1 ? <Modal style={styles.lyricsOverlay}>
        <View style={styles.lyricsWindow}>
          <Text style={styles.lyricsHeader}>LYRICS / {lyricInfo[5] === 1 ? "TIMECODE" : "PLAIN TEXT"}</Text>
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
          <Image style={styles.modalBackground} source="signal-modal" />
          <Text style={styles.panelTitle}>NOW PLAYING / CONTROL</Text>
          <View style={styles.menuList}>
            <View style={action === 0 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 0 ? styles.menuTextOn : styles.menuTextOff}>QUEUE BUS</Text>
            </View>
            <View style={action === 1 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 1 ? styles.menuTextOn : styles.menuTextOff}>
                {info[7] === 1 ? "CLEAR MEMORY" : "STORE MEMORY"}
              </Text>
            </View>
            <View style={action === 2 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 2 ? styles.menuTextOn : styles.menuTextOff}>ROUTING MODE</Text>
            </View>
            <View style={action === 3 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 3 ? styles.menuTextOn : styles.menuTextOff}>{lyricsEnabled === 1 ? "HIDE TEXT MONITOR" : "SHOW TEXT MONITOR"}</Text>
            </View>
            <View style={action === 4 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 4 ? styles.menuTextOn : styles.menuTextOff}>TIME TRIM</Text>
            </View>
          </View>
        </Modal>
      ) : <View style={styles.hidden} />}

      {panel === 2 ? (
        <Modal style={styles.overlay}>
          <Image style={styles.modalBackground} source="signal-modal" />
          <Text style={styles.panelTitle}>
            QUEUE / {queueInfo[1] > 0 ? queueSelected + 1 : 0} OF {queueInfo[1]}
          </Text>
          <View style={styles.queueList}>
            <View style={styles.queueRow}>
              <MarqueeText style={styles.queueTitle}>{queueTitle0}</MarqueeText>
              <MarqueeText style={styles.queueArtist}>{queueArtist0}</MarqueeText>
            </View>
            <View style={styles.queueRowOn}>
              <MarqueeText style={styles.queueTitleOn}>{queueInfo[1] > 0 ? queueTitle1 : "NO SIGNAL / QUEUE EMPTY"}</MarqueeText>
              <MarqueeText style={styles.queueArtistOn}>{queueArtist1}</MarqueeText>
            </View>
            <View style={styles.queueRow}>
              <MarqueeText style={styles.queueTitle}>{queueTitle2}</MarqueeText>
              <MarqueeText style={styles.queueArtist}>{queueArtist2}</MarqueeText>
            </View>
          </View>
          <Text style={styles.panelHint}>SELECT PLAYS / MENU RETURNS</Text>
        </Modal>
      ) : <View style={styles.hidden} />}

      {panel === 3 ? (
        <Modal style={styles.overlay}>
          <Image style={styles.modalBackground} source="signal-modal" />
          <Text style={styles.panelTitle}>PLAYBACK / MODE</Text>
          <View style={styles.modeList}>
            <View style={mode === 0 ? styles.menuOn : styles.menuOff}>
              <Text style={mode === 0 ? styles.menuTextOn : styles.menuTextOff}>DIRECT</Text>
            </View>
            <View style={mode === 1 ? styles.menuOn : styles.menuOff}>
              <Text style={mode === 1 ? styles.menuTextOn : styles.menuTextOff}>RANDOM BUS</Text>
            </View>
            <View style={mode === 2 ? styles.menuOn : styles.menuOff}>
              <Text style={mode === 2 ? styles.menuTextOn : styles.menuTextOff}>LOOP PROGRAM</Text>
            </View>
            <View style={mode === 3 ? styles.menuOn : styles.menuOff}>
              <Text style={mode === 3 ? styles.menuTextOn : styles.menuTextOff}>LOOP SOURCE</Text>
            </View>
          </View>
          <Text style={styles.panelHint}>SELECT APPLIES / MENU RETURNS</Text>
        </Modal>
      ) : <View style={styles.hidden} />}

      {panel === 5 ? (
        <Modal style={styles.overlay}>
          <Image style={styles.modalBackground} source="signal-modal" />
          <Text style={styles.panelTitle}>PLAYBACK / SEEK</Text>
          <View style={styles.seekPanel}>
            <Text style={styles.seekLabel}>POSITION</Text>
            <View style={styles.seekTrack}>
              <View style={[styles.seekFill, {
                width: progressWidth(seekDraft, info[2]) * 198 / 228,
              }]} />
            </View>
            <Text style={styles.seekValue}>{seekDraft / 1000}s / {info[2] / 1000}s</Text>
            <Text style={styles.seekInstruction}>WHEEL 5 SEC / SELECT APPLIES</Text>
          </View>
        </Modal>
      ) : <View style={styles.hidden} />}
    </View>
  );
}

const styles = StyleSheet.create({
  screen: { width: 320, height: 240, backgroundColor: "#d4d0c8" },
  console: { position: "absolute", left: 0, top: 0, width: 320, height: 240 },
  topRail: { position: "absolute", left: 18, top: 4, width: 284, height: 20,
    backgroundColor: "#d4d0c8", borderColor: "#878780", borderWidth: 1,
    borderRadius: 3, opacity: 0.96 },
  topTime: { position: "absolute", left: 27, top: 7, width: 52, height: 15,
    color: "#555651", fontSize: 12 },
  topState: { position: "absolute", left: 112, top: 7, width: 96, height: 15,
    color: "#d94d0b", fontSize: 12, textAlign: "center" },
  topBattery: { position: "absolute", left: 239, top: 7, width: 54, height: 15,
    color: "#555651", fontSize: 12, textAlign: "right" },
  artwork: { position: "absolute", left: 26, top: 40, width: 89, height: 89,
    borderRadius: 3 },
  artworkFrame: { position: "absolute", left: 24, top: 38, width: 93, height: 93,
    borderColor: "#8b8a85", borderWidth: 1, borderRadius: 4, opacity: 0.7 },
  coverLabel: { position: "absolute", left: 27, top: 134, width: 88, height: 15,
    color: "#a9aaa7", fontSize: 12, textAlign: "center" },
  brand: { position: "absolute", left: 136, top: 28, width: 104, height: 15,
    color: "#555651", fontFamily: "mono", fontSize: 12 },
  metadataPanel: { position: "absolute", left: 138, top: 50, width: 154, height: 94 },
  sourceLabel: { position: "absolute", left: 0, top: 0, width: 94, height: 15,
    color: "#9b9e98", fontSize: 12 },
  liveDot: { position: "absolute", left: 142, top: 1, width: 5, height: 5,
    backgroundColor: "#f36b21", borderRadius: 3 },
  title: { position: "absolute", left: 0, top: 16, width: 148, height: 20,
    color: "#f2f0e9", fontFamily: "mono", fontSize: 16, fontWeight: "700" },
  artist: { position: "absolute", left: 0, top: 39, width: 148, height: 19,
    color: "#d0d1cc", fontSize: 15 },
  album: { position: "absolute", left: 0, top: 59, width: 148, height: 15,
    color: "#b8bbb4", fontSize: 12 },
  rule: { position: "absolute", left: 0, top: 76, width: 148, height: 1,
    backgroundColor: "#595b58" },
  statusText: { position: "absolute", left: 0, top: 78, width: 34, height: 15,
    color: "#f36b21", fontSize: 12 },
  modeText: { position: "absolute", left: 39, top: 78, width: 34, height: 15,
    color: "#bfc1bc", fontSize: 12 },
  favorite: { position: "absolute", left: 68, top: 78, width: 53, height: 15,
    color: "#f2f0e9", fontSize: 12, textAlign: "right" },
  volume: { position: "absolute", left: 125, top: 78, width: 23, height: 15,
    color: "#bfc1bc", fontSize: 12, textAlign: "right" },
  lyricsOverlay: { backgroundColor: "#242522", opacity: 1 },
  lyricsWindow: { position: "absolute", left: 16, top: 36, width: 288, height: 190,
    backgroundColor: "#c9c5bd", borderColor: "#f36b21", borderWidth: 1, padding: 12 },
  lyricsRows: { width: 262, height: 116 },
  lyricsHeader: { width: 262, height: 15, color: "#555651", fontSize: 12,
    fontFamily: "mono", fontWeight: "700", textAlign: "center", marginBottom: 4 },
  lyricsCloseHint: { width: 262, height: 15, color: "#555651", fontSize: 12,
    textAlign: "center" },
  progressTrack: { position: "absolute", left: 61, top: 187, width: 228, height: 3,
    backgroundColor: "#9f9e97", borderRadius: 2 },
  progressFill: { height: 3, backgroundColor: "#f36b21", borderRadius: 2 },
  progressLabel: { position: "absolute", left: 61, top: 195, width: 70, height: 15,
    color: "#6d6e69", fontSize: 12 },
  progressValue: { position: "absolute", left: 183, top: 195, width: 106, height: 15,
    color: "#6d6e69", fontSize: 12, textAlign: "right" },
  transportHint: { position: "absolute", left: 34, top: 217, width: 252, height: 19,
    color: "#6d6e69", fontSize: 15, textAlign: "center" },
  hidden: { position: "absolute", left: 0, top: 0, width: 0, height: 0,
    backgroundColor: "#000000", opacity: 0 },
  overlay: { backgroundColor: "#c9c5bd", opacity: 1 },
  modalBackground: { position: "absolute", left: 0, top: 0, width: 320, height: 240 },
  panelTitle: { position: "absolute", left: 47, top: 28, width: 222, height: 20,
    color: "#555651", fontFamily: "mono", fontSize: 16, fontWeight: "700" },
  menuList: { position: "absolute", left: 49, top: 56, width: 222, height: 145 },
  menuOn: { width: 222, height: 27, backgroundColor: "#f36b21",
    borderColor: "#ff9b63", borderWidth: 1, borderRadius: 3,
    marginBottom: 2, justifyContent: "center" },
  menuOff: { width: 222, height: 27, backgroundColor: "#3a3b39",
    borderColor: "#50514e", borderWidth: 1, borderRadius: 3,
    marginBottom: 2, justifyContent: "center" },
  menuTextOn: { width: 206, height: 20, color: "#171816", fontSize: 16,
    marginLeft: 8 },
  menuTextOff: { width: 206, height: 20, color: "#d6d6d0", fontSize: 16,
    marginLeft: 8 },
  queueList: { position: "absolute", left: 49, top: 56, width: 222, height: 138 },
  queueRow: { width: 222, height: 43, backgroundColor: "#373836",
    borderColor: "#4d4e4a", borderWidth: 1, borderRadius: 3, marginBottom: 3 },
  queueRowOn: { width: 222, height: 43, backgroundColor: "#f36b21",
    borderColor: "#ff9b63", borderWidth: 1, borderRadius: 3, marginBottom: 3 },
  queueTitle: { width: 208, height: 19, color: "#e3e2dc", fontSize: 15,
    marginLeft: 7, marginTop: 3 },
  queueArtist: { width: 208, height: 15, color: "#9a9c97", fontSize: 12,
    marginLeft: 7, marginTop: 1 },
  queueTitleOn: { width: 208, height: 19, color: "#171816", fontSize: 15,
    marginLeft: 7, marginTop: 3 },
  queueArtistOn: { width: 208, height: 15, color: "#592512", fontSize: 12,
    marginLeft: 7, marginTop: 1 },
  modeList: { position: "absolute", left: 49, top: 56, width: 222, height: 116 },
  panelHint: { position: "absolute", left: 49, top: 206, width: 222, height: 15,
    color: "#9ea09a", fontSize: 12, textAlign: "center" },
  lyricsList: { position: "absolute", left: 49, top: 67, width: 222, height: 92 },
  lyricDim: { color: "#777a75", fontSize: 15, lineHeight: 20,
    textAlign: "center" },
  lyricCurrent: { color: "#d74f10", fontSize: 18, lineHeight: 24,
    fontWeight: "700", textAlign: "center" },
  seekPanel: { position: "absolute", left: 49, top: 65, width: 222, height: 105 },
  seekLabel: { width: 222, height: 15, color: "#969893", fontSize: 12,
    textAlign: "center" },
  seekTrack: { width: 198, height: 8, backgroundColor: "#555651",
    borderColor: "#767872", borderWidth: 1, borderRadius: 4,
    marginLeft: 12, marginTop: 15 },
  seekFill: { height: 6, backgroundColor: "#f36b21", borderRadius: 3 },
  seekValue: { width: 222, height: 20, color: "#f0eee7", fontSize: 16,
    textAlign: "center", marginTop: 12 },
  seekInstruction: { width: 222, height: 15, color: "#969893", fontSize: 12,
    textAlign: "center", marginTop: 10 },
});
