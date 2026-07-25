import os
import subprocess
import sys
import time
import shutil

try:
    import pyautogui
except ImportError:
    print("Install: pip install pyautogui")
    sys.exit(1)

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(REPO, "build-sim")
EXE = os.path.join(BUILD_DIR, "rockboxui.exe")
CONVO_DIR = r"C:\Users\Jason\.gemini\antigravity\brain\43f14da4-a398-468e-98f2-f81a5f52f89f"

def find_sim_window():
    for w in pyautogui.getAllWindows():
        t = (w.title or "").strip()
        if t in ("iPod Video", "iPod 6G", "iPod Color", "Rockbox"):
            return w
    for w in pyautogui.getAllWindows():
        t = w.title or ""
        if "iPod" in t and "Cursor" not in t:
            return w
    return None

def focus(w):
    try:
        w.activate()
    except Exception:
        pass
    time.sleep(1.0)

def cap(name, w):
    img = pyautogui.screenshot(region=(w.left, w.top, w.width, w.height))
    path = os.path.join(CONVO_DIR, f"{name}.png")
    img.save(path)
    print(f"  saved: {path}")

def main():
    print("Starting simulator for audit...")
    proc = subprocess.Popen([EXE], cwd=BUILD_DIR, close_fds=True)
    time.sleep(15.0)

    w = find_sim_window()
    if w is None:
        print("No window found from the following windows:")
        for win in pyautogui.getAllWindows():
            print(f"'{win.title}'")
        proc.terminate()
        return 1

    focus(w)
    
    # Ensure we start from a clean state. ESC a few times.
    for _ in range(5):
        pyautogui.press('escape')
        time.sleep(0.2)
        
    cap("0_main_menu", w)

    # 1. TEST MUSIC
    print("Testing Music back-navigation...")
    pyautogui.press('return') # enter Music
    time.sleep(1)
    cap("1_music_root", w)
    
    pyautogui.press('down')
    pyautogui.press('return') # deeper 1
    time.sleep(1)
    
    pyautogui.press('down')
    pyautogui.press('return') # deeper 2
    time.sleep(1)
    cap("2_music_deep", w)
    
    # Back out completely
    for _ in range(3):
        pyautogui.press('left') # Back
        time.sleep(0.5)
        
    time.sleep(1)
    cap("3_after_music_back", w)
    
    # 2. TEST FILES
    print("Testing Files back-navigation...")
    # Assume we are back at Main Menu at the top (Music selected)
    # Walk down to Extras without relying on wraparound.
    for _ in range(7):
        pyautogui.press('down')
        time.sleep(0.2)
    cap("4_extras_selected", w)
    
    pyautogui.press('return') # enter Extras
    time.sleep(1)
    cap("5_extras_menu", w) # Should see Files selected
    
    pyautogui.press('return') # enter Files
    time.sleep(1)
    cap("6_files_root", w) # Should see / raw filesystem
    
    # Back out
    pyautogui.press('left') # Back
    time.sleep(1)
    cap("7_after_files_back", w)

    proc.terminate()
    print("Audit complete.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
