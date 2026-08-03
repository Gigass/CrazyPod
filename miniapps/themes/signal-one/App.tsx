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
    : elapsed >= length ? 228
      : length > 9000000
        ? (elapsed / 1000) * 228 / (length / 1000)
        : elapsed * 228 / length;
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
        else if (panel === 2) setQueueSelected((value) => value <= 0 ? 0 : value - 1);
        else if (panel === 3) setMode((value) => value <= 0 ? 3 : value - 1);
        else if (panel === 4) setPanel(1);
        else if (panel === 5) setSeekDraft((value) => value <= 5000 ? 0 : value - 5000);
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
      <Image style={styles.console} source="signal-console" />

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
          {info[6] === 1 ? "SHUF" : info[5] === 2 ? "RPT1" :
            info[5] === 1 ? "RPTA" : "NORM"}
        </Text>
        <Text style={styles.favorite}>{info[7] === 1 ? "FAV" : "---"}</Text>
        <Text style={styles.volume}>V {info[4]}</Text>
      </View>

      <View style={styles.progressTrack}>
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
          audioLevel(info[8], info[9]) >= 650 ? styles.meterHot : styles.meterOff]} />
        <View style={[styles.meterSegment, info[3] === 2 &&
          audioLevel(info[8], info[9]) >= 850 ? styles.meterHot : styles.meterOff]} />
      </View>
      <Text style={styles.transportHint}>MENU / OPTIONS       PLAY / PAUSE       WHEEL / VOLUME</Text>

      {panel === 1 ? (
        <Modal style={styles.overlay}>
          <Image style={styles.modalBackground} source="signal-modal" />
          <Text style={styles.panelTitle}>NOW PLAYING / CONTROL</Text>
          <View style={styles.menuList}>
            <View style={action === 0 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 0 ? styles.menuTextOn : styles.menuTextOff}>01  QUEUE</Text>
            </View>
            <View style={action === 1 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 1 ? styles.menuTextOn : styles.menuTextOff}>
                02  {info[7] === 1 ? "REMOVE FAVORITE" : "ADD FAVORITE"}
              </Text>
            </View>
            <View style={action === 2 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 2 ? styles.menuTextOn : styles.menuTextOff}>03  PLAYBACK MODE</Text>
            </View>
            <View style={action === 3 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 3 ? styles.menuTextOn : styles.menuTextOff}>04  LYRICS</Text>
            </View>
            <View style={action === 4 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 4 ? styles.menuTextOn : styles.menuTextOff}>05  SEEK</Text>
            </View>
            <View style={action === 5 ? styles.menuOn : styles.menuOff}>
              <Text style={action === 5 ? styles.menuTextOn : styles.menuTextOff}>06  CLOSE</Text>
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
              <MarqueeText style={styles.queueTitleOn}>{queueTitle1}</MarqueeText>
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
              <Text style={mode === 0 ? styles.menuTextOn : styles.menuTextOff}>01  NORMAL</Text>
            </View>
            <View style={mode === 1 ? styles.menuOn : styles.menuOff}>
              <Text style={mode === 1 ? styles.menuTextOn : styles.menuTextOff}>02  SHUFFLE</Text>
            </View>
            <View style={mode === 2 ? styles.menuOn : styles.menuOff}>
              <Text style={mode === 2 ? styles.menuTextOn : styles.menuTextOff}>03  REPEAT ALL</Text>
            </View>
            <View style={mode === 3 ? styles.menuOn : styles.menuOff}>
              <Text style={mode === 3 ? styles.menuTextOn : styles.menuTextOff}>04  REPEAT ONE</Text>
            </View>
          </View>
          <Text style={styles.panelHint}>SELECT APPLIES / MENU RETURNS</Text>
        </Modal>
      ) : <View style={styles.hidden} />}

      {panel === 4 ? (
        <Modal style={styles.overlay}>
          <Image style={styles.modalBackground} source="signal-modal" />
          <Text style={styles.panelTitle}>LYRICS / LIVE</Text>
          <View style={styles.lyricsList}>
            <MarqueeText style={styles.lyricDim}>
              {lyricInfo[0] === 1 ? lyricPrevious : ""}
            </MarqueeText>
            <MarqueeText style={styles.lyricCurrent}>
              {lyricInfo[0] === 1 ? lyricCurrent : "No lyrics available"}
            </MarqueeText>
            <MarqueeText style={styles.lyricDim}>
              {lyricInfo[0] === 1 ? lyricNext : ""}
            </MarqueeText>
          </View>
          <Text style={styles.panelHint}>SELECT OR MENU RETURNS</Text>
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
  artwork: { position: "absolute", left: 26, top: 40, width: 89, height: 89,
    borderRadius: 3 },
  artworkFrame: { position: "absolute", left: 24, top: 38, width: 93, height: 93,
    borderColor: "#8b8a85", borderWidth: 1, borderRadius: 4, opacity: 0.7 },
  coverLabel: { position: "absolute", left: 27, top: 136, width: 88, height: 9,
    color: "#a9aaa7", fontSize: 6, textAlign: "center" },
  brand: { position: "absolute", left: 136, top: 31, width: 104, height: 10,
    color: "#555651", fontSize: 7, fontFamily: "mono" },
  metadataPanel: { position: "absolute", left: 138, top: 50, width: 154, height: 94 },
  sourceLabel: { position: "absolute", left: 0, top: 0, width: 94, height: 10,
    color: "#9b9e98", fontSize: 6 },
  liveDot: { position: "absolute", left: 142, top: 1, width: 5, height: 5,
    backgroundColor: "#f36b21", borderRadius: 3 },
  title: { position: "absolute", left: 0, top: 14, width: 148, height: 17,
    color: "#f2f0e9", fontSize: 14, fontFamily: "mono", fontWeight: "700" },
  artist: { position: "absolute", left: 0, top: 36, width: 148, height: 14,
    color: "#d0d1cc", fontSize: 11 },
  album: { position: "absolute", left: 0, top: 54, width: 148, height: 12,
    color: "#92958f", fontSize: 9 },
  rule: { position: "absolute", left: 0, top: 71, width: 148, height: 1,
    backgroundColor: "#595b58" },
  statusText: { position: "absolute", left: 0, top: 78, width: 34, height: 9,
    color: "#f36b21", fontSize: 7 },
  modeText: { position: "absolute", left: 39, top: 78, width: 34, height: 9,
    color: "#bfc1bc", fontSize: 7 },
  favorite: { position: "absolute", left: 78, top: 78, width: 24, height: 9,
    color: "#bfc1bc", fontSize: 7 },
  volume: { position: "absolute", left: 112, top: 78, width: 36, height: 9,
    color: "#bfc1bc", fontSize: 7, textAlign: "right" },
  progressTrack: { position: "absolute", left: 61, top: 193, width: 228, height: 3,
    backgroundColor: "#9f9e97", borderRadius: 2 },
  progressFill: { height: 3, backgroundColor: "#f36b21", borderRadius: 2 },
  meterRow: { position: "absolute", left: 62, top: 204, width: 226, height: 4,
    flexDirection: "row" },
  meterSegment: { width: 21, height: 3, marginRight: 4, borderRadius: 2 },
  meterOn: { backgroundColor: "#f36b21", opacity: 1 },
  meterHot: { backgroundColor: "#e7411b", opacity: 1 },
  meterOff: { backgroundColor: "#565753", opacity: 0.56 },
  transportHint: { position: "absolute", left: 34, top: 222, width: 252, height: 8,
    color: "#6d6e69", fontSize: 6, textAlign: "center" },
  hidden: { position: "absolute", left: 0, top: 0, width: 0, height: 0,
    backgroundColor: "#000000", opacity: 0 },
  overlay: { backgroundColor: "#c9c5bd", opacity: 1 },
  modalBackground: { position: "absolute", left: 0, top: 0, width: 320, height: 240 },
  panelTitle: { position: "absolute", left: 47, top: 34, width: 222, height: 11,
    color: "#555651", fontSize: 8, fontFamily: "mono", fontWeight: "700" },
  menuList: { position: "absolute", left: 49, top: 56, width: 222, height: 128 },
  menuOn: { width: 222, height: 19, backgroundColor: "#f36b21",
    borderColor: "#ff9b63", borderWidth: 1, borderRadius: 3,
    marginBottom: 2, justifyContent: "center" },
  menuOff: { width: 222, height: 19, backgroundColor: "#3a3b39",
    borderColor: "#50514e", borderWidth: 1, borderRadius: 3,
    marginBottom: 2, justifyContent: "center" },
  menuTextOn: { width: 206, height: 11, color: "#171816", fontSize: 9,
    marginLeft: 8 },
  menuTextOff: { width: 206, height: 11, color: "#d6d6d0", fontSize: 9,
    marginLeft: 8 },
  queueList: { position: "absolute", left: 49, top: 56, width: 222, height: 119 },
  queueRow: { width: 222, height: 36, backgroundColor: "#373836",
    borderColor: "#4d4e4a", borderWidth: 1, borderRadius: 3, marginBottom: 3 },
  queueRowOn: { width: 222, height: 36, backgroundColor: "#f36b21",
    borderColor: "#ff9b63", borderWidth: 1, borderRadius: 3, marginBottom: 3 },
  queueTitle: { width: 208, height: 13, color: "#e3e2dc", fontSize: 10,
    marginLeft: 7, marginTop: 3 },
  queueArtist: { width: 208, height: 11, color: "#9a9c97", fontSize: 8,
    marginLeft: 7, marginTop: 1 },
  queueTitleOn: { width: 208, height: 13, color: "#171816", fontSize: 10,
    marginLeft: 7, marginTop: 3 },
  queueArtistOn: { width: 208, height: 11, color: "#592512", fontSize: 8,
    marginLeft: 7, marginTop: 1 },
  modeList: { position: "absolute", left: 49, top: 68, width: 222, height: 92 },
  panelHint: { position: "absolute", left: 49, top: 178, width: 222, height: 9,
    color: "#9ea09a", fontSize: 6, textAlign: "center" },
  lyricsList: { position: "absolute", left: 49, top: 67, width: 222, height: 92 },
  lyricDim: { width: 222, height: 18, color: "#92948f", fontSize: 9,
    textAlign: "center", marginBottom: 8 },
  lyricCurrent: { width: 222, height: 24, color: "#f36b21", fontSize: 12,
    textAlign: "center", marginBottom: 8 },
  seekPanel: { position: "absolute", left: 49, top: 65, width: 222, height: 105 },
  seekLabel: { width: 222, height: 10, color: "#969893", fontSize: 7,
    textAlign: "center" },
  seekTrack: { width: 198, height: 8, backgroundColor: "#555651",
    borderColor: "#767872", borderWidth: 1, borderRadius: 4,
    marginLeft: 12, marginTop: 15 },
  seekFill: { height: 6, backgroundColor: "#f36b21", borderRadius: 3 },
  seekValue: { width: 222, height: 18, color: "#f0eee7", fontSize: 11,
    textAlign: "center", marginTop: 12 },
  seekInstruction: { width: 222, height: 10, color: "#969893", fontSize: 7,
    textAlign: "center", marginTop: 10 },
});
