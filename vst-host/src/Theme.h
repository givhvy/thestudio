#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// FL Studio + Engineered/Skeuomorphic theme - matches design spec
namespace Theme
{
    enum class Preset { Default, Blue, Purple, Emerald, Crimson, Gold, FrutigerAero, LiquidGlass };

    // True while the Frutiger Aero style is active — paint methods can check
    // this to switch to the bright glossy aqua/green look + bubbles.
    inline bool aeroMode = false;
    // True while the iOS 26 Liquid Glass style is active. Reuses the same paint
    // routes as aeroMode, but the aero helpers re-tint to a clean frosted glass
    // palette when this flag is on.
    inline bool liquidGlassMode = false;

    // Backgrounds (zinc palette from spec)
    inline juce::Colour zinc950 = juce::Colour(0xff09090b);  // Darkest
    inline juce::Colour zinc900 = juce::Colour(0xff18181b);
    inline juce::Colour zinc850 = juce::Colour(0xff141417);
    inline juce::Colour zinc825 = juce::Colour(0xff121214);
    inline juce::Colour zinc800 = juce::Colour(0xff27272a);
    inline juce::Colour zinc750 = juce::Colour(0xff2a2a2e);
    inline juce::Colour zinc700 = juce::Colour(0xff3f3f46);
    inline juce::Colour zinc600 = juce::Colour(0xff52525b);
    inline juce::Colour zinc500 = juce::Colour(0xff71717a);
    inline juce::Colour zinc400 = juce::Colour(0xffa1a1aa);
    inline juce::Colour zinc300 = juce::Colour(0xffd4d4d8);
    inline juce::Colour zinc200 = juce::Colour(0xffe4e4e7);
    inline juce::Colour zinc100 = juce::Colour(0xfffafafa);
    
    // Aliases (backwards compatibility)
    inline juce::Colour bg0 = zinc950;
    inline juce::Colour bg1 = zinc825;
    inline juce::Colour bg2 = zinc850;
    inline juce::Colour bg3 = zinc750;
    inline juce::Colour bg4 = zinc900;
    inline juce::Colour bg5 = zinc850;
    inline juce::Colour bg6 = zinc800;
    inline juce::Colour bg7 = zinc800;
    inline juce::Colour bg8 = zinc700;
    
    inline juce::Colour text1 = zinc100;
    inline juce::Colour text2 = zinc200;
    inline juce::Colour text3 = zinc300;
    inline juce::Colour text4 = zinc400;
    inline juce::Colour text5 = zinc500;
    inline juce::Colour text6 = zinc600;
    inline juce::Colour text7 = zinc700;
    
    // Orange (FL Studio brand + spec)
    inline juce::Colour orange1 = juce::Colour(0xfffb923c);  // orange-400
    inline juce::Colour orange2 = juce::Colour(0xfff97316);  // orange-500 main
    inline juce::Colour orange3 = juce::Colour(0xffea580c);  // orange-600
    inline juce::Colour orange4 = juce::Colour(0xffc2410c);  // orange-700
    inline juce::Colour orange5 = juce::Colour(0xff7c2d12);  // orange-900
    inline juce::Colour orange6 = juce::Colour(0xff431407);  // orange-950
    
    // Reverb purple
    inline juce::Colour purple = juce::Colour(0xff8b5cf6);
    inline juce::Colour purpleBright = juce::Colour(0xffa78bfa);
    
    // Status colors
    inline juce::Colour green1 = juce::Colour(0xff86efac);
    inline juce::Colour green2 = juce::Colour(0xff22c55e);
    inline juce::Colour green3 = juce::Colour(0xff16a34a);
    inline juce::Colour red1 = juce::Colour(0xfffca5a5);
    inline juce::Colour red2 = juce::Colour(0xffef4444);
    inline juce::Colour yellow1 = juce::Colour(0xfffde68a);
    inline juce::Colour yellow2 = juce::Colour(0xfff59e0b);
    inline juce::Colour blue = juce::Colour(0xff3b82f6);
    
    inline juce::Colour accent = orange2;
    inline juce::Colour accentBright = orange1;
    inline juce::Colour accentDim = orange3;
    inline juce::Colour green = green2;
    inline juce::Colour red = red2;
    inline juce::Colour yellow = yellow2;
    inline juce::Colour reverbPurple = purple;
    inline juce::Colour reverbPurpleBright = purpleBright;
    inline juce::Colour border1 = juce::Colours::black;
    inline juce::Colour border2 = zinc900;
    inline juce::Colour border3 = zinc700;

    inline Preset currentPreset = Preset::Default;

    inline void setAccentRamp(juce::Colour bright, juce::Colour main, juce::Colour dim,
                              juce::Colour dark, juce::Colour darker, juce::Colour darkest)
    {
        orange1 = bright;
        orange2 = main;
        orange3 = dim;
        orange4 = dark;
        orange5 = darker;
        orange6 = darkest;
        accent = orange2;
        accentBright = orange1;
        accentDim = orange3;
    }

    inline void applyPreset(Preset preset)
    {
        currentPreset = preset;
        liquidGlassMode = (preset == Preset::LiquidGlass);
        // LiquidGlass piggybacks on the aero paint routes so every panel/button
        // gets re-styled automatically — the helpers re-tint when liquidGlassMode is set.
        aeroMode = (preset == Preset::FrutigerAero) || (preset == Preset::LiquidGlass);
        switch (preset)
        {
            case Preset::FrutigerAero:
                // Glossy aqua → cyan → teal ramp — the iconic Frutiger Aero accent.
                setAccentRamp(juce::Colour(0xff7ee8fa), juce::Colour(0xff22d3ee), juce::Colour(0xff06b6d4),
                              juce::Colour(0xff0891b2), juce::Colour(0xff155e75), juce::Colour(0xff083344));
                break;
            case Preset::LiquidGlass:
                // iOS 26 cool-mint glass accent — pale blue → cyan, very soft.
                setAccentRamp(juce::Colour(0xffbfe4ff), juce::Colour(0xff8ec5f0), juce::Colour(0xff5aa6d8),
                              juce::Colour(0xff3a85b8), juce::Colour(0xff224f72), juce::Colour(0xff132a40));
                break;
            case Preset::Blue:
                setAccentRamp(juce::Colour(0xff60a5fa), juce::Colour(0xff3b82f6), juce::Colour(0xff2563eb),
                              juce::Colour(0xff1d4ed8), juce::Colour(0xff1e3a8a), juce::Colour(0xff172554));
                break;
            case Preset::Purple:
                setAccentRamp(juce::Colour(0xffc084fc), juce::Colour(0xffa855f7), juce::Colour(0xff9333ea),
                              juce::Colour(0xff7e22ce), juce::Colour(0xff581c87), juce::Colour(0xff3b0764));
                break;
            case Preset::Emerald:
                setAccentRamp(juce::Colour(0xff6ee7b7), juce::Colour(0xff10b981), juce::Colour(0xff059669),
                              juce::Colour(0xff047857), juce::Colour(0xff064e3b), juce::Colour(0xff022c22));
                break;
            case Preset::Crimson:
                setAccentRamp(juce::Colour(0xfffb7185), juce::Colour(0xfff43f5e), juce::Colour(0xffe11d48),
                              juce::Colour(0xffbe123c), juce::Colour(0xff881337), juce::Colour(0xff4c0519));
                break;
            case Preset::Gold:
                setAccentRamp(juce::Colour(0xfffacc15), juce::Colour(0xffeab308), juce::Colour(0xffca8a04),
                              juce::Colour(0xffa16207), juce::Colour(0xff713f12), juce::Colour(0xff422006));
                break;
            case Preset::Default:
            default:
                setAccentRamp(juce::Colour(0xfffb923c), juce::Colour(0xfff97316), juce::Colour(0xffea580c),
                              juce::Colour(0xffc2410c), juce::Colour(0xff7c2d12), juce::Colour(0xff431407));
                break;
        }
    }
    
    // ─── Frutiger Aero glossy surface (bright sky-blue→white→green glass) ───
    // Fills `bounds` with the signature glossy gradient, a top sheen, and a few
    // translucent bubbles — the look that defines the Frutiger Aero aesthetic.
    inline void drawAeroGloss(juce::Graphics& g, juce::Rectangle<float> bounds, float bubbleSeed = 1.0f)
    {
        if (liquidGlassMode)
        {
            // iOS 26 Liquid Glass wallpaper: bright mesh of pastel blobs
            // (pink → lavender → blue → mint) — the kind of vivid wallpaper
            // Liquid Glass is designed to refract over.
            const float W = bounds.getWidth();
            const float H = bounds.getHeight();
            // Base soft lavender.
            g.setColour(juce::Colour(0xffe9e6f5));
            g.fillRect(bounds);
            // Color blobs via radial gradients.
            auto blob = [&](float fx, float fy, float r, juce::Colour c)
            {
                juce::ColourGradient gr(c.withAlpha(0.85f),
                                        bounds.getX() + W * fx, bounds.getY() + H * fy,
                                        c.withAlpha(0.0f),
                                        bounds.getX() + W * fx + r, bounds.getY() + H * fy + r, true);
                g.setGradientFill(gr);
                g.fillRect(bounds);
            };
            const float bigR = juce::jmax(W, H) * 0.55f;
            blob(0.15f, 0.20f, bigR, juce::Colour(0xffffc8dd));  // pink
            blob(0.85f, 0.15f, bigR, juce::Colour(0xffbde0fe));  // sky
            blob(0.10f, 0.90f, bigR, juce::Colour(0xffcdb4f7));  // lavender
            blob(0.90f, 0.95f, bigR, juce::Colour(0xffb5ead7));  // mint
            blob(0.55f, 0.55f, bigR * 0.8f, juce::Colour(0xffffe5b4)); // peach
            // Soft top-to-bottom luminance — keeps it readable.
            juce::ColourGradient lum(juce::Colours::white.withAlpha(0.20f), bounds.getX(), bounds.getY(),
                                     juce::Colours::white.withAlpha(0.0f), bounds.getX(),
                                     bounds.getBottom(), false);
            g.setGradientFill(lum);
            g.fillRect(bounds);
            return;
        }
        // Sky-blue at top → bright white middle → fresh aqua-green at the bottom.
        juce::ColourGradient grad(juce::Colour(0xff2bb8f1), bounds.getX(), bounds.getY(),
                                  juce::Colour(0xff39d07a), bounds.getX(), bounds.getBottom(), false);
        grad.addColour(0.45, juce::Colour(0xffd6f6ff));
        grad.addColour(0.55, juce::Colour(0xffeafff2));
        g.setGradientFill(grad);
        g.fillRect(bounds);

        // Glossy top sheen (bright white fading down over the upper half).
        juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.55f), bounds.getX(), bounds.getY(),
                                   juce::Colours::white.withAlpha(0.0f), bounds.getX(),
                                   bounds.getY() + bounds.getHeight() * 0.5f, false);
        g.setGradientFill(sheen);
        g.fillRect(bounds.withHeight(bounds.getHeight() * 0.5f));

        // Floating bubbles — a couple of soft translucent circles.
        auto bubble = [&](float fx, float fy, float r)
        {
            const float cx = bounds.getX() + bounds.getWidth() * fx;
            const float cy = bounds.getY() + bounds.getHeight() * fy;
            g.setColour(juce::Colours::white.withAlpha(0.18f));
            g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.2f);
            // little highlight glint
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.fillEllipse(cx - r * 0.35f, cy - r * 0.45f, r * 0.35f, r * 0.35f);
        };
        const float h = bounds.getHeight();
        bubble(0.62f, 0.62f, h * 0.30f * bubbleSeed);
        bubble(0.74f, 0.30f, h * 0.16f * bubbleSeed);
        bubble(0.90f, 0.70f, h * 0.22f * bubbleSeed);
    }

    // Frutiger Aero glass PANEL fill — a glossy aqua-teal surface with a bright
    // top sheen and faint bubbles, dark enough that light text stays readable.
    // Use this as the background of every major panel when aeroMode is on.
    inline void drawAeroPanel(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        if (liquidGlassMode)
        {
            // iOS 26 Liquid Glass card: white frosted glass with strong refraction.
            // Rounded shape (squircle-ish), drop shadow below, bright rim on top,
            // very translucent so the wallpaper mesh shows through.
            const float radius = juce::jmin(28.0f, juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.18f);
            // Drop shadow layers — soft, large, offset down.
            for (int i = 8; i > 0; --i)
            {
                g.setColour(juce::Colours::black.withAlpha(0.06f / (float)i));
                g.fillRoundedRectangle(bounds.translated(0.0f, (float)i * 1.5f).expanded((float)i * 0.4f), radius);
            }
            // Frosted glass body — very translucent white with a tiny luminance ramp.
            juce::ColourGradient glass(juce::Colour(0xffffffff).withAlpha(0.62f),
                                       bounds.getX(), bounds.getY(),
                                       juce::Colour(0xfff5f5fa).withAlpha(0.45f),
                                       bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill(glass);
            g.fillRoundedRectangle(bounds, radius);
            // Inner top specular sheen (the iOS rim light).
            juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.85f),
                                       bounds.getX(), bounds.getY(),
                                       juce::Colours::white.withAlpha(0.0f), bounds.getX(),
                                       bounds.getY() + bounds.getHeight() * 0.35f, false);
            g.setGradientFill(sheen);
            g.fillRoundedRectangle(bounds.withHeight(bounds.getHeight() * 0.5f).reduced(1.5f, 1.5f),
                                   radius * 0.75f);
            // Hairline white rim — refraction edge.
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
            // Subtle inner dark line just below the top rim for crispness.
            g.setColour(juce::Colours::black.withAlpha(0.06f));
            g.drawRoundedRectangle(bounds.reduced(1.5f), radius - 1.0f, 0.6f);
            return;
        }
        // Aqua-teal vertical glass gradient.
        juce::ColourGradient grad(juce::Colour(0xff1f6f93), bounds.getX(), bounds.getY(),
                                  juce::Colour(0xff0a2e3e), bounds.getX(), bounds.getBottom(), false);
        grad.addColour(0.5, juce::Colour(0xff124b63));
        g.setGradientFill(grad);
        g.fillRect(bounds);

        // Bright top sheen (glass reflection over the upper third).
        juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.22f), bounds.getX(), bounds.getY(),
                                   juce::Colours::white.withAlpha(0.0f), bounds.getX(),
                                   bounds.getY() + bounds.getHeight() * 0.35f, false);
        g.setGradientFill(sheen);
        g.fillRect(bounds.withHeight(bounds.getHeight() * 0.35f));

        // A few soft bubbles low in the panel.
        auto bubble = [&](float fx, float fy, float r)
        {
            const float cx = bounds.getX() + bounds.getWidth() * fx;
            const float cy = bounds.getY() + bounds.getHeight() * fy;
            g.setColour(juce::Colours::white.withAlpha(0.06f));
            g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.16f));
            g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.2f);
            g.setColour(juce::Colours::white.withAlpha(0.30f));
            g.fillEllipse(cx - r * 0.4f, cy - r * 0.5f, r * 0.4f, r * 0.4f);
        };
        bubble(0.18f, 0.78f, bounds.getHeight() * 0.10f);
        bubble(0.55f, 0.88f, bounds.getHeight() * 0.07f);
        bubble(0.85f, 0.72f, bounds.getHeight() * 0.12f);
    }

    // Glossy Aero glass button (rounded, aqua, strong top highlight).
    inline void drawAeroButton(juce::Graphics& g, juce::Rectangle<float> bounds,
                               bool active, float radius = 8.0f)
    {
        if (liquidGlassMode)
        {
            // iOS 26 Liquid Glass pill button: full-capsule shape, frosted glass,
            // floating with shadow, accent tint when active.
            const float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
            // Drop shadow underneath (floating effect).
            for (int i = 4; i > 0; --i)
            {
                g.setColour(juce::Colours::black.withAlpha(0.05f / (float)i));
                g.fillRoundedRectangle(bounds.translated(0.0f, (float)i * 1.2f).expanded((float)i * 0.3f), r);
            }
            // Frosted body — when active, tint with a soft blue accent (iOS selection).
            const juce::Colour topC = active ? juce::Colour(0xff86b8ff).withAlpha(0.95f)
                                             : juce::Colour(0xffffffff).withAlpha(0.78f);
            const juce::Colour botC = active ? juce::Colour(0xff5191e6).withAlpha(0.95f)
                                             : juce::Colour(0xfff0f3fa).withAlpha(0.60f);
            juce::ColourGradient grad(topC, 0.0f, bounds.getY(),
                                      botC, 0.0f, bounds.getBottom(), false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(bounds, r);
            // Strong inner top specular highlight (glass refraction edge).
            g.setColour(juce::Colours::white.withAlpha(active ? 0.45f : 0.85f));
            g.fillRoundedRectangle(bounds.withHeight(bounds.getHeight() * 0.50f).reduced(2.0f, 1.5f), r * 0.85f);
            // Hairline outer rim.
            g.setColour(juce::Colours::white.withAlpha(0.95f));
            g.drawRoundedRectangle(bounds.reduced(0.5f), r, 1.0f);
            return;
        }
        juce::ColourGradient grad(
            active ? juce::Colour(0xff5fe0ff) : juce::Colour(0xff2aa7cc), 0.0f, bounds.getY(),
            active ? juce::Colour(0xff1493c4) : juce::Colour(0xff0d5d7a), 0.0f, bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, radius);
        // Glossy upper-half highlight.
        g.setColour(juce::Colours::white.withAlpha(active ? 0.55f : 0.40f));
        g.fillRoundedRectangle(bounds.withHeight(bounds.getHeight() * 0.45f).reduced(1.5f, 1.0f), radius * 0.7f);
        // Crisp rim.
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
    }

    // ─── DRAWING HELPERS (Engineered/Skeuomorphic style) ───────────────────
    
    // Brushed metal texture overlay (repeating vertical lines)
    inline void drawBrushedMetalOverlay(juce::Graphics& g, juce::Rectangle<float> bounds, float opacity = 0.04f)
    {
        g.saveState();
        g.reduceClipRegion(bounds.toNearestInt());
        g.setColour(juce::Colours::white.withAlpha(opacity));
        for (int x = (int)bounds.getX(); x < bounds.getRight(); x += 4)
            g.drawVerticalLine(x, bounds.getY(), bounds.getBottom());
        g.restoreState();
    }
    
    // Volumetric light glint at top-left (radial gradient)
    inline void drawVolumetricLight(juce::Graphics& g, juce::Rectangle<float> bounds, float opacity = 0.06f)
    {
        juce::ColourGradient grad(
            juce::Colours::white.withAlpha(opacity), bounds.getX(), bounds.getY(),
            juce::Colours::white.withAlpha(0.0f), 
            bounds.getX() + bounds.getWidth() * 0.6f, 
            bounds.getY() + bounds.getHeight() * 0.6f, 
            true
        );
        g.setGradientFill(grad);
        g.fillRect(bounds);
    }
    
    // Engineered chassis (rounded panel with diagonal gradient + inner border)
    inline void drawChassis(juce::Graphics& g, juce::Rectangle<float> bounds, float radius = 16.0f)
    {
        // Outer shadow (multi-layer)
        for (int i = 4; i > 0; --i)
        {
            g.setColour(juce::Colours::black.withAlpha(0.15f / i));
            g.fillRoundedRectangle(bounds.translated(i * 2.0f, i * 4.0f), radius);
        }
        
        // Main fill - 135deg diagonal gradient from #2a2a2e to #121214
        juce::ColourGradient grad(
            zinc750, bounds.getX(), bounds.getY(),
            zinc825, bounds.getRight(), bounds.getBottom(), false
        );
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, radius);
        
        // Inner top-left highlight (1px white at 10%)
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
        
        // Brushed metal texture
        drawBrushedMetalOverlay(g, bounds, 0.03f);
        
        // Volumetric light at top-left
        drawVolumetricLight(g, bounds);
        
        // Border
        g.setColour(zinc700);
        g.drawRoundedRectangle(bounds, radius, 1.0f);
    }
    
    // Fastener screw (rotated slot)
    inline void drawScrew(juce::Graphics& g, juce::Rectangle<float> bounds, float rotation = 30.0f)
    {
        // Shadow
        g.setColour(juce::Colours::black.withAlpha(0.9f));
        g.fillEllipse(bounds.translated(0, 1.5f));
        
        // Body gradient
        juce::ColourGradient grad(
            zinc600, bounds.getX(), bounds.getY(),
            zinc800, bounds.getRight(), bounds.getBottom(), false
        );
        g.setGradientFill(grad);
        g.fillEllipse(bounds);
        
        // Top highlight
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawEllipse(bounds.reduced(0.5f), 0.5f);
        
        // Slot (rotated line)
        auto cx = bounds.getCentreX();
        auto cy = bounds.getCentreY();
        auto len = bounds.getWidth() * 0.7f;
        auto rad = juce::degreesToRadians(rotation);
        float x1 = cx - std::cos(rad) * len / 2.0f;
        float y1 = cy - std::sin(rad) * len / 2.0f;
        float x2 = cx + std::cos(rad) * len / 2.0f;
        float y2 = cy + std::sin(rad) * len / 2.0f;
        g.setColour(zinc950);
        g.drawLine(x1, y1, x2, y2, 1.5f);
    }
    
    // Glowing LED with bright core
    inline void drawGlowLED(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour color, bool on = true)
    {
        if (on)
        {
            // Outer glow (large, soft)
            for (int i = 3; i > 0; --i)
            {
                g.setColour(color.withAlpha(0.15f / i));
                g.fillEllipse(bounds.expanded(i * 2.0f));
            }
            
            // Body
            juce::ColourGradient grad(
                color.brighter(0.3f), bounds.getCentreX() - bounds.getWidth() * 0.2f,
                bounds.getCentreY() - bounds.getHeight() * 0.2f,
                color.darker(0.3f), bounds.getRight(), bounds.getBottom(), true);
            g.setGradientFill(grad);
            g.fillEllipse(bounds);
            
            // Bright white core (top-left)
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.fillEllipse(bounds.getX() + bounds.getWidth() * 0.2f,
                           bounds.getY() + bounds.getHeight() * 0.2f,
                           bounds.getWidth() * 0.4f, bounds.getHeight() * 0.4f);
            
            // Sharp edge highlight
            g.setColour(juce::Colours::white.withAlpha(0.4f));
            g.drawEllipse(bounds.reduced(0.5f), 0.5f);
        }
        else
        {
            // Off: dark recessed
            g.setColour(zinc900);
            g.fillEllipse(bounds);
            // Inner shadow
            g.setColour(juce::Colours::black.withAlpha(0.9f));
            g.drawEllipse(bounds.reduced(0.5f), 1.0f);
            // Tiny top highlight
            g.setColour(juce::Colours::white.withAlpha(0.05f));
            g.drawEllipse(bounds.translated(0, -0.5f), 0.5f);
        }
    }
    
    // Mechanical button (skeuomorphic with gradient + inset highlights)
    inline void drawMechanicalButton(juce::Graphics& g, juce::Rectangle<float> bounds, 
                                       bool active = false, juce::Colour activeColor = orange2)
    {
        // Outer shadow
        for (int i = 3; i > 0; --i)
        {
            g.setColour(juce::Colours::black.withAlpha(0.2f / i));
            g.fillRoundedRectangle(bounds.translated(0, i * 2.0f), 8.0f);
        }
        
        // Body gradient
        if (active)
        {
            juce::ColourGradient grad(
                activeColor.brighter(0.2f), 0.0f, bounds.getY(),
                activeColor.darker(0.4f), 0.0f, bounds.getBottom(), false);
            g.setGradientFill(grad);
        }
        else
        {
            juce::ColourGradient grad(
                zinc700, 0.0f, bounds.getY(),
                zinc800, 0.0f, bounds.getBottom(), false);
            g.setGradientFill(grad);
        }
        g.fillRoundedRectangle(bounds, 8.0f);
        
        // Inner top highlight (engraved)
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        juce::Path topHighlight;
        topHighlight.addRoundedRectangle(bounds.reduced(1.0f), 7.0f);
        g.strokePath(topHighlight, juce::PathStrokeType(0.5f));
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawHorizontalLine((int)bounds.getY() + 1, bounds.getX() + 4, bounds.getRight() - 4);
        
        // Border (dark)
        g.setColour(active ? activeColor.darker(0.5f) : zinc900);
        g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
        
        // Active glow
        if (active)
        {
            g.setColour(activeColor.withAlpha(0.3f));
            g.drawRoundedRectangle(bounds.expanded(1.0f), 9.0f, 1.5f);
        }
    }
    
    // Recessed/sunken bezel (for screens, displays, VU meters)
    inline void drawRecessedBezel(juce::Graphics& g, juce::Rectangle<float> bounds, float radius = 4.0f,
                                    juce::Colour fill = zinc950)
    {
        // Body
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, radius);
        
        // Inner top shadow (deep inset)
        g.setColour(juce::Colours::black.withAlpha(0.9f));
        g.drawRoundedRectangle(bounds, radius, 1.0f);
        
        // Inner top dark line
        juce::Path topShadow;
        topShadow.addRoundedRectangle(bounds.reduced(1.0f), radius - 1);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.strokePath(topShadow, juce::PathStrokeType(1.0f));
        
        // Tiny bottom highlight
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawHorizontalLine((int)bounds.getBottom() - 1, bounds.getX() + 2, bounds.getRight() - 2);
    }
    
    // ─── Legacy Helpers (backwards compatibility) ──────────────────────────
    
    inline void drawSkeuoButton(juce::Graphics& g, juce::Rectangle<float> bounds,
                                  bool active = false, bool hovered = false)
    {
        drawMechanicalButton(g, bounds, active, orange2);
    }
    
    inline void drawLED(juce::Graphics& g, juce::Rectangle<float> bounds,
                          juce::Colour color, bool on = true)
    {
        drawGlowLED(g, bounds, color, on);
    }
    
    inline void drawPanelBackground(juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        // Radial gradient like spec: dark zinc center, darker outer
        juce::ColourGradient grad(
            zinc800, bounds.getCentreX() * 0.6f, -50.0f,
            zinc950, bounds.getCentreX(), bounds.getBottom() * 1.0f, true);
        g.setGradientFill(grad);
        g.fillRect(bounds);
    }
    
    inline void drawHeaderBar(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
    {
        // Header gradient (darker, like nav bar in spec)
        juce::ColourGradient gradient(zinc800, 0.0f, (float)bounds.getY(),
                                       zinc900, 0.0f, (float)bounds.getBottom(), false);
        g.setGradientFill(gradient);
        g.fillRect(bounds);
        
        // Bottom border (1px dark)
        g.setColour(juce::Colours::black);
        g.drawHorizontalLine(bounds.getBottom() - 1, (float)bounds.getX(), (float)bounds.getRight());
        
        // Inner top highlight
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawHorizontalLine(bounds.getY(), (float)bounds.getX(), (float)bounds.getRight());
        
        // Glowing orange LED indicator
        auto ledRect = juce::Rectangle<float>((float)bounds.getX() + 12, (float)bounds.getCentreY() - 3, 6.0f, 6.0f);
        drawGlowLED(g, ledRect, orange2, true);
        
        // Title (DM Sans-like, uppercase, tracked widely)
        g.setColour(text2);
        g.setFont(juce::FontOptions().withName("Segoe UI").withHeight(10.0f).withStyle("Bold"));
        g.drawText(title.toUpperCase(), bounds.getX() + 28, bounds.getY(), 220, bounds.getHeight(),
                   juce::Justification::centredLeft);
    }
    
    inline void drawStepButton(juce::Graphics& g, juce::Rectangle<float> bounds, 
                                 bool active, bool current)
    {
        if (active)
        {
            // Multi-layer outer shadow
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillRoundedRectangle(bounds.translated(0, 1.5f), 3.0f);
            
            // Active orange gradient
            juce::ColourGradient grad(orange1, 0.0f, bounds.getY(),
                                       orange3, 0.0f, bounds.getBottom(), false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(bounds, 3.0f);
            
            // Top highlight (engraved white)
            g.setColour(juce::Colours::white.withAlpha(0.4f));
            g.drawHorizontalLine((int)bounds.getY() + 1, bounds.getX() + 2, bounds.getRight() - 2);
            
            // Border
            g.setColour(orange5);
            g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
            
            // Glow halo
            g.setColour(orange2.withAlpha(0.4f));
            g.drawRoundedRectangle(bounds.expanded(2.0f), 5.0f, 1.5f);
        }
        else if (aeroMode)
        {
            // Frosted aqua-glass well for inactive steps.
            juce::ColourGradient grad(juce::Colour(0xff0e4256), 0.0f, bounds.getY(),
                                      juce::Colour(0xff06212c), 0.0f, bounds.getBottom(), false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(bounds, 3.0f);
            g.setColour(juce::Colours::white.withAlpha(0.20f));
            g.drawHorizontalLine((int)bounds.getY() + 1, bounds.getX() + 2, bounds.getRight() - 2);
            g.setColour(juce::Colour(0xff0e7490).withAlpha(0.7f));
            g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
        }
        else
        {
            // Inactive: deep recessed
            juce::ColourGradient grad(zinc850, 0.0f, bounds.getY(),
                                       zinc950, 0.0f, bounds.getBottom(), false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(bounds, 3.0f);

            // Top inner shadow (deep)
            g.setColour(juce::Colours::black.withAlpha(0.8f));
            g.drawHorizontalLine((int)bounds.getY() + 1, bounds.getX() + 2, bounds.getRight() - 2);

            // Bottom highlight
            g.setColour(juce::Colours::white.withAlpha(0.03f));
            g.drawHorizontalLine((int)bounds.getBottom() - 2, bounds.getX() + 2, bounds.getRight() - 2);

            // Border
            g.setColour(juce::Colours::black);
            g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
        }
        
        // Current step (white frame + glow)
        if (current)
        {
            g.setColour(juce::Colours::white);
            g.drawRoundedRectangle(bounds, 3.0f, 1.5f);
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.drawRoundedRectangle(bounds.expanded(2.0f), 5.0f, 2.0f);
        }
    }
    
    inline void drawKnob(juce::Graphics& g, juce::Rectangle<float> bounds, float value,
                         juce::Colour color, bool showLine = true)
    {
        auto cx = bounds.getCentreX();
        auto cy = bounds.getCentreY();
        auto radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        
        // Outer shadow
        g.setColour(juce::Colours::black.withAlpha(0.9f));
        g.fillEllipse(bounds.translated(0, 2.0f));
        
        // Outer ring (radial gradient simulating brushed metal)
        juce::ColourGradient ringGrad(
            zinc600, cx - radius * 0.3f, cy - radius * 0.3f,
            zinc900, cx + radius, cy + radius, true);
        g.setGradientFill(ringGrad);
        g.fillEllipse(cx - radius, cy - radius, radius * 2, radius * 2);
        
        // Inner cap (recessed)
        auto innerR = radius - 3;
        juce::ColourGradient capGrad(
            zinc750, cx, cy - innerR,
            zinc900, cx, cy + innerR, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(cx - innerR, cy - innerR, innerR * 2, innerR * 2);
        
        // Top inner highlight
        g.setColour(juce::Colours::white.withAlpha(0.2f));
        g.drawEllipse(cx - innerR, cy - innerR + 0.5f, innerR * 2, innerR * 2, 0.5f);
        
        // Border
        g.setColour(juce::Colours::black);
        g.drawEllipse(cx - radius, cy - radius, radius * 2, radius * 2, 1.0f);
        
        // Indicator line (rotating)
        if (showLine)
        {
            float angle = (value - 0.5f) * juce::MathConstants<float>::twoPi * 0.75f;
            float lineLen = radius - 4;
            float lineX1 = cx + std::sin(angle) * 2.0f;
            float lineY1 = cy - std::cos(angle) * 2.0f;
            float lineX2 = cx + std::sin(angle) * lineLen;
            float lineY2 = cy - std::cos(angle) * lineLen;
            // Glow
            g.setColour(color.withAlpha(0.4f));
            g.drawLine(lineX1, lineY1, lineX2, lineY2, 4.0f);
            // Main line
            g.setColour(color);
            g.drawLine(lineX1, lineY1, lineX2, lineY2, 2.0f);
        }
    }
}
