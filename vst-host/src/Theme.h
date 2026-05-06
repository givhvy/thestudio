#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// FL Studio + Engineered/Skeuomorphic theme - matches design spec
namespace Theme
{
    // Backgrounds (zinc palette from spec)
    static const juce::Colour zinc950 = juce::Colour(0xff09090b);  // Darkest
    static const juce::Colour zinc900 = juce::Colour(0xff18181b);
    static const juce::Colour zinc850 = juce::Colour(0xff141417);
    static const juce::Colour zinc825 = juce::Colour(0xff121214);
    static const juce::Colour zinc800 = juce::Colour(0xff27272a);
    static const juce::Colour zinc750 = juce::Colour(0xff2a2a2e);
    static const juce::Colour zinc700 = juce::Colour(0xff3f3f46);
    static const juce::Colour zinc600 = juce::Colour(0xff52525b);
    static const juce::Colour zinc500 = juce::Colour(0xff71717a);
    static const juce::Colour zinc400 = juce::Colour(0xffa1a1aa);
    static const juce::Colour zinc300 = juce::Colour(0xffd4d4d8);
    static const juce::Colour zinc200 = juce::Colour(0xffe4e4e7);
    static const juce::Colour zinc100 = juce::Colour(0xfffafafa);
    
    // Aliases (backwards compatibility)
    static const juce::Colour bg0 = zinc950;
    static const juce::Colour bg1 = zinc825;
    static const juce::Colour bg2 = zinc850;
    static const juce::Colour bg3 = zinc750;
    static const juce::Colour bg4 = zinc900;
    static const juce::Colour bg5 = zinc850;
    static const juce::Colour bg6 = zinc800;
    static const juce::Colour bg7 = zinc800;
    static const juce::Colour bg8 = zinc700;
    
    static const juce::Colour text1 = zinc100;
    static const juce::Colour text2 = zinc200;
    static const juce::Colour text3 = zinc300;
    static const juce::Colour text4 = zinc400;
    static const juce::Colour text5 = zinc500;
    static const juce::Colour text6 = zinc600;
    static const juce::Colour text7 = zinc700;
    
    // Orange (FL Studio brand + spec)
    static const juce::Colour orange1 = juce::Colour(0xfffb923c);  // orange-400
    static const juce::Colour orange2 = juce::Colour(0xfff97316);  // orange-500 main
    static const juce::Colour orange3 = juce::Colour(0xffea580c);  // orange-600
    static const juce::Colour orange4 = juce::Colour(0xffc2410c);  // orange-700
    static const juce::Colour orange5 = juce::Colour(0xff7c2d12);  // orange-900
    static const juce::Colour orange6 = juce::Colour(0xff431407);  // orange-950
    
    // Reverb purple
    static const juce::Colour purple = juce::Colour(0xff8b5cf6);
    static const juce::Colour purpleBright = juce::Colour(0xffa78bfa);
    
    // Status colors
    static const juce::Colour green1 = juce::Colour(0xff86efac);
    static const juce::Colour green2 = juce::Colour(0xff22c55e);
    static const juce::Colour green3 = juce::Colour(0xff16a34a);
    static const juce::Colour red1 = juce::Colour(0xfffca5a5);
    static const juce::Colour red2 = juce::Colour(0xffef4444);
    static const juce::Colour yellow1 = juce::Colour(0xfffde68a);
    static const juce::Colour yellow2 = juce::Colour(0xfff59e0b);
    static const juce::Colour blue = juce::Colour(0xff3b82f6);
    
    static const juce::Colour accent = orange2;
    static const juce::Colour accentBright = orange1;
    static const juce::Colour accentDim = orange3;
    static const juce::Colour green = green2;
    static const juce::Colour red = red2;
    static const juce::Colour yellow = yellow2;
    static const juce::Colour reverbPurple = purple;
    static const juce::Colour reverbPurpleBright = purpleBright;
    static const juce::Colour border1 = juce::Colours::black;
    static const juce::Colour border2 = zinc900;
    static const juce::Colour border3 = zinc700;
    
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
