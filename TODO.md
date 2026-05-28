# Stratum DAW - TODO List

## 🔴 HIGH PRIORITY

### AI Panel - Complete Rewrite Needed
**Status:** Current implementation has critical threading issues causing crashes

**Problems:**
- Crashes when sending messages or using preset buttons
- Thread safety issues with UI updates
- `addMessage()` and `updateChatDisplay()` not properly synchronized
- Viewport width calculations causing crashes
- Text height estimation unreliable

**Solution Required:**
- [ ] Redesign AI panel with proper JUCE threading model
- [ ] Use `AsyncUpdater` or `Timer` for UI updates instead of direct calls
- [ ] Implement message queue with thread-safe access
- [ ] Add proper mutex locks for shared data
- [ ] Test thoroughly with all preset buttons and commands
- [ ] Consider using `ListBox` instead of manual label management
- [ ] Add error handling and logging for debugging

**Alternative Approach:**
- [ ] Research JUCE best practices for chat-style interfaces
- [ ] Look into using `TextEditor` with read-only mode for chat display
- [ ] Consider third-party JUCE chat components

---

## 🟡 MEDIUM PRIORITY

### MIDI Generation
- [ ] Add more genre presets (Lo-fi, Reggaeton, Afrobeat, etc.)
- [ ] Implement velocity variation and humanization
- [ ] Add swing/groove settings
- [ ] Support for longer patterns (64, 128 steps)
- [ ] MIDI export functionality

### Channel Rack
- [ ] Fix channel selection when generating MIDI to specific channels
- [ ] Add visual feedback when MIDI is generated
- [ ] Implement channel mute/solo functionality
- [ ] Add volume and pan controls per channel
- [ ] Support for more than default channels

### Audio Engine
- [ ] Implement actual audio playback for MIDI notes
- [ ] Add sample loading for drum channels
- [ ] Integrate VST plugin support
- [ ] Add audio recording functionality
- [ ] Implement mixer routing

### Piano Roll
- [ ] Add note editing (drag, resize, delete)
- [ ] Implement velocity editing
- [ ] Add quantization options
- [ ] Support for multiple note selection
- [ ] Copy/paste functionality

---

## 🟢 LOW PRIORITY

### UI/UX Improvements
- [ ] Add keyboard shortcuts
- [ ] Implement undo/redo system
- [ ] Add project save/load functionality
- [ ] Improve drag-and-drop for samples
- [ ] Add tooltips for all controls
- [ ] Dark/light theme toggle

### Browser Panel
- [ ] Implement file browser for samples
- [ ] Add preset browser for instruments
- [ ] Search functionality
- [ ] Favorites/bookmarks system

### Transport Bar
- [ ] Add tempo tap functionality
- [ ] Implement time signature changes
- [ ] Add loop markers
- [ ] Metronome functionality

### Mixer
- [ ] Add EQ per channel
- [ ] Implement send/return effects
- [ ] Add master channel controls
- [ ] VU meters and level monitoring

---

## 📝 DOCUMENTATION

- [ ] Add user manual
- [ ] Create developer documentation
- [ ] Add code comments for complex sections
- [ ] Write build instructions for different platforms
- [ ] Create video tutorials

---

## 🐛 KNOWN BUGS

1. **AI Panel crashes** - See HIGH PRIORITY section above
2. Channel selection not working properly with preset buttons
3. Viewport scroll position sometimes incorrect
4. Window dragging can be glitchy on some systems

---

## 🚀 FUTURE FEATURES

- [ ] Cloud sync for projects
- [ ] Collaboration features
- [ ] AI-powered melody generation (actual AI, not just presets)
- [ ] Integration with external MIDI controllers
- [ ] Mobile companion app
- [ ] Plugin marketplace
- [ ] Stem export
- [ ] Video timeline integration

---

## 📊 PERFORMANCE

- [ ] Profile application for bottlenecks
- [ ] Optimize audio rendering
- [ ] Reduce memory usage
- [ ] Improve startup time
- [ ] Add loading indicators for long operations

---

**Last Updated:** 2026-05-06
**Version:** 1.0.0-alpha
