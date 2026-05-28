#!/usr/bin/env python3
"""
Chordify automation via Chrome DevTools Protocol (CDP).
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import time
import traceback
from pathlib import Path

APP_DIR = Path.home() / "AppData" / "Roaming" / "Stratum DAW"
READY_FLAG = APP_DIR / "chordify.ready"
CONFIG_FILE = APP_DIR / "chordify.config"
DEFAULT_CDP_PORT = 9222


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


def _pick_page(browser):
    best = None
    for context in browser.contexts:
        for page in context.pages:
            url = page.url or ""
            if "chordify.net/chords/" in url:
                return page
            if "chordify.net" in url and "cloudflare" not in url.lower():
                best = page
    if best is not None:
        return best
    context = browser.contexts[0] if browser.contexts else browser.new_context()
    page = context.new_page()
    page.goto("https://chordify.net/", wait_until="domcontentloaded", timeout=120_000)
    return page


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


def _upload_audio(page, audio_path: Path) -> None:
    if "/chords/" in (page.url or ""):
        return

    file_input = page.locator("input[type='file']").first
    if file_input.count() > 0:
        file_input.set_input_files(str(audio_path))
        return

    for sel in ("text=Upload Song", "text=Upload song", "button:has-text('Upload')"):
        try:
            with page.expect_file_chooser(timeout=8000) as fc_info:
                page.locator(sel).first.click(timeout=8000)
            fc_info.value.set_files(str(audio_path))
            return
        except Exception:
            continue
    raise RuntimeError("Upload control not found on chordify.net home page.")


def _wait_for_song_page(page, timeout_sec: int = 180) -> None:
    deadline = time.time() + timeout_sec
    while time.time() < deadline:
        if "/chords/" in (page.url or ""):
            break
        time.sleep(0.5)
    else:
        raise RuntimeError("Timed out waiting for Chordify song page")

    for sel in ("[class*='chord-card']", "[class*='chord']", "text=/Cm|Fm|G7|m7|maj/i"):
        try:
            page.locator(sel).first.wait_for(state="visible", timeout=60_000)
            return
        except Exception:
            continue


def _open_kebab_menu(page) -> bool:
    """Open the 3-dot (kebab) menu on Chordify song page header."""
    page.bring_to_front()

    selectors = [
        "button[aria-label*='More' i]",
        "button[aria-label*='Menu' i]",
        "button[aria-label*='Options' i]",
        "button[data-testid*='menu' i]",
        "button[data-testid*='more' i]",
        "[role='button'][aria-haspopup='true']",
    ]
    for sel in selectors:
        try:
            loc = page.locator(sel).first
            if loc.count() > 0 and loc.is_visible(timeout=2000):
                loc.click(timeout=3000)
                time.sleep(0.5)
                if _menu_visible(page):
                    return True
        except Exception:
            continue

    # Fallback: find any header-ish button whose innerHTML hints kebab (3 vertical dots)
    js_click = """
        () => {
            const buttons = Array.from(document.querySelectorAll('button'));
            for (const b of buttons) {
                const r = b.getBoundingClientRect();
                if (r.width < 8 || r.height < 8) continue;
                if (r.top > 300) continue;
                const svg = b.querySelector('svg');
                if (!svg) continue;
                const html = svg.outerHTML.toLowerCase();
                const looksKebab =
                    (html.match(/circle/g) || []).length >= 3
                    || html.includes('more')
                    || html.includes('kebab');
                if (looksKebab) {
                    b.scrollIntoView({block: 'center'});
                    b.click();
                    return true;
                }
            }
            return false;
        }
    """
    try:
        if page.evaluate(js_click):
            time.sleep(0.6)
            if _menu_visible(page):
                return True
    except Exception:
        pass

    return False


def _menu_visible(page) -> bool:
    try:
        for label in ("MIDI Time aligned", "MIDI time aligned", "MIDI Fixed tempo"):
            if page.locator(f"text={label}").first.is_visible(timeout=1500):
                return True
    except Exception:
        pass
    return False


def _click_midi_export(page) -> bool:
    js_click_midi = """
        () => {
            const labels = ['MIDI Time aligned','MIDI time aligned','Time aligned','MIDI Fixed tempo'];
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


def _download_midi_time_aligned(page, output_midi: Path) -> None:
    page.bring_to_front()
    time.sleep(0.6)

    deadline = time.time() + 30.0
    while time.time() < deadline:
        if _menu_visible(page):
            break
        if _open_kebab_menu(page):
            break
        time.sleep(1.0)
    else:
        raise RuntimeError(
            "Could not open the 3-dot menu on Chordify song page. "
            "Open it manually (next to Share/Heart) so MIDI Time aligned is visible, then retry."
        )

    output_midi.parent.mkdir(parents=True, exist_ok=True)

    with page.expect_download(timeout=180_000) as dl_info:
        clicked = False
        for attempt in range(3):
            if not _menu_visible(page):
                _open_kebab_menu(page)
                time.sleep(0.4)
            if _click_first(
                page,
                [
                    "text=MIDI Time aligned",
                    "text=MIDI time aligned",
                    "text=Time aligned",
                    "text=MIDI Fixed tempo",
                ],
                timeout_ms=4000,
            ):
                clicked = True
                break
            if _click_midi_export(page):
                clicked = True
                break
            time.sleep(1.0)

        if not clicked:
            raise RuntimeError(
                "Found menu but could not click MIDI Time aligned. "
                "Premium account required for MIDI export."
            )
        download = dl_info.value
        download.save_as(str(output_midi))


def automate_cdp(input_path: Path, output_midi: Path, port: int, bpm_hint: float | None) -> dict:
    if not input_path.is_file():
        raise FileNotFoundError(str(input_path))

    from playwright.sync_api import sync_playwright

    with sync_playwright() as p:
        browser = _connect_browser(p, port)
        try:
            page = _pick_page(browser)
            _wait_past_cloudflare(page, timeout_sec=60)

            if "/chords/" not in (page.url or ""):
                _upload_audio(page, input_path)
            _wait_for_song_page(page)

            _download_midi_time_aligned(page, output_midi)
        finally:
            browser.close()

    if not output_midi.is_file() or output_midi.stat().st_size < 32:
        raise RuntimeError("MIDI download failed or file empty")

    bpm = bpm_hint or 120.0
    m = re.search(r"(\d{2,3})\s*bpm", output_midi.stem, re.I)
    if m:
        bpm = float(m.group(1))

    notes: list[dict] = []
    try:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        from midi_to_bass import midi_file_to_all_notes

        notes = midi_file_to_all_notes(output_midi, bpm)
    except Exception:
        pass

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
