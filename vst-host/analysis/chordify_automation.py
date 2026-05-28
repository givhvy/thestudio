#!/usr/bin/env python3
"""
Chordify automation via Chrome DevTools Protocol (CDP).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import time
import traceback
from contextlib import contextmanager
from pathlib import Path

APP_DIR = Path.home() / "AppData" / "Roaming" / "Stratum DAW"
READY_FLAG = APP_DIR / "chordify.ready"
CONFIG_FILE = APP_DIR / "chordify.config"
DEBUG_LOG = APP_DIR / "chordify.log"
LOCK_FILE = APP_DIR / "chordify.lock"
DEFAULT_CDP_PORT = 9222


def _dbg(msg: str) -> None:
    """Write timestamped diagnostic line to both stderr and a rolling log
    file in %APPDATA%\\Stratum DAW\\chordify.log. Lets us inspect what
    actually happened during automation runs without needing a console."""
    ts = time.strftime("%H:%M:%S")
    line = f"[{ts}] {msg}"
    print(line, file=sys.stderr, flush=True)
    try:
        APP_DIR.mkdir(parents=True, exist_ok=True)
        with DEBUG_LOG.open("a", encoding="utf-8") as f:
            f.write(line + "\n")
    except Exception:
        pass


def _mark_ready(port: int) -> None:
    APP_DIR.mkdir(parents=True, exist_ok=True)
    READY_FLAG.write_text("ok\n", encoding="utf-8")
    CONFIG_FILE.write_text(f"mode=cdp\nport={port}\n", encoding="utf-8")


def _read_cdp_port() -> int:
    if CONFIG_FILE.is_file():
        for line in CONFIG_FILE.read_text(encoding="utf-8").splitlines():
            if line.startswith("port="):
                try:
                    return int(line.split("=", 1)[1].strip())
                except ValueError:
                    pass
    return DEFAULT_CDP_PORT


def _profile_ready() -> bool:
    return READY_FLAG.is_file()


def _connect_browser(playwright, port: int):
    url = f"http://127.0.0.1:{port}"
    try:
        return playwright.chromium.connect_over_cdp(url)
    except Exception as exc:
        raise RuntimeError(
            f"Cannot connect to Chrome CDP on port {port}. "
            f"Run launch-chordify-chrome.ps1 and keep Chrome open.\n{exc}"
        ) from exc


@contextmanager
def _run_lock(timeout_sec: int = 660):
    """Ensure only one automation subprocess owns Chrome CDP at a time."""
    APP_DIR.mkdir(parents=True, exist_ok=True)
    if LOCK_FILE.is_file():
        age = time.time() - LOCK_FILE.stat().st_mtime
        if age < timeout_sec:
            _dbg(f"LOCK: another automation running (lock age {age:.0f}s)")
            raise RuntimeError(
                "Another Chordify automation is already running. "
                "Wait for 'Chordify chords ready' before dragging another loop."
            )
        _dbg(f"stale lock removed (age {age:.0f}s)")
        LOCK_FILE.unlink(missing_ok=True)

    LOCK_FILE.write_text(str(os.getpid()), encoding="utf-8")
    try:
        yield
    finally:
        try:
            LOCK_FILE.unlink(missing_ok=True)
        except Exception:
            pass


def _is_chordify_home(url: str) -> bool:
    u = (url or "").rstrip("/")
    return u == "https://chordify.net" or u.endswith("chordify.net")


def _pick_page(browser):
    """Prefer chordify.net home — never a stale /chords/ song page."""
    home = None
    fallback = None
    for context in browser.contexts:
        for page in context.pages:
            url = page.url or ""
            if "chordify.net" not in url or "cloudflare" in url.lower():
                continue
            if fallback is None:
                fallback = page
            if _is_chordify_home(url) and "/chords/" not in url:
                home = page
    if home is not None:
        return home
    if fallback is not None:
        return fallback
    context = browser.contexts[0] if browser.contexts else browser.new_context()
    page = context.new_page()
    page.goto("https://chordify.net/", wait_until="domcontentloaded", timeout=120_000)
    return page


def _goto_upload_home(page) -> None:
    """Always land on chordify.net/ before uploading a new loop."""
    current_url = page.url or ""
    if _is_chordify_home(current_url) and "/chords/" not in current_url:
        _dbg(f"already on upload home: {current_url}")
        return

    _dbg(f"navigating to chordify.net home (was {current_url})")
    page.goto("https://chordify.net/", wait_until="domcontentloaded", timeout=60_000)
    time.sleep(1.0)
    _wait_past_cloudflare(page, timeout_sec=60)
    _dismiss_blocking_modals(page)


def _dismiss_blocking_modals(page) -> None:
    """Close cookie banners / upsell dialogs that block upload."""
    labels = (
        "Continue with free", "Continue for free", "Maybe later", "Not now",
        "No thanks", "Remind me later", "Got it", "Accept all", "Accept",
        "Dismiss", "Skip", "Close", "I agree", "Restore",
    )
    for _ in range(4):
        dismissed = False
        for label in labels:
            try:
                loc = page.get_by_text(label, exact=False).first
                if loc.count() > 0 and loc.is_visible(timeout=400):
                    loc.click(timeout=1500)
                    _dbg(f"dismissed modal via text={label!r}")
                    dismissed = True
                    time.sleep(0.4)
            except Exception:
                continue
        for sel in (
            "button[aria-label='Close']",
            "button[aria-label='Dismiss']",
            "[data-testid='close']",
            "[data-testid='modal-close']",
        ):
            try:
                loc = page.locator(sel).first
                if loc.count() > 0 and loc.is_visible(timeout=400):
                    loc.click(timeout=1500)
                    _dbg(f"dismissed modal via {sel}")
                    dismissed = True
                    time.sleep(0.4)
            except Exception:
                continue
        if not dismissed:
            break


def _page_blocked_by_cloudflare(page) -> bool:
    try:
        title = (page.title() or "").lower()
        if "just a moment" in title:
            return True
        body = page.locator("body").inner_text(timeout=3000).lower()
        return "security verification" in body or "verify you are not a bot" in body
    except Exception:
        return False


def _wait_past_cloudflare(page, timeout_sec: int = 120) -> None:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        if not _page_blocked_by_cloudflare(page):
            return
        time.sleep(1.0)
    raise RuntimeError("Still on Cloudflare. Complete verify in Chrome.")


def _looks_logged_in(page) -> bool:
    try:
        if page.locator("text=Upload Song").first.is_visible(timeout=2000):
            return True
        if "/chords/" in (page.url or ""):
            return True
    except Exception:
        pass
    return not _page_blocked_by_cloudflare(page)


def cmd_login_cdp(port: int) -> int:
    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        print("pip install playwright", file=sys.stderr)
        return 1

    with sync_playwright() as p:
        browser = _connect_browser(p, port)
        page = _pick_page(browser)
        _wait_past_cloudflare(page, timeout_sec=120)
        if not _looks_logged_in(page):
            try:
                input("Login Chordify Premium in Chrome, then press Enter...")
            except EOFError:
                pass
        browser.close()

    _mark_ready(port)
    print(f"Ready. CDP port {port}")
    return 0


def _click_first(page, selectors: list[str], timeout_ms: int = 8000) -> bool:
    for sel in selectors:
        try:
            loc = page.locator(sel).first
            if loc.count() > 0 and loc.is_visible(timeout=timeout_ms):
                loc.click(timeout=timeout_ms)
                return True
        except Exception:
            continue
    return False


def _find_chordify_page(context) -> object | None:
    pages = [p for p in context.pages if "chordify.net" in (p.url or "")]
    if not pages:
        return None
    for p in pages:
        if _is_chordify_home(p.url or "") and "/chords/" not in (p.url or ""):
            return p
    for p in pages:
        if "/chords/" in (p.url or ""):
            return p
    return pages[0]


def _click_upload_song_button(page) -> bool:
    """Premium upload entry — header button next to search field."""
    candidates = (
        page.get_by_role("button", name=re.compile(r"upload\s+song", re.I)),
        page.get_by_role("link", name=re.compile(r"upload\s+song", re.I)),
        page.locator("button:has-text('Upload song')"),
        page.locator("a:has-text('Upload song')"),
        page.locator("button:has-text('Upload Song')"),
        page.locator("a:has-text('Upload Song')"),
        page.locator("header >> text=Upload Song"),
        page.locator("[data-testid*='upload' i]"),
    )
    for loc in candidates:
        try:
            target = loc.first
            if target.count() > 0 and target.is_visible(timeout=1500):
                target.scroll_into_view_if_needed(timeout=2000)
                target.click(timeout=4000)
                _dbg("upload: clicked Upload Song button")
                time.sleep(0.8)
                return True
        except Exception as exc:
            _dbg(f"upload: Upload Song click attempt failed: {exc}")
    return False


def _upload_cdp_set_files(page, audio_path: Path) -> bool:
    """Set files on hidden input via CDP — avoids Playwright navigation hangs."""
    try:
        cdp = page.context.new_cdp_session(page)
        doc = cdp.send("DOM.getDocument", {"depth": 0})
        root_id = doc["root"]["nodeId"]
        qr = cdp.send(
            "DOM.querySelector",
            {"nodeId": root_id, "selector": "input[type='file']"},
        )
        node_id = qr.get("nodeId", 0)
        if not node_id:
            _dbg("upload: CDP found no input[type=file]")
            return False

        file_path = str(audio_path.resolve())
        cdp.send(
            "DOM.setFileInputFiles",
            {"nodeId": node_id, "files": [file_path]},
        )
        cdp.send(
            "Runtime.evaluate",
            {
                "expression": """
                    () => {
                        const input = document.querySelector("input[type='file']");
                        if (!input) return false;
                        input.dispatchEvent(new Event('input', { bubbles: true }));
                        input.dispatchEvent(new Event('change', { bubbles: true }));
                        return true;
                    }
                """,
                "returnByValue": True,
            },
        )
        _dbg(f"upload: CDP setFileInputFiles ok ({audio_path.name})")
        return True
    except Exception as exc:
        _dbg(f"upload: CDP setFileInputFiles failed: {exc}")
        return False


def _wait_upload_kickoff(context, page, timeout_sec: float = 45.0):
    """Return the page that shows upload/analysis progress."""
    browser = getattr(context, "browser", None)
    _dbg("upload: waiting for upload to start...")
    deadline = time.time() + timeout_sec
    start_url = _live_url(page)
    while time.time() < deadline:
        if browser is not None:
            active = _resolve_song_page(browser, page)
            if active is not None and _live_url(active) != start_url:
                return active

        for candidate in context.pages:
            url = _live_url(candidate)
            if "chordify.net" not in url:
                continue
            if "/chords/" in url and url != start_url:
                _dbg(f"upload: navigated to song page {url}")
                return candidate
            try:
                for label in ("Uploading", "Analyzing", "Processing", "Transcribing"):
                    if candidate.locator(f"text={label}").first.is_visible(timeout=300):
                        _dbg(f"upload: saw status text '{label}'")
                        return candidate
            except Exception:
                pass
        time.sleep(0.5)

    if browser is not None:
        active = _resolve_song_page(browser, page)
        if active is not None:
            _dbg(f"upload: song tab already open: {_live_url(active)}")
            return active

    refreshed = _find_chordify_page(context)
    if refreshed is not None:
        _dbg(f"upload: no explicit kickoff — continuing on {_live_url(refreshed)}")
        return refreshed
    return page


def _song_name_hints(audio_path: Path) -> list[str]:
    stem = audio_path.stem
    hints: list[str] = [stem]
    if stem.lower().endswith(".wav"):
        hints.append(stem[:-4])
    quoted = re.findall(r"'([^']+)'", stem)
    hints.extend(quoted)
    if len(stem) > 12:
        hints.append(stem[:48])
    # de-dupe preserving order
    seen: set[str] = set()
    out: list[str] = []
    for h in hints:
        key = h.strip().lower()
        if key and key not in seen:
            seen.add(key)
            out.append(h.strip())
    return out


def _song_visible_in_play_again(page, audio_path: Path) -> bool:
    for hint in _song_name_hints(audio_path):
        try:
            loc = page.get_by_text(hint, exact=False).first
            if loc.count() > 0 and loc.is_visible(timeout=400):
                return True
        except Exception:
            continue
    return False


def _wait_for_song_in_library(page, audio_path: Path, timeout_sec: float = 120.0) -> bool:
    """After upload Chordify often stays on home — song shows in Play again."""
    _dbg(f"library: waiting for {audio_path.name!r} in Play again...")
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        if _song_visible_in_play_again(page, audio_path):
            _dbg("library: song appeared in Play again list")
            return True
        time.sleep(2.0)
    _dbg("library: song not seen in Play again yet")
    return False


def _open_song_from_play_again(page, audio_path: Path) -> bool:
    """Click uploaded song in Play again / history to open /chords/ page."""
    hints = _song_name_hints(audio_path)
    _dbg(f"play-again: opening song, hints={hints[:3]}")

    page.bring_to_front()
    for hint in hints:
        try:
            loc = page.get_by_text(hint, exact=False).first
            if loc.count() == 0 or not loc.is_visible(timeout=800):
                continue
            loc.scroll_into_view_if_needed(timeout=2000)
            loc.click(timeout=5000)
            try:
                page.wait_for_url("**/chords/**", timeout=25_000)
            except Exception:
                time.sleep(2.0)
            if "/chords/" in _live_url(page):
                _dbg(f"play-again: opened via text {hint!r} -> {_live_url(page)}")
                return True
        except Exception as exc:
            _dbg(f"play-again: locator click {hint!r} failed: {exc}")

    js = """
        (hints) => {
            const lower = hints.map(h => h.toLowerCase());
            const links = [...document.querySelectorAll('a,[role=link]')];
            for (const a of links) {
                const t = (a.innerText || '').trim();
                if (!t || t.length > 220) continue;
                const tl = t.toLowerCase();
                for (const h of lower) {
                    if (tl.includes(h) || h.includes(tl.slice(0, Math.min(32, tl.length)))) {
                        a.scrollIntoView({block: 'center'});
                        a.click();
                        return t;
                    }
                }
            }
            return null;
        }
    """
    try:
        clicked = page.evaluate(js, hints)
        if clicked:
            _dbg(f"play-again: js clicked {clicked!r}")
            time.sleep(2.0)
            try:
                page.wait_for_url("**/chords/**", timeout=25_000)
            except Exception:
                pass
            if "/chords/" in _live_url(page):
                _dbg(f"play-again: song page {_live_url(page)}")
                return True
    except Exception as exc:
        _dbg(f"play-again: js failed: {exc}")
    return False


def _upload_audio(page, audio_path: Path):
    """Upload an audio file to Chordify."""
    context = page.context
    page.bring_to_front()
    _dismiss_blocking_modals(page)

    if not _click_upload_song_button(page):
        _dbg("upload: Upload Song button not visible — trying file input directly")

    if _upload_cdp_set_files(page, audio_path):
        return _wait_upload_kickoff(context, page)

    inputs = page.locator("input[type='file']").all()
    _dbg(f"upload: found {len(inputs)} file input(s) on {_live_url(page)}")

    last_exc: Exception | None = None
    for idx, file_input in enumerate(inputs):
        try:
            _dbg(f"upload: playwright set_input_files on input #{idx}")
            file_input.set_input_files(str(audio_path), timeout=12_000)
            _dbg(f"upload: set_input_files ok on input #{idx}")
            return _wait_upload_kickoff(context, page)
        except Exception as exc:
            last_exc = exc
            _dbg(f"upload: input #{idx} failed: {exc}")
            recovered = _find_chordify_page(context)
            if recovered is not None:
                page = recovered

    detail = str(last_exc) if last_exc else "no file input found"
    raise RuntimeError(
        f"Could not upload {audio_path.name} to Chordify. "
        "Ensure Premium is active and Chrome shows the Upload song button. "
        f"Detail: {detail}"
    )


def _all_chordify_pages(browser) -> list:
    pages = []
    for context in browser.contexts:
        for pg in context.pages:
            if "chordify.net" in (pg.url or ""):
                pages.append(pg)
            else:
                try:
                    href = pg.evaluate("location.href")
                    if "chordify.net" in (href or ""):
                        pages.append(pg)
                except Exception:
                    pass
    return pages


def _live_url(page) -> str:
    try:
        href = page.evaluate("() => location.href")
        if href:
            return href
    except Exception:
        pass
    return page.url or ""


def _looks_like_song_page(page) -> bool:
    url = _live_url(page)
    if "/chords/" in url:
        return True
    try:
        for sel in (
            "text=Generate Lyrics",
            "text=Chord diagrams",
            "button:has-text('TRANSPOSE')",
            "[class*='chord-card']",
        ):
            if page.locator(sel).first.is_visible(timeout=400):
                return True
    except Exception:
        pass
    return False


def _resolve_song_page(browser, page):
    """Return the Chordify tab that shows the analyzed song."""
    for candidate in _all_chordify_pages(browser):
        url = _live_url(candidate)
        if "/chords/" in url:
            _dbg(f"found song tab: {url}")
            try:
                candidate.bring_to_front()
            except Exception:
                pass
            return candidate
    if _looks_like_song_page(page):
        _dbg(f"song UI detected on current tab: {_live_url(page)}")
        return page
    return None


def _wait_for_song_page(browser, page, audio_path: Path, timeout_sec: int = 180):
    """Wait for analysis, open song from Play again if needed, return song tab."""
    _dbg(f"waiting for song page (timeout {timeout_sec}s)...")
    deadline = time.time() + timeout_sec
    last_log = 0.0
    last_play_again = 0.0

    while time.time() < deadline:
        active = _resolve_song_page(browser, page)
        if active is not None and _looks_like_song_page(active):
            _dbg(f"song page ready: {_live_url(active)}")
            return active

        now = time.time()
        if now - last_play_again > 6.0:
            last_play_again = now
            home = _find_chordify_page(page.context) or page
            if _song_visible_in_play_again(home, audio_path):
                _dbg("play-again: song listed — clicking to open chords page")
                if _open_song_from_play_again(home, audio_path):
                    page = _resolve_song_page(browser, home) or home
                    if _looks_like_song_page(page):
                        return page

        if now - last_log > 10.0:
            urls = [_live_url(p) for p in _all_chordify_pages(browser)]
            _dbg(f"poll: chordify tabs = {urls}")
            last_log = now
        time.sleep(1.5)

    raise RuntimeError(
        f"Timed out after {timeout_sec}s waiting for Chordify song page. "
        f"Song may still be analyzing — open '{audio_path.stem}' from Play again manually."
    )


def _find_song_kebab_box(page):
    """Kebab menu beside Share/Heart on song page (NOT global nav)."""
    js = r"""
        () => {
            const share = [...document.querySelectorAll('button,[role=button],a')].find(el => {
                const t = (el.getAttribute('aria-label') || el.title || el.innerText || '').toLowerCase();
                return t.includes('share');
            });
            if (!share) return null;
            const shareRect = share.getBoundingClientRect();
            const container = share.closest('div,section,header') || share.parentElement;
            const buttons = [...(container || document).querySelectorAll('button,[role=button]')];
            let best = null;
            let bestX = -1;
            for (const b of buttons) {
                const r = b.getBoundingClientRect();
                if (r.width < 8 || r.height < 8) continue;
                if (Math.abs(r.top - shareRect.top) > 60) continue;
                if (r.left <= shareRect.left + 4) continue;
                if (!b.querySelector('svg')) continue;
                if (r.left > bestX) {
                    bestX = r.left;
                    best = b;
                }
            }
            if (!best) return null;
            best.scrollIntoView({block: 'center'});
            const rr = best.getBoundingClientRect();
            return { x: rr.left + rr.width / 2, y: rr.top + rr.height / 2 };
        }
    """
    try:
        result = page.evaluate(js)
        if result and isinstance(result, dict):
            return float(result["x"]), float(result["y"])
    except Exception as exc:
        _dbg(f"kebab box lookup failed: {exc}")
    return None


def _open_kebab_menu(page) -> bool:
    """Open the 3-dot menu beside Share on the song page."""
    page.bring_to_front()
    time.sleep(0.3)

    box = _find_song_kebab_box(page)
    if box is not None:
        x, y = box
        _dbg(f"kebab: mouse.click at ({x:.0f},{y:.0f}) beside Share")
        try:
            page.mouse.move(x, y)
            time.sleep(0.05)
            page.mouse.click(x, y)
            time.sleep(0.7)
            if _menu_visible(page):
                _dbg("kebab menu opened (Share-adjacent click)")
                return True
        except Exception as exc:
            _dbg(f"kebab mouse click failed: {exc}")

    if _menu_visible(page):
        return True

    selectors = [
        "button[aria-label*='More' i]",
        "button[aria-label*='Menu' i]",
        "button[aria-label*='Options' i]",
    ]
    for sel in selectors:
        try:
            loc = page.locator(sel).first
            if loc.count() > 0 and loc.is_visible(timeout=1000):
                loc.click(timeout=3000)
                time.sleep(0.5)
                if _menu_visible(page):
                    return True
        except Exception:
            continue

    js_click = """
        () => {
            const share = [...document.querySelectorAll('button,a,[role=button]')].find(el => {
                const label = (el.getAttribute('aria-label') || el.innerText || '').toLowerCase();
                return label.includes('share');
            });
            if (share && share.parentElement) {
                const siblings = [...share.parentElement.querySelectorAll('button,[role=button]')];
                for (const b of siblings) {
                    if (b === share) continue;
                    const r = b.getBoundingClientRect();
                    if (r.width < 8 || r.height < 8) continue;
                    if (b.querySelector('svg')) {
                        b.click();
                        return 'near-share';
                    }
                }
            }
            return null;
        }
    """
    try:
        result = page.evaluate(js_click)
        if result:
            _dbg(f"kebab js fallback via {result}")
            time.sleep(0.6)
            return _menu_visible(page)
    except Exception as exc:
        _dbg(f"kebab js fallback failed: {exc}")

    return False


def _menu_visible(page) -> bool:
    try:
        for label in ("MIDI Fixed tempo", "MIDI Time aligned", "MIDI time aligned"):
            loc = page.get_by_role("menuitem", name=label)
            if loc.count() > 0 and loc.first.is_visible(timeout=400):
                return True
            loc = page.locator(f"text={label}").first
            if loc.count() > 0 and loc.is_visible(timeout=400):
                return True
    except Exception:
        pass
    return False


def _click_midi_export(page) -> bool:
    js_click_midi = """
        () => {
            // Prefer "MIDI Fixed tempo" — works without premium and is what
            // Stratum expects for clean step-aligned import. Time aligned is
            // a fallback (premium-only on most accounts).
            const labels = ['MIDI Fixed tempo','MIDI Time aligned','MIDI time aligned','Time aligned'];
            const all = Array.from(document.querySelectorAll('a, button, [role="menuitem"], li, span, div'));
            for (const el of all) {
                const t = (el.innerText || el.textContent || '').trim();
                if (!t) continue;
                for (const lbl of labels) {
                    if (t === lbl || t.startsWith(lbl)) {
                        const target = el.closest('a,button,[role="menuitem"],li') || el;
                        target.scrollIntoView({block: 'center'});
                        target.click();
                        return lbl;
                    }
                }
            }
            return null;
        }
    """
    try:
        return bool(page.evaluate(js_click_midi))
    except Exception:
        return False


def _is_valid_midi(path: Path) -> bool:
    try:
        return path.read_bytes()[:4] == b"MThd"
    except Exception:
        return False


def _find_recent_chordify_mid_download() -> Path | None:
    """Fallback: Chrome may also save a UUID-named copy in Downloads."""
    downloads = Path.home() / "Downloads"
    if not downloads.is_dir():
        return None
    candidates: list[Path] = []
    try:
        for p in downloads.iterdir():
            if p.is_file() and _is_valid_midi(p):
                candidates.append(p)
    except Exception:
        return None
    if not candidates:
        return None
    return max(candidates, key=lambda p: p.stat().st_mtime)


def _archive_midi_copy(output_midi: Path) -> None:
    try:
        APP_DIR.mkdir(parents=True, exist_ok=True)
        archive = APP_DIR / "chordify-last.mid"
        archive.write_bytes(output_midi.read_bytes())
        _dbg(f"archived midi -> {archive}")
    except Exception as exc:
        _dbg(f"archive midi failed: {exc}")


def _download_midi_time_aligned(page, output_midi: Path) -> None:
    """Download Chordify MIDI via kebab menu → MIDI Fixed tempo."""
    page.bring_to_front()
    time.sleep(0.6)

    _dbg("opening kebab menu...")
    deadline = time.time() + 45.0
    opened = False
    while time.time() < deadline:
        if _open_kebab_menu(page):
            _dbg("kebab menu opened")
            opened = True
            break
        time.sleep(0.8)
    if not opened:
        raise RuntimeError(
            "Could not open the 3-dot menu beside Share on Chordify song page."
        )

    output_midi.parent.mkdir(parents=True, exist_ok=True)
    part = output_midi.with_suffix(".part")

    with page.expect_download(timeout=180_000) as dl_info:
        if not _click_midi_menu_item(page):
            raise RuntimeError(
                "Found menu but could not click MIDI Fixed tempo / MIDI Time aligned."
            )
        _dbg("waiting for download event...")
        download = dl_info.value
        suggested = ""
        try:
            suggested = download.suggested_filename or ""
            _dbg(f"download started: {suggested}")
        except Exception:
            pass
        download.save_as(str(part))

    if not _is_valid_midi(part):
        _dbg(f"download rejected: not a MIDI file (suggested={suggested!r}, size={part.stat().st_size})")
        part.unlink(missing_ok=True)
        fallback = _find_recent_chordify_mid_download()
        if fallback is not None:
            _dbg(f"using fallback midi from Downloads: {fallback.name}")
            part.write_bytes(fallback.read_bytes())
        else:
            raise RuntimeError(
                "Downloaded file is not valid MIDI. "
                "Chrome may have saved a UUID file — try manual MIDI Fixed tempo once."
            )

    part.replace(output_midi)
    _archive_midi_copy(output_midi)
    _dbg(f"saved valid midi ({output_midi.stat().st_size} bytes) -> {output_midi}")


def _click_midi_menu_item(page) -> bool:
    """Click "MIDI Fixed tempo" via REAL mouse-coordinate clicks.

    Chordify's menu is rendered through a React Portal with the click
    handler attached at React's synthetic root. The reliable way to
    trigger it is ``page.mouse.click(x, y)`` — Playwright sends a CDP
    ``Input.dispatchMouseEvent`` which Chrome dispatches as a real
    user-gesture event. This bypasses every event-filtering issue we've
    hit with element.click(), locator.click() and DOM dispatchEvent.

    Strategy order (tries each, retries up to 4 cycles):
      A. Look up the item's bounding box via JS → mouse-move + click
         at the centre coordinates.
      B. Playwright locator with force=True (works for some menu styles).
      C. JS pointer-event dispatch (last-ditch fallback).
    """
    labels = ["MIDI Fixed tempo", "MIDI Time aligned", "MIDI time aligned"]

    for attempt in range(4):
        if not _menu_visible(page):
            _dbg(f"attempt {attempt+1}: menu not visible, reopening")
            _open_kebab_menu(page)
            time.sleep(1.0)

        # --- Strategy A — real mouse click at bounding-box centre. ---
        box = _find_menu_item_box(page, labels)
        if box is not None:
            x, y, label = box
            _dbg(f"strategy A: mouse.click at ({x:.0f},{y:.0f}) for '{label}'")
            try:
                page.mouse.move(x, y)
                time.sleep(0.1)
                page.mouse.down()
                time.sleep(0.05)
                page.mouse.up()
                time.sleep(0.4)
                _dbg("strategy A click dispatched")
                return True
            except Exception as exc:
                _dbg(f"strategy A failed: {exc}")

        # --- Strategy B — Playwright forced click. ---
        for label in labels:
            try:
                loc = page.get_by_text(label, exact=True).first
                if loc.count() > 0:
                    _dbg(f"strategy B: locator.click '{label}'")
                    try:
                        loc.scroll_into_view_if_needed(timeout=1500)
                    except Exception:
                        pass
                    loc.click(timeout=3000, force=True)
                    time.sleep(0.4)
                    return True
            except Exception as exc:
                _dbg(f"strategy B '{label}' failed: {exc}")

        # --- Strategy C — JS pointer-event dispatch. ---
        if _js_click_midi_fixed_tempo(page):
            _dbg("strategy C: JS dispatch claimed success")
            time.sleep(0.4)
            return True

        _dbg(f"attempt {attempt+1} all strategies failed, retrying")
        time.sleep(1.0)

    _dbg("ALL ATTEMPTS FAILED")
    return False


def _find_menu_item_box(page, labels):
    """Return (centre_x, centre_y, label) for the first visible menu
    item matching any of ``labels``, or None if not found.

    Coordinates are in CSS pixels relative to the viewport — exactly what
    ``page.mouse.click(x, y)`` expects. We use JS to traverse the DOM
    because Chordify's menu uses a Portal and the item might not match
    standard ``[role="menuitem"]`` selectors."""
    js = r"""
        (wantedLabels) => {
            // Outer loop over LABELS so the priority order is respected —
            // "MIDI Fixed tempo" wins over "MIDI Time aligned" even when
            // the latter appears first in the DOM.
            const all = Array.from(document.querySelectorAll(
                'a, button, [role="menuitem"], li, div, span'));
            for (const lbl of wantedLabels) {
                for (const el of all) {
                    const t = (el.innerText || el.textContent || '').trim();
                    if (!t || t.length > 80) continue;
                    if (t === lbl || t.startsWith(lbl)) {
                        const target = el.closest(
                            'button, a, [role="menuitem"], li'
                        ) || el;
                        const r = target.getBoundingClientRect();
                        if (r.width < 4 || r.height < 4) continue;
                        target.scrollIntoView({block: 'center'});
                        const rr = target.getBoundingClientRect();
                        return {
                            x: rr.left + rr.width / 2,
                            y: rr.top + rr.height / 2,
                            label: lbl,
                        };
                    }
                }
            }
            return null;
        }
    """
    try:
        result = page.evaluate(js, labels)
        if result and isinstance(result, dict):
            return (float(result["x"]), float(result["y"]), result["label"])
    except Exception as exc:
        _dbg(f"_find_menu_item_box error: {exc}")
    return None


def _js_click_midi_fixed_tempo(page) -> bool:
    """Last-resort: dispatch synthetic pointer + mouse + click events on
    the MIDI menu item DOM node. Used only when real mouse clicks fail."""
    js = r"""
        () => {
            // Outer loop over LABELS — priority order matters: Fixed tempo
            // is preferred (works without premium and gives clean step-
            // aligned MIDI). Time aligned is the fallback.
            const labels = ['MIDI Fixed tempo', 'MIDI Time aligned',
                            'MIDI time aligned', 'Time aligned'];
            const all = Array.from(document.querySelectorAll(
                'a, button, [role="menuitem"], li, div, span'));
            for (const lbl of labels) {
                for (const el of all) {
                    const t = (el.innerText || el.textContent || '').trim();
                    if (!t || t.length > 80) continue;
                    if (t === lbl || t.startsWith(lbl)) {
                        const target = el.closest(
                            'button, a, [role="menuitem"], li'
                        ) || el;
                        target.scrollIntoView({block: 'center'});
                        const rect = target.getBoundingClientRect();
                        const x = rect.left + rect.width / 2;
                        const y = rect.top + rect.height / 2;
                        const o = {bubbles: true, cancelable: true,
                                    clientX: x, clientY: y, button: 0,
                                    pointerType: 'mouse', isPrimary: true};
                        try { target.dispatchEvent(new PointerEvent('pointerover', o)); } catch(e) {}
                        try { target.dispatchEvent(new PointerEvent('pointerenter', o)); } catch(e) {}
                        target.dispatchEvent(new MouseEvent('mouseover', o));
                        target.dispatchEvent(new MouseEvent('mousemove', o));
                        try { target.dispatchEvent(new PointerEvent('pointerdown', o)); } catch(e) {}
                        target.dispatchEvent(new MouseEvent('mousedown', o));
                        try { target.dispatchEvent(new PointerEvent('pointerup', o)); } catch(e) {}
                        target.dispatchEvent(new MouseEvent('mouseup', o));
                        target.dispatchEvent(new MouseEvent('click', o));
                        try { target.click(); } catch(e) {}
                        return lbl;
                    }
                }
            }
            return null;
        }
    """
    try:
        result = page.evaluate(js)
        return result is not None
    except Exception:
        return False


def _goto_song_page_after_upload(browser, page, audio_path: Path):
    """Poll for /chords/ tab — skip Play again wait when page opens directly."""
    _dismiss_blocking_modals(page)
    _dbg("waiting for /chords/ song page after upload...")
    deadline = time.time() + 180.0
    last_log = 0.0

    while time.time() < deadline:
        for pg in _all_chordify_pages(browser):
            url = _live_url(pg)
            if "/chords/" in url:
                try:
                    pg.bring_to_front()
                except Exception:
                    pass
                _dbg(f"song page ready: {url}")
                return pg

        now = time.time()
        if now - last_log > 8.0:
            urls = [_live_url(p) for p in _all_chordify_pages(browser)]
            _dbg(f"poll: chordify tabs = {urls}")
            last_log = now

        home = _find_chordify_page(page.context) or page
        if _song_visible_in_play_again(home, audio_path):
            _dbg("play-again: song listed — opening chords page")
            if _open_song_from_play_again(home, audio_path):
                for pg in _all_chordify_pages(browser):
                    if "/chords/" in _live_url(pg):
                        return pg

        time.sleep(2.0)

    raise RuntimeError(
        f"Timed out waiting for Chordify song page for '{audio_path.stem}'."
    )


def automate_cdp(input_path: Path, output_midi: Path, port: int, bpm_hint: float | None) -> dict:
    if not input_path.is_file():
        raise FileNotFoundError(str(input_path))

    from playwright.sync_api import sync_playwright

    _dbg(f"===== chordify automation run =====")
    _dbg(f"input={input_path}")
    _dbg(f"output={output_midi}")
    _dbg(f"port={port}")

    with _run_lock():
        with sync_playwright() as p:
            browser = _connect_browser(p, port)
            _dbg("connected to Chrome via CDP")
            try:
                page = _pick_page(browser)
                _dbg(f"picked page url={page.url}")
                _goto_upload_home(page)

                _dbg("uploading audio...")
                page = _upload_audio(page, input_path)
                _dbg(f"upload dispatched, url={_live_url(page)}")
                page = _goto_song_page_after_upload(browser, page, input_path)
                _dbg(f"song page ready, url={_live_url(page)}")

                _download_midi_time_aligned(page, output_midi)
                _dbg("download flow complete")
            finally:
                # CDP attach — disconnect only; never close the user's Chrome.
                _dbg("playwright disconnected (Chrome stays open)")

    if not output_midi.is_file() or not _is_valid_midi(output_midi):
        raise RuntimeError("MIDI download failed or file is not valid .mid (missing MThd header)")

    bpm = bpm_hint or 120.0
    m = re.search(r"(\d{2,3})\s*bpm", output_midi.stem, re.I)
    if m:
        bpm = float(m.group(1))

    notes: list[dict] = []
    try:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from midi_to_bass import midi_file_to_bass_notes

        notes = midi_file_to_bass_notes(output_midi, bpm)
        _dbg(f"parsed {len(notes)} bass notes from midi")
    except Exception as exc:
        _dbg(f"midi_to_bass failed: {exc}")

    return {
        "ok": True,
        "engine": "chordify-cdp",
        "midi_path": str(output_midi.resolve()),
        "bpm": bpm,
        "notes": notes,
        "note_count": len(notes),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--login", action="store_true")
    parser.add_argument("--use-cdp", action="store_true")
    parser.add_argument("--cdp-port", type=int, default=0)
    parser.add_argument("--input")
    parser.add_argument("--output")
    parser.add_argument("--bpm", type=float, default=0.0)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    port = args.cdp_port if args.cdp_port > 0 else _read_cdp_port()

    if args.login:
        return cmd_login_cdp(port)

    if not args.input or not args.output:
        parser.error("--input and --output required")

    bpm_hint = args.bpm if args.bpm > 0 else None
    try:
        result = automate_cdp(
            Path(args.input).resolve(),
            Path(args.output).resolve(),
            port,
            bpm_hint,
        )
        payload = json.dumps(result, ensure_ascii=False)
        if args.json:
            sys.stdout.buffer.write(payload.encode("utf-8"))
            sys.stdout.buffer.write(b"\n")
        return 0
    except Exception as exc:
        err = {"ok": False, "error": str(exc), "traceback": traceback.format_exc()}
        payload = json.dumps(err, ensure_ascii=False)
        sys.stderr.buffer.write(payload.encode("utf-8"))
        sys.stderr.buffer.write(b"\n")
        if args.json:
            print(payload, file=sys.stdout)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
